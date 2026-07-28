#include "bmi088_transport.hpp"

#include <cstdlib>

namespace host_test::fake_bmi088
{
void reset() noexcept;
std::size_t write_count() noexcept;
} // namespace host_test::fake_bmi088

namespace
{

void require(bool condition) noexcept
{
    if (!condition)
    {
        std::abort();
    }
}

void no_delay(std::uint32_t) noexcept
{
}

} // namespace

int main()
{
    host_test::fake_bmi088::reset();
    imu::bmi088_transport sensor{
        bsp::spi::bus{0U},
        bsp::spi::chip_select{0U},
        bsp::spi::chip_select{1U}};

    require(sensor.init() == types::status::ok);

    imu::bmi088_chip_ids ids{};
    require(sensor.read_chip_ids(ids) == types::status::ok);
    require(ids.accel == 0x1EU);
    require(ids.gyro == 0x0FU);
    require(ids.valid());

    require(sensor.configure(no_delay) == types::status::ok);
    require(host_test::fake_bmi088::write_count() >= 10U);

    imu::bmi088_raw_sample sample{};
    require(sensor.read_raw(sample) == types::status::ok);
    require(sample.accel_x == 0x1234);
    require(sample.accel_y == -2);
    require(sample.accel_z == 0x0102);
    require(sample.gyro_x == 0x5678);
    require(sample.gyro_y == -3);
    require(sample.gyro_z == 0x0304);

    float temperature = 0.0F;
    require(sensor.read_temperature(temperature) == types::status::ok);
    require(temperature == 25.0F);
    return EXIT_SUCCESS;
}
