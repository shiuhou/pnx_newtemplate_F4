# 靜態 contract：多通道 PWM 的板級實作無法在 host 編譯（需要 HAL），
# 故此處驗證 bsp_pwm.cpp 與 pwm_channels.hpp 的關鍵不變量：
#   - 四路舵機通道常數存在且對到 TIM1 CH1..CH4 / PE9,PE11,PE13,PE14
#   - 共用單一 TIM1 時基（單一 handle、單一 ARR）
#   - 每通道獨立 compare/start 狀態
#   - 只有最後一路停止時才釋放 timer 與腳位
if(NOT DEFINED PNX_PWM_SOURCE OR NOT DEFINED PNX_PWM_CHANNELS)
    message(FATAL_ERROR
        "pwm_multichannel_source_contract requires PNX_PWM_SOURCE and PNX_PWM_CHANNELS")
endif()

file(READ "${PNX_PWM_SOURCE}" pwm_source)
file(READ "${PNX_PWM_CHANNELS}" channels_source)

# --- 通道常數（fake 與板級共用的唯一來源） ---
foreach(constant IN ITEMS servo_c2 servo_pe9 servo_pe13 servo_pe14)
    string(FIND "${channels_source}" "channel ${constant}{" found_index)
    if(found_index EQUAL -1)
        message(FATAL_ERROR
            "pwm_channels.hpp must declare channel ${constant}")
    endif()
endforeach()

string(REGEX MATCH
    "servo_channel_count[ \t]*=[ \t]*([0-9]+)U"
    count_match "${channels_source}")
if(NOT count_match)
    message(FATAL_ERROR "pwm_channels.hpp must declare servo_channel_count")
endif()
if(NOT CMAKE_MATCH_1 EQUAL 4)
    message(FATAL_ERROR
        "servo_channel_count must be 4; found ${CMAKE_MATCH_1}")
endif()

# 邏輯通道值必須互異，否則 binding 查找會撞到同一路。
foreach(slot IN ITEMS 0 1 2 3)
    string(REGEX MATCHALL "channel servo_[a-z0-9_]+\\{${slot}U\\}"
        slot_matches "${channels_source}")
    list(LENGTH slot_matches slot_count)
    if(NOT slot_count EQUAL 1)
        message(FATAL_ERROR
            "Exactly one servo channel must use logical slot ${slot}; found ${slot_count}")
    endif()
endforeach()

# --- TIM1 硬體通道與腳位綁定 ---
foreach(pair IN ITEMS
    "servo_c2,TIM_CHANNEL_2,GPIO_PIN_11"
    "servo_pe9,TIM_CHANNEL_1,GPIO_PIN_9"
    "servo_pe13,TIM_CHANNEL_3,GPIO_PIN_13"
    "servo_pe14,TIM_CHANNEL_4,GPIO_PIN_14"
)
    string(REPLACE "," ";" fields "${pair}")
    list(GET fields 0 logical)
    list(GET fields 1 hw_channel)
    list(GET fields 2 pin)
    string(FIND "${pwm_source}"
        "{board::pwm::${logical}, ${hw_channel}, ${pin}}" binding_index)
    if(binding_index EQUAL -1)
        message(FATAL_ERROR
            "bsp_pwm.cpp must bind ${logical} to ${hw_channel}/${pin}")
    endif()
endforeach()

# --- 單一共用時基 ---
string(REGEX MATCHALL "TIM_HandleTypeDef[ \t]+[a-z_]+\\{\\}"
    handle_matches "${pwm_source}")
list(LENGTH handle_matches handle_count)
if(NOT handle_count EQUAL 1)
    message(FATAL_ERROR
        "Servo channels must share exactly one TIM handle; found ${handle_count}")
endif()

string(FIND "${pwm_source}" "timer.Instance = TIM1;" instance_index)
if(instance_index EQUAL -1)
    message(FATAL_ERROR "Shared PWM time base must remain TIM1")
endif()

# 週期是 timer 層級的單一狀態，不可退回每通道週期陣列。
string(REGEX MATCHALL "shared_period_us" shared_period_matches "${pwm_source}")
list(LENGTH shared_period_matches shared_period_count)
if(shared_period_count LESS 4)
    message(FATAL_ERROR
        "bsp_pwm.cpp must track one shared period; found ${shared_period_count} references")
