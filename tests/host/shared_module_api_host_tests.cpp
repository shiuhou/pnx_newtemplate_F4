#include "bmi088.hpp"
#include "lkmotors.hpp"
#include "pid.hpp"
#include "remoter.hpp"

#include <type_traits>

using bmi088_read_acc =
    bool (imu::bmi088::*)(imu::accdata*);
static_assert(std::is_same_v<
              decltype(&imu::bmi088::read_acc),
              bmi088_read_acc>);

using pid_reset_state =
    void (control::pid::*)(float, float);
static_assert(std::is_same_v<
              decltype(&control::pid::reset_state),
              pid_reset_state>);

static_assert(std::is_base_of_v<
              motors::lkmotor, motors::lk9025>);
static_assert(
    remoter::ps2_config{}.uart_port == app::uart::ps2);

int main()
{
    return 0;
}
