#!/usr/bin/env python3
"""Wrap a raw .COM image in a minimal MZ .EXE."""

from __future__ import annotations

import argparse
import struct
from pathlib import Path


def pack_mz(com: bytes, *, stack: int = 0xFFFE, min_size: int = 0) -> bytes:
    header_paras = 2
    header = bytearray(header_paras * 16)
    image = bytearray(com)
    total = len(header) + len(image)
    if min_size > total:
        image.extend(b"\0" * (min_size - total))
        total = len(header) + len(image)
    pages = (total + 511) // 512
    last = total % 512
    struct.pack_into("<H", header, 0, 0x5A4D)  # MZ
    struct.pack_into("<H", header, 2, last)
    struct.pack_into("<H", header, 4, pages)
    struct.pack_into("<H", header, 6, 0)  # relocs
    struct.pack_into("<H", header, 8, header_paras)
    struct.pack_into("<H", header, 10, 0x10)  # minalloc
    struct.pack_into("<H", header, 12, 0xFFFF)  # maxalloc
    struct.pack_into("<H", header, 14, 0xFFF0)  # SS = load_seg-0x10 = PSP
    struct.pack_into("<H", header, 16, stack)  # SP
    struct.pack_into("<H", header, 18, 0)  # checksum
    struct.pack_into("<H", header, 20, 0x0100)  # IP (COM-style)
    struct.pack_into("<H", header, 22, 0xFFF0)  # CS = PSP
    struct.pack_into("<H", header, 24, 0x1C)  # reloc table off
    struct.pack_into("<H", header, 26, 0)  # overlay
    return bytes(header) + bytes(image)


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--com", type=Path, required=True)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument(
        "--min-size",
        type=int,
        default=0,
        help="Pad EXE to at least this many bytes (streaming EXEC stress)",
    )
    args = ap.parse_args()
    data = pack_mz(args.com.read_bytes(), min_size=args.min_size)
    args.out.write_bytes(data)
    print(f"wrote {args.out} ({len(data)} bytes)")


if __name__ == "__main__":
    main()
