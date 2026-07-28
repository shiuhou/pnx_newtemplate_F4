#pragma once

#include <cstdint>

namespace pnx::f407::usb_detail
{

inline constexpr std::uint16_t cdc_full_speed_max_packet_size = 64U;

enum class tx_next_action : std::uint8_t
{
    complete,
    send_zlp,
};

struct tx_completion_result
{
    tx_next_action next = tx_next_action::complete;
    std::uint16_t requested = 0U;
    std::uint16_t actual = 0U;
    bool success = false;
};

class tx_completion_state
{
public:
    void start(std::uint16_t requested) noexcept
    {
        requested_ = requested;
        actual_ = 0U;
        phase_ = phase::data;
    }

    [[nodiscard]] bool busy() const noexcept
    {
        return phase_ != phase::idle;
    }

    tx_completion_result on_callback(
        bool success, std::uint16_t actual) noexcept
    {
        if (phase_ == phase::data)
        {
            actual_ = actual;
            if (success && actual_ == requested_ && requested_ != 0U &&
                requested_ % cdc_full_speed_max_packet_size == 0U)
            {
                phase_ = phase::zlp;
                return {
                    tx_next_action::send_zlp,
                    requested_,
                    actual_,
                    true};
            }
        }
        return finish(success);
    }

    tx_completion_result abort() noexcept
    {
        return finish(false);
    }

    void reset() noexcept
    {
        requested_ = 0U;
        actual_ = 0U;
        phase_ = phase::idle;
    }

private:
    enum class phase : std::uint8_t
    {
        idle,
        data,
        zlp,
    };

    tx_completion_result finish(bool success) noexcept
    {
        const tx_completion_result result{
            tx_next_action::complete,
            requested_,
            actual_,
            success};
        reset();
        return result;
    }

    std::uint16_t requested_ = 0U;
    std::uint16_t actual_ = 0U;
    phase phase_ = phase::idle;
};

} // namespace pnx::f407::usb_detail
