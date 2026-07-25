#!/usr/bin/env python3
"""Fail-closed CubeMX provenance and F407 production-closure gate."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path, PurePosixPath
from typing import Any, Iterable


BOARD_REL = Path("boards/dji_c_board_f407")
PROVENANCE_REL = BOARD_REL / "cubemx_provenance.json"
PRODUCTION_IOC_REL = BOARD_REL / "dji_c_board_f407.ioc"
REFERENCE_IOC_REL = BOARD_REL / "reference/dji_c_board_f407_full_reference.ioc"
CANDIDATE_PROVENANCE_REL = Path("release/f407-only-provenance.json")
TREE_FORMAT = "pnx-canonical-tree-v1"
HASH_RE = re.compile(r"^[0-9A-F]{64}$")

REQUIRED_IPS = {
    "CAN1", "CAN2", "DMA", "NVIC", "RCC", "SYS", "USART1", "USART3",
    "USART6", "USB_OTG_FS",
}
FORBIDDEN_IOC_TOKENS = (
    "I2C3", "SPI1", "TIM1.", "TIM2.", "TIM4.",
    "TIM5.", "TIM8.", "TIM10.", "S_TIM1_", "S_TIM2_", "S_TIM4_",
    "S_TIM5_", "S_TIM8_", "S_TIM10_", "ALGOBUILD", "MEMS",
)
REQUIRED_IOC_ASSIGNMENTS = (
    "Mcu.CPN=STM32F407IGH6TR", "Mcu.Family=STM32F4", "Mcu.Package=UFBGA176",
    "RCC.HSE_VALUE=12000000", "RCC.PLLM=6", "RCC.PLLN=168",
    "RCC.PLLP=RCC_PLLP_DIV2", "RCC.PLLQ=7",
    "RCC.PLLSourceVirtual=RCC_PLLSOURCE_HSE", "RCC.SYSCLKFreq_VALUE=168000000",
    "RCC.APB1Freq_Value=42000000", "RCC.APB2Freq_Value=84000000",
    "PA13.Signal=SYS_JTMS-SWDIO", "PA14.Signal=SYS_JTCK-SWCLK",
    "PH0-OSC_IN.Signal=RCC_OSC_IN", "PH1-OSC_OUT.Signal=RCC_OSC_OUT",
    "PD0.Signal=CAN1_RX", "PD1.Signal=CAN1_TX", "PB5.Signal=CAN2_RX",
    "PB6.Signal=CAN2_TX", "CAN1.CalculateBaudRate=1000000",
    "CAN1.BS1=CAN_BS1_10TQ", "CAN1.BS2=CAN_BS2_3TQ", "CAN1.Prescaler=3",
    "CAN2.CalculateBaudRate=1000000", "CAN2.BS1=CAN_BS1_10TQ",
    "CAN2.BS2=CAN_BS2_3TQ", "CAN2.Prescaler=3", "PA9.Signal=USART1_TX",
    "PB7.Signal=USART1_RX", "PC10.Signal=USART3_TX", "PC11.Signal=USART3_RX",
    "PG9.Signal=USART6_RX", "PG14.Signal=USART6_TX", "USART1.BaudRate=921600",
    "USART3.BaudRate=100000", "USART3.Mode=MODE_RX", "USART3.Parity=PARITY_EVEN",
    "USART3.WordLength=WORDLENGTH_9B", "USART6.BaudRate=115200",
    "Dma.USART1_RX.0.Instance=DMA2_Stream2", "Dma.USART1_TX.1.Instance=DMA2_Stream7",
    "Dma.USART3_RX.2.Instance=DMA1_Stream1", "Dma.USART3_RX.2.Mode=DMA_CIRCULAR",
    "Dma.USART6_RX.3.Instance=DMA2_Stream1", "Dma.USART6_TX.4.Instance=DMA2_Stream6",
    "NVIC.CAN1_RX0_IRQn=true\\:0\\:0\\:false\\:false\\:true\\:false\\:true\\:true\\:true",
    "NVIC.CAN1_SCE_IRQn=true\\:0\\:0\\:false\\:false\\:true\\:false\\:true\\:true\\:true",
    "NVIC.CAN1_TX_IRQn=true\\:0\\:0\\:false\\:false\\:true\\:false\\:true\\:true\\:true",
    "NVIC.CAN2_RX0_IRQn=true\\:0\\:0\\:true\\:false\\:true\\:false\\:true\\:true\\:true",
    "NVIC.CAN2_SCE_IRQn=true\\:0\\:0\\:true\\:false\\:true\\:false\\:true\\:true\\:true",
    "NVIC.CAN2_TX_IRQn=true\\:0\\:0\\:true\\:false\\:true\\:false\\:true\\:true\\:true",
    "NVIC.TimeBaseIP=TIM14", "VP_SYS_VS_tim14.Signal=SYS_VS_tim14",
    "STMicroelectronics.X-CUBE-AZRTOS-F4.1.1.0.RTOSJjThreadX_Checked=true",
    "STMicroelectronics.X-CUBE-AZRTOS-F4.1.1.0.TX_APP_MEM_POOL_SIZE=1024*11",
    "STMicroelectronics.X-CUBE-AZRTOS-F4.1.1.0.TX_ENABLE_STACK_CHECKING=1",
    "STMicroelectronics.X-CUBE-AZRTOS-F4.1.1.0.TX_TIMER_TICKS_PER_SECOND=1000",
    "PH10.GPIO_Label=LED_B", "PH11.GPIO_Label=LED_G",
    "PH12.GPIO_Label=LED_R", "PH10.PinState=GPIO_PIN_RESET",
    "PH11.PinState=GPIO_PIN_RESET", "PH12.PinState=GPIO_PIN_RESET",
    "PA4.GPIO_Label=IMU_ACCEL_CS", "PA4.PinState=GPIO_PIN_SET",
    "PB0.GPIO_Label=IMU_GYRO_CS", "PB0.PinState=GPIO_PIN_SET",
    "PG6.GPIO_Label=IST8310_RSTN", "PG6.PinState=GPIO_PIN_SET",
    "PA11.Signal=USB_OTG_FS_DM", "PA12.Signal=USB_OTG_FS_DP",
    "USB_OTG_FS.VirtualMode=Device_Only",
    "NVIC.OTG_FS_IRQn=true\\:5\\:0\\:false\\:false\\:true\\:false\\:true\\:true\\:true",
    "STMicroelectronics.X-CUBE-AZRTOS-F4.1.1.0.USBJjUSBX_Checked=true",
    "STMicroelectronics.X-CUBE-AZRTOS-F4.1.1.0.USBXCcUSBJjUSBXJjUXOoDeviceOoClassOoCDCOoACM=true",
    "STMicroelectronics.X-CUBE-AZRTOS-F4.1.1.0.UX_DEVICE_APP_MEM_POOL_SIZE=1024*12",
)
REQUIRED_REQUIREMENTS = {
    "mcu", "clock", "debug_swd", "hal_threadx_tick", "gpio_led_safe_state",
    "unused_pin_policy", "can1", "can2", "can_transceiver_control", "usart1",
    "usart3", "usart6", "dma", "irq", "threadx", "dwt_board_code",
    "full_to_minimal_mapping", "usb_otg_fs", "usbx_cdc",
    "usb_descriptor_identity",
}
GENERATED_ROOTS = ("AZURE_RTOS", "Core", "Drivers", "Middlewares", "USBX")
GENERATED_FILES = ("startup_stm32f407xx.s", "STM32F407XX_FLASH.ld")
FORBIDDEN_FORMAL_PATHS = {
    "Core/Inc/crc.h", "Core/Inc/i2c.h", "Core/Inc/spi.h", "Core/Inc/tim.h",
    "Core/Src/crc.c", "Core/Src/i2c.c", "Core/Src/spi.c",
    "Core/Src/tim.c",
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_crc.c",
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_i2c.c",
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_i2c_ex.c",
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_spi.c",
    "Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_crc.h",
    "Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_i2c.h",
    "Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_i2c_ex.h",
    "Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_spi.h",
    "Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_crc.h",
    "Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_i2c.h",
    "Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_spi.h",
    "Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_tim.h",
}
BASE_APPLICATION_SOURCES = {
    "AZURE_RTOS/App/app_azure_rtos.c", "Core/Src/tx_initialize_low_level.s",
    "Core/Src/main.c", "Core/Src/gpio.c", "Core/Src/app_threadx.c",
    "Core/Src/can.c", "Core/Src/dma.c", "Core/Src/usart.c",
    "Core/Src/stm32f4xx_it.c", "Core/Src/stm32f4xx_hal_msp.c",
    "Core/Src/stm32f4xx_hal_timebase_tim.c", "Core/Src/sysmem.c",
    "Core/Src/syscalls.c", "startup_stm32f407xx.s",
}
DRIVER_SOURCES = {
    "Core/Src/system_stm32f4xx.c",
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c",
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_can.c",
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c",
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c",
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma_ex.c",
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_exti.c",
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c",
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ex.c",
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ramfunc.c",
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c",
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr.c",
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr_ex.c",
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c",
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc_ex.c",
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim.c",
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim_ex.c",
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c",
}
USB_APPLICATION_SOURCES = {
    "USBX/App/app_usbx_device.c",
    "USBX/App/ux_device_cdc_acm.c",
    "USBX/App/ux_device_descriptors.c",
    "Core/Src/usb_otg.c",
}
USB_DRIVER_SOURCES = {
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pcd.c",
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pcd_ex.c",
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_usb.c",
}


class GateFailure(RuntimeError):
    pass


def require(condition: bool, message: str, errors: list[str]) -> None:
    if not condition:
        errors.append(message)


def unique_entry_index(
    entries: Any,
    key: str,
    label: str,
    errors: list[str],
) -> dict[str, dict[str, Any]]:
    indexed: dict[str, dict[str, Any]] = {}
    if not isinstance(entries, list):
        return indexed
    for position, item in enumerate(entries):
        require(isinstance(item, dict), f"{label}[{position}] must be an object", errors)
        if not isinstance(item, dict):
            continue
        value = item.get(key)
        require(
            isinstance(value, str) and bool(value),
            f"{label}[{position}] has invalid {key}",
            errors,
        )
        if not isinstance(value, str) or not value:
            continue
        require(value not in indexed, f"duplicate {label} {key}: {value}", errors)
        if value not in indexed:
            indexed[value] = item
    return indexed


def raw_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def canonical_metadata(path: Path) -> dict[str, Any]:
    raw = path.read_bytes()
    kind = "binary" if b"\0" in raw else "text"
    payload = raw if kind == "binary" else raw.replace(b"\r\n", b"\n").replace(b"\r", b"\n")
    return {
        "kind": kind,
        "sha256": hashlib.sha256(payload).hexdigest().upper(),
        "size": len(payload),
    }


def unique_object(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    value: dict[str, Any] = {}
    for key, item in pairs:
        if key in value:
            raise GateFailure(f"duplicate JSON key: {key}")
        value[key] = item
    return value


def load_json(path: Path) -> dict[str, Any]:
    if not path.is_file():
        raise GateFailure(f"missing required file: {path}")
    try:
        value = json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=unique_object)
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise GateFailure(f"cannot parse {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise GateFailure(f"JSON root must be an object: {path}")
    return value


def safe_relative(value: str) -> bool:
    candidate = PurePosixPath(value)
    return bool(value) and not candidate.is_absolute() and ".." not in candidate.parts and not any(
        token in value for token in ("\\", ":", "\t", "\n", "\r")
    )


def validate_hash(value: Any, label: str, errors: list[str]) -> None:
    require(isinstance(value, str) and bool(HASH_RE.fullmatch(value)), f"invalid SHA256: {label}", errors)


def parse_ioc_assignments(text: str) -> dict[str, str]:
    assignments: dict[str, str] = {}
    for line_number, raw in enumerate(text.splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if "=" not in line:
            raise GateFailure(f"invalid IOC line {line_number}: missing '='")
        key, value = line.split("=", 1)
        if not key:
            raise GateFailure(f"invalid IOC line {line_number}: empty key")
        if key in assignments:
            raise GateFailure(f"duplicate IOC key at line {line_number}: {key}")
        assignments[key] = value
    return assignments


def check_required_ioc_assignments(
    assignments: dict[str, str],
    required: Iterable[str],
    errors: list[str],
) -> None:
    for expected in required:
        key, value = expected.split("=", 1)
        actual = assignments.get(key)
        require(
            actual == value,
            f"IOC assignment mismatch: {key}: expected {value!r}, got {actual!r}",
            errors,
        )


def tree_records(manifest: dict[str, Any], label: str, errors: list[str]) -> dict[str, dict[str, Any]]:
    require(manifest.get("format") == TREE_FORMAT, f"wrong tree format: {label}", errors)
    files = manifest.get("files")
    require(isinstance(files, list), f"tree files must be a list: {label}", errors)
    if not isinstance(files, list):
        return {}
    records: dict[str, dict[str, Any]] = {}
    folded: set[str] = set()
    previous = ""
    serialized = bytearray()
    for index, item in enumerate(files):
        if not isinstance(item, dict):
            errors.append(f"tree entry {index} is not an object: {label}")
            continue
        require(set(item) == {"kind", "path", "sha256", "size"}, f"wrong tree entry schema: {label}[{index}]", errors)
        path = item.get("path")
        require(isinstance(path, str) and safe_relative(path), f"unsafe tree path: {label}[{index}]", errors)
        if not isinstance(path, str) or not safe_relative(path):
            continue
        require(path > previous, f"tree paths are not strictly sorted: {label}: {path}", errors)
        previous = path
        require(path.casefold() not in folded, f"case-insensitive path collision: {label}: {path}", errors)
        folded.add(path.casefold())
        kind = item.get("kind")
        size = item.get("size")
        digest = item.get("sha256")
        require(kind in {"text", "binary"}, f"wrong file kind: {label}: {path}", errors)
        require(isinstance(size, int) and size >= 0, f"wrong file size: {label}: {path}", errors)
        validate_hash(digest, f"{label}:{path}", errors)
        if kind in {"text", "binary"} and isinstance(size, int) and isinstance(digest, str):
            serialized.extend(f"{kind}\t{size}\t{digest}\t{path}\n".encode("utf-8"))
        records[path] = item
    require(manifest.get("file_count") == len(files), f"wrong file_count: {label}", errors)
    expected_tree = hashlib.sha256(serialized).hexdigest().upper()
    validate_hash(manifest.get("tree_sha256"), f"{label}.tree_sha256", errors)
    require(manifest.get("tree_sha256") == expected_tree, f"tree hash mismatch: {label}", errors)
    return records


def load_tree(repo_root: Path, entry: Any, label: str, errors: list[str]) -> tuple[dict[str, Any], dict[str, dict[str, Any]]]:
    require(isinstance(entry, dict), f"{label} entry must be an object", errors)
    if not isinstance(entry, dict):
        return {}, {}
    relative = entry.get("path")
    require(isinstance(relative, str) and safe_relative(relative), f"unsafe manifest path: {label}", errors)
    if not isinstance(relative, str) or not safe_relative(relative):
        return {}, {}
    path = repo_root / relative
    manifest = load_json(path)
    validate_hash(entry.get("sha256"), f"{label}.sha256", errors)
    require(
        entry.get("sha256") == canonical_metadata(path)["sha256"],
        f"manifest file hash mismatch: {label}",
        errors,
    )
    records = tree_records(manifest, label, errors)
    require(entry.get("tree_sha256") == manifest.get("tree_sha256"), f"declared tree hash mismatch: {label}", errors)
    require(entry.get("file_count") == manifest.get("file_count"), f"declared file count mismatch: {label}", errors)
    return manifest, records


def actual_formal_paths(board_root: Path) -> list[str]:
    result: list[str] = []
    for root in GENERATED_ROOTS:
        base = board_root / root
        result.extend(path.relative_to(board_root).as_posix() for path in base.rglob("*") if path.is_file())
    for name in GENERATED_FILES:
        if (board_root / name).is_file():
            result.append(name)
    return sorted(result)


def check_formal_tree(board_root: Path, manifest: dict[str, Any], records: dict[str, dict[str, Any]], errors: list[str]) -> None:
    actual = actual_formal_paths(board_root)
    require(actual == sorted(records), "formal generated-owned inventory has missing or unlisted extra files", errors)
    for relative in actual:
        path = board_root / relative
        require(not path.is_symlink(), f"symlink forbidden in formal tree: {relative}", errors)
        if relative in records:
            metadata = canonical_metadata(path)
            expected = {key: records[relative][key] for key in ("kind", "sha256", "size")}
            require(metadata == expected, f"formal file drift: {relative}", errors)
    require(not (set(actual) & FORBIDDEN_FORMAL_PATHS), "reference-only peripheral files re-entered formal tree", errors)


def cache_value(value: Any) -> Any:
    if isinstance(value, dict):
        if set(value) - {"type", "value"} or "value" not in value:
            raise GateFailure("unsupported cacheVariable object")
        return value["value"]
    return value


def named_presets(data: dict[str, Any], key: str) -> dict[str, dict[str, Any]]:
    items = data.get(key, [])
    if not isinstance(items, list):
        raise GateFailure(f"{key} must be a list")
    result: dict[str, dict[str, Any]] = {}
    for item in items:
        if not isinstance(item, dict) or not isinstance(item.get("name"), str):
            raise GateFailure(f"invalid {key} entry")
        if item["name"] in result:
            raise GateFailure(f"duplicate preset name: {item['name']}")
        if "condition" in item:
            raise GateFailure(f"preset conditions are unsupported by the production gate: {item['name']}")
        result[item["name"]] = item
    return result


def inherited_names(preset: dict[str, Any]) -> list[str]:
    inherits = preset.get("inherits", [])
    if isinstance(inherits, str):
        return [inherits]
    if not isinstance(inherits, list) or not all(isinstance(item, str) for item in inherits):
        raise GateFailure("invalid preset inheritance")
    return inherits


def resolved_cache(name: str, presets: dict[str, dict[str, Any]], stack: tuple[str, ...] = ()) -> dict[str, Any]:
    if name in stack:
        raise GateFailure(f"preset inheritance cycle at {name}")
    if name not in presets:
        raise GateFailure(f"unknown inherited configure preset: {name}")
    preset = presets[name]
    variables: dict[str, Any] = {}
    # CMake gives earlier parents precedence, so merge parents in reverse order.
    for parent in reversed(inherited_names(preset)):
        variables.update(resolved_cache(parent, presets, (*stack, name)))
    own = preset.get("cacheVariables", {})
    if not isinstance(own, dict):
        raise GateFailure(f"cacheVariables must be an object: {name}")
    for key, raw in own.items():
        value = cache_value(raw)
        if value is None:
            variables.pop(key, None)
        else:
            variables[key] = value
    return variables


def resolved_build_configure(name: str, presets: dict[str, dict[str, Any]], stack: tuple[str, ...] = ()) -> str | None:
    if name in stack:
        raise GateFailure(f"build preset inheritance cycle at {name}")
    if name not in presets:
        raise GateFailure(f"unknown inherited build preset: {name}")
    preset = presets[name]
    value: str | None = None
    for parent in reversed(inherited_names(preset)):
        parent_value = resolved_build_configure(parent, presets, (*stack, name))
        if parent_value is not None:
            value = parent_value
    if "configurePreset" in preset:
        own = preset["configurePreset"]
        if not isinstance(own, str):
            raise GateFailure(f"invalid configurePreset: {name}")
        value = own
    return value


def f407_only_provenance(repo_root: Path) -> dict[str, Any] | None:
    path = repo_root / CANDIDATE_PROVENANCE_REL
    if not path.is_file():
        return None
    data = load_json(path)
    if not isinstance(data, dict):
        raise GateFailure("F407-only provenance must be an object")
    if data.get("manifest_type") != "pnx-f407-only-export-provenance":
        raise GateFailure("wrong F407-only provenance manifest type")
    return data


def f407_presets(repo_root: Path) -> list[str]:
    data = load_json(repo_root / "CMakePresets.json")
    if "include" in data:
        raise GateFailure("CMakePresets include is unsupported; extend the gate before using it")
    configure = named_presets(data, "configurePresets")
    builds = named_presets(data, "buildPresets")
    candidate = f407_only_provenance(repo_root)
    if candidate is not None:
        selected: list[str] = []
        for name, preset in configure.items():
            cache = resolved_cache(name, configure)
            if "PNX_BOARD" in cache:
                raise GateFailure(
                    f"F407-only preset must not expose PNX_BOARD: {name}"
                )
            host_only = cache.get("PNX_HOST_TESTS") in {
                True,
                1,
                "1",
                "ON",
                "TRUE",
                "YES",
            }
            if not preset.get("hidden", False) and not host_only:
                selected.append(name)
        declared = candidate.get("build", {}).get("firmware_presets")
        if (
            not isinstance(declared, list)
            or not all(isinstance(item, str) for item in declared)
            or len(set(declared)) != len(declared)
        ):
            raise GateFailure(
                "candidate build.firmware_presets must be a unique string list"
            )
        if selected != declared:
            raise GateFailure(
                "candidate F407 preset inventory differs from provenance"
            )
        build_targets = {
            resolved_build_configure(name, builds)
            for name, preset in builds.items()
            if not preset.get("hidden", False)
        }
        missing = [name for name in selected if name not in build_targets]
        if missing:
            raise GateFailure(
                "F407 configure presets lack build presets: "
                + ", ".join(missing)
            )
        return selected

    selected = sorted(
        name for name, preset in configure.items()
        if not preset.get("hidden", False)
        and resolved_cache(name, configure).get("PNX_BOARD") == "dji_c_board_f407"
    )
    for name in configure:
        if name.startswith("dji-c-board") and resolved_cache(name, configure).get("PNX_BOARD") != "dji_c_board_f407":
            raise GateFailure(f"F407-named preset resolves to another board: {name}")
    build_targets = {
        resolved_build_configure(name, builds)
        for name, preset in builds.items() if not preset.get("hidden", False)
    }
    missing = sorted(set(selected) - build_targets)
    if missing:
        raise GateFailure("F407 configure presets lack build presets: " + ", ".join(missing))
    return selected


def run_git(repo_root: Path, *args: str) -> str:
    return subprocess.run(["git", *args], cwd=repo_root, check=True, capture_output=True, text=True).stdout


def submodule_inventory(repo_root: Path, check_clean: bool, errors: list[str]) -> list[dict[str, str]]:
    modules_output = run_git(repo_root, "config", "--file", ".gitmodules", "--get-regexp", "path")
    module_paths = sorted(line.split(maxsplit=1)[1].replace("\\", "/") for line in modules_output.splitlines())
    for path in module_paths:
        require(safe_relative(path), f"unsafe submodule path: {path}", errors)
    index: dict[str, str] = {}
    for line in run_git(repo_root, "ls-files", "--stage").splitlines():
        match = re.match(r"^160000 ([0-9a-f]{40}) \d\t(.+)$", line)
        if match:
            index[match.group(2).replace("\\", "/")] = match.group(1)
    require(module_paths == sorted(index), ".gitmodules and index gitlink sets differ", errors)

    status_paths: set[str] = set()
    for line in run_git(repo_root, "submodule", "status", "--recursive").splitlines():
        match = re.match(r"^(.)([0-9a-f]{40}) (.+?)(?: \(.*\))?$", line)
        require(match is not None, f"cannot parse submodule status: {line}", errors)
        if match is None:
            continue
        prefix, commit, path = match.group(1), match.group(2), match.group(3).replace("\\", "/")
        status_paths.add(path)
        require(prefix == " ", f"submodule is uninitialized, conflicted, or mismatched: {path}", errors)
        require(index.get(path) == commit, f"submodule HEAD does not match gitlink: {path}", errors)
        if check_clean and (repo_root / path).is_dir():
            require(not run_git(repo_root / path, "status", "--short").strip(), f"dirty submodule: {path}", errors)
    require(status_paths == set(module_paths), "initialized recursive submodule set differs", errors)
    return [{"gitlink": index[path], "path": path} for path in module_paths]


def expected_source_paths(
    formal_records: dict[str, dict[str, Any]],
    include_usb: bool = False,
) -> set[str]:
    common = {
        path for path in formal_records
        if path.startswith("Middlewares/ST/threadx/common/src/") and path.endswith(".c")
    }
    port = {
        path for path in formal_records
        if path.startswith("Middlewares/ST/threadx/ports/cortex_m4/gnu/src/") and path.endswith(".S")
    }
    expected = BASE_APPLICATION_SOURCES | DRIVER_SOURCES | common | port
    if include_usb:
        def device_core_source(path: str) -> bool:
            name = PurePosixPath(path).name
            forbidden_prefixes = (
                "ux_host_",
                "ux_hcd_",
                "ux_dcd_sim_",
                "ux_system_otg_",
                "ux_utility_pci_",
            )
            return not name.startswith(forbidden_prefixes)

        usbx = {
            path for path in formal_records
            if path.endswith(".c") and (
                (
                    path.startswith(
                        "Middlewares/ST/usbx/common/core/src/")
                    and device_core_source(path)
                )
                or path.startswith(
                    "Middlewares/ST/usbx/common/usbx_device_classes/src/"
                    "ux_device_class_cdc_acm_"
                )
                or path.startswith(
                    "Middlewares/ST/usbx/common/"
                    "usbx_stm32_device_controllers/"
                )
            )
        }
        expected |= USB_APPLICATION_SOURCES | USB_DRIVER_SOURCES | usbx
    return expected


def check_source_graph(
    repo_root: Path,
    board_root: Path,
    source_values: Iterable[str],
    formal_records: dict[str, dict[str, Any]],
    errors: list[str],
) -> None:
    repo_resolved = repo_root.resolve()
    board_resolved = board_root.resolve()
    legacy_board_root = (repo_resolved / "board").resolve()
    board_collection_root = (repo_resolved / "boards").resolve()
    actual: set[str] = set()
    for raw in source_values:
        if "$<" in raw:
            errors.append(f"generator expression is unsupported in F407 source graph: {raw}")
            continue
        path = Path(raw).resolve()
        try:
            relative = path.relative_to(board_resolved).as_posix()
        except ValueError:
            # Shared PnX sources live outside the selected board tree. Reject
            # known board-owned sources here instead of silently delegating
            # them to the layer-boundary gate.
            foreign = False
            for candidate in (legacy_board_root, board_collection_root):
                try:
                    path.relative_to(candidate)
                except ValueError:
                    continue
                foreign = True
                break
            if foreign:
                errors.append(
                    f"foreign board source entered F407 target graph: "
                    f"{path.relative_to(repo_resolved).as_posix()}"
                )
            continue
        if relative.startswith("pnx_backends/"):
            continue
        if relative in GENERATED_FILES or relative.split("/", 1)[0] in GENERATED_ROOTS:
            actual.add(relative)
            continue
        errors.append(f"unclassified F407 board source entered target graph: {relative}")
    include_usb = bool(actual & (
        USB_APPLICATION_SOURCES | USB_DRIVER_SOURCES
    )) or any(path.startswith("Middlewares/ST/usbx/") for path in actual)
    expected = expected_source_paths(
        formal_records, include_usb=include_usb)
    require(actual == expected, "F407 generated source graph differs from the approved closure", errors)
    require(not (actual & FORBIDDEN_FORMAL_PATHS), "reference-only source entered F407 target graph", errors)


def check_semantics(repo_root: Path, errors: list[str]) -> None:
    board = repo_root / BOARD_REL
    hal_conf = (board / "Core/Inc/stm32f4xx_hal_conf.h").read_text(encoding="utf-8")
    for module in ("CRC", "I2C", "SPI"):
        enabled = re.search(
            rf"(?m)^\s*#define\s+HAL_{module}_MODULE_ENABLED\b",
            hal_conf,
        )
        require(enabled is None, f"HAL {module} closure is enabled", errors)
    rte = (board / "Core/Inc/RTE_Components.h").read_text(encoding="utf-8")
    require(
        "USBXDEVICE_ENABLED" in rte,
        "USBX device component closure is not enabled",
        errors,
    )
    require(
        re.search(r"(?m)^\s*#define\s+HAL_PCD_MODULE_ENABLED\b", hal_conf)
        is not None,
        "HAL PCD closure is not enabled",
        errors,
    )
    gpio = (board / "Core/Src/gpio.c").read_text(encoding="utf-8")
    for token in ("IST8310_RSTN_Pin", "LED_R_Pin|LED_G_Pin|LED_B_Pin", "IMU_ACCEL_CS_Pin", "IMU_GYRO_CS_Pin"):
        require(token in gpio, f"minimal GPIO evidence missing: {token}", errors)
    for token in ("GPIO_PIN_8", "GPIO_PIN_3", "GPIO_MODE_IT_FALLING"):
        require(token not in gpio, f"reference-only GPIO behavior remains: {token}", errors)
    main = (board / "Core/Src/main.c").read_text(encoding="utf-8")
    for token in ("MX_CRC_Init", "MX_I2C", "MX_SPI", "MX_TIM1_Init"):
        require(token not in main, f"reference-only init call remains: {token}", errors)
    require(
        "MX_USB_OTG_FS_PCD_Init" in main,
        "F407 USB PCD init is missing",
        errors,
    )
    lifecycle = (board / "USBX/App/app_usbx_device.c").read_text(
        encoding="utf-8")
    for token in (
        "ux_system_initialize",
        "ux_device_stack_initialize",
        "ux_device_stack_class_register",
    ):
        require(token in lifecycle, f"USBX lifecycle missing: {token}", errors)
    descriptor = (board / "USBX/App/ux_device_descriptors.h").read_text(
        encoding="utf-8")
    require(
        "#define USBD_VID                                       PNX_USB_DEVICE_VID"
        in descriptor,
        "USB VID is not controlled by the PnX identity overlay",
        errors,
    )
    require(
        not re.search(
            r"#define\s+USBD_MANUFACTURER_STRING\s+\"STMicroelectronics\"",
            descriptor,
        ),
        "USB descriptor still claims ST manufacturer identity",
        errors,
    )
    linker = (board / "STM32F407XX_FLASH.ld").read_text(encoding="utf-8")
    for token in (".dma_buffer", ".noinit", ".ram_d1_bss", "__noinit_start__", "_sstack"):
        require(token in linker, f"linker overlay missing: {token}", errors)
    dwt = (repo_root / "pnx_bsp/dwt/src/bsp_dwt.cpp").read_text(encoding="utf-8")
    for token in ("CoreDebug->DEMCR", "DWT->CYCCNT", "DWT_CTRL_CYCCNTENA_Msk"):
        require(token in dwt, f"DWT board-code evidence missing: {token}", errors)


def check_gate(repo_root: Path, args: argparse.Namespace) -> tuple[list[str], list[dict[str, str]]]:
    errors: list[str] = []
    provenance = load_json(repo_root / PROVENANCE_REL)
    require(provenance.get("schema_version") == 2, "unsupported provenance schema", errors)
    require(provenance.get("a07_status") == "CLOSED_WITH_DEFERRED_EVIDENCE", "wrong A-07 status", errors)

    iocs = provenance.get("iocs", {})
    require(isinstance(iocs, dict), "iocs must be an object", errors)
    for key, expected in (("production", PRODUCTION_IOC_REL), ("reference", REFERENCE_IOC_REL)):
        entry = iocs.get(key, {}) if isinstance(iocs, dict) else {}
        path_value = entry.get("path") if isinstance(entry, dict) else None
        require(path_value == expected.as_posix(), f"wrong {key} IOC path", errors)
        path = repo_root / expected
        require(path.is_file(), f"missing {key} IOC", errors)
        if path.is_file() and isinstance(entry, dict):
            validate_hash(entry.get("sha256"), f"iocs.{key}.sha256", errors)
            require(entry.get("sha256") == raw_sha256(path), f"{key} IOC hash drift", errors)
    require(isinstance(iocs.get("reference"), dict) and iocs["reference"].get("production_use") == "forbidden", "reference IOC is not forbidden from production", errors)

    generator = provenance.get("generator", {})
    require(isinstance(generator, dict), "generator must be an object", errors)
    if isinstance(generator, dict):
        authoring = generator.get("authoring", {})
        audit = generator.get("audit", {})
        package = generator.get("firmware_package", {})
        require(authoring == {"cubemx_version": "6.15.0", "db_version": "DB.6.0.150"}, "wrong IOC authoring pin", errors)
        require(audit.get("cubemx_version") == "6.17.0-RC5", "wrong audit CubeMX version", errors)
        validate_hash(audit.get("executable_sha256"), "generator.audit.executable_sha256", errors)
        require(package.get("id") == "STM32Cube_FW_F4_V1.28.3", "wrong F4 firmware package", errors)
        validate_hash(package.get("package_xml_sha256"), "generator.firmware_package.package_xml_sha256", errors)
        require(generator.get("exact_replay_status") == "DEFERRED_NON_BLOCKING_FOR_HARDWARE_BRINGUP", "wrong exact replay status", errors)
        require(generator.get("production_toolchain_reproducibility") == "DEFERRED_PENDING_VERSION_FREEZE", "wrong production reproducibility status", errors)

    generation = provenance.get("generation", {})
    require(isinstance(generation, dict), "generation must be an object", errors)
    gen1_manifest, gen1_records = load_tree(repo_root, generation.get("gen1", {}), "generation.gen1", errors)
    gen2_manifest, gen2_records = load_tree(repo_root, generation.get("gen2", {}), "generation.gen2", errors)
    require(gen1_records == gen2_records, "Gen1 and Gen2 manifests differ", errors)
    delta_entry = generation.get("delta", {}) if isinstance(generation, dict) else {}
    require(isinstance(delta_entry, dict), "generation.delta must be an object", errors)
    if isinstance(delta_entry, dict) and isinstance(delta_entry.get("path"), str):
        delta_path = repo_root / delta_entry["path"]
        delta = load_json(delta_path)
        validate_hash(delta_entry.get("sha256"), "generation.delta.sha256", errors)
        require(
            delta_entry.get("sha256") == canonical_metadata(delta_path)["sha256"],
            "delta manifest hash drift",
            errors,
        )
        require(delta.get("changes") == [], "Gen1 -> Gen2 has unaccepted changes", errors)
        require(delta.get("gen1_tree_sha256") == gen1_manifest.get("tree_sha256"), "delta Gen1 hash mismatch", errors)
        require(delta.get("gen2_tree_sha256") == gen2_manifest.get("tree_sha256"), "delta Gen2 hash mismatch", errors)
        require(delta_entry.get("nonsemantic_noise") == [], "unexpected noise classification", errors)

    integration = provenance.get("formal_integration", {})
    require(isinstance(integration, dict) and integration.get("mode") == "reviewed_patch", "formal integration must be reviewed_patch", errors)
    formal_manifest, formal_records = load_tree(repo_root, integration.get("formal_inventory", {}) if isinstance(integration, dict) else {}, "formal_integration.formal_inventory", errors)
    board_root = repo_root / BOARD_REL
    check_formal_tree(board_root, formal_manifest, formal_records, errors)
    governed_gen2 = {
        path: record for path, record in gen2_records.items()
        if path in GENERATED_FILES or path.split("/", 1)[0] in GENERATED_ROOTS
    }
    hand_entries = integration.get("hand_owned_files", []) if isinstance(integration, dict) else []
    patch_entries = integration.get("reviewed_patches", []) if isinstance(integration, dict) else []
    require(isinstance(hand_entries, list), "hand_owned_files must be a list", errors)
    require(isinstance(patch_entries, list), "reviewed_patches must be a list", errors)
    hand = unique_entry_index(hand_entries, "path", "hand_owned_files", errors)
    patches = unique_entry_index(patch_entries, "path", "reviewed_patches", errors)
    require(set(formal_records) == set(governed_gen2) | set(hand), "formal tree is not Gen2 plus declared hand-owned files", errors)
    for path in sorted(set(formal_records) & set(governed_gen2)):
        changed = formal_records[path] != governed_gen2[path]
        require(changed == (path in patches), f"reviewed-patch classification mismatch: {path}", errors)
        if changed and path in patches:
            entry = patches[path]
            require(entry.get("base_sha256") == governed_gen2[path].get("sha256"), f"wrong patch base hash: {path}", errors)
            require(entry.get("production_sha256") == formal_records[path].get("sha256"), f"wrong patch production hash: {path}", errors)
            require(bool(entry.get("reason")), f"missing patch reason: {path}", errors)
    for path, entry in hand.items():
        require(path in formal_records and path not in governed_gen2, f"invalid hand-owned path: {path}", errors)
        require(bool(entry.get("reason")), f"missing hand-owned reason: {path}", errors)
    excluded = sorted(set(gen2_records) - set(governed_gen2))
    require(integration.get("excluded_generated_project_files") == excluded, "excluded CubeMX project-file inventory is stale", errors)

    requirements = provenance.get("requirements", [])
    require(isinstance(requirements, list), "requirements must be a list", errors)
    requirement_index = unique_entry_index(requirements, "id", "requirements", errors)
    require(set(requirement_index) == REQUIRED_REQUIREMENTS, "requirement mapping is incomplete or has unknown IDs", errors)
    for item in requirement_index.values():
        require(item.get("status") in {"static_verified", "hardware_unverified", "not_applicable"}, f"invalid requirement status: {item.get('id')}", errors)
        require(bool(item.get("evidence")), f"missing requirement evidence: {item.get('id')}", errors)

    production_path = repo_root / PRODUCTION_IOC_REL
    if production_path.is_file():
        ioc_text = production_path.read_text(encoding="utf-8")
        assignments = parse_ioc_assignments(ioc_text)
        ips = {
            value for key, value in assignments.items()
            if re.fullmatch(r"Mcu\.IP\d+", key)
        }
        require(ips == REQUIRED_IPS, f"production IOC IP closure mismatch: {sorted(ips)}", errors)
        for token in FORBIDDEN_IOC_TOKENS:
            require(token not in ioc_text, f"forbidden production IOC token: {token}", errors)
        check_required_ioc_assignments(assignments, REQUIRED_IOC_ASSIGNMENTS, errors)
        require(
            not any("DWT" in key or "DWT" in value for key, value in assignments.items()),
            "DWT must be board/BSP code, not IOC metadata",
            errors,
        )

    check_semantics(repo_root, errors)
    presets = f407_presets(repo_root)
    submodules = submodule_inventory(repo_root, args.check_submodules_clean, errors)
    baseline = provenance.get("inventory_baseline", {})
    require(isinstance(baseline, dict), "inventory_baseline must be an object", errors)
    if isinstance(baseline, dict):
        if f407_only_provenance(repo_root) is None:
            require(baseline.get("f407_configure_presets") == presets, "reviewed F407 preset inventory is stale", errors)
        require(baseline.get("submodules") == submodules, "reviewed submodule inventory is stale", errors)

    if args.selected_ioc is not None:
        require(args.selected_ioc.resolve() == production_path.resolve(), "selected IOC is not the locked production IOC", errors)
    source_values = list(args.source)
    if args.source_list_file is not None:
        source_values.extend(
            line for line in args.source_list_file.read_text(encoding="utf-8").splitlines()
            if line
        )
    if source_values:
        check_source_graph(
            repo_root, board_root, source_values, formal_records, errors
        )
    if errors:
        raise GateFailure("\n".join(f"- {error}" for error in errors))
    return presets, submodules


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--list-f407-presets", action="store_true")
    parser.add_argument("--list-submodules", action="store_true")
    parser.add_argument("--check-submodules-clean", action="store_true")
    parser.add_argument("--selected-ioc", type=Path)
    parser.add_argument("--source", action="append", default=[])
    parser.add_argument("--source-list-file", type=Path)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    repo_root = args.repo_root.resolve()
    try:
        if args.list_f407_presets:
            print("\n".join(f407_presets(repo_root)))
            return 0
        if args.list_submodules:
            errors: list[str] = []
            inventory = submodule_inventory(repo_root, args.check_submodules_clean, errors)
            if errors:
                raise GateFailure("\n".join(errors))
            print("\n".join(item["path"] for item in inventory))
            return 0
        presets, submodules = check_gate(repo_root, args)
    except (GateFailure, OSError, subprocess.CalledProcessError, UnicodeError) as exc:
        print(f"CubeMX production gate failed:\n{exc}", file=sys.stderr)
        return 1
    print("CubeMX production gate passed.")
    print(f"F407 presets ({len(presets)}): {', '.join(presets)}")
    print(f"Submodules ({len(submodules)}): {', '.join(item['path'] for item in submodules)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
