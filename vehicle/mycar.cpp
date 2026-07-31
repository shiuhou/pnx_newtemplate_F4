#include "vehicle/mycar.hpp"

#include "vehicle/chassis/config.hpp"
#include "vehicle/chassis/runtime.hpp"

namespace vehicle::mycar
{

void run() noexcept
{
    chassis::runtime::start(chassis::mycar_configuration());
}

} // namespace vehicle::mycar
