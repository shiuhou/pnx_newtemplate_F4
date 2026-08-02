#include "vehicle/mycar.hpp"

#include "vehicle/chassis/runtime/config.hpp"
#include "vehicle/chassis/runtime/runtime.hpp"

namespace vehicle::mycar
{

// MyCar 車輛組合的唯一入口：
// 1. 從 config.cpp 取出這台車的幾何、DR16、方向與 PI 設定；
// 2. 把設定交給 chassis runtime 建立馬達、遙控器與控制 thread。
// Board/HAL 初始化在更下層完成，這裡不直接碰 STM32 handle。
void run() noexcept
{
    chassis::runtime::start(chassis::mycar_configuration());
}

} // namespace vehicle::mycar
