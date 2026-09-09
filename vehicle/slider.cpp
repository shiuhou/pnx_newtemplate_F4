#include "vehicle/slider.hpp"

#include "vehicle/slider/runtime/runtime.hpp"

namespace vehicle::slider
{

void run() noexcept
{
    // Conservative attended bring-up: 12 rad/s manual, 8 rad/s automatic,
    // and C610 current capped at 1500 raw (about 1.5 A).
    runtime::start({12.0F, 8.0F, 0.08F, 1.0F,
                    {400.0F, 0.0F, 0.0F, 1500.0F}});
}

} // namespace vehicle::slider
