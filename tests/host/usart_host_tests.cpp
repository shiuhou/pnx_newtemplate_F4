#include "bsp_usart.hpp"
#include "fake_usart_backend.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string_view>
#include <thread>
#include <vector>

namespace
{

[[noreturn]] void fail() noexcept
{
    std::abort();
}

void require(bool condition) noexcept
{
    if (!condition)
    {
        fail();
    }
}

void test_tx_count(bsp::usart::mode selected_mode,
                   bool complete_from_isr) noexcept
{
    constexpr bsp::usart::port selected = 0U;
    constexpr std::array<std::uint8_t, 1> payload{0xA5U};
    require(bsp::usart::init(selected, selected_mode) ==
            types::status::ok);
    require(bsp::usart::transmit(selected, payload.data(),
                                 payload.size(), 1U) ==
            types::status::ok);
    if (complete_from_isr)
    {
        bsp::usart::detail::tx_complete_from_isr(selected);
    }
    require(bsp::usart::snapshot(selected).tx_count == 1U);
}

void test_start_rx_rollback() noexcept
{
    constexpr bsp::usart::port selected = 1U;
    std::array<std::uint8_t, 8> first{};
    std::array<std::uint8_t, 16> second{};
    require(bsp::usart::init(selected, bsp::usart::mode::dma) ==
            types::status::ok);
    host_test::fake_usart::set_start_status(types::status::error);
    require(bsp::usart::start_rx_to_idle(
                selected, first.data(), first.size(), nullptr, nullptr) ==
            types::status::error);
    host_test::fake_usart::set_start_status(types::status::ok);
    require(bsp::usart::start_rx_to_idle(
                selected, second.data(), second.size(), nullptr, nullptr) ==
            types::status::ok);
}

void test_concurrent_rx_count()
{
    constexpr bsp::usart::port selected = 2U;
    constexpr std::uint32_t thread_count = 8U;
    constexpr std::uint32_t iterations = 100000U;
    std::array<std::uint8_t, 1> buffer{};
    require(bsp::usart::init(selected, bsp::usart::mode::dma) ==
            types::status::ok);
    require(bsp::usart::start_rx_to_idle(
                selected, buffer.data(), buffer.size(), nullptr, nullptr) ==
            types::status::ok);

    std::vector<std::thread> workers;
    workers.reserve(thread_count);
    for (std::uint32_t thread = 0U; thread < thread_count; ++thread)
    {
        workers.emplace_back([=] {
            for (std::uint32_t i = 0U; i < iterations; ++i)
            {
                bsp::usart::detail::rx_from_isr(selected, 1U);
            }
        });
    }
    for (auto& worker : workers)
    {
        worker.join();
    }
    require(bsp::usart::snapshot(selected).rx_count ==
            thread_count * iterations);
}

struct rx_observation
{
    const std::uint8_t* expected_data = nullptr;
    std::size_t expected_len = 0U;
    std::uint32_t handler_calls = 0U;
    std::uint32_t notify_calls = 0U;
};

void observe_rx(bsp::usart::port, const bsp::usart::rx_frame& frame,
                void* user_data)
{
    auto& observation = *static_cast<rx_observation*>(user_data);
    require(frame.data == observation.expected_data);
    require(frame.len == observation.expected_len);
    ++observation.handler_calls;
}

void observe_notify(void* user_data)
{
    auto& observation = *static_cast<rx_observation*>(user_data);
    ++observation.notify_calls;
}

void test_offset_delivery() noexcept
{
    constexpr bsp::usart::port selected = 3U;
    std::array<std::uint8_t, 8> buffer{};
    rx_observation observation{buffer.data() + 4U, 3U};
    require(bsp::usart::init(selected, bsp::usart::mode::dma) ==
            types::status::ok);
    require(bsp::usart::start_rx_to_idle(
                selected, buffer.data(), buffer.size(), observe_rx,
                &observation, observe_notify, &observation) ==
            types::status::ok);

    bsp::usart::detail::rx_from_isr(selected, 8U, 1U);
    require(bsp::usart::snapshot(selected).rx_count == 0U);

    bsp::usart::detail::rx_from_isr(selected, 4U, 3U);
    const auto stats = bsp::usart::snapshot(selected);
    require(stats.rx_count == 1U);
    require(stats.last_rx_len == 3U);
    require(observation.notify_calls == 1U);
    require(observation.handler_calls == 1U);
}

void test_tx_failure_telemetry() noexcept
{
    constexpr bsp::usart::port selected = 3U;
    constexpr std::array<std::uint8_t, 1> payload{0x5AU};
    require(bsp::usart::init(selected, bsp::usart::mode::block) ==
            types::status::ok);

    host_test::fake_usart::set_transmit_status(types::status::busy);
    require(bsp::usart::transmit(selected, payload.data(),
                                 payload.size(), 1U) ==
            types::status::busy);
    host_test::fake_usart::set_transmit_status(types::status::error);
    require(bsp::usart::transmit(selected, payload.data(),
                                 payload.size(), 1U) ==
            types::status::error);
    host_test::fake_usart::set_transmit_status(types::status::ok);
    require(bsp::usart::transmit(selected, payload.data(),
                                 payload.size(), 1U) ==
            types::status::ok);

    const auto stats = bsp::usart::snapshot(selected);
    require(stats.busy_count == 1U);
    require(stats.error_count == 1U);
    require(stats.tx_count == 1U);
}

void test_rx_delivery_contract() noexcept
{
    constexpr bsp::usart::port selected = 0U;
    std::array<std::uint8_t, 8> buffer{};
    require(bsp::usart::init(selected, bsp::usart::mode::dma) ==
            types::status::ok);
    require(bsp::usart::start_rx_to_idle(
                selected, buffer.data(), buffer.size(), nullptr, nullptr,
                nullptr, nullptr,
                bsp::usart::rx_delivery::stream_segments) ==
            types::status::ok);
    require(host_test::fake_usart::last_delivery() ==
            static_cast<std::uint32_t>(
                bsp::usart::rx_delivery::stream_segments));
    require(host_test::fake_usart::start_calls() == 1U);

    require(bsp::usart::start_rx_to_idle(
                selected, buffer.data(), buffer.size(), nullptr,
                nullptr) == types::status::busy);
    require(host_test::fake_usart::start_calls() == 1U);

    require(bsp::usart::start_rx_to_idle(
                selected, buffer.data(),
                static_cast<std::size_t>(
                    std::numeric_limits<std::uint16_t>::max()) +
                    1U,
                nullptr, nullptr, nullptr, nullptr,
                bsp::usart::rx_delivery::stream_segments) ==
            types::status::invalid_arg);
    require(host_test::fake_usart::start_calls() == 1U);
}

} // namespace

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        return 2;
    }
    host_test::fake_usart::reset();
    const std::string_view scenario{argv[1]};
    if (scenario == "block_tx_count")
    {
        test_tx_count(bsp::usart::mode::block, false);
    }
    else if (scenario == "it_tx_count")
    {
        test_tx_count(bsp::usart::mode::it, true);
    }
    else if (scenario == "dma_tx_count")
    {
        test_tx_count(bsp::usart::mode::dma, true);
    }
    else if (scenario == "start_rx_rollback")
    {
        test_start_rx_rollback();
    }
    else if (scenario == "concurrent_rx_count")
    {
        test_concurrent_rx_count();
    }
    else if (scenario == "offset_delivery")
    {
        test_offset_delivery();
    }
    else if (scenario == "tx_failure_telemetry")
    {
        test_tx_failure_telemetry();
    }
    else if (scenario == "rx_delivery_contract")
    {
        test_rx_delivery_contract();
    }
    else
    {
        return 2;
    }
    return 0;
}