endif()
string(FIND "${pwm_source}" "periods_us" per_channel_period_index)
if(NOT per_channel_period_index EQUAL -1)
    message(FATAL_ERROR
        "Shared ARR forbids a per-channel period table (periods_us)")
endif()

# 1MHz tick + 20ms 預設週期 = 50Hz 舵機時基。
string(FIND "${pwm_source}" "(clock_hz / 1000000U) - 1U" prescaler_index)
if(prescaler_index EQUAL -1)
    message(FATAL_ERROR "PWM prescaler must produce a 1MHz tick")
endif()
string(REGEX MATCH
    "default_period_us[ \t]*=[ \t]*([0-9]+)U"
    default_period_match "${pwm_source}")
if(NOT default_period_match)
    message(FATAL_ERROR "bsp_pwm.cpp must declare default_period_us")
endif()
if(NOT CMAKE_MATCH_1 EQUAL 20000)
    message(FATAL_ERROR
        "Servo default period must be 20000us (50Hz); found ${CMAKE_MATCH_1}")
endif()

# --- 每通道獨立輸出狀態 ---
string(FIND "${pwm_source}" "channel_started" started_index)
if(started_index EQUAL -1)
    message(FATAL_ERROR
        "bsp_pwm.cpp must track per-channel started state")
endif()

# compare 寫入必須帶該通道的 binding，不能寫死單一 timer_channel。
string(REGEX MATCHALL "__HAL_TIM_SET_COMPARE\\(&timer, binding->timer_channel"
    compare_matches "${pwm_source}")
list(LENGTH compare_matches compare_count)
if(compare_count LESS 2)
    message(FATAL_ERROR
        "Pulse and stop paths must address the selected channel's compare register")
endif()
string(REGEX MATCH "constexpr std::uint32_t timer_channel"
    fixed_channel_match "${pwm_source}")
if(fixed_channel_match)
    message(FATAL_ERROR
        "Multi-channel PWM must not keep a single fixed timer_channel constant")
endif()

# 上電零輸出：初始化時每通道 Pulse 為 0。
string(FIND "${pwm_source}" "output.Pulse = 0U;" zero_pulse_index)
if(zero_pulse_index EQUAL -1)
    message(FATAL_ERROR "Channel configuration must start at zero pulse")
endif()

# 縮短共用 ARR 前必須檢查所有既有 CCR，避免 CCR > ARR 形成整週期有效輸出。
string(FIND "${pwm_source}"
    "__HAL_TIM_GET_COMPARE(&timer, candidate.timer_channel)"
    compare_read_index)
string(FIND "${pwm_source}"
    "__HAL_TIM_SET_AUTORELOAD(&timer, period_us - 1U)"
    arr_write_index)
if(compare_read_index EQUAL -1)
    message(FATAL_ERROR
        "set_period_us() must inspect every existing channel compare")
endif()
if(arr_write_index EQUAL -1 OR NOT compare_read_index LESS arr_write_index)
    message(FATAL_ERROR
        "Existing compares must be validated before shortening the shared ARR")
endif()

# --- 共用資源只在最後一路停止時釋放 ---
string(FIND "${pwm_source}" "if (!any_channel_started())" guard_index)
string(FIND "${pwm_source}" "HAL_TIM_PWM_DeInit(&timer)" deinit_index)
string(FIND "${pwm_source}" "__HAL_RCC_TIM1_CLK_DISABLE()" clk_disable_index)
if(guard_index EQUAL -1)
    message(FATAL_ERROR
        "stop() must guard shared teardown with any_channel_started()")
endif()
if(deinit_index EQUAL -1 OR clk_disable_index EQUAL -1)
    message(FATAL_ERROR "stop() must still release TIM1 on the last channel")
endif()
if(NOT guard_index LESS deinit_index OR NOT guard_index LESS clk_disable_index)
    message(FATAL_ERROR
        "Shared TIM1 teardown must happen inside the last-channel guard")
endif()
