#include "ist8310_lab_protocol.hpp"

#include <cassert>

int main()
{
    using namespace cboard::ist8310_lab;
    static_assert(i2c_address_7bit == 0x0EU);
    static_assert(who_am_i_value == 0x10U);
    assert(decode_i16(0x34U, 0x12U) == 0x1234);
    assert(decode_i16(0x00U, 0x80U) == -32768);
    assert(identity_valid(0x10U));
    assert(!identity_valid(0xFFU));

    const raw_sample still{100, -200, 300};
    const raw_sample same{100, -200, 300};
    const raw_sample moved{900, 50, -400};
    assert(!sample_changed(still, same, 32));
    assert(sample_changed(still, moved, 32));
    return 0;
}
