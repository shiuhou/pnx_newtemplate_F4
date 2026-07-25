#!/usr/bin/env python3
"""Cross-platform PnX F407 USB CDC enumeration and echo harness."""

from __future__ import annotations

import argparse
import json
import statistics
import sys
import time
import uuid
from dataclasses import asdict, dataclass
from typing import Any, Iterable


def integer(text: str) -> int:
    return int(text, 0)


def port_value(port: Any, name: str) -> Any:
    return getattr(port, name, None)


def select_port(
    ports: Iterable[Any],
    explicit: str | None,
    vid: int | None,
    pid: int | None,
    serial: str | None,
) -> Any:
    candidates = list(ports)
    if explicit is not None:
        matches = [
            item for item in candidates
            if port_value(item, "device") == explicit
        ]
    else:
        if vid is None or pid is None:
            raise ValueError(
                "select by --port or provide both --vid and --pid")
        matches = [
            item for item in candidates
            if port_value(item, "vid") == vid
            and port_value(item, "pid") == pid
            and (
                serial is None
                or port_value(item, "serial_number") == serial
            )
        ]
    if len(matches) != 1:
        devices = [port_value(item, "device") for item in matches]
        raise ValueError(
            f"expected exactly one matching CDC port, found "
            f"{len(matches)}: {devices}")
    return matches[0]


@dataclass
class Report:
    port: str
    banner: str
    requested: int
    passed: int
    reopen_passed: bool
    min_latency_ms: float
    mean_latency_ms: float
    max_latency_ms: float


def read_until_exact(
    connection: Any, payload: bytes, timeout: float
) -> bytes:
    deadline = time.monotonic() + timeout
    received = bytearray()
    while time.monotonic() < deadline and len(received) < len(payload):
        chunk = connection.read(len(payload) - len(received))
        if chunk:
            received.extend(chunk)
    return bytes(received)


def exercise(
    serial_module: Any,
    device: str,
    count: int,
    timeout: float,
    reopen: bool,
) -> Report:
    latencies: list[float] = []
    passed = 0
    banner = ""

    def open_port() -> Any:
        return serial_module.Serial(
            device, baudrate=115200, timeout=0.05,
            write_timeout=timeout)

    connection = open_port()
    try:
        banner = connection.readline().decode(
            "utf-8", errors="replace").strip()
        for sequence in range(count):
            payload = (
                f"PNX-ECHO:{sequence}:{uuid.uuid4().hex}\n"
            ).encode("ascii")
            start = time.perf_counter()
            connection.write(payload)
            connection.flush()
            reply = read_until_exact(connection, payload, timeout)
            latency = (time.perf_counter() - start) * 1000.0
            if reply != payload:
                raise RuntimeError(
                    f"echo mismatch at sequence {sequence}: "
                    f"expected={payload!r} actual={reply!r}")
            latencies.append(latency)
            passed += 1
    finally:
        connection.close()

    reopen_passed = not reopen
    if reopen:
        time.sleep(0.25)
        connection = open_port()
        try:
            probe = b"PNX-ECHO:REOPEN\n"
            connection.write(probe)
            connection.flush()
            reopen_passed = (
                read_until_exact(connection, probe, timeout) == probe
            )
        finally:
            connection.close()

    return Report(
        port=device,
        banner=banner,
        requested=count,
        passed=passed,
        reopen_passed=reopen_passed,
        min_latency_ms=min(latencies) if latencies else 0.0,
        mean_latency_ms=statistics.fmean(latencies)
        if latencies else 0.0,
        max_latency_ms=max(latencies) if latencies else 0.0,
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port")
    parser.add_argument("--vid", type=integer)
    parser.add_argument("--pid", type=integer)
    parser.add_argument("--serial")
    parser.add_argument("--count", type=int, default=20)
    parser.add_argument("--timeout", type=float, default=1.0)
    parser.add_argument("--reopen", action="store_true")
    parser.add_argument("--enumerate-only", action="store_true")
    args = parser.parse_args()

    try:
        import serial
        from serial.tools import list_ports
    except ImportError:
        print(
            "pyserial is required: python -m pip install pyserial",
            file=sys.stderr,
        )
        return 2

    ports = list(list_ports.comports())
    inventory = [
        {
            "device": port.device,
            "vid": port.vid,
            "pid": port.pid,
            "serial": port.serial_number,
            "description": port.description,
        }
        for port in ports
    ]
    print(json.dumps({"ports": inventory}, indent=2))
    if args.enumerate_only:
        return 0

    try:
        selected = select_port(
            ports, args.port, args.vid, args.pid, args.serial)
        report = exercise(
            serial, selected.device, args.count,
            args.timeout, args.reopen)
    except (ValueError, OSError, RuntimeError, serial.SerialException) as exc:
        print(f"CDC harness: FAIL: {exc}", file=sys.stderr)
        return 1
    print(json.dumps(asdict(report), indent=2))
    return 0 if (
        report.passed == report.requested
        and report.reopen_passed
    ) else 1


if __name__ == "__main__":
    raise SystemExit(main())
