#!/usr/bin/env python3
"""Focused negative tests for the CubeMX production gate."""

from __future__ import annotations

import argparse
import copy
import hashlib
import importlib.util
import json
import tempfile
import unittest
from pathlib import Path
from unittest import mock


REPO_ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "check_cubemx_production",
    REPO_ROOT / "scripts/check_cubemx_production.py",
)
assert SPEC is not None and SPEC.loader is not None
gate = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(gate)


class CubeMxGateUnitTests(unittest.TestCase):
    def test_rejects_unsafe_manifest_paths(self) -> None:
        for value in (
            "../escape",
            "/absolute",
            "C:" + "/drive",
            "a\\b",
            "a\tb",
            "a\nb",
        ):
            with self.subTest(value=value):
                self.assertFalse(gate.safe_relative(value))
        self.assertTrue(gate.safe_relative("Core/Src/main.c"))

    def test_tree_manifest_must_be_a_sorted_exact_list(self) -> None:
        payload = b"text\t1\t" + hashlib.sha256(b"x").hexdigest().upper().encode() + b"\tCore/x.c\n"
        entry = {
            "kind": "text",
            "path": "Core/x.c",
            "sha256": hashlib.sha256(b"x").hexdigest().upper(),
            "size": 1,
        }
        manifest = {
            "file_count": 1,
            "files": [entry],
            "format": gate.TREE_FORMAT,
            "tree_sha256": hashlib.sha256(payload).hexdigest().upper(),
        }
        errors: list[str] = []
        records = gate.tree_records(manifest, "test", errors)
        self.assertEqual(errors, [])
        self.assertEqual(set(records), {"Core/x.c"})

        malformed = dict(manifest)
        malformed["files"] = {"Core/x.c": entry}
        errors = []
        self.assertEqual(gate.tree_records(malformed, "test", errors), {})
        self.assertTrue(any("must be a list" in error for error in errors))

    def test_manifest_file_hash_is_line_ending_stable(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            relative = "manifest.json"
            manifest = {
                "file_count": 0,
                "files": [],
                "format": gate.TREE_FORMAT,
                "tree_sha256": hashlib.sha256(b"").hexdigest().upper(),
            }
            canonical = (json.dumps(manifest, indent=2) + "\n").encode("utf-8")
            (root / relative).write_bytes(canonical.replace(b"\n", b"\r\n"))
            entry = {
                "file_count": 0,
                "path": relative,
                "sha256": hashlib.sha256(canonical).hexdigest().upper(),
                "tree_sha256": manifest["tree_sha256"],
            }
            errors: list[str] = []

            loaded, records = gate.load_tree(root, entry, "test", errors)

            self.assertEqual(errors, [])
            self.assertEqual(loaded, manifest)
            self.assertEqual(records, {})

    def test_unlisted_formal_file_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            board = Path(temporary)
            for name in gate.GENERATED_ROOTS:
                (board / name).mkdir()
            tracked = board / "Core/main.c"
            tracked.write_text("x", encoding="utf-8")
            metadata = gate.canonical_metadata(tracked)
            records = {"Core/main.c": {"path": "Core/main.c", **metadata}}
            errors: list[str] = []
            gate.check_formal_tree(board, {}, records, errors)
            self.assertEqual(errors, [])

            (board / "Core/unlisted.c").write_text("y", encoding="utf-8")
            errors = []
            gate.check_formal_tree(board, {}, records, errors)
            self.assertTrue(any("unlisted extra" in error for error in errors))

    def test_first_parent_wins_in_multiple_inheritance(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            data = {
                "version": 6,
                "configurePresets": [
                    {"name": "f407", "hidden": True, "cacheVariables": {"PNX_BOARD": {"type": "STRING", "value": "dji_c_board_f407"}}},
                    {"name": "h723", "hidden": True, "cacheVariables": {"PNX_BOARD": "stm32h723"}},
                    {"name": "dji-c-board-future", "inherits": ["f407", "h723"]},
                ],
                "buildPresets": [
                    {"name": "dji-c-board-future", "configurePreset": "dji-c-board-future"}
                ],
            }
            (root / "CMakePresets.json").write_text(json.dumps(data), encoding="utf-8")
            self.assertEqual(gate.f407_presets(root), ["dji-c-board-future"])

    def test_preset_include_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "CMakePresets.json").write_text(
                json.dumps({"version": 6, "include": ["other.json"]}),
                encoding="utf-8",
            )
            with self.assertRaises(gate.GateFailure):
                gate.f407_presets(root)

    def test_f407_only_presets_come_from_candidate_provenance(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            presets = {
                "version": 6,
                "configurePresets": [
                    {
                        "name": "firmware-default",
                        "hidden": True,
                        "cacheVariables": {},
                    },
                    {
                        "name": "board-smoke",
                        "inherits": "firmware-default",
                        "cacheVariables": {"PNX_DEMO": "board_smoke"},
                    },
                    {
                        "name": "usb-cdc",
                        "inherits": "firmware-default",
                        "cacheVariables": {"PNX_DEMO": "usb_cdc"},
                    },
                    {
                        "name": "host-tests",
                        "cacheVariables": {"PNX_HOST_TESTS": True},
                    },
                ],
                "buildPresets": [
                    {"name": "board-smoke", "configurePreset": "board-smoke"},
                    {"name": "usb-cdc", "configurePreset": "usb-cdc"},
                    {"name": "host-tests", "configurePreset": "host-tests"},
                ],
            }
            (root / "CMakePresets.json").write_text(
                json.dumps(presets), encoding="utf-8"
            )
            provenance = root / "release" / "f407-only-provenance.json"
            provenance.parent.mkdir()
            provenance.write_text(
                json.dumps(
                    {
                        "manifest_type": "pnx-f407-only-export-provenance",
                        "build": {
                            "firmware_presets": ["board-smoke", "usb-cdc"]
                        },
                    }
                ),
                encoding="utf-8",
            )

            self.assertEqual(
                gate.f407_presets(root), ["board-smoke", "usb-cdc"]
            )

            presets["configurePresets"][1]["cacheVariables"]["PNX_BOARD"] = (
                "dji_c_board_f407"
            )
            (root / "CMakePresets.json").write_text(
                json.dumps(presets), encoding="utf-8"
            )
            with self.assertRaisesRegex(gate.GateFailure, "PNX_BOARD"):
                gate.f407_presets(root)

    def test_ioc_assignments_are_unique_and_exact(self) -> None:
        with self.assertRaises(gate.GateFailure):
            gate.parse_ioc_assignments("RCC.PLLM=6\nRCC.PLLM=7\n")

        assignments = gate.parse_ioc_assignments(
            "RCC.PLLM=7 # RCC.PLLM=6\nMcu.CPN=STM32F407IGH6TR\n"
        )
        errors: list[str] = []
        gate.check_required_ioc_assignments(
            assignments,
            ("RCC.PLLM=6", "Mcu.CPN=STM32F407IGH6TR"),
            errors,
        )
        self.assertTrue(any("RCC.PLLM" in error for error in errors))
        self.assertFalse(any("Mcu.CPN" in error for error in errors))

    def test_forbidden_inventory_keeps_unrelated_peripherals_out(self) -> None:
        expected = {
            "Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_crc.h",
            "Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_i2c.h",
            "Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_i2c_ex.h",
            "Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_spi.h",
            "Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_crc.h",
            "Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_i2c.h",
            "Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_spi.h",
            "Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_tim.h",
        }
        self.assertTrue(expected.issubset(gate.FORBIDDEN_FORMAL_PATHS))
        self.assertNotIn(
            "Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_pcd.h",
            gate.FORBIDDEN_FORMAL_PATHS,
        )
        self.assertNotIn(
            "Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_usb.h",
            gate.FORBIDDEN_FORMAL_PATHS,
        )

    def test_usb_source_graph_is_an_explicit_optional_closure(self) -> None:
        formal = {
            "USBX/App/app_usbx_device.c": {},
            "Core/Src/usb_otg.c": {},
            "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pcd.c": {},
            "Middlewares/ST/usbx/common/core/src/ux_system_initialize.c": {},
            "Middlewares/ST/usbx/common/core/src/"
            "ux_device_stack_host_wakeup.c": {},
            "Middlewares/ST/usbx/common/core/src/ux_host_stack_initialize.c": {},
            "Middlewares/ST/usbx/common/core/src/ux_hcd_sim_host_initialize.c": {},
            "Middlewares/ST/usbx/common/core/src/ux_dcd_sim_slave_initialize.c": {},
            "Middlewares/ST/usbx/common/core/src/ux_system_otg_initialize.c": {},
            "Middlewares/ST/usbx/common/core/src/ux_utility_pci_read.c": {},
            "Middlewares/ST/usbx/common/core/src/ux_utility_pci_write.c": {},
            "Middlewares/ST/usbx/common/core/src/ux_utility_pci_class_scan.c": {},
            "Middlewares/ST/usbx/common/usbx_device_classes/src/"
            "ux_device_class_cdc_acm_entry.c": {},
            "Middlewares/ST/usbx/common/usbx_device_classes/src/"
            "ux_device_class_hid_entry.c": {},
            "Middlewares/ST/usbx/common/usbx_stm32_device_controllers/"
            "ux_dcd_stm32_initialize.c": {},
        }
        base = gate.expected_source_paths(formal, include_usb=False)
        usb = gate.expected_source_paths(formal, include_usb=True)
        self.assertNotIn("Core/Src/usb_otg.c", base)
        self.assertIn("Core/Src/usb_otg.c", usb)
        self.assertIn(
            "Middlewares/ST/usbx/common/core/src/ux_system_initialize.c",
            usb,
        )
        self.assertIn(
            "Middlewares/ST/usbx/common/core/src/"
            "ux_device_stack_host_wakeup.c",
            usb,
        )
        for helper in ("class_scan", "read", "write"):
            self.assertNotIn(
                "Middlewares/ST/usbx/common/core/src/"
                f"ux_utility_pci_{helper}.c",
                usb,
            )
        for forbidden in (
            "Middlewares/ST/usbx/common/core/src/"
            "ux_host_stack_initialize.c",
            "Middlewares/ST/usbx/common/core/src/"
            "ux_hcd_sim_host_initialize.c",
            "Middlewares/ST/usbx/common/core/src/"
            "ux_dcd_sim_slave_initialize.c",
            "Middlewares/ST/usbx/common/core/src/"
            "ux_system_otg_initialize.c",
            "Middlewares/ST/usbx/common/usbx_device_classes/src/"
            "ux_device_class_hid_entry.c",
        ):
            self.assertNotIn(forbidden, usb)

    def test_cmake_gate_uses_actual_targets_and_clean_submodules(self) -> None:
        top = (REPO_ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        board = (
            REPO_ROOT
            / "boards/dji_c_board_f407/cmake/stm32cubemx/CMakeLists.txt"
        ).read_text(encoding="utf-8")
        self.assertIn("--check-submodules-clean", top)
        self.assertIn("get_target_property", top)
        self.assertIn("STM32_Drivers", top)
        self.assertIn("ThreadX", top)
        self.assertNotIn("PNX_F407_GENERATED_SOURCE_LIST", top)
        self.assertNotIn("PNX_F407_GENERATED_SOURCE_LIST", board)
        self.assertGreater(
            top.find("get_target_property"),
            top.rfind("target_sources("),
            "actual-target source inspection must follow the last source attachment",
        )

    def test_source_graph_rejects_foreign_board_source(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = Path(temporary)
            board = repo / gate.BOARD_REL
            expected = gate.expected_source_paths({})
            sources = [str(board / path) for path in expected]
            sources.append(str(repo / "board/Core/Src/main.c"))
            errors: list[str] = []

            gate.check_source_graph(repo, board, sources, {}, errors)

            self.assertTrue(
                any("foreign board source" in error for error in errors),
                errors,
            )

    def test_provenance_entry_keys_must_be_unique(self) -> None:
        provenance_path = REPO_ROOT / gate.PROVENANCE_REL
        original_load_json = gate.load_json
        original_provenance = original_load_json(provenance_path)
        cases = (
            ("formal_integration", "hand_owned_files", "path"),
            ("formal_integration", "reviewed_patches", "path"),
            (None, "requirements", "id"),
        )
        args = argparse.Namespace(
            check_submodules_clean=False,
            selected_ioc=None,
            source=[],
            source_list_file=None,
        )

        for parent, collection, key in cases:
            with self.subTest(collection=collection):
                provenance = copy.deepcopy(original_provenance)
                target = provenance[parent][collection] if parent else provenance[collection]
                target.append(copy.deepcopy(target[0]))

                def load_json(path: Path) -> object:
                    if path.resolve() == provenance_path.resolve():
                        return provenance
                    return original_load_json(path)

                with mock.patch.object(gate, "load_json", side_effect=load_json):
                    with self.assertRaisesRegex(
                        gate.GateFailure,
                        rf"duplicate {collection} {key}",
                    ):
                        gate.check_gate(REPO_ROOT, args)


if __name__ == "__main__":
    unittest.main()
