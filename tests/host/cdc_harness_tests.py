#!/usr/bin/env python3
"""Native tests for the cross-platform CDC harness selection and echo flow."""

from __future__ import annotations

import importlib.util
import sys
import unittest
from dataclasses import dataclass
from pathlib import Path


HARNESS_PATH = (
    Path(__file__).resolve().parents[2] / "tools" / "usb" / "cdc_harness.py"
)
SPEC = importlib.util.spec_from_file_location("pnx_cdc_harness", HARNESS_PATH)
assert SPEC is not None and SPEC.loader is not None
HARNESS = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = HARNESS
SPEC.loader.exec_module(HARNESS)


@dataclass
class FakePort:
    device: str
    vid: int
    pid: int
    serial_number: str


class FakeConnection:
    def __init__(self) -> None:
        self.pending = bytearray()
        self.closed = False

    def readline(self) -> bytes:
        return b"PNX_F407_USB_CDC READY\n"

    def write(self, payload: bytes) -> int:
        self.pending.extend(payload)
        return len(payload)

    def flush(self) -> None:
        pass

    def read(self, length: int) -> bytes:
        result = bytes(self.pending[:length])
        del self.pending[:length]
        return result

    def close(self) -> None:
        self.closed = True


class FakeSerialModule:
    def __init__(self) -> None:
        self.connections: list[FakeConnection] = []

    def Serial(self, *args, **kwargs) -> FakeConnection:  # noqa: N802
        connection = FakeConnection()
        self.connections.append(connection)
        return connection


class CdcHarnessTests(unittest.TestCase):
    def test_explicit_port_selects_exactly_one_device(self) -> None:
        ports = [
            FakePort("COM4", 0x1209, 0x0001, "A"),
            FakePort("COM7", 0x1209, 0x0001, "B"),
        ]
        selected = HARNESS.select_port(
            ports, "COM7", None, None, None)
        self.assertEqual(selected.serial_number, "B")

    def test_vid_pid_requires_serial_when_multiple_match(self) -> None:
        ports = [
            FakePort("COM4", 0x1209, 0x0001, "A"),
            FakePort("COM7", 0x1209, 0x0001, "B"),
        ]
        with self.assertRaisesRegex(ValueError, "exactly one"):
            HARNESS.select_port(
                ports, None, 0x1209, 0x0001, None)
        selected = HARNESS.select_port(
            ports, None, 0x1209, 0x0001, "B")
        self.assertEqual(selected.device, "COM7")

    def test_echo_and_reopen_report_success(self) -> None:
        serial_module = FakeSerialModule()
        report = HARNESS.exercise(
            serial_module, "COM7", count=3, timeout=0.1,
            reopen=True)
        self.assertEqual(report.passed, 3)
        self.assertTrue(report.reopen_passed)
        self.assertIn("READY", report.banner)
        self.assertEqual(len(serial_module.connections), 2)


if __name__ == "__main__":
    unittest.main()
