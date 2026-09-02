#!/usr/bin/env python3
"""Wrap a raw .COM image in a minimal MZ .EXE."""

from __future__ import annotations

import argparse
import struct
from pathlib import Path


def pack_mz(
    com: bytes,
    *,
    stack: int = 0xFFFE,
    min_size: int = 0,
    minalloc: int = 0x10,
    maxalloc: int = 0xFFFF,
    relocs: list[tuple[int, int]] | None = None,
) -> bytes:
    """Build MZ. Each reloc is (offset, segment) relative to the load image."""
    reloc_list = list(relocs or [])
    reloc_bytes = len(reloc_list) * 4
    # Header must cover fixed fields through 0x1C plus reloc table.
    header_bytes = max(32, 0x1C + reloc_bytes)
    header_paras = (header_bytes + 15) // 16
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
    struct.pack_into("<H", header, 6, len(reloc_list))
    struct.pack_into("<H", header, 8, header_paras)
    struct.pack_into("<H", header, 10, minalloc & 0xFFFF)
    struct.pack_into("<H", header, 12, maxalloc & 0xFFFF)
    struct.pack_into("<H", header, 14, 0xFFF0)  # SS = load_seg-0x10 = PSP
    struct.pack_into("<H", header, 16, stack)  # SP
    struct.pack_into("<H", header, 18, 0)  # checksum
    struct.pack_into("<H", header, 20, 0x0100)  # IP (COM-style)
    struct.pack_into("<H", header, 22, 0xFFF0)  # CS = PSP
    struct.pack_into("<H", header, 24, 0x1C)  # reloc table off
    struct.pack_into("<H", header, 26, 0)  # overlay number
    for i, (off, seg) in enumerate(reloc_list):
        struct.pack_into("<HH", header, 0x1C + i * 4, off & 0xFFFF, seg & 0xFFFF)
    return bytes(header) + bytes(image)


def _parse_reloc(s: str) -> tuple[int, int]:
    """Parse OFF or OFF:SEG (hex or decimal)."""
    part = s.replace(",", ":")
    if ":" in part:
        off_s, seg_s = part.split(":", 1)
        return int(off_s, 0), int(seg_s, 0)
    return int(part, 0), 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--com", type=Path, required=True)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument(
        "--min-size",
        type=int,
        default=0,
        help="Pad EXE to at least this many bytes (streaming EXEC stress)",
    )
    ap.add_argument(
        "--minalloc",
        type=lambda s: int(s, 0),
        default=0x10,
        help="MZ minalloc paragraphs (default 0x10)",
    )
    ap.add_argument(
        "--maxalloc",
        type=lambda s: int(s, 0),
        default=0xFFFF,
        help="MZ maxalloc paragraphs (default 0xFFFF = take all free RAM)",
    )
    ap.add_argument(
        "--reloc",
        action="append",
        default=[],
        metavar="OFF[:SEG]",
        help="Add MZ relocation entry (repeatable); SEG defaults to 0",
    )
    args = ap.parse_args()
    try:
        com_data = args.com.read_bytes()
    except FileNotFoundError:
        raise SystemExit(f"source file not found: {args.com}")
    data = pack_mz(
        com_data,
        min_size=args.min_size,
        minalloc=args.minalloc,
        maxalloc=args.maxalloc,
        relocs=[_parse_reloc(r) for r in args.reloc],
    )
    args.out.write_bytes(data)
    print(f"wrote {args.out} ({len(data)} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
