#include "vehicle/chassis/runtime/config.hpp"

#include <cmath>

namespace vehicle::chassis
{

// 這個函式集中保存「只屬於這一台 MyCar」的參數。
// 修改車體尺寸、搖桿速度、馬達方向或 PI 時，優先從這裡開始看；
// 不要把這些車輛差異寫進共享的 pnx_* submodule 或 F407 Board 層。
configuration mycar_configuration() noexcept
{
    // 幾何、輪序、馬達方向與 DR16 映射均已完成實車驗證；速度與 P 控制值
    // 是目前可駕駛基線，競賽級調參仍應另行量測與驗證。
    return {
        // 1. 底盤幾何：
        // {有效輪半徑 m, 前後輪中心距的一半 m,
        //  左右輪中心距的一半 m, 任一輪允許的最大輸出端角速度 rad/s}
        {0.038F, 0.070F, 0.1275F, 40.0F},

        // 2. DR16 手動控制：
        // {搖桿死區, 最大前後速度 m/s, 最大橫移速度 m/s,
        //  最大旋轉速度 rad/s, 前後軸方向, 橫移軸方向, 旋轉軸方向}
        // 三個方向只能填 +1 或 -1；以下為 2026-08-04 實車驗證值。
        {0.05F, 1.50F, 1.50F, 2.00F, 1.0F, -1.0F, -1.0F},

        // 3. 正常行駛速度斜坡：
        // {平移加速度 m/s^2, 平移減速度 m/s^2,
        //  yaw 加速度 rad/s^2, yaw 減速度 rad/s^2}
        // safety 事件不經此斜坡，仍在當週期立即歸零。
        {4.5F, 6.0F, 6.0F, 9.0F},

        // 4. 馬達方向，固定按 FL/FR/RL/RR（左前／右前／左後／右後）排列：
        // +1 表示「車輪正方向」與該馬達回報的正速度相同，-1 表示相反。
        // 架空方向測試確認：後側兩顆 encoder 正方向與車輪正方向相反。
        // 不能按實體 ID 1/2/3/4 順序重排。
        {1.0F, 1.0F, -1.0F, -1.0F},

        // 5. 四輪共用 PI：
        // {Kp raw/(rad/s), Ki raw/(rad/s*s), 積分輸出上限 raw,
        //  C610 torque-current 指令上限 raw}
        // 第二階段仍只啟用 P；2000 raw 將 C610 轉矩電流限制在約 2 A。
        {400.0F, 0.0F, 0.0F, 2000.0F},

        // 6. 控制週期，單位秒：0.005 s = 5 ms = 200 Hz。
        0.005F,
    };
}

// 啟動前的總設定檢查。任何一組必要值無效，都讓 controller/runtime
// 維持零輸出；這是安全門，不是單純用來顯示設定錯誤。
bool valid(const configuration& config) noexcept
{
    // control_period_s 也是執行期排程契約；目前 runtime 固定為 5 ms。
    const controller_configuration controller_config{
        config.geometry,
        config.manual,
        config.command_slew,
        config.motor_direction,
        config.pi,
    };
    return valid(controller_config) &&
           std::isfinite(config.control_period_s) &&
           config.control_period_s > 0.0F;
}

} // namespace vehicle::chassis
