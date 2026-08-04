#!/usr/bin/env python3
"""Build a partitioned FAT12/FAT16 hard-disk image (PARTEDIT + FORMAT layout).

Geometry defaults match k8086's XT ~10 MiB disk (306 cyl × 4 heads × 17 spt).
Creates an active primary at LBA 17 (same as PARTEDIT /CREATE) and a
FORMAT-compatible FAT volume inside it so rmDOS maps it as C:.
"""

from __future__ import annotations

import argparse
import struct
from pathlib import Path

from scripts.disk import SECTOR_SIZE, atomic_write, encode_name

# k8086 default XT geometry / PARTEDIT first-primary start
DEFAULT_CYL = 306
DEFAULT_HEADS = 4
DEFAULT_SPT = 17
PART_START_LBA = 17


def geometry_totsec(
    size_bytes: int, *, spt: int = DEFAULT_SPT, heads: int = DEFAULT_HEADS
) -> tuple[int, int, int, int]:
    """Return (totsec, cylinders, heads, spt) like k8086 HdGeometry.fromImageSize."""
    total = size_bytes // SECTOR_SIZE
    if total == 0:
        raise ValueError("image too small")
    cyl = total // (heads * spt)
    h = heads
    while cyl > 1023 and h < 255:
        h += 1
        cyl = total // (h * spt)
    if cyl < 1:
        cyl = 1
    cyl = min(cyl, 1024)
    return cyl * h * spt, cyl, h, spt


def compute_layout(totsec: int) -> dict[str, int]:
    """Mirror FORMAT.COM HD layout (prefer FAT12 by growing SPC up to 8)."""
    if totsec < 64:
        raise ValueError("partition too small")
    root_ents = 512
    reserved = 2
    fats = 2
    media = 0xF8
    spc = 1
    while True:
        root_secs = root_ents // 16
        spf = 1
        while True:
            data_lba = reserved + fats * spf + root_secs
            if data_lba >= totsec:
                raise ValueError("no data region")
            clusters = (totsec - data_lba) // spc
            if clusters < 4085:
                fat_type = 12
                fat_bytes = ((clusters + 2) * 3 + 1) // 2
                need = (fat_bytes + 511) // 512
                if need <= spf:
                    return {
                        "totsec": totsec,
                        "spc": spc,
                        "reserved": reserved,
                        "fats": fats,
                        "root_ents": root_ents,
                        "root_secs": root_secs,
                        "spf": spf,
                        "media": media,
                        "fat_type": fat_type,
                        "data_lba": data_lba,
                        "clusters": clusters,
                        "fat1_lba": reserved,
                        "fat2_lba": reserved + spf,
                        "root_lba": reserved + fats * spf,
                    }
                spf = need
                continue
            if spc < 8:
                spc *= 2
                break
            if clusters > 65525:
                if spc >= 64:
                    raise ValueError("volume too large for FAT16")
                spc *= 2
                break
            fat_type = 16
            fat_bytes = (clusters + 2) * 2
            need = (fat_bytes + 511) // 512
            if need <= spf:
                return {
                    "totsec": totsec,
                    "spc": spc,
                    "reserved": reserved,
                    "fats": fats,
                    "root_ents": root_ents,
                    "root_secs": root_secs,
                    "spf": spf,
                    "media": media,
                    "fat_type": fat_type,
                    "data_lba": data_lba,
                    "clusters": clusters,
                    "fat1_lba": reserved,
                    "fat2_lba": reserved + spf,
                    "root_lba": reserved + fats * spf,
                }
            spf = need


