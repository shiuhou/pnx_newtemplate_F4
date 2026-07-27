#include "usb_arm_command.hpp"

#include <cstdlib>
#include <string_view>

namespace
{

void require(bool condition) noexcept
{
    if (!condition)
    {
        std::abort();
    }
}

bool feed(cboard::motor_safe::usb_arm_command_parser& parser,
          std::string_view bytes) noexcept
{
    return parser.consume(
        reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
}

void test_exact_fragmented_command_arms_once() noexcept
{
    cboard::motor_safe::usb_arm_command_parser parser;
    require(!feed(parser, "PNX_ARM M2006 "));
    require(feed(parser, "CAN1 0x203 +500\n"));
    require(!feed(parser, "PNX_ARM M2006 CAN1 0x203 +500\n"));
}

void test_wrong_or_incomplete_command_never_arms() noexcept
{
    cboard::motor_safe::usb_arm_command_parser parser;
    require(!feed(parser, "PNX_ARM M3508 CAN1 0x203 +500\n"));
    require(!feed(parser, "PNX_ARM M2006 CAN1 0x203 +100\n"));
    require(!feed(parser, "PNX_ARM M2006 CAN1 0x203 +500"));
}

void test_overflowed_line_is_rejected() noexcept
{
    cboard::motor_safe::usb_arm_command_parser parser;
    require(!feed(parser, std::string_view(
        "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX")));
    require(!feed(parser, "PNX_ARM M2006 CAN1 0x203 +500\n"));
}

} // namespace

int main()
{
    test_exact_fragmented_command_arms_once();
    test_wrong_or_incomplete_command_never_arms();
    test_overflowed_line_is_rejected();
    return EXIT_SUCCESS;
}
