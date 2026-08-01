#!/usr/bin/env python3
"""Emit U19 (8 KiB of 0xFF) and verify/finalize U18 (32 KiB)."""

from __future__ import annotations

import argparse
from pathlib import Path

U18_SIZE = 32768
U19_SIZE = 8192
# Top 8 KiB of U18 maps to linear FE000–FFFFF (F000:E000–FFFF).
TOP8K_OFF = 0x6000
TOP8K_LEN = 8192


def force_top8k_checksum(data: bytearray) -> None:
    """Set FFFF so bytes FE000–FFFFF sum to 0 mod 256."""
    region = data[TOP8K_OFF : TOP8K_OFF + TOP8K_LEN]
    region[-1] = 0
    s = sum(region) & 0xFF
    region[-1] = (-s) & 0xFF
    data[TOP8K_OFF : TOP8K_OFF + TOP8K_LEN] = region
    if sum(data[TOP8K_OFF : TOP8K_OFF + TOP8K_LEN]) & 0xFF:
        raise SystemExit("U18 top-8K checksum failed after patch")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--u18", type=Path, help="existing U18 binary to verify/pad")
    ap.add_argument("--u19-out", type=Path, help="write blank U19 image here")
    args = ap.parse_args()

    if args.u18 is not None:
        data = bytearray(args.u18.read_bytes())
        if len(data) < U18_SIZE:
            data.extend(b"\x00" * (U18_SIZE - len(data)))
        elif len(data) > U18_SIZE:
            raise SystemExit(f"U18 larger than {U18_SIZE}: {len(data)}")

        reset = data[0x7FF0:0x7FF5]
        expect = bytes([0xEA, 0x5B, 0xE0, 0x00, 0xF0])
        if reset != expect:
            raise SystemExit(f"U18 reset vector {reset.hex()} != {expect.hex()}")

        if data[0x7FFE] != 0xFE:
            raise SystemExit(f"U18 machine type {data[0x7FFE]:02x} != fe")

        force_top8k_checksum(data)
        args.u18.write_bytes(data)
        print(
            f"OK u18 {args.u18} ({U18_SIZE} bytes, reset->F000:E05B, "
            f"type=FE, top8k checksum=0)"
        )

    if args.u19_out is not None:
        args.u19_out.parent.mkdir(parents=True, exist_ok=True)
        args.u19_out.write_bytes(b"\xFF" * U19_SIZE)
        print(f"wrote {args.u19_out} ({U19_SIZE} bytes, 0xFF pad)")


if __name__ == "__main__":
    main()
