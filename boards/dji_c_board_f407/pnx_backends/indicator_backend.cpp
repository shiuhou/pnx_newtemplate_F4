#include "bsp_indicator.hpp"

#include "main.h"

namespace
{

std::uint16_t pin_of(bsp::indicator::channel selected) noexcept
{
    switch (selected)
    {
    case bsp::indicator::channel::red:
        return GPIO_PIN_12;
    case bsp::indicator::channel::green:
        return GPIO_PIN_11;
    case bsp::indicator::channel::blue:
        return GPIO_PIN_10;
    default:
        return 0U;
    }
}

} // namespace

namespace bsp::indicator::detail
{

types::status backend_init() noexcept
{
    HAL_GPIO_WritePin(GPIOH, GPIO_PIN_12 | GPIO_PIN_11 | GPIO_PIN_10,
                      GPIO_PIN_RESET);
    return types::status::ok;
}

types::status backend_set(channel selected, bool on) noexcept
{
    const std::uint16_t pin = pin_of(selected);
    if (pin == 0U)
    {
        return types::status::invalid_arg;
    }
    HAL_GPIO_WritePin(GPIOH, pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
    return types::status::ok;
}

} // namespace bsp::indicator::detail
