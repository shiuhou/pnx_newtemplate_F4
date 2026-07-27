#!/usr/bin/env python3
"""Static regression checks for the F407 CAN RX IRQ bridge."""

from __future__ import annotations

import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
BACKEND = ROOT / "boards" / "dji_c_board_f407" / "pnx_backends" / "can_backend.cpp"


def function_body(text: str, signature: str) -> str:
    start = text.find(signature)
    if start < 0:
        raise AssertionError(f"missing function: {signature}")
    opening = text.find("{", start)
    if opening < 0:
        raise AssertionError(f"missing body: {signature}")
    depth = 0
    for index in range(opening, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[opening + 1 : index]
    raise AssertionError(f"unterminated body: {signature}")


def test_rx_irq_drains_fifo0() -> None:
    text = BACKEND.read_text(encoding="utf-8")
    body = function_body(
        text,
        "extern \"C\" void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef* handle)",
    )
    required = (
        "HAL_CAN_GetRxFifoFillLevel(handle, CAN_RX_FIFO0)",
        "while",
        "HAL_CAN_GetRxMessage(handle, CAN_RX_FIFO0, &header, frame.data)",
    )
    for token in required:
        if token not in body:
            raise AssertionError(f"CAN1 RX callback must drain FIFO0: missing {token}")


def main() -> int:
    try:
        test_rx_irq_drains_fifo0()
    except Exception as exc:  # noqa: BLE001 - concise standalone failure
        print(f"F407 CAN backend source acceptance: FAIL: {exc}", file=sys.stderr)
        return 1
    print("F407 CAN backend source acceptance: PASS (FIFO0 drain)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
