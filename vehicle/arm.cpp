#include "vehicle/arm.hpp"

#include "vehicle/arm/runtime/arm_config.hpp"
#include "vehicle/arm/runtime/arm_runtime.hpp"

namespace vehicle::arm
{

void run() noexcept
{
    runtime::start(arm_configuration());
}

} // namespace vehicle::arm
