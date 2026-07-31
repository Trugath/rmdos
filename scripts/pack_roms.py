#!/usr/bin/env python3
"""Emit U19 (8 KiB of 0xFF) and verify U18 is exactly 32 KiB."""

from __future__ import annotations

import argparse
from pathlib import Path

U18_SIZE = 32768
U19_SIZE = 8192


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
        args.u18.write_bytes(data)
        reset = data[0x7FF0:0x7FF5]
        expect = bytes([0xEA, 0x5B, 0xE0, 0x00, 0xF0])
        if reset != expect:
            raise SystemExit(f"U18 reset vector {reset.hex()} != {expect.hex()}")
        print(f"OK u18 {args.u18} ({U18_SIZE} bytes, reset->F000:E05B)")

    if args.u19_out is not None:
        args.u19_out.parent.mkdir(parents=True, exist_ok=True)
        args.u19_out.write_bytes(b"\xFF" * U19_SIZE)
        print(f"wrote {args.u19_out} ({U19_SIZE} bytes, 0xFF pad)")


if __name__ == "__main__":
    main()
