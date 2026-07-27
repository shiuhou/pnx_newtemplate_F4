#include "bmi088_lab_protocol.hpp"

#include <cassert>
#include <cstdint>

int main()
{
    using namespace cboard::bmi088_lab;

    static_assert(accel_chip_id == 0x1EU);
    static_assert(gyro_chip_id == 0x0FU);
    assert(decode_i16(0x34U, 0x12U) == 0x1234);
    assert(decode_i16(0x00U, 0x80U) == -32768);
    assert(chip_ids_valid(0x1EU, 0x0FU));
    assert(!chip_ids_valid(0xFFU, 0x0FU));

    const raw_sample still{100, -200, 16000, 3, -4, 5};
    const raw_sample same{100, -200, 16000, 3, -4, 5};
    const raw_sample moved{1200, -200, 8000, 300, -4, 5};
    assert(!sample_changed(still, same, 32));
    assert(sample_changed(still, moved, 32));
    return 0;
}
