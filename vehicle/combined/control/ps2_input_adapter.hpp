#pragma once

#include "pnx_modules/remoter/include/types.hpp"

namespace vehicle::combined
{

class ps2_input_adapter {
public:
    remoter::state update(const remoter::state& remote) noexcept;
    void reset() noexcept;

private:
    bool unlocked_{};
    bool circle_release_seen_{};
};

} // namespace vehicle::combined
