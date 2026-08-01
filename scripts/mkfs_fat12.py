#!/usr/bin/env python3
"""Build a bootable FAT12 720 KB floppy with KERNEL.SYS (+ optional root/subdir files)."""

from __future__ import annotations

import argparse
from pathlib import Path

from scripts import fat12
from scripts.disk import ATTR_DIRECTORY, atomic_write


def _allocate_file(
    image: bytearray,
    fat: bytearray,
    content: bytes,
    next_cluster: int,
) -> tuple[int, int]:
    cluster_count = fat12.sectors_for_size(len(content))
    start_cluster = next_cluster
    end_cluster = start_cluster + cluster_count
    if fat12.cluster_to_sector(end_cluster - 1) >= fat12.TOTAL_SECTORS:
        raise ValueError("files do not fit in FAT12 floppy image")

    for cluster in range(start_cluster, end_cluster):
        value = cluster + 1 if cluster < end_cluster - 1 else 0x0FFF
        fat12.set_fat_entry(fat, cluster, value)
        sector = fat12.cluster_to_sector(cluster)
        offset = sector * fat12.SECTOR_SIZE
        chunk = content[
            (cluster - start_cluster) * fat12.SECTOR_SIZE : (cluster - start_cluster + 1)
            * fat12.SECTOR_SIZE
        ]
        image[offset : offset + fat12.SECTOR_SIZE] = chunk.ljust(fat12.SECTOR_SIZE, b"\0")

    return start_cluster, end_cluster


