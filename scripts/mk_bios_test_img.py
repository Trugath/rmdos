#!/usr/bin/env python3
"""Pack a 512-byte BIOS test boot sector into a raw floppy image."""

from __future__ import annotations

import argparse
from pathlib import Path

SECTOR = 512
# Default: 720 KB DD floppy (80 cyl × 2 heads × 9 spt)
DEFAULT_SECTORS = 80 * 2 * 9


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--output", "-o", type=Path, required=True)
    ap.add_argument("--boot", type=Path, required=True, help="512-byte boot sector")
    ap.add_argument(
        "--sectors",
        type=int,
        default=DEFAULT_SECTORS,
        help=f"image size in sectors (default {DEFAULT_SECTORS}=720K; "
        "2880=1.44M, 2400=1.2M, 720=360K)",
    )
    args = ap.parse_args()

    if args.sectors < 1:
        raise SystemExit("--sectors must be >= 1")

    boot = args.boot.read_bytes()
    if len(boot) != SECTOR:
        raise SystemExit(f"boot must be {SECTOR} bytes, got {len(boot)}")
    if boot[-2:] != b"\x55\xaa":
        raise SystemExit("boot missing 0x55AA signature")

    image = bytearray(args.sectors * SECTOR)
    image[0:SECTOR] = boot

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(image)
    print(f"wrote {args.output} ({args.sectors} sectors)")


if __name__ == "__main__":
    main()
