#!/usr/bin/env python3
"""Pack small-model MZ EXE: code + data + optional const; BSS via minalloc."""

from __future__ import annotations

import argparse
import struct
from pathlib import Path


def _para_ceil(n: int) -> int:
    return (n + 15) // 16


def pack_exe(
    code: bytes,
    data: bytes,
    *,
    const: bytes = b"",
    bss_bytes: int = 0,
    bss_end: int | None = None,
    stack: int = 0x1000,
    minalloc_extra: int = 0,
    maxalloc: int = 0x100,
    ip: int = 0,
) -> bytes:
    """Build MZ with CS=0, SS=code_paras, DS set to SS by program startup.

    Image = para-padded code + data [+ para pad + const]. BSS+stack via
    minalloc (not stored). When const is present, data is para-padded so
    const begins on a paragraph (CONST seg = CS + code_paras + data_paras).
    """
    if ip < 0 or ip > 0xFFFF:
        raise ValueError("ip out of range")
    code_paras = _para_ceil(len(code))
    code_pad = code + (b"\0" * (code_paras * 16 - len(code)))

    if const:
        data_paras = _para_ceil(len(data)) if len(data) else 0
        # Even empty data: const still follows code; pad length 0.
        data_pad = data + (b"\0" * (data_paras * 16 - len(data)))
        image = code_pad + data_pad + const
        # DS high-water before stack: end of const hole + BSS (linker __bss_end).
        if bss_end is not None:
            data_span = bss_end
        else:
            data_span = len(data_pad) + len(const) + bss_bytes
    else:
        image = code_pad + data
        if bss_end is not None:
            data_span = bss_end
        else:
            data_span = len(data) + bss_bytes

    # SP relative to SS (= start of data): room through BSS + stack.
    sp = data_span + stack
    if sp < 2:
        sp = 2
    if sp > 0x10000:
        raise ValueError(f"data+bss+stack exceeds 64K ({sp})")
    sp = (sp - 2) & 0xFFFF

    # Extra paras beyond the file image for BSS + stack (+ optional pad).
    need_extra = _para_ceil(bss_bytes + stack) + minalloc_extra
    if need_extra < 1:
        need_extra = 1
    minalloc = need_extra & 0xFFFF
    # DOS maxalloc is also "above image". It must be >= minalloc or the
    # loader under-allocates and BSS/stack corrupt the MCB chain (and hang
    # on terminate coalesce). Cap at minalloc so children still get free RAM
    # (unlike maxalloc=0xFFFF claiming the arena).
    if maxalloc < minalloc:
        maxalloc = minalloc
    maxalloc = maxalloc & 0xFFFF

    header_paras = 2
    header = bytearray(header_paras * 16)
    total = len(header) + len(image)
    pages = (total + 511) // 512
    last = total % 512
    struct.pack_into("<H", header, 0, 0x5A4D)  # MZ
    struct.pack_into("<H", header, 2, last)
    struct.pack_into("<H", header, 4, pages)
    struct.pack_into("<H", header, 6, 0)  # relocs
    struct.pack_into("<H", header, 8, header_paras)
    struct.pack_into("<H", header, 10, minalloc)
    struct.pack_into("<H", header, 12, maxalloc)
    struct.pack_into("<H", header, 14, code_paras & 0xFFFF)  # SS
    struct.pack_into("<H", header, 16, sp)  # SP
    struct.pack_into("<H", header, 18, 0)  # checksum
    struct.pack_into("<H", header, 20, ip & 0xFFFF)  # IP
    struct.pack_into("<H", header, 22, 0)  # CS
    struct.pack_into("<H", header, 24, 0x1C)  # reloc table off
    struct.pack_into("<H", header, 26, 0)  # overlay
    return bytes(header) + image


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--code", type=Path, required=True, help="Raw .text image")
    ap.add_argument("--data", type=Path, required=True, help="Raw .data/.rodata image")
    ap.add_argument(
        "--const",
        type=Path,
        default=None,
        help="Optional raw .const image (blit tables)",
    )
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument(
        "--bss-bytes",
        type=lambda s: int(s, 0),
        default=0,
        help="Uninitialized data size (minalloc)",
    )
    ap.add_argument(
        "--bss-end",
        type=lambda s: int(s, 0),
        default=None,
        help="DS offset of __bss_end (SP base before stack); preferred with --const",
    )
    ap.add_argument(
        "--stack",
        type=lambda s: int(s, 0),
        default=0x1000,
        help="Stack bytes above BSS (default 4KiB)",
    )
    ap.add_argument(
        "--maxalloc",
        type=lambda s: int(s, 0),
        default=0x100,
        help="MZ maxalloc paragraphs above image (raised to minalloc if smaller)",
    )
    ap.add_argument(
        "--ip",
        type=lambda s: int(s, 0),
        default=0,
        help="Entry IP within code segment (default 0 = _start)",
    )
    args = ap.parse_args()
    const = args.const.read_bytes() if args.const else b""
    mz = pack_exe(
        args.code.read_bytes(),
        args.data.read_bytes(),
        const=const,
        bss_bytes=args.bss_bytes,
        bss_end=args.bss_end,
        stack=args.stack,
        maxalloc=args.maxalloc,
        ip=args.ip,
    )
    args.out.write_bytes(mz)
    csz = len(const)
    print(
        f"wrote {args.out} ({len(mz)} bytes; "
        f"code={args.code.stat().st_size} data={args.data.stat().st_size} "
        f"const={csz} bss={args.bss_bytes})"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
