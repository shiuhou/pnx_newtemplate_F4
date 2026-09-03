#include "vehicle/combined.hpp"

#include "vehicle/arm/runtime/arm_config.hpp"
#include "vehicle/chassis/runtime/config.hpp"
#include "vehicle/combined/runtime/runtime.hpp"
#include "vehicle/rfid/reader.hpp"

#include "config.hpp"

namespace vehicle::combined
{

void run() noexcept
{
    if constexpr (config::feature::has_rfid)
    {
        (void)rfid::init();
    }
    runtime::start(chassis::mycar_configuration(),
                   arm::arm_configuration());
}

} // namespace vehicle::combined
