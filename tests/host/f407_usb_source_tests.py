#!/usr/bin/env python3
"""Static acceptance tests for the F407 USBX CDC production integration."""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
BOARD = ROOT / "boards" / "dji_c_board_f407"


def read(path: Path) -> str:
    if not path.is_file():
        raise AssertionError(f"required file is missing: {path.relative_to(ROOT)}")
    return path.read_text(encoding="utf-8")


def require(text: str, token: str, label: str) -> None:
    if token not in text:
        raise AssertionError(f"{label} is missing required token: {token}")


def function_body(text: str, signature: str, label: str) -> str:
    start = text.find(signature)
    if start < 0:
        raise AssertionError(f"{label} is missing function: {signature}")
    opening = text.find("{", start)
    if opening < 0:
        raise AssertionError(f"{label} function has no body: {signature}")
    depth = 0
    for index in range(opening, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[opening + 1 : index]
    raise AssertionError(f"{label} function body is unterminated: {signature}")


def test_generated_closure() -> None:
    required = (
        BOARD / "Core" / "Inc" / "usb_otg.h",
        BOARD / "Core" / "Src" / "usb_otg.c",
        BOARD / "USBX" / "App" / "app_usbx_device.c",
        BOARD / "USBX" / "App" / "ux_device_descriptors.c",
        BOARD / "USBX" / "App" / "ux_device_descriptors.h",
        BOARD / "Middlewares" / "ST" / "usbx" / "common" / "core"
        / "src" / "ux_system_initialize.c",
        BOARD / "Drivers" / "STM32F4xx_HAL_Driver" / "Src"
        / "stm32f4xx_hal_pcd.c",
    )
    for path in required:
        assert path.is_file(), f"required generated closure is missing: {path.relative_to(ROOT)}"


def test_descriptor_identity_is_fail_closed() -> None:
    descriptor = read(BOARD / "USBX" / "App" / "ux_device_descriptors.h")
    cmake = read(ROOT / "CMakeLists.txt")
    require(descriptor, "PNX_USB_DEVICE_VID", "descriptor overlay")
    require(descriptor, "PNX_USB_DEVICE_PID", "descriptor overlay")
    require(descriptor, "PNX_USB_DEVICE_IDENTITY_CONFIRMED", "descriptor overlay")
    if re.search(r"#define\s+USBD_VID\s+0x0483", descriptor):
        raise AssertionError("production descriptor must not default to ST VID 0x0483")
    if re.search(r"#define\s+USBD_PID\s+0x5710", descriptor):
        raise AssertionError("production descriptor must not default to ST PID 0x5710")
    if re.search(
        r"#define\s+USBD_MANUFACTURER_STRING\s+\"STMicroelectronics\"",
        descriptor,
    ):
        raise AssertionError("production descriptor must not claim ST manufacturer identity")
    require(
        cmake,
        'set(PNX_USB_DEVICE_PRODUCT "PnX F407 CDC (unassigned)"',
        "fail-closed CMake identity",
    )


def test_usbx_lifecycle_and_backend() -> None:
    lifecycle = read(BOARD / "USBX" / "App" / "app_usbx_device.c")
    for token in (
        "ux_system_initialize",
        "ux_device_stack_initialize",
        "ux_device_stack_class_register",
        "_ux_system_slave_class_cdc_acm_name",
    ):
        require(lifecycle, token, "USBX lifecycle")

    backend = read(BOARD / "pnx_backends" / "usb_backend.cpp")
    if backend.count("tx_thread_create(") != 1:
        raise AssertionError("F407 USB backend must create exactly one BSP worker thread")
    for token in (
        "ux_device_class_cdc_acm_ioctl",
        "ux_device_class_cdc_acm_write_with_callback",
        "ux_dcd_stm32_initialize",
        "HAL_PCD_Start",
        "PNX_USB_DEVICE_IDENTITY_CONFIRMED",
    ):
        require(backend, token, "F407 USB backend")
    if "ux_device_class_cdc_acm_read(" in backend:
        raise AssertionError("F407 backend must not add a blocking BSP CDC read worker")


def test_irq_is_a_constant_time_bridge() -> None:
    irq = read(BOARD / "Core" / "Src" / "stm32f4xx_it.c")
    require(irq, "void OTG_FS_IRQHandler(void)", "F407 IRQ")
    require(irq, "HAL_PCD_IRQHandler(&hpcd_USB_OTG_FS);", "F407 IRQ")
    body = irq.split("void OTG_FS_IRQHandler(void)", 1)[1].split("}", 1)[0]
    forbidden = ("ux_device", "tx_", "memcpy", "printf")
    for token in forbidden:
        if token in body:
            raise AssertionError(f"OTG_FS_IRQHandler contains forbidden work: {token}")


def test_build_selection_and_presets() -> None:
    cmake = read(ROOT / "CMakeLists.txt")
    require(cmake, "usb_cdc", "top-level demo selection")
    require(cmake, "PNX_F407_USB_ENABLED", "F407 USB conditional build")
    presets = json.loads(read(ROOT / "CMakePresets.json"))
    names = {entry["name"] for entry in presets["configurePresets"]}
    candidate_path = ROOT / "release" / "f407-only-provenance.json"
    if candidate_path.is_file():
        candidate = json.loads(read(candidate_path))
        expected = {"usb-cdc", "usb-cdc-release"}
        declared = set(candidate.get("build", {}).get("firmware_presets", []))
        assert expected <= declared, "candidate provenance lacks USB presets"
        assert "PNX_BOARD" not in read(ROOT / "CMakePresets.json"), (
            "F407-only presets expose the board selector"
        )
    else:
        expected = {
            "dji-c-board-usb-cdc",
            "dji-c-board-usb-cdc-release",
        }
    missing = expected - names
    assert not missing, f"missing F407 USB presets: {sorted(missing)}"


def test_safe_demo_and_harness() -> None:
    demo = read(ROOT / "demo" / "cboard" / "usb_cdc" / "app.cpp")
    debug = read(
        ROOT / "demo" / "cboard" / "common" / "cboard_demo_debug.hpp"
    )
    debug_source = read(
        ROOT / "demo" / "cboard" / "common" / "cboard_demo_debug.cpp"
    )
    for token in (
        "bsp::usb::init",
        "bsp::usb::write",
        "demo_debug_instance",
        "heartbeat",
        "echo",
    ):
        require(demo, token, "USB CDC demo")
    require(debug, "usb_status", "USB CDC debugger state")
    require(
        debug,
        "abi_version = 4",
        "USB capability telemetry ABI version",
    )
    require(
        demo,
        "demo_debug_instance.usb_status",
        "USB CDC demo status publication",
    )
    require(
        debug_source,
        "bsp::usb::get_capabilities",
        "USB capability telemetry consumer",
    )
    require(
        debug,
        "usb_max_packet_size",
        "USB capability debugger state",
    )
    if "demo_debug_instance.uart_status = status_code(usb_status)" in demo:
        raise AssertionError("USB status must not overwrite UART telemetry")

    for token in (
        '#include "bsp_usart.hpp"',
        '#include "config.hpp"',
        "bsp::usart::init(",
        "app::uart::usart1",
        "bsp::usart::mode::dma",
        "bsp::usart::snapshot(",
        "send_uart_telemetry();",
    ):
        require(demo, token, "USB CDC USART1 telemetry")

    uart_body = function_body(
        demo,
        "void send_uart_telemetry() noexcept",
        "USB CDC USART1 telemetry",
    )
    require(
        uart_body,
        "bsp::usart::transmit(",
        "USB CDC USART1 telemetry",
    )
    for token in (
        "demo_debug_instance.abi_version",
        "demo_debug_instance.threadx_tick",
        "bsp::dwt::timeline_us()",
        "demo_debug_instance.thread_info_status",
        "demo_debug_instance.thread_stack_free",
        "demo_debug_instance.byte_pool_available",
        "demo_debug_instance.reset_reason_mask",
        "demo_debug_instance.crash_valid",
        "demo_debug_instance.fault_count",
        "demo_debug_instance.usb_identity_confirmed",
        "demo_debug_instance.usb_status",
        "demo_debug_instance.usb_link_state",
        "demo_debug_instance.usb_tx_queue_depth",
        "usb_state.pending_write_len",
        "usb_state.tx_queue_size",
        "usb_state.tx_queue_high_water",
        "demo_debug_instance.usb_error_count",
        "demo_debug_instance.usb_tx_drop_count",
        "demo_debug_instance.usb_connect_count",
        "demo_debug_instance.usb_disconnect_count",
        "blocked-unassigned",
    ):
        require(uart_body, token, "USB CDC USART1 telemetry")
    for forbidden in (
        "bsp::usb::connected()",
        "PNX_USB_DEVICE_IDENTITY_CONFIRMED",
    ):
        if forbidden in uart_body:
            raise AssertionError(
                "UART telemetry must remain available while USB identity "
                f"is blocked: {forbidden}"
            )
    if demo.count("tx_thread_create(") != 1:
        raise AssertionError(
            "F407 USB demo must keep exactly one application-owned thread"
        )

    harness = read(ROOT / "tools" / "usb" / "cdc_harness.py")
    for token in (
        "--port",
        "--vid",
        "--pid",
        "--serial",
        "--reopen",
        "--enumerate-only",
    ):
        require(harness, token, "CDC harness")


def test_expected_identity_block_is_not_a_runtime_fault() -> None:
    demo = read(ROOT / "demo" / "cboard" / "usb_cdc" / "app.cpp")
    for token in (
        "types::status usb_status = types::status::not_configured;",
        "const bool usb_status_expected =",
        "usb_status == types::status::not_configured",
        "!usb_status_expected",
    ):
        require(demo, token, "expected descriptor identity block")
    guarded_init = re.search(
        r"#if\s+PNX_USB_DEVICE_IDENTITY_CONFIRMED"
        r"(?P<body>[\s\S]*?)"
        r"#endif",
        demo,
    )
    if guarded_init is None:
        raise AssertionError(
            "USB init must be compile-time guarded by confirmed identity"
        )
    require(
        guarded_init.group("body"),
        "bsp::usb::init(",
        "confirmed-identity USB init guard",
    )
    outside_guard = (
        demo[: guarded_init.start()] + demo[guarded_init.end() :]
    )
    if "bsp::usb::init(" in outside_guard:
        raise AssertionError(
            "unconfirmed identity build must not invoke bsp::usb::init"
        )


def test_public_boundary_is_board_neutral() -> None:
    public_root = ROOT / "pnx_bsp" / "usb" / "include"
    forbidden = re.compile(
        r"(stm32|HAL_|hpcd_|USB_OTG|ux_api|UX_SLAVE|tx_api|TX_THREAD|"
        r"bridge_usb|dji_c_board)",
        re.IGNORECASE,
    )
    for path in public_root.glob("*.hpp"):
        match = forbidden.search(read(path))
        if match is not None:
            raise AssertionError(
                f"public USB header leaks board/runtime detail: "
                f"{path.relative_to(ROOT)}: {match.group(0)}"
            )


def test_usb_memory_intent_is_normal_sram() -> None:
    linker = read(BOARD / "STM32F407XX_FLASH.ld")
    backend = read(BOARD / "pnx_backends" / "usb_backend.cpp")
    azure = read(BOARD / "AZURE_RTOS" / "App" / "app_azure_rtos.c")
    require(linker, ".ram_d1_bss", "F407 linker")
    if not re.search(r"\.ram_d1_bss[\s\S]*?>RAM\b", linker):
        raise AssertionError("F407 ram_d1_bss is not mapped to normal SRAM")
    require(backend, "RAM_D1_BSS", "F407 USB backend SRAM placement")
    require(azure, "ux_device_byte_pool_buffer", "USBX pool")
    for text, label in ((backend, "backend"), (azure, "USBX pool")):
        if re.search(r"CCMRAM|ccmram", text):
            raise AssertionError(f"{label} must not place USB state in CCMRAM")


TESTS = (
    test_generated_closure,
    test_descriptor_identity_is_fail_closed,
    test_usbx_lifecycle_and_backend,
    test_irq_is_a_constant_time_bridge,
    test_build_selection_and_presets,
    test_safe_demo_and_harness,
    test_expected_identity_block_is_not_a_runtime_fault,
    test_public_boundary_is_board_neutral,
    test_usb_memory_intent_is_normal_sram,
)


def main() -> int:
    failures: list[str] = []
    for test in TESTS:
        try:
            test()
        except Exception as exc:  # noqa: BLE001 - aggregate all acceptance failures
            failures.append(f"{test.__name__}: {exc}")
    if failures:
        print("F407 USB source acceptance: FAIL", file=sys.stderr)
        for failure in failures:
            print(f" - {failure}", file=sys.stderr)
        return 1
    print(f"F407 USB source acceptance: PASS ({len(TESTS)} checks)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
