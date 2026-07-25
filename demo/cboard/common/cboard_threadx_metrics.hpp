#pragma once

#include <cstddef>
#include <cstdint>

namespace cboard::demo::threadx
{

struct stack_metrics
{
    bool valid = false;
    std::size_t size_bytes = 0U;
    std::size_t used_bytes = 0U;
    std::size_t free_bytes = 0U;
};

constexpr stack_metrics
stack_usage(std::uintptr_t start, std::uintptr_t end_inclusive,
            std::uintptr_t highest_used) noexcept
{
    if (end_inclusive < start || highest_used < start ||
        highest_used > end_inclusive)
    {
        return {};
    }
    const std::size_t size = end_inclusive - start + 1U;
    const std::size_t used = end_inclusive - highest_used + 1U;
    return {true, size, used, size - used};
}

} // namespace cboard::demo::threadx
