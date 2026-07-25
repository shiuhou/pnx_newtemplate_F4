#include "bsp_diagnostics.h"
#include "cboard_threadx_metrics.hpp"
#include "diagnostics_f407.hpp"

#include <cstdint>
#include <cstdlib>

namespace
{

[[noreturn]] void fail() noexcept
{
    std::abort();
}

void require(bool condition) noexcept
{
    if (!condition)
    {
        fail();
    }
}

} // namespace

int main()
{
    static_assert(sizeof(bsp_diagnostics_crash_record) == 104U);
    require(bsp_diagnostics_record_checksum(nullptr) == 0U);
    require(bsp_diagnostics_record_valid(nullptr) == 0);

    bsp_diagnostics_crash_record record{};
    require(bsp_diagnostics_record_valid(&record) == 0);

    record.magic = BSP_DIAGNOSTICS_CRASH_MAGIC;
    record.version = BSP_DIAGNOSTICS_ABI_VERSION;
    record.size = sizeof(record);
    record.sequence = 3U;
    record.boot_count = 7U;
    record.kind = BSP_DIAGNOSTICS_FAULT_HARD;
    record.frame_valid = 1U;
    record.pc = 0x08001234U;
    record.lr = 0x08005678U;
    record.cfsr = 0x00008200U;
    record.checksum = bsp_diagnostics_record_checksum(&record);
    require(bsp_diagnostics_record_valid(&record) != 0);

    auto invalid_record = record;
    invalid_record.magic = 0U;
    invalid_record.checksum =
        bsp_diagnostics_record_checksum(&invalid_record);
    require(bsp_diagnostics_record_valid(&invalid_record) == 0);

    invalid_record = record;
    ++invalid_record.version;
    invalid_record.checksum =
        bsp_diagnostics_record_checksum(&invalid_record);
    require(bsp_diagnostics_record_valid(&invalid_record) == 0);

    invalid_record = record;
    --invalid_record.size;
    invalid_record.checksum =
        bsp_diagnostics_record_checksum(&invalid_record);
    require(bsp_diagnostics_record_valid(&invalid_record) == 0);

    record.pc ^= 1U;
    require(bsp_diagnostics_record_valid(&record) == 0);
    record.pc ^= 1U;

    const std::uint32_t raw_reset_flags = 0xFE000001UL;
    const std::uint32_t reset_reasons =
        pnx::board::f407::diagnostics::decode_reset_reasons(
            raw_reset_flags);
    require((reset_reasons & BSP_DIAGNOSTICS_RESET_BROWNOUT) != 0U);
    require((reset_reasons & BSP_DIAGNOSTICS_RESET_PIN) != 0U);
    require((reset_reasons & BSP_DIAGNOSTICS_RESET_POWER_ON) != 0U);
    require((reset_reasons & BSP_DIAGNOSTICS_RESET_SOFTWARE) != 0U);
    require((reset_reasons & BSP_DIAGNOSTICS_RESET_IWDG) != 0U);
    require((reset_reasons & BSP_DIAGNOSTICS_RESET_WWDG) != 0U);
    require((reset_reasons & BSP_DIAGNOSTICS_RESET_LOW_POWER) != 0U);
    require(pnx::board::f407::diagnostics::decode_reset_reasons(1U) ==
            BSP_DIAGNOSTICS_RESET_NONE);

    using pnx::board::f407::diagnostics::
        basic_frame_pointer_valid;
    static_assert(basic_frame_pointer_valid(0x20000000U, 0U));
    static_assert(basic_frame_pointer_valid(0x2001FFE0U, 0U));
    static_assert(basic_frame_pointer_valid(0x10000000U, 0U));
    static_assert(basic_frame_pointer_valid(0x1000FFE0U, 0U));
    static_assert(!basic_frame_pointer_valid(0U, 0U));
    static_assert(!basic_frame_pointer_valid(0x20000001U, 0U));
    static_assert(!basic_frame_pointer_valid(0x2001FFE4U, 0U));
    static_assert(!basic_frame_pointer_valid(0x1000FFE4U, 0U));
    static_assert(!basic_frame_pointer_valid(
        0x20000000U, 1UL << 12U));

    constexpr auto stack =
        cboard::demo::threadx::stack_usage(0x1000U, 0x17FFU,
                                           0x1600U);
    static_assert(stack.valid);
    static_assert(stack.size_bytes == 0x800U);
    static_assert(stack.used_bytes == 0x200U);
    static_assert(stack.free_bytes == 0x600U);

    constexpr auto full =
        cboard::demo::threadx::stack_usage(0x1000U, 0x17FFU,
                                           0x1000U);
    static_assert(full.valid && full.used_bytes == 0x800U &&
                  full.free_bytes == 0U);

    constexpr auto minimum =
        cboard::demo::threadx::stack_usage(0x1000U, 0x17FFU,
                                           0x17FFU);
    static_assert(minimum.valid && minimum.used_bytes == 1U &&
                  minimum.free_bytes == 0x7FFU);

    constexpr auto invalid =
        cboard::demo::threadx::stack_usage(0x1800U, 0x17FFU,
                                           0x1600U);
    static_assert(!invalid.valid);
    static_assert(!cboard::demo::threadx::stack_usage(
                       0x1000U, 0x17FFU, 0x1800U)
                       .valid);
    return 0;
}
