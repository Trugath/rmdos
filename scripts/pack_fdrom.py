#!/usr/bin/env python3
"""Pad Fixed Disk option ROM to 2 KiB and force checksum byte to 0."""
from __future__ import annotations

import argparse
from pathlib import Path


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", required=True, type=Path)
    ap.add_argument("--output", required=True, type=Path)
    ap.add_argument("--size", type=int, default=2048)
    args = ap.parse_args()
    data = bytearray(args.input.read_bytes())
    if len(data) < 3 or data[0] != 0x55 or data[1] != 0xAA:
        raise SystemExit("missing 55 AA header")
    blocks = args.size // 512
    data[2] = blocks
    if len(data) > args.size:
        raise SystemExit(f"ROM too large: {len(data)} > {args.size}")
    data.extend(b"\x00" * (args.size - len(data)))
    # Last byte adjusts so sum of all bytes is 0
    data[-1] = 0
    s = sum(data) & 0xFF
    data[-1] = (-s) & 0xFF
    args.output.write_bytes(data)
    print(f"OK {args.output} ({len(data)} bytes, checksum=0)")


if __name__ == "__main__":
    main()