def _dirent_raw(name_11: bytes, *, attr: int, cluster: int, size: int = 0) -> bytes:
    import struct

    data = bytearray(32)
    data[0:11] = name_11
    data[11] = attr & 0xFF
    struct.pack_into("<H", data, 26, cluster)
    struct.pack_into("<I", data, 28, size)
    return bytes(data)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--output", "-o", type=Path, required=True)
    ap.add_argument("--boot", type=Path, required=True, help="512-byte boot sector")
    ap.add_argument("--kernel", type=Path, required=True, help="KERNEL.SYS payload")
    ap.add_argument(
        "--file",
        action="append",
        default=[],
        metavar="NAME=PATH",
        help="Root or DIR\\NAME file (e.g. HELLO.COM=build/hello.com); repeatable",
    )
    ap.add_argument(
        "--dir",
        action="append",
        default=[],
        metavar="NAME",
        help="Create empty root subdirectory NAME; repeatable",
    )
    args = ap.parse_args()

    boot = bytearray(args.boot.read_bytes())
    if len(boot) != fat12.SECTOR_SIZE:
        raise SystemExit(f"boot sector must be {fat12.SECTOR_SIZE} bytes, got {len(boot)}")
    if boot[-2:] != b"\x55\xaa":
        raise SystemExit("boot missing 0x55AA signature")

    kernel = args.kernel.read_bytes()
    if not kernel:
        raise SystemExit("kernel is empty")

    # Parse files: "NAME=PATH" or "DIR\\NAME=PATH"
    root_files: list[tuple[str, bytes]] = []
    sub_files: dict[str, list[tuple[str, bytes]]] = {}
    for spec in args.file:
        if "=" not in spec:
            raise SystemExit(f"bad --file spec (want NAME=PATH): {spec}")
        name, path_s = spec.split("=", 1)
        data = Path(path_s).read_bytes()
        name = name.upper().replace("/", "\\")
        if "\\" in name:
            dirname, _, fname = name.partition("\\")
            sub_files.setdefault(dirname, []).append((fname, data))
        else:
            root_files.append((name, data))

    dirs = {d.upper() for d in args.dir}
    dirs.update(sub_files.keys())

    bpb = fat12.build_bpb()
    boot[3:62] = bpb[3:62]

    image = bytearray(fat12.TOTAL_SECTORS * fat12.SECTOR_SIZE)
    image[0 : fat12.SECTOR_SIZE] = boot

    directory = bytearray(fat12.ROOT_DIR_SECTORS * fat12.SECTOR_SIZE)
    fat = bytearray(fat12.SECTORS_PER_FAT * fat12.SECTOR_SIZE)
    fat[0] = fat12.MEDIA_DESCRIPTOR
    fat[1] = 0xFF
    fat[2] = 0xFF

    start_cluster, next_cluster = _allocate_file(image, fat, kernel, next_cluster=2)
    cluster_count = next_cluster - start_cluster
    kernel_lba = fat12.cluster_to_sector(start_cluster)

    dir_i = 0
    directory[dir_i : dir_i + 32] = fat12.build_dir_entry(
        "KERNEL.SYS",
        attributes=fat12.ATTR_READONLY,
        size_bytes=len(kernel),
        start_cluster=start_cluster,
    )
    dir_i += 32

    dir_clusters: dict[str, int] = {}
    for dname in sorted(dirs):
        # allocate first cluster for directory
        dclust = next_cluster
        next_cluster += 1
        if fat12.cluster_to_sector(dclust) >= fat12.TOTAL_SECTORS:
            raise SystemExit("out of space for directories")
        fat12.set_fat_entry(fat, dclust, 0x0FFF)
        directory[dir_i : dir_i + 32] = fat12.build_dir_entry(
            dname, attributes=ATTR_DIRECTORY, size_bytes=0, start_cluster=dclust
        )
        dir_i += 32
        dir_clusters[dname] = dclust

        # Build full directory stream (. .. + files), then write across clusters.
        dir_blob = bytearray()
        dir_blob += _dirent_raw(b".          ", attr=ATTR_DIRECTORY, cluster=dclust)
        dir_blob += _dirent_raw(b"..         ", attr=ATTR_DIRECTORY, cluster=0)
        for fname, content in sub_files.get(dname, []):
            if content:
                sc, next_cluster = _allocate_file(image, fat, content, next_cluster)
            else:
                sc = 0
            dir_blob += fat12.build_dir_entry(
                fname,
                attributes=fat12.ATTR_READONLY,
                size_bytes=len(content),
                start_cluster=sc,
            )

        # Pad to whole sectors and allocate a chain if needed.
        while len(dir_blob) % fat12.SECTOR_SIZE:
            dir_blob += b"\0" * (fat12.SECTOR_SIZE - (len(dir_blob) % fat12.SECTOR_SIZE))
        need = max(1, len(dir_blob) // fat12.SECTOR_SIZE)
        clusters = [dclust]
        while len(clusters) < need:
            c = next_cluster
            next_cluster += 1
            if fat12.cluster_to_sector(c) >= fat12.TOTAL_SECTORS:
                raise SystemExit("out of space for directory growth")
            clusters.append(c)
        for i, c in enumerate(clusters):
            nxt = clusters[i + 1] if i + 1 < len(clusters) else 0x0FFF
            fat12.set_fat_entry(fat, c, nxt)
            sector = fat12.cluster_to_sector(c)
            off = sector * fat12.SECTOR_SIZE
            chunk = dir_blob[i * fat12.SECTOR_SIZE : (i + 1) * fat12.SECTOR_SIZE]
            image[off : off + fat12.SECTOR_SIZE] = chunk.ljust(fat12.SECTOR_SIZE, b"\0")

    for name, content in root_files:
        if content:
            start_cluster, next_cluster = _allocate_file(image, fat, content, next_cluster)
        else:
            start_cluster = 0
        directory[dir_i : dir_i + 32] = fat12.build_dir_entry(
            name,
            attributes=fat12.ATTR_READONLY,
            size_bytes=len(content),
            start_cluster=start_cluster,
        )
        dir_i += 32

    loader_info = fat12.build_loader_info(
        boot_kernel_start=kernel_lba,
        boot_kernel_sectors=cluster_count,
    )

    image[
        fat12.LOADER_INFO_SECTOR * fat12.SECTOR_SIZE : (fat12.LOADER_INFO_SECTOR + 1)
        * fat12.SECTOR_SIZE
    ] = loader_info
    image[
        fat12.FAT1_START * fat12.SECTOR_SIZE : (fat12.FAT1_START + fat12.SECTORS_PER_FAT)
        * fat12.SECTOR_SIZE
    ] = fat
    image[
        fat12.FAT2_START * fat12.SECTOR_SIZE : (fat12.FAT2_START + fat12.SECTORS_PER_FAT)
        * fat12.SECTOR_SIZE
    ] = fat
    image[
        fat12.ROOT_DIR_START * fat12.SECTOR_SIZE : (
            fat12.ROOT_DIR_START + fat12.ROOT_DIR_SECTORS
        )
        * fat12.SECTOR_SIZE
    ] = directory

    atomic_write(args.output, image)
    extra_names = ",".join(n for n, _ in root_files) or "(none)"
    dir_names = ",".join(sorted(dirs)) or "(none)"
    print(
        f"wrote {args.output} (FAT12, KERNEL.SYS={cluster_count} sector(s) @ LBA {kernel_lba}, "
        f"extra={extra_names}, dirs={dir_names})"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
