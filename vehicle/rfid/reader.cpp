#include "vehicle/rfid/reader.hpp"

#include "config.hpp"
#include "vehicle/rfid/reader_core.hpp"
#include "vehicle/rfid/transport.hpp"

#include <bsp_usart.hpp>
#include <tx_api.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace vehicle::rfid
{
namespace
{

constexpr std::size_t dma_buffer_size = 64U;
constexpr std::size_t ring_buffer_size = 256U;
constexpr std::size_t worker_stack_size = 1536U;
constexpr UINT worker_priority = 8U;
constexpr ULONG worker_poll_ticks = 10U;

static_assert(params::rfid::address <= 0xFFU);
static_assert(params::rfid::baud != 0U);

reader_core core{static_cast<std::uint8_t>(params::rfid::address)};
state published_state{};
bsp::usart::dma_rx_storage<dma_buffer_size> dma_rx{};
std::array<std::uint8_t, ring_buffer_size> ring{};
volatile std::size_t ring_head{};
volatile std::size_t ring_tail{};
volatile bool ring_overflow{};
TX_SEMAPHORE rx_semaphore{};
TX_THREAD worker_thread{};
alignas(8) std::array<std::uint8_t, worker_stack_size> worker_stack{};
CHAR worker_name[] = "rfid";
bool init_attempted{};
bool initialized{};

void publish() noexcept
{
    const state next = core.snapshot();
    TX_INTERRUPT_SAVE_AREA
    TX_DISABLE
    published_state = next;
    TX_RESTORE
}

void on_rx(bsp::usart::port, const bsp::usart::rx_frame& frame,
           void*) noexcept
{
    if (frame.data == nullptr || frame.len == 0U)
    {
        return;
    }

    for (std::size_t i = 0U; i < frame.len; ++i)
    {
        const std::size_t next = (ring_head + 1U) % ring.size();
        if (next == ring_tail)
        {
            ring_overflow = true;
            break;
        }
        ring[ring_head] = frame.data[i];
        ring_head = next;
    }
    (void)tx_semaphore_put(&rx_semaphore);
}

void worker_entry(ULONG) noexcept
{
    for (;;)
    {
        (void)tx_semaphore_get(&rx_semaphore, worker_poll_ticks);

        std::array<std::uint8_t, ring_buffer_size> bytes{};
        std::size_t byte_count{};
        bool overflow{};
        TX_INTERRUPT_SAVE_AREA
        TX_DISABLE
        overflow = ring_overflow;
        ring_overflow = false;
        while (ring_tail != ring_head && byte_count < bytes.size())
        {
            bytes[byte_count++] = ring[ring_tail];
            ring_tail = (ring_tail + 1U) % ring.size();
        }
        TX_RESTORE

        const std::uint32_t now = static_cast<std::uint32_t>(tx_time_get());
        if (overflow)
        {
            core.record_overflow();
        }
        core.feed(bytes.data(), byte_count, now);
        if (const auto command = core.poll(now); command.has_value() &&
            !transport::write(app::uart::rfid, command->bytes.data(),
                              command->bytes.size()))
        {
            core.record_io_error();
        }
        publish();
    }
}

} // namespace

bool init() noexcept
{
    if constexpr (!config::feature::has_rfid)
    {
        return true;
    }
    if (init_attempted)
    {
        return initialized;
    }
    init_attempted = true;

    if (tx_semaphore_create(&rx_semaphore, const_cast<CHAR*>("rfid_rx"),
                            0U) != TX_SUCCESS ||
        !transport::open(app::uart::rfid, params::rfid::baud, dma_rx.data(),
                         dma_rx.logical_size, on_rx, nullptr))
    {
        core.start(static_cast<std::uint32_t>(tx_time_get()));
        core.record_io_error();
        publish();
        return false;
    }

    core.start(static_cast<std::uint32_t>(tx_time_get()));
    publish();
    if (tx_thread_create(&worker_thread, worker_name, worker_entry, 0U,
                         worker_stack.data(), worker_stack.size(),
                         worker_priority, worker_priority, TX_NO_TIME_SLICE,
                         TX_AUTO_START) != TX_SUCCESS)
    {
        core.record_io_error();
        publish();
        return false;
    }

    initialized = true;
    return true;
}

state snapshot() noexcept
{
    state copy{};
    TX_INTERRUPT_SAVE_AREA
    TX_DISABLE
    copy = published_state;
    TX_RESTORE
    return copy;
}

} // namespace vehicle::rfid
