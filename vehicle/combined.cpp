#include "vehicle/combined.hpp"

#include "vehicle/arm/runtime/arm_config.hpp"
#include "vehicle/chassis/runtime/config.hpp"
#include "vehicle/combined/runtime/runtime.hpp"

namespace vehicle::combined
{

void run() noexcept
{
    runtime::start(chassis::mycar_configuration(),
                   arm::arm_configuration());
}

} // namespace vehicle::combined
