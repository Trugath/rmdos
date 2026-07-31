#!/usr/bin/env python3
"""Build a raw 720 KB floppy image: boot sector + contiguous kernel sectors."""

from __future__ import annotations

import argparse
from pathlib import Path

SECTOR = 512
# 720 KB DD floppy: 80 cyl × 2 heads × 9 spt
IMAGE_SECTORS = 80 * 2 * 9


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--output", "-o", type=Path, required=True)
    ap.add_argument("--boot", type=Path, required=True, help="512-byte boot sector")
    ap.add_argument("--kernel", type=Path, required=True, help="kernel binary")
    args = ap.parse_args()

    boot = args.boot.read_bytes()
    if len(boot) != SECTOR:
        raise SystemExit(f"boot must be {SECTOR} bytes, got {len(boot)}")
    if boot[-2:] != b"\x55\xaa":
        raise SystemExit("boot missing 0x55AA signature")

    kernel = args.kernel.read_bytes()
    if not kernel:
        raise SystemExit("kernel is empty")

    image = bytearray(IMAGE_SECTORS * SECTOR)
    image[0:SECTOR] = boot
    image[SECTOR : SECTOR + len(kernel)] = kernel

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(image)
    ksec = (len(kernel) + SECTOR - 1) // SECTOR
    print(f"wrote {args.output} ({IMAGE_SECTORS} sectors, kernel={ksec} sector(s))")


if __name__ == "__main__":
    main()
