#include "rfid_uid.hpp"

#include "config.hpp"
#include "vehicle/rfid/reader.hpp"

#include <bsp_usart.hpp>
#include <tx_api.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace demo::cboard::rfid_uid
{
namespace
{

constexpr std::size_t report_stack_size = 1024U;
constexpr ULONG report_period_ticks = 100U;

TX_THREAD report_thread{};
alignas(8) std::array<std::uint8_t, report_stack_size> report_stack{};
CHAR report_thread_name[] = "rfid report";

void append_char(std::array<char, 128U>& output, std::size_t& size,
                 char value) noexcept
{
    if (size < output.size())
    {
        output[size++] = value;
    }
}

void append_text(std::array<char, 128U>& output, std::size_t& size,
                 const char* text) noexcept
{
    while (*text != '\0')
    {
        append_char(output, size, *text++);
    }
}

void append_decimal(std::array<char, 128U>& output, std::size_t& size,
                    std::uint32_t value) noexcept
{
    char digits[10U]{};
    std::size_t count{};
    do
    {
        digits[count++] = static_cast<char>('0' + value % 10U);
        value /= 10U;
    } while (value != 0U);
    while (count != 0U)
    {
        append_char(output, size, digits[--count]);
    }
}

void append_hex(std::array<char, 128U>& output, std::size_t& size,
                std::uint8_t value) noexcept
{
    constexpr char digits[] = "0123456789ABCDEF";
    append_char(output, size, digits[value >> 4U]);
    append_char(output, size, digits[value & 0x0FU]);
}

std::size_t format_report(const vehicle::rfid::state& current,
                          std::array<char, 128U>& output) noexcept
{
    std::size_t size{};
    append_text(output, size, "RFID link=");
    append_decimal(output, size, static_cast<std::uint8_t>(current.link));
    append_text(output, size, " event=");
    append_decimal(output, size, current.event_count);
    append_text(output, size, " uid=");
    for (const std::uint8_t byte : current.last_uid)
    {
        append_hex(output, size, byte);
    }
    append_text(output, size, " err=");
    append_decimal(output, size, current.checksum_errors);
    append_char(output, size, '/');
    append_decimal(output, size, current.frame_errors);
    append_char(output, size, '/');
    append_decimal(output, size, current.overflow_errors);
    append_char(output, size, '/');
    append_decimal(output, size, current.timeout_errors);
    append_text(output, size, "\r\n");
    return size;
}

bool changed(const vehicle::rfid::state& left,
             const vehicle::rfid::state& right) noexcept
{
    return left.link != right.link ||
           left.event_count != right.event_count ||
           left.checksum_errors != right.checksum_errors ||
           left.frame_errors != right.frame_errors ||
           left.overflow_errors != right.overflow_errors ||
           left.timeout_errors != right.timeout_errors;
}

void report_entry(ULONG) noexcept
{
    vehicle::rfid::state previous{};
    bool first = true;
    for (;;)
    {
        const vehicle::rfid::state current = vehicle::rfid::snapshot();
        if (first || changed(current, previous))
        {
            std::array<char, 128U> text{};
            const std::size_t size = format_report(current, text);
            (void)bsp::usart::transmit(
                app::uart::test_report,
                reinterpret_cast<const std::uint8_t*>(text.data()), size,
                20U);
            previous = current;
            first = false;
        }
        tx_thread_sleep(report_period_ticks);
    }
}

} // namespace

void run() noexcept
{
    (void)vehicle::rfid::init();
    const bsp::usart::line_config report_line{
        115200U,
        bsp::usart::word_length::bits_8,
        bsp::usart::stop_bits::one,
        bsp::usart::parity::none,
        true,
        false,
    };
    if (bsp::usart::configure(app::uart::test_report, report_line) !=
            types::status::ok ||
        bsp::usart::init(app::uart::test_report, bsp::usart::mode::block) !=
            types::status::ok)
    {
        return;
    }
    (void)tx_thread_create(
        &report_thread, report_thread_name, report_entry, 0U,
        report_stack.data(), report_stack.size(), params::test::thread_priority,
        params::test::thread_priority, TX_NO_TIME_SLICE, TX_AUTO_START);
}

} // namespace demo::cboard::rfid_uid
