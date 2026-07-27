#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace cboard::motor_safe
{

class usb_arm_command_parser
{
  public:
    bool consume(const std::uint8_t* data, std::size_t length) noexcept
    {
        if (data == nullptr && length != 0U)
        {
            return false;
        }

        bool accepted = false;
        for (std::size_t index = 0; index < length; ++index)
        {
            const std::uint8_t byte = data[index];
            if (byte == static_cast<std::uint8_t>('\r'))
            {
                continue;
            }
            if (byte == static_cast<std::uint8_t>('\n'))
            {
                if (!dropping_ && !latched_ && size_ == command_length &&
                    std::memcmp(buffer_.data(), command, command_length) == 0)
                {
                    latched_ = true;
                    accepted = true;
                }
                size_ = 0U;
                dropping_ = false;
                continue;
            }
            if (dropping_)
            {
                continue;
            }
            if (size_ >= buffer_.size())
            {
                size_ = 0U;
                dropping_ = true;
                continue;
            }
            buffer_[size_++] = byte;
        }
        return accepted;
    }

  private:
    inline static constexpr char command[] =
        "PNX_ARM M2006 CAN1 0x203 +500";
    inline static constexpr std::size_t command_length =
        sizeof(command) - 1U;

    std::array<std::uint8_t, 48U> buffer_{};
    std::size_t size_ = 0U;
    bool dropping_ = false;
    bool latched_ = false;
};

} // namespace cboard::motor_safe
