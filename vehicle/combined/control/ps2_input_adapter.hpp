#pragma once

#include "pnx_modules/remoter/include/types.hpp"

#include <cstdint>

namespace vehicle::combined
{

enum class operator_mode : std::uint8_t {
    manual,
    vision_auto,
};

class ps2_input_adapter {
public:
    remoter::state update(const remoter::state& remote) noexcept;
    void reset() noexcept;
    operator_mode mode() const noexcept;
    bool unlocked() const noexcept;
    bool l1_held() const noexcept;
    bool entered_auto() const noexcept;
    bool stop_requested() const noexcept;

private:
    bool unlocked_{};
    bool circle_release_seen_{};
    bool l1_held_{};
    bool entered_auto_{};
    bool stop_requested_{};
    operator_mode mode_{operator_mode::manual};
};

} // namespace vehicle::combined
