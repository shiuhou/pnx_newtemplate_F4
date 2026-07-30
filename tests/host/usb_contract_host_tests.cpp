#include "bsp_usb.hpp"
#include "fake_usb.hpp"
#include "usb_tx_completion.hpp"

#include <array>
#include <cstdlib>
#include <cstring>
#include <string_view>

namespace
{

struct callback_state
{
    std::array<std::uint8_t, 32> rx{};
    std::uint16_t rx_length = 0U;
    std::uint32_t rx_calls = 0U;
    std::uint32_t tx_done_calls = 0U;
    std::uint16_t tx_requested = 0U;
    std::uint16_t tx_actual = 0U;
    std::uint32_t tx_native_status = 0U;
};

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

void on_rx(const std::uint8_t* data, std::uint16_t length,
           void* user) noexcept
{
    auto& state = *static_cast<callback_state*>(user);
    require(length <= state.rx.size());
    std::memcpy(state.rx.data(), data, length);
    state.rx_length = length;
    ++state.rx_calls;
}

void on_tx_done(std::uint16_t requested, std::uint16_t actual,
                std::uint32_t native_status, void* user) noexcept
{
    auto& state = *static_cast<callback_state*>(user);
    state.tx_requested = requested;
    state.tx_actual = actual;
    state.tx_native_status = native_status;
    ++state.tx_done_calls;
}

bsp::usb::config make_config(callback_state& callbacks) noexcept
{
    bsp::usb::config config{};
    config.on_rx = on_rx;
    config.on_tx_done = on_tx_done;
    config.user = &callbacks;
    config.max_tx_size = 128U;
    return config;
}

void initialize(callback_state& callbacks) noexcept
{
    host_test::fake_usb::reset();
    require(bsp::usb::init(make_config(callbacks)) ==
            types::status::ok);
    host_test::fake_usb::complete_startup(types::status::ok);
}

void connect() noexcept
{
    host_test::fake_usb::set_connected(true);
    require(bsp::usb::connected());
}

void test_same_config_reinit() noexcept
{
    callback_state callbacks{};
    initialize(callbacks);
    require(bsp::usb::init(make_config(callbacks)) ==
            types::status::ok);
    require(host_test::fake_usb::init_calls() == 1U);
    const bsp::usb::runtime_state state = bsp::usb::snapshot();
    require(state.initialized);
    require(state.link == bsp::usb::link_state::initialized);
}

void test_init_resource_failure() noexcept
{
    callback_state callbacks{};
    host_test::fake_usb::reset();
    host_test::fake_usb::set_init_status(types::status::error);
    require(bsp::usb::init(make_config(callbacks)) ==
            types::status::error);
    require(host_test::fake_usb::init_calls() == 1U);
    const bsp::usb::runtime_state state = bsp::usb::snapshot();
    require(!state.initialized);
    require(state.link == bsp::usb::link_state::fault);
    require(state.error_count == 1U);
}

void test_async_startup_success() noexcept
{
    callback_state callbacks{};
    host_test::fake_usb::reset();
    require(bsp::usb::init(make_config(callbacks)) ==
            types::status::ok);

    bsp::usb::runtime_state state = bsp::usb::snapshot();
    require(state.initialized);
    require(!state.connected);
    require(state.link == bsp::usb::link_state::initialized);

    host_test::fake_usb::complete_startup(types::status::ok);
    state = bsp::usb::snapshot();
    require(state.initialized);
    require(!state.connected);
    require(state.link == bsp::usb::link_state::initialized);

    host_test::fake_usb::set_connected(true);
    require(bsp::usb::connected());
    require(bsp::usb::snapshot().link ==
            bsp::usb::link_state::connected);
}

void test_controller_startup_failure() noexcept
{
    callback_state callbacks{};
    host_test::fake_usb::reset();
    require(bsp::usb::init(make_config(callbacks)) ==
            types::status::ok);

    host_test::fake_usb::complete_startup(types::status::error);
    const bsp::usb::runtime_state state = bsp::usb::snapshot();
    require(state.initialized);
    require(!state.connected);
    require(state.link == bsp::usb::link_state::fault);
    require(state.error_count == 1U);

    host_test::fake_usb::set_connected(true);
    require(!bsp::usb::connected());
    constexpr std::array<std::uint8_t, 1> payload{0x5AU};
    require(bsp::usb::write(payload.data(), payload.size()).status ==
            types::status::not_configured);
}

void test_controller_activation_failure() noexcept
{
    callback_state callbacks{};
    host_test::fake_usb::reset();
    require(bsp::usb::init(make_config(callbacks)) ==
            types::status::ok);
    host_test::fake_usb::complete_startup(types::status::ok);

    host_test::fake_usb::complete_activation(types::status::error);
    const bsp::usb::runtime_state state = bsp::usb::snapshot();
    require(state.initialized);
    require(!state.connected);
    require(state.link == bsp::usb::link_state::fault);
    require(state.error_count == 1U);

    host_test::fake_usb::set_connected(true);
    require(!bsp::usb::connected());
}

void test_incompatible_reinit() noexcept
{
    callback_state first{};
    callback_state second{};
    host_test::fake_usb::reset();
    const bsp::usb::config first_config = make_config(first);
    bsp::usb::config incompatible = make_config(second);
    incompatible.write_priority =
        static_cast<std::uint8_t>(first_config.write_priority + 1U);

    require(bsp::usb::init(first_config) == types::status::ok);
    require(bsp::usb::init(incompatible) ==
            types::status::invalid_arg);
    require(host_test::fake_usb::init_calls() == 1U);
}

void test_callback_ownership() noexcept
{
    callback_state first{};
    callback_state second{};
    host_test::fake_usb::reset();

    require(bsp::usb::init(make_config(first)) ==
            types::status::ok);
    require(bsp::usb::init(make_config(second)) ==
            types::status::invalid_arg);
    host_test::fake_usb::complete_startup(types::status::ok);
    host_test::fake_usb::set_connected(true);

    constexpr std::array<std::uint8_t, 2> payload{0x12U, 0x34U};
    host_test::fake_usb::inject_rx(payload.data(), payload.size());
    require(first.rx_calls == 1U);
    require(second.rx_calls == 0U);
}

void test_identity_fail_closed() noexcept
{
    callback_state callbacks{};
    host_test::fake_usb::reset();
    host_test::fake_usb::set_identity_confirmed(false);

    require(bsp::usb::init(make_config(callbacks)) ==
            types::status::not_configured);
    const bsp::usb::runtime_state state = bsp::usb::snapshot();
    require(!state.initialized);
    require(!state.connected);
    require(state.link == bsp::usb::link_state::fault);
    require(state.error_count == 1U);
}

void test_write_before_connected() noexcept
{
    callback_state callbacks{};
    initialize(callbacks);
    constexpr std::array<std::uint8_t, 2> payload{1U, 2U};
    const bsp::usb::write_result result =
        bsp::usb::write(payload.data(), payload.size());
    require(result.status == types::status::not_configured);
    require(result.accepted == 0U);
}

void test_write_lock_contention() noexcept
{
    callback_state callbacks{};
    initialize(callbacks);
    connect();
    host_test::fake_usb::set_write_gate_available(false);

    constexpr std::array<std::uint8_t, 2> payload{0x11U, 0x22U};
    const std::uint32_t signals_before =
        host_test::fake_usb::signal_calls();
    const bsp::usb::write_result result =
        bsp::usb::write(payload.data(), payload.size());

    require(result.status == types::status::busy);
    require(result.accepted == 0U);
    require(host_test::fake_usb::signal_calls() == signals_before);
    require(bsp::usb::snapshot().tx_queue_size == 0U);
}

void test_bounded_write() noexcept
{
    callback_state callbacks{};
    initialize(callbacks);
    connect();
    constexpr std::array<std::uint8_t, 4> payload{
        0x10U, 0x20U, 0x30U, 0x40U};
    const bsp::usb::write_result result =
        bsp::usb::write(payload.data(), payload.size());
    require(result.status == types::status::ok);
    require(result.accepted == payload.size());
    require(host_test::fake_usb::signal_calls() == 1U);

    std::array<std::uint8_t, 128> transmitted{};
    std::uint16_t length = 0U;
    require(host_test::fake_usb::pop_tx(transmitted, length));
    require(length == payload.size());
    require(std::memcmp(
                transmitted.data(), payload.data(), payload.size()) == 0);
    host_test::fake_usb::complete_tx(
        length, length, types::status::ok, 0U);

    const bsp::usb::runtime_state state = bsp::usb::snapshot();
    require(state.write_count == 1U);
    require(state.tx_bytes == payload.size());
    require(state.tx_queue_size == 0U);
    require(callbacks.tx_done_calls == 1U);
}

void test_backpressure() noexcept
{
    callback_state callbacks{};
    initialize(callbacks);
    connect();
    constexpr std::array<std::uint8_t, 1> first{1U};
    constexpr std::array<std::uint8_t, 1> second{2U};
    constexpr std::array<std::uint8_t, 1> third{3U};
    require(bsp::usb::write(first.data(), first.size()).status ==
            types::status::ok);
    require(bsp::usb::write(second.data(), second.size()).status ==
            types::status::ok);
    const bsp::usb::write_result full =
        bsp::usb::write(third.data(), third.size());
    require(full.status == types::status::busy);
    require(full.accepted == 0U);
    const bsp::usb::runtime_state state = bsp::usb::snapshot();
    require(state.tx_queue_size == 2U);
    require(state.tx_queue_high_water == 2U);
    require(state.tx_drop_count == 1U);
}

void test_invalid_write() noexcept
{
    callback_state callbacks{};
    initialize(callbacks);
    connect();
    std::array<std::uint8_t, 65> too_large{};
    require(bsp::usb::write(nullptr, 1U).status ==
            types::status::invalid_arg);
    require(bsp::usb::write(too_large.data(), 0U).status ==
            types::status::invalid_arg);
    require(bsp::usb::write(
                too_large.data(), too_large.size()).status ==
            types::status::invalid_arg);
}

void test_rx_callback() noexcept
{
    callback_state callbacks{};
    initialize(callbacks);
    connect();
    constexpr std::array<std::uint8_t, 3> payload{7U, 8U, 9U};
    host_test::fake_usb::inject_rx(payload.data(), payload.size());
    require(callbacks.rx_calls == 1U);
    require(callbacks.rx_length == payload.size());
    require(std::memcmp(
                callbacks.rx.data(), payload.data(), payload.size()) == 0);
    const bsp::usb::runtime_state state = bsp::usb::snapshot();
    require(state.read_count == 1U);
    require(state.rx_bytes == payload.size());
}

void test_disconnect_cancels_without_replay() noexcept
{
    callback_state callbacks{};
    initialize(callbacks);
    connect();
    constexpr std::array<std::uint8_t, 2> stale{0xA5U, 0x5AU};
    require(bsp::usb::write(stale.data(), stale.size()).status ==
            types::status::ok);
    require(bsp::usb::write(stale.data(), stale.size()).status ==
            types::status::ok);

    host_test::fake_usb::set_connected(false);
    require(!bsp::usb::connected());
    host_test::fake_usb::set_connected(true);
    require(bsp::usb::connected());

    std::array<std::uint8_t, 128> transmitted{};
    std::uint16_t length = 0U;
    require(!host_test::fake_usb::pop_tx(transmitted, length));
    const bsp::usb::runtime_state state = bsp::usb::snapshot();
    require(state.disconnect_count == 1U);
    require(state.connect_count == 2U);
    require(state.tx_drop_count == 2U);
    require(state.tx_queue_size == 0U);

    constexpr std::array<std::uint8_t, 1> fresh{0x42U};
    require(bsp::usb::write(fresh.data(), fresh.size()).status ==
            types::status::ok);
    require(host_test::fake_usb::pop_tx(transmitted, length));
    require(length == 1U && transmitted[0] == fresh[0]);
}

void test_tx_error() noexcept
{
    callback_state callbacks{};
    initialize(callbacks);
    connect();
    constexpr std::array<std::uint8_t, 2> payload{1U, 2U};
    require(bsp::usb::write(payload.data(), payload.size()).status ==
            types::status::ok);
    std::array<std::uint8_t, 128> transmitted{};
    std::uint16_t length = 0U;
    require(host_test::fake_usb::pop_tx(transmitted, length));
    host_test::fake_usb::complete_tx(
        length, 0U, types::status::error, 77U);
    const bsp::usb::runtime_state state = bsp::usb::snapshot();
    require(state.error_count == 1U);
    require(state.last_write_status == 77U);
    require(callbacks.tx_done_calls == 1U);
    require(callbacks.tx_native_status == 77U);
}

void test_capabilities() noexcept
{
    callback_state callbacks{};
    initialize(callbacks);
    const bsp::usb::capabilities capabilities =
        bsp::usb::get_capabilities();
    require(capabilities.device_mode);
    require(!capabilities.high_speed);
    require(!capabilities.supports_vbus_sense);
    require(capabilities.max_packet_size == 64U);
    require(capabilities.max_transfer_size == 64U);
    require(capabilities.tx_queue_depth == 2U);
}

std::uint16_t fill_legacy(
    std::uint8_t* output, std::uint16_t capacity, void*) noexcept
{
    require(capacity >= 3U);
    output[0] = 4U;
    output[1] = 5U;
    output[2] = 6U;
    return 3U;
}

void test_fill_tx_compat() noexcept
{
    callback_state callbacks{};
    host_test::fake_usb::reset();
    bsp::usb::config config = make_config(callbacks);
    config.fill_tx = fill_legacy;
    require(bsp::usb::init(config) == types::status::ok);
    host_test::fake_usb::complete_startup(types::status::ok);
    connect();
    std::array<std::uint8_t, 128> transmitted{};
    std::uint16_t length = 0U;
    require(host_test::fake_usb::pop_tx(transmitted, length));
    require(length == 3U);
    require(transmitted[0] == 4U);
    require(transmitted[1] == 5U);
    require(transmitted[2] == 6U);
    require(bsp::usb::snapshot().fill_count == 1U);
}

struct deferred_fill_state
{
    std::uint32_t calls = 0U;
};

std::uint16_t fill_after_empty(
    std::uint8_t* output, std::uint16_t capacity, void* user) noexcept
{
    auto& state = *static_cast<deferred_fill_state*>(user);
    ++state.calls;
    if (state.calls == 1U)
    {
        return 0U;
    }
    require(capacity >= 1U);
    output[0] = 0xA5U;
    return 1U;
}

void test_fill_tx_empty_retry() noexcept
{
    deferred_fill_state fill_state{};
    host_test::fake_usb::reset();
    bsp::usb::config config{};
    config.max_tx_size = 128U;
    config.fill_tx = fill_after_empty;
    config.user = &fill_state;
    require(bsp::usb::init(config) == types::status::ok);
    host_test::fake_usb::complete_startup(types::status::ok);
    connect();

    host_test::fake_usb::poll_worker();
    require(fill_state.calls == 1U);
    require(!bsp::usb::snapshot().write_busy);

    host_test::fake_usb::poll_worker();
    require(fill_state.calls == 2U);
    require(bsp::usb::snapshot().write_busy);
    require(bsp::usb::snapshot().last_write_requested == 1U);
}

void test_disconnect_during_pending_tx() noexcept
{
    callback_state callbacks{};
    initialize(callbacks);
    host_test::fake_usb::complete_startup(types::status::ok);
    connect();

    constexpr std::array<std::uint8_t, 4> payload{
        0xA1U, 0xB2U, 0xC3U, 0xD4U};
    require(bsp::usb::write(payload.data(), payload.size()).status ==
            types::status::ok);

    std::array<std::uint8_t, 128> transmitted{};
    std::uint16_t length = 0U;
    require(host_test::fake_usb::pop_tx(transmitted, length));
    require(bsp::usb::snapshot().write_busy);

    host_test::fake_usb::set_connected(false);
    require(!bsp::usb::connected());
    host_test::fake_usb::complete_tx(
        length, length, types::status::ok, 0U);

    const bsp::usb::runtime_state state = bsp::usb::snapshot();
    require(state.write_count == 0U);
    require(state.tx_bytes == 0U);
    require(callbacks.tx_done_calls == 0U);
}

void test_f407_zlp_boundary() noexcept
{
    using pnx::f407::usb_detail::tx_next_action;

    pnx::f407::usb_detail::tx_completion_state state{};

    state.start(63U);
    auto completion = state.on_callback(true, 63U);
    require(completion.next == tx_next_action::complete);
    require(completion.requested == 63U);
    require(completion.actual == 63U);
    require(completion.success);
    require(!state.busy());

    state.start(64U);
    completion = state.on_callback(true, 64U);
    require(completion.next == tx_next_action::send_zlp);
    require(state.busy());

    completion = state.on_callback(true, 0U);
    require(completion.next == tx_next_action::complete);
    require(completion.requested == 64U);
    require(completion.actual == 64U);
    require(completion.success);
    require(!state.busy());

    state.start(64U);
    completion = state.on_callback(true, 64U);
    require(completion.next == tx_next_action::send_zlp);
    completion = state.abort();
    require(completion.next == tx_next_action::complete);
    require(completion.requested == 64U);
    require(completion.actual == 64U);
    require(!completion.success);
    require(!state.busy());

    state.start(64U);
    completion = state.on_callback(true, 64U);
    require(completion.next == tx_next_action::send_zlp);
    completion = state.on_callback(false, 0U);
    require(completion.next == tx_next_action::complete);
    require(completion.requested == 64U);
    require(completion.actual == 64U);
    require(!completion.success);
    require(!state.busy());

    state.start(32U);
    completion = state.on_callback(false, 5U);
    require(completion.next == tx_next_action::complete);
    require(completion.requested == 32U);
    require(completion.actual == 5U);
    require(!completion.success);
    require(!state.busy());

    state.start(64U);
    completion = state.on_callback(true, 32U);
    require(completion.next == tx_next_action::complete);
    require(completion.requested == 64U);
    require(completion.actual == 32U);
    require(!completion.success);
    require(!state.busy());
}

void test_f407_disconnect_completion_race() noexcept
{
    using pnx::f407::usb_detail::tx_session_state;
    namespace usb_detail = pnx::f407::usb_detail;

    int reused_instance = 0;
    int unrelated_instance = 0;
    tx_session_state session{};

    std::uint32_t pending = usb_detail::connection_event_none;
    pending = usb_detail::merge_connection_event(pending, false);
    pending = usb_detail::merge_connection_event(pending, true);
    require(
        (pending & usb_detail::connection_event_disconnected) != 0U);
    require(
        (pending & usb_detail::connection_event_connected) != 0U);

    pending = usb_detail::connection_event_none;
    pending = usb_detail::merge_connection_event(pending, true);
    pending = usb_detail::merge_connection_event(pending, false);
    require(
        (pending & usb_detail::connection_event_disconnected) != 0U);
    require(
        (pending & usb_detail::connection_event_connected) == 0U);

    require(session.activate(&reused_instance));
    const auto stale_ticket = session.begin_tx();
    require(stale_ticket.valid());
    require(session.start(stale_ticket, 64U));

    require(session.deactivate(&reused_instance));
    const auto stale_completion =
        session.on_callback(&reused_instance, true, 64U);
    require(!stale_completion.accepted);
    require(!session.busy());

    // USBX reuses the same class instance across reconnects. Its deactivate
    // path aborts transfers and suspends callback threads before invoking the
    // application deactivate callback; the stale ticket must still not become
    // current when that same pointer is activated again.
    require(session.activate(&reused_instance));
    require(!session.start(stale_ticket, 4U));
    const auto current_ticket = session.begin_tx();
    require(current_ticket.valid());
    require(session.start(current_ticket, 4U));

    const auto wrong_instance =
        session.on_callback(&unrelated_instance, true, 4U);
    require(!wrong_instance.accepted);
    require(session.busy());

    const auto current_completion =
        session.on_callback(&reused_instance, true, 4U);
    require(current_completion.accepted);
    require(current_completion.completion.success);
    require(!session.busy());
}

void test_f407_startup_lifecycle() noexcept
{
    using pnx::f407::usb_detail::startup_lifecycle;

    startup_lifecycle lifecycle{};
    require(!lifecycle.controller_active());
    require(!lifecycle.transport_usable());
    require(!lifecycle.faulted());

    lifecycle.schedule();
    require(lifecycle.scheduled());
    require(lifecycle.mark_controller_started());
    require(lifecycle.controller_active());
    require(!lifecycle.transport_usable());
    require(lifecycle.mark_transport_ready());
    require(lifecycle.transport_usable());

    lifecycle.disconnect();
    require(lifecycle.controller_active());
    require(!lifecycle.transport_usable());

    lifecycle.fail();
    require(lifecycle.faulted());
    require(!lifecycle.controller_active());
    require(!lifecycle.transport_usable());
    require(!lifecycle.mark_transport_ready());
}

} // namespace

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        return EXIT_FAILURE;
    }
    const std::string_view scenario{argv[1]};
    if (scenario == "same_config_reinit") test_same_config_reinit();
    else if (scenario == "init_resource_failure")
        test_init_resource_failure();
    else if (scenario == "async_startup_success")
        test_async_startup_success();
    else if (scenario == "controller_startup_failure")
        test_controller_startup_failure();
    else if (scenario == "controller_activation_failure")
        test_controller_activation_failure();
    else if (scenario == "incompatible_reinit")
        test_incompatible_reinit();
    else if (scenario == "callback_ownership")
        test_callback_ownership();
    else if (scenario == "identity_fail_closed")
        test_identity_fail_closed();
    else if (scenario == "write_before_connected") test_write_before_connected();
    else if (scenario == "write_lock_contention")
        test_write_lock_contention();
    else if (scenario == "bounded_write") test_bounded_write();
    else if (scenario == "backpressure") test_backpressure();
    else if (scenario == "invalid_write") test_invalid_write();
    else if (scenario == "rx_callback") test_rx_callback();
    else if (scenario == "disconnect_cancels") test_disconnect_cancels_without_replay();
    else if (scenario == "disconnect_pending_tx")
        test_disconnect_during_pending_tx();
    else if (scenario == "tx_error") test_tx_error();
    else if (scenario == "capabilities") test_capabilities();
    else if (scenario == "fill_tx_compat") test_fill_tx_compat();
    else if (scenario == "fill_tx_empty_retry")
        test_fill_tx_empty_retry();
    else if (scenario == "f407_zlp_boundary") test_f407_zlp_boundary();
    else if (scenario == "f407_disconnect_race")
        test_f407_disconnect_completion_race();
    else if (scenario == "f407_startup_lifecycle")
        test_f407_startup_lifecycle();
    else return EXIT_FAILURE;
    return EXIT_SUCCESS;
}