def _set_fat12(fat: bytearray, cluster: int, value: int) -> None:
    offset = cluster + (cluster // 2)
    packed = fat[offset] | (fat[offset + 1] << 8)
    if cluster & 1:
        packed = (packed & 0x000F) | ((value & 0x0FFF) << 4)
    else:
        packed = (packed & 0xF000) | (value & 0x0FFF)
    fat[offset] = packed & 0xFF
    fat[offset + 1] = (packed >> 8) & 0xFF


def _set_fat16(fat: bytearray, cluster: int, value: int) -> None:
    struct.pack_into("<H", fat, cluster * 2, value & 0xFFFF)


def _dirent(name: str, *, attr: int, cluster: int, size: int) -> bytes:
    data = bytearray(32)
    data[0:11] = encode_name(name)
    data[11] = attr & 0xFF
    struct.pack_into("<H", data, 26, cluster)
    struct.pack_into("<I", data, 28, size)
    return bytes(data)


def _lba_to_chs(lba: int, *, heads: int, spt: int) -> tuple[int, int, int]:
    """Return (head, sector, cylinder) for a partition table CHS field."""
    sector = (lba % spt) + 1
    tmp = lba // spt
    head = tmp % heads
    cyl = tmp // heads
    return head, sector, cyl


def _pack_chs(head: int, sector: int, cyl: int) -> bytes:
    return bytes(
        [
            head & 0xFF,
            (sector & 0x3F) | ((cyl >> 2) & 0xC0),
            cyl & 0xFF,
        ]
    )


def _partition_type(part_secs: int) -> int:
    if part_secs < 32768:
        return 0x01
    if part_secs <= 65535:
        return 0x04
    return 0x06


def build_image(
    *,
    files: list[tuple[str, bytes]],
    size_bytes: int | None = None,
    cyl: int = DEFAULT_CYL,
    heads: int = DEFAULT_HEADS,
    spt: int = DEFAULT_SPT,
) -> bytes:
    if size_bytes is None:
        disk_totsec = cyl * heads * spt
    else:
        disk_totsec, cyl, heads, spt = geometry_totsec(size_bytes, spt=spt, heads=heads)

    part_start = PART_START_LBA
    if disk_totsec <= part_start + 64:
        raise ValueError("disk too small for partitioned volume")
    part_secs = disk_totsec - part_start
    layout = compute_layout(part_secs)
    image = bytearray(disk_totsec * SECTOR_SIZE)

    # MBR with one active primary (PARTEDIT /CREATE style).
    mbr = bytearray(SECTOR_SIZE)
    mbr[510:512] = b"\x55\xaa"
    ent = bytearray(16)
    ent[0] = 0x80
    ent[1:4] = _pack_chs(*_lba_to_chs(part_start, heads=heads, spt=spt))
    ent[4] = _partition_type(part_secs)
    # PARTEDIT fills end CHS with 0xFF bytes (LBA fields are authoritative).
    ent[5:8] = b"\xff\xff\xff"
    struct.pack_into("<I", ent, 8, part_start)
    struct.pack_into("<I", ent, 12, part_secs)
    mbr[0x1BE : 0x1BE + 16] = ent
    image[0:SECTOR_SIZE] = mbr

    # Volume boot record / BPB at partition start.
    boot = bytearray(SECTOR_SIZE)
    boot[0:3] = b"\xeb\x3c\x90"
    boot[3:11] = b"rmDOS   "
    struct.pack_into("<H", boot, 11, SECTOR_SIZE)
    boot[13] = layout["spc"]
    struct.pack_into("<H", boot, 14, layout["reserved"])
    boot[16] = layout["fats"]
    struct.pack_into("<H", boot, 17, layout["root_ents"])
    if part_secs < 65536:
        struct.pack_into("<H", boot, 19, part_secs)
        struct.pack_into("<I", boot, 32, 0)
    else:
        struct.pack_into("<H", boot, 19, 0)
        struct.pack_into("<I", boot, 32, part_secs)
    boot[21] = layout["media"]
    struct.pack_into("<H", boot, 22, layout["spf"])
    struct.pack_into("<H", boot, 24, spt)
    struct.pack_into("<H", boot, 26, heads)
    struct.pack_into("<I", boot, 28, part_start)  # HiddenSectors
    boot[36] = 0x80
    boot[38] = 0x29
    struct.pack_into("<I", boot, 39, 0x57334C46)  # 'W3LF'
    boot[43:54] = b"WOLF3D     "
    boot[54:62] = b"FAT16   " if layout["fat_type"] == 16 else b"FAT12   "
    boot[510:512] = b"\x55\xaa"
    vbr_off = part_start * SECTOR_SIZE
    image[vbr_off : vbr_off + SECTOR_SIZE] = boot

    fat = bytearray(layout["spf"] * SECTOR_SIZE)
    if layout["fat_type"] == 12:
        _set_fat12(fat, 0, 0xF00 | layout["media"])
        _set_fat12(fat, 1, 0xFFF)
        eoc = 0xFFF
        set_fat = _set_fat12
    else:
        struct.pack_into("<H", fat, 0, 0xFF00 | layout["media"])
        struct.pack_into("<H", fat, 2, 0xFFFF)
        eoc = 0xFFFF
        set_fat = _set_fat16

    root = bytearray(layout["root_secs"] * SECTOR_SIZE)
    next_cluster = 2
    root_idx = 0
    spc = layout["spc"]
    cluster_bytes = spc * SECTOR_SIZE
    max_cluster = layout["clusters"] + 2
    data_base = part_start + layout["data_lba"]

    for name, content in files:
        if root_idx >= layout["root_ents"]:
            raise ValueError("root directory full")
        clusters_needed = (
            max(1, (len(content) + cluster_bytes - 1) // cluster_bytes) if content else 0
        )
        if content and next_cluster + clusters_needed > max_cluster:
            raise ValueError(f"{name}: does not fit on volume")
        start = 0
        if content:
            start = next_cluster
            for i in range(clusters_needed):
                c = next_cluster + i
                set_fat(fat, c, eoc if i + 1 == clusters_needed else c + 1)
                off = (data_base + (c - 2) * spc) * SECTOR_SIZE
                chunk = content[i * cluster_bytes : (i + 1) * cluster_bytes]
                image[off : off + len(chunk)] = chunk
            next_cluster += clusters_needed
        root[root_idx * 32 : (root_idx + 1) * 32] = _dirent(
            name, attr=0, cluster=start, size=len(content)
        )
        root_idx += 1

    for fat_lba in (layout["fat1_lba"], layout["fat2_lba"]):
        off = (part_start + fat_lba) * SECTOR_SIZE
        image[off : off + len(fat)] = fat
    root_off = (part_start + layout["root_lba"]) * SECTOR_SIZE
    image[root_off : root_off + len(root)] = root
    return bytes(image)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--output", "-o", type=Path, required=True)
    ap.add_argument(
        "--file",
        action="append",
        default=[],
        metavar="NAME=PATH",
        help="Root file NAME=hostpath; repeatable",
    )
    ap.add_argument(
        "--dir",
        type=Path,
        help="Pack all regular files from DIR into the volume root (8.3 names)",
    )
    ap.add_argument(
        "--size-mb",
        type=int,
        default=0,
        help="Image size in MiB (default: XT 10MB geometry)",
    )
    args = ap.parse_args()

    files: list[tuple[str, bytes]] = []
    for spec in args.file:
        if "=" not in spec:
            raise SystemExit(f"bad --file spec (want NAME=PATH): {spec}")
        name, path_s = spec.split("=", 1)
        files.append((name.upper().replace("/", "\\"), Path(path_s).read_bytes()))

    if args.dir is not None:
        if not args.dir.is_dir():
            raise SystemExit(f"not a directory: {args.dir}")
        for path in sorted(args.dir.iterdir()):
            if not path.is_file() or path.name.startswith("."):
                continue
            if path.suffix.upper() == ".MD":
                continue
            name = path.name.upper()
            try:
                encode_name(name)
            except ValueError as exc:
                raise SystemExit(f"not an 8.3 name: {path.name}") from exc
            files.append((name, path.read_bytes()))

    if not files:
        raise SystemExit("no files to pack (pass --file and/or --dir)")

    size_bytes = args.size_mb * 1024 * 1024 if args.size_mb else None
    image = build_image(files=files, size_bytes=size_bytes)
    atomic_write(args.output, image)
    print(
        f"wrote {args.output} ({len(image)} bytes, partitioned FAT, {len(files)} file(s): "
        + ",".join(n for n, _ in files)
        + ")"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
