#include "bsp_spi.hpp"
#include "fake_spi.hpp"

#include <array>
#include <cstdlib>

namespace
{

void require(bool condition) noexcept
{
    if (!condition)
    {
        std::abort();
    }
}

} // namespace

int main()
{
    constexpr bsp::spi::bus imu_bus{0U};
    constexpr bsp::spi::bus missing_bus{1U};
    constexpr bsp::spi::chip_select accel{0U};
    constexpr bsp::spi::chip_select gyro{1U};
    constexpr bsp::spi::chip_select missing_select{2U};
    host_test::fake_spi::reset();

    require(!bsp::spi::bus_enabled(missing_bus));
    require(bsp::spi::init(missing_bus) ==
            types::status::not_configured);
    require(bsp::spi::transmit(imu_bus, nullptr, 1U, 10U) ==
            types::status::invalid_arg);
    std::array<std::uint8_t, 3U> data{};
    require(bsp::spi::receive(imu_bus, data.data(), 0U, 10U) ==
            types::status::invalid_arg);
    require(bsp::spi::set_select(missing_select, true) ==
            types::status::not_configured);

    require(bsp::spi::init(imu_bus) == types::status::ok);
    require(host_test::fake_spi::initialized());
    require(bsp::spi::wait_ready(imu_bus, 10U) ==
            types::status::ok);
    require(bsp::spi::set_select(accel, true) ==
            types::status::ok);
    require(host_test::fake_spi::selected(0U));
    require(bsp::spi::set_select(accel, false) ==
            types::status::ok);
    require(!host_test::fake_spi::selected(0U));
    bsp::spi::cs_set(bsp::spi::cs::bmi088_gyro, true);
    require(host_test::fake_spi::selected(1U));
    bsp::spi::cs_set(bsp::spi::cs::bmi088_gyro, false);
    require(!host_test::fake_spi::selected(1U));

    require(bsp::spi::transmit(
                imu_bus, data.data(), data.size(), 10U) ==
            types::status::ok);
    require(bsp::spi::receive(
                imu_bus, data.data(), data.size(), 10U) ==
            types::status::ok);
    require(host_test::fake_spi::transmit_count() == 1U);
    require(host_test::fake_spi::receive_count() == 1U);
    require(data[0] == 1U && data[1] == 2U && data[2] == 3U);
    return EXIT_SUCCESS;
}
