#include "vehicle/chassis/runtime/runtime.hpp"

#include <config.hpp>
#include <djimotorhandler.hpp>
#include <msg.hpp>
#include <remoter.hpp>
#include <robot_config.hpp>
#include <tx_api.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace vehicle::chassis::runtime
{
namespace
{

// 本檔是車輛層的「接線員」：它把 ThreadX、DR16 訊息、CAN、M2006 driver
// 與純 controller 串起來。運動學與 PI 不應在這裡重寫。

// ---- 排程與資料新鮮度契約 -------------------------------------------------

constexpr std::uint32_t control_period_ticks = 5U;
// 1 kHz ThreadX tick 下的 200 Hz 控制迴圈；config.cpp 必須同樣填 0.005 s。
constexpr float control_period_s = 0.005F;
constexpr UINT control_priority = 6U;              // ThreadX：數字越小優先級越高。
// 實機啟動時 1024 B 曾在 msg/ThreadX mutex 路徑逼近 stack 底界並觸發
// IMPRECISERR HardFault；2048 B 為控制呼叫鏈保留約 1 KiB 額外餘量。
constexpr std::size_t control_stack_bytes = 2048U;
constexpr std::uint32_t remote_freshness_ticks = 120U; // DR16 超過 120 ms 視為離線。
constexpr UINT remote_ingest_priority = 4U;        // ingest 高於 control，先更新快照。
constexpr std::size_t remote_ingest_stack_bytes = 768U;

static_assert(TX_TIMER_TICKS_PER_SECOND == 1000U,
              "MyCar runtime requires a one-millisecond ThreadX tick");

controller_configuration controller_config_of(
    const configuration& config) noexcept
{
    // runtime 專用的 control_period_s 不屬於純 controller，這裡只取控制器需要的四組資料。
    return {
        config.geometry,
        config.manual,
        config.motor_direction,
        config.pi,
    };
}

// ---- 四顆實體馬達與固定輪序 -----------------------------------------------

// robot::motors::* 由 configs/vehicles/mycar/robot.json 產生，內含 CAN1、ID
// 與初始 relax mode；此處將生成設定實例化成真正的 M2006 物件。
motors::m2006 front_left{robot::motors::front_left};
motors::m2006 front_right{robot::motors::front_right};
motors::m2006 rear_left{robot::motors::rear_left};
motors::m2006 rear_right{robot::motors::rear_right};
std::array<motors::m2006*, 4U> motor_order{
    // 所有車輛層陣列固定 FL/FR/RL/RR；對應 feedback ID 是 201/202/204/203。
    &front_left,
    &front_right,
    &rear_left,
    &rear_right,
};
motors::djimotorhandler& motor_handler =
    motors::djimotorhandler::instance();

// ---- ThreadX 物件與跨 thread 快照 ----------------------------------------

TX_THREAD control_thread{};
alignas(8) std::uint8_t control_stack[control_stack_bytes]{};
CHAR control_thread_name[] = "mycar control";
TX_THREAD remote_ingest_thread{};
alignas(8) std::uint8_t remote_ingest_stack[remote_ingest_stack_bytes]{};
CHAR remote_ingest_thread_name[] = "mycar remote";

struct remote_ingest_snapshot {
    remoter::state state{};       // 最近成功讀到的 remoter::state。
    std::uint32_t sample_tick{};  // 收到該狀態時的 ThreadX 1 ms tick。
    bool seen{};                  // 自上電後是否至少收到過一筆狀態。
};

// ---- Runtime 唯一狀態 -----------------------------------------------------

configuration runtime_configuration{};
controller runtime_controller{controller_config_of(runtime_configuration)};
runtime_policy policy{false, false, {}};
msg::subscriber remote_subscriber{};
remote_ingest_snapshot shared_remote{};
telemetry runtime_telemetry{};

std::array<bool, 4U> registration_results{}; // FL/FR/RL/RR 各自的註冊結果。
bool any_motor_registered{};                 // 至少一顆可讓 handler 組出 0x200 零幀。
bool runtime_start_attempted{};              // 防止重複建 thread/註冊馬達。
bool startup_zero_sent{};                    // 啟動失敗路徑是否已嘗試送過一次零幀。
bool watchdog_sampled{};                     // 是否已有一次真實 alive_check 結果。
bool handler_all_online{};                   // 最近 alive_check 是否四顆都有新回饋。
watchdog_phase motor_watchdog_phase{};
std::uint32_t control_loop_count{};           // 已開始的控制週期數。
std::uint32_t control_overrun_count{};        // deadline 超時累計。

// ---- 馬達輸出助手 ---------------------------------------------------------

void relax_all() noexcept
{
    // relax 會把 motor command 清零；control thread 仍持續送 0x200 零幀。
    for (motors::m2006* const motor : motor_order)
    {
        motor->relax();
    }
}

void set_all_current(
    const std::array<std::int16_t, 4U>& current_raw) noexcept
{
    // current_raw 與 motor_order 使用相同 FL/FR/RL/RR 索引；DJI handler 最後
    // 會依每顆 motor 的 CAN ID 放進 0x200 正確的兩個 byte 槽。
    for (std::size_t index = 0U; index < motor_order.size(); ++index)
    {
        motor_order[index]->set_current(current_raw[index]);
    }
}

void publish_telemetry(const telemetry& next) noexcept
{
    // telemetry 由 control thread 寫、debugger/其他讀者讀；短暫關中斷避免
    // 讀到一半新、一半舊的撕裂快照。
    TX_INTERRUPT_SAVE_AREA
    TX_DISABLE
    runtime_telemetry = next;
    TX_RESTORE
}

remote_ingest_snapshot copy_remote_snapshot() noexcept
{
    // control thread 每週期複製一次，不直接與 ingest thread 共用可變物件。
    remote_ingest_snapshot copy{};
    TX_INTERRUPT_SAVE_AREA
    TX_DISABLE
    copy = shared_remote;
    TX_RESTORE
    return copy;
}

void publish_remote_snapshot(
    const remote_ingest_snapshot& next) noexcept
{
    // ingest thread 是唯一 writer；短暫關中斷完成固定大小複製。
    TX_INTERRUPT_SAVE_AREA
    TX_DISABLE
    shared_remote = next;
    TX_RESTORE
}

void sync_fault_telemetry(const bsp::can::telemetry& can) noexcept
{
    // 啟動早期尚未進入正常 loop 時，也留下可由 debug_state() 讀到的故障原因。
    telemetry next{};
    next.state = policy.reported_state(safety_state::disabled);
    next.faults = policy.faults();
    next.watchdog_sampled = watchdog_sampled;
    next.can = can;
    publish_telemetry(next);
}

void latch_overrun() noexcept
{
    // 週期超時代表「算完但送得太晚」；鎖故障並清 PI，當次及後續都歸零。
    policy.latch(runtime_fault::overrun);
    runtime_controller.reset();
    ++control_overrun_count;
}

void send_startup_zero_if_possible(
    const bsp::can::telemetry& can) noexcept
{
    // 啟動失敗時不建立正常控制能力；若 handler 至少認得一顆馬達且 CAN active，
    // 仍盡力送一次 relax/零命令。這只是呼叫證據，不是總線送達證據。
    relax_all();
    if (!startup_zero_sent && any_motor_registered &&
        can.bus_state == bsp::can::state::active)
    {
        motor_handler.send_control();
        startup_zero_sent = true;
    }
}

void terminal_startup_failure(runtime_fault fault) noexcept
{
    // 所有不可繼續的啟動錯誤都經過同一路徑：latch -> telemetry -> 嘗試零幀。
    policy.latch(fault);
    const auto can = bsp::can::snapshot(bsp::can::bus::can1);
    runtime_policy_input observation{};
    observation.can = can;
    (void)policy.update(observation);
    sync_fault_telemetry(can);
    send_startup_zero_if_possible(can);
}

void sleep_until(std::uint32_t deadline) noexcept
{
    // 只睡到絕對 deadline，避免每次「工作時間 + 固定 sleep」造成週期逐漸漂移。
    const std::uint32_t now =
        static_cast<std::uint32_t>(tx_time_get());
    const std::int32_t remaining =
        static_cast<std::int32_t>(deadline - now);
    if (remaining > 0)
    {
        tx_thread_sleep(static_cast<ULONG>(remaining));
    }
}

void remote_ingest_entry(ULONG)
{
    // ---- DR16 ingest thread ------------------------------------------------
    // 唯一消費 remoter 訊息的 thread；收到新資料就連同時間戳發布快照。
    for (;;)
    {
        remoter::state latest{};
        if (msg::read(remote_subscriber, latest) == types::status::ok)
        {
            remote_ingest_snapshot next{};
            next.state = latest;
            next.sample_tick =
                static_cast<std::uint32_t>(tx_time_get());
            next.seen = true;
            publish_remote_snapshot(next);
        }
        else
        {
            tx_thread_sleep(1U);
        }
    }
}

void control_entry(ULONG)
{
    // ---- 200 Hz control thread 啟動 ----------------------------------------
    // 先初始化 remoter、建立訂閱與 ingest thread；任一步失敗都走終止啟動路徑。
    ::remoter::config config{};
    config.dr16.thread_priority = params::remoter::thread_priority;
    config.dr16.rx_timeout_ticks = params::remoter::rx_timeout_ticks;
    config.thread_priority = params::remoter::thread_priority + 1U;
    config.offline_timeout_ticks = remote_freshness_ticks;

    if (!::remoter::service::instance().init(config))
    {
        terminal_startup_failure(runtime_fault::remoter_init_failed);
        return;
    }

    remote_subscriber = msg::subscribe<::remoter::state>();
    if (!remote_subscriber.valid())
    {
        terminal_startup_failure(runtime_fault::subscribe_failed);
        return;
    }

    if (tx_thread_create(
            &remote_ingest_thread, remote_ingest_thread_name,
            remote_ingest_entry, 0U, remote_ingest_stack,
            sizeof(remote_ingest_stack), remote_ingest_priority,
            remote_ingest_priority, TX_NO_TIME_SLICE,
            TX_AUTO_START) != TX_SUCCESS)
    {
        terminal_startup_failure(runtime_fault::thread_create_failed);
        return;
    }

    // deadline 指向「本週期必須完成並送出 CAN」的絕對 tick。
    std::uint32_t next_deadline =
        static_cast<std::uint32_t>(tx_time_get()) + control_period_ticks;

    for (;;)
    {
        // STEP 1：取得 DR16 快照並檢查是否超過 120 ms。
        const auto remote_snapshot = copy_remote_snapshot();
        const std::uint32_t now =
            static_cast<std::uint32_t>(tx_time_get());
        remoter::state control_remote = remote_snapshot.state;
        if (!remote_snapshot_fresh(
                remote_snapshot.seen, remote_snapshot.sample_tick,
                now, remote_freshness_ticks))
        {
            // 保留的最後一筆搖桿值不能繼續驅動車；明確覆寫為 offline/none。
            control_remote.offline = true;
            control_remote.active_source = remoter::source::none;
        }

        // STEP 2：取得 CAN1 健康快照及四顆 M2006 的最新輸出端速度。
        const auto can = bsp::can::snapshot(bsp::can::bus::can1);
        const std::uint32_t cycle = control_loop_count + 1U;
        wheel_vector measured{};
        const bool sample_watchdog = motor_watchdog_phase.advance();

        TX_INTERRUPT_SAVE_AREA
        TX_DISABLE
        for (std::size_t index = 0U; index < motor_order.size(); ++index)
        {
            measured.rad_s[index] = motor_order[index]->get_feedback().velocity;
        }
        if (sample_watchdog)
        {
            // alive_check 只判斷自上個 20 ms 視窗後是否有新回饋，不判斷速度大小。
            handler_all_online = motor_handler.alive_check();
            watchdog_sampled = true;
        }
        TX_RESTORE

        // STEP 3：將 DR16/CAN/watchdog 觀測交給 runtime_policy，形成 force_zero。
        runtime_policy_input policy_input{};
        policy_input.remote = control_remote;
        policy_input.can = can;
        policy_input.watchdog_sampled = watchdog_sampled;
        policy_input.handler_all_online = handler_all_online;
        auto policy_output = policy.update(policy_input);
        // STEP 4：純 controller 完成搖桿映射、麥輪逆解、安全狀態與四輪 PI。
        const auto controller_safety = controller_safety_for(policy_output);
        const auto controller_output = runtime_controller.update(
            policy_output.manual, measured, controller_safety,
            control_period_s);
        if (trusted_release_observed(policy_output))
        {
            // fresh DR16 明確釋放時清 PI，下一次解鎖從零積分開始。
            runtime_controller.reset();
        }

        // STEP 5：在選擇 current 前先檢查是否已錯過 deadline。
        bool iteration_overrun = deadline_reached(
            static_cast<std::uint32_t>(tx_time_get()), next_deadline);
        if (iteration_overrun)
        {
            latch_overrun();
            policy_output.force_zero = true;
            policy_output.fault_latched = true;
        }

        // STEP 6：兩層安全都通過才採用 controller current，否則四輪 relax。
        const bool current_selected =
            should_set_current(policy_output, controller_output.state);
        if (current_selected)
        {
            set_all_current(controller_output.motor_current_raw);
        }
        else
        {
            // policy 或 safety gate 任一未通過，都覆蓋先前可能保存的馬達命令。
            relax_all();
        }

        // STEP 7：DJI handler 依 motor CAN ID 封裝 0x200 並呼叫 BSP transmit。
        // 目前 handler 回傳 void，所以這裡只能證明已呼叫，不能證明 CAN 實體送達；
        // 失敗只能由之後的 CAN telemetry 或外部分析儀觀察。
        motor_handler.send_control();

        const std::uint32_t after_send =
            static_cast<std::uint32_t>(tx_time_get());
        if (!iteration_overrun &&
            deadline_reached(after_send, next_deadline))
        {
            iteration_overrun = true;
            latch_overrun();
        }

        // STEP 8：保存這一週期的目標、回饋、實際選中 current 與安全狀態。
        telemetry next_telemetry{};
        next_telemetry.watchdog_sampled = watchdog_sampled;
        next_telemetry.loop_count = cycle;
        next_telemetry.overrun_count = control_overrun_count;
        next_telemetry.remote_update_count = control_remote.update_count;
        next_telemetry.target_rad_s =
            controller_output.wheel_target_rad_s.rad_s;
        next_telemetry.measured_rad_s = measured.rad_s;
        next_telemetry.current_raw =
            current_selected ? controller_output.motor_current_raw
                             : std::array<std::int16_t, 4U>{};
        next_telemetry.can = can;

        if (!iteration_overrun &&
            deadline_reached(
                static_cast<std::uint32_t>(tx_time_get()), next_deadline))
        {
            iteration_overrun = true;
            latch_overrun();
            next_telemetry.overrun_count = control_overrun_count;
        }
        next_telemetry.state =
            policy.reported_state(controller_output.state);
        next_telemetry.faults = policy.faults();
        publish_telemetry(next_telemetry);

        if (!iteration_overrun &&
            deadline_reached(
                static_cast<std::uint32_t>(tx_time_get()), next_deadline))
        {
            iteration_overrun = true;
            latch_overrun();
            next_telemetry.overrun_count = control_overrun_count;
            next_telemetry.state =
                policy.reported_state(controller_output.state);
            next_telemetry.faults = policy.faults();
            publish_telemetry(next_telemetry);
        }
        control_loop_count = cycle;

        // STEP 9：準時就睡到原定 deadline；超時則從現在重新建立下一個週期，
        // 避免在落後狀態連續追趕而進一步壓縮安全裕量。
        if (iteration_overrun)
        {
            next_deadline =
                static_cast<std::uint32_t>(tx_time_get()) +
                control_period_ticks;
        }
        sleep_until(next_deadline);
        next_deadline += control_period_ticks;
    }
}

} // namespace

void start(const configuration& config) noexcept
{
    // ---- 車輛 runtime 一次性啟動 ------------------------------------------
    // 第二次呼叫直接返回，避免重複註冊 CAN handler 或建立重複 thread。
    if (runtime_start_attempted)
    {
        return;
    }
    runtime_start_attempted = true;
    // 保存設定並重建 controller/policy/telemetry 的上電初始狀態。
    runtime_configuration = config;
    runtime_controller = controller{controller_config_of(config)};
    policy = runtime_policy{false, false, {}};
    remote_subscriber = {};
    publish_remote_snapshot({});
    publish_telemetry({});
    watchdog_sampled = false;
    handler_all_online = false;
    motor_watchdog_phase = {};
    control_loop_count = 0U;
    control_overrun_count = 0U;

    // 註冊順序必須與所有四輪陣列一致：FL/FR/RL/RR。
    registration_results[0] =
        motor_handler.register_motor(front_left);
    registration_results[1] =
        motor_handler.register_motor(front_right);
    registration_results[2] =
        motor_handler.register_motor(rear_left);
    registration_results[3] =
        motor_handler.register_motor(rear_right);

    // any_registered 用於失敗時盡力送零；all_registered 才允許建立正常 runtime。
    any_motor_registered = registration_results[0] ||
                           registration_results[1] ||
                           registration_results[2] ||
                           registration_results[3];
    const bool all_registered = registration_results[0] &&
                                registration_results[1] &&
                                registration_results[2] &&
                                registration_results[3];
    // 註冊第一顆 DJI motor 時 handler 會初始化 CAN；完成四顆註冊後才取基線。
    const auto can_baseline =
        bsp::can::snapshot(bsp::can::bus::can1);
    const bool config_valid =
        valid(runtime_configuration) &&
        runtime_configuration.control_period_s == control_period_s;
    policy = runtime_policy{config_valid, all_registered, can_baseline};
    // 設定非法仍可建立觀測用 control thread，但 policy 會 latch invalid_config
    // 並永久 force_zero；註冊失敗則不建立 control thread，只嘗試啟動零幀。
    sync_fault_telemetry(can_baseline);

    if (!all_registered)
    {
        send_startup_zero_if_possible(can_baseline);
        return;
    }

    if (tx_thread_create(
            &control_thread, control_thread_name, control_entry, 0U,
            control_stack, sizeof(control_stack), control_priority,
            control_priority, TX_NO_TIME_SLICE,
            TX_AUTO_START) != TX_SUCCESS)
    {
        terminal_startup_failure(runtime_fault::thread_create_failed);
    }
}

telemetry debug_state() noexcept
{
    // 回傳副本而不是內部引用，避免呼叫者在控制 thread 更新時看到撕裂資料。
    telemetry copy{};
    TX_INTERRUPT_SAVE_AREA
    TX_DISABLE
    copy = runtime_telemetry;
    TX_RESTORE
    return copy;
}

} // namespace vehicle::chassis::runtime
