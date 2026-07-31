"""FAT12 layout helpers for 720 KB floppies (adapted from WispOS)."""

from __future__ import annotations

import struct
from dataclasses import dataclass
from pathlib import Path

from scripts.disk import (
    ATTR_DIRECTORY,
    ATTR_READONLY,
    SECTOR_SIZE,
    TOTAL_SECTORS,
    atomic_write,
    decode_name,
    encode_name,
    sectors_for_size,
)

RESERVED_SECTORS = 2
FAT_COUNT = 2
SECTORS_PER_FAT = 3
ROOT_ENTRIES = 112
ROOT_DIR_SECTORS = 7
FAT1_START = 2
FAT2_START = FAT1_START + SECTORS_PER_FAT
ROOT_DIR_START = FAT2_START + SECTORS_PER_FAT
DATA_START = ROOT_DIR_START + ROOT_DIR_SECTORS
MEDIA_DESCRIPTOR = 0xF9
SECTORS_PER_TRACK = 9
HEADS = 2

LOADER_INFO_SECTOR = 1
LOADER_MAGIC = b"RFAT1"
LOADER_BOOT_KERNEL_START = 0x1C
LOADER_BOOT_KERNEL_SECTORS = 0x1E

BPB_BYTES_PER_SECTOR = 11
BPB_SECTORS_PER_CLUSTER = 13
BPB_RESERVED_SECTORS = 14
BPB_FAT_COUNT = 16
BPB_ROOT_ENTRIES = 17
BPB_TOTAL_SECTORS = 19
BPB_MEDIA_DESCRIPTOR = 21
BPB_SECTORS_PER_FAT = 22
BPB_SECTORS_PER_TRACK = 24
BPB_HEADS = 26
BPB_HIDDEN_SECTORS = 28
BPB_LARGE_SECTORS = 32
BPB_DRIVE_NUMBER = 36
BPB_RESERVED1 = 37
BPB_BOOT_SIGNATURE = 38
BPB_VOLUME_ID = 39
BPB_VOLUME_LABEL = 43
BPB_FS_TYPE = 54


@dataclass(frozen=True)
class DirectoryEntry:
    name: str
    attributes: int
    size_bytes: int
    start_cluster: int
    offset: int | None = None


def build_loader_info(*, boot_kernel_start: int, boot_kernel_sectors: int) -> bytes:
    data = bytearray(SECTOR_SIZE)
    data[0:5] = LOADER_MAGIC
    data[5] = 1
    struct.pack_into(
        "<HH",
        data,
        LOADER_BOOT_KERNEL_START,
        boot_kernel_start,
        boot_kernel_sectors,
    )
    return bytes(data)


def build_bpb() -> bytes:
    data = bytearray(SECTOR_SIZE)
    data[0:3] = b"\xeb\x3c\x90"
    data[3:11] = b"RMDOS   "
    struct.pack_into("<H", data, BPB_BYTES_PER_SECTOR, SECTOR_SIZE)
    data[BPB_SECTORS_PER_CLUSTER] = 1
    struct.pack_into("<H", data, BPB_RESERVED_SECTORS, RESERVED_SECTORS)
    data[BPB_FAT_COUNT] = FAT_COUNT
    struct.pack_into("<H", data, BPB_ROOT_ENTRIES, ROOT_ENTRIES)
    struct.pack_into("<H", data, BPB_TOTAL_SECTORS, TOTAL_SECTORS)
    data[BPB_MEDIA_DESCRIPTOR] = MEDIA_DESCRIPTOR
    struct.pack_into("<H", data, BPB_SECTORS_PER_FAT, SECTORS_PER_FAT)
    struct.pack_into("<H", data, BPB_SECTORS_PER_TRACK, SECTORS_PER_TRACK)
    struct.pack_into("<H", data, BPB_HEADS, HEADS)
    struct.pack_into("<I", data, BPB_HIDDEN_SECTORS, 0)
    struct.pack_into("<I", data, BPB_LARGE_SECTORS, 0)
    data[BPB_DRIVE_NUMBER] = 0
    data[BPB_RESERVED1] = 0
    data[BPB_BOOT_SIGNATURE] = 0x29
    struct.pack_into("<I", data, BPB_VOLUME_ID, 0x524D444F)  # 'RMDO'
    data[BPB_VOLUME_LABEL : BPB_VOLUME_LABEL + 11] = b"RMDOS BOOT "
    data[BPB_FS_TYPE : BPB_FS_TYPE + 8] = b"FAT12   "
    return bytes(data)


def build_dir_entry(name: str, *, attributes: int, size_bytes: int, start_cluster: int) -> bytes:
    data = bytearray(32)
    data[0:11] = encode_name(name)
    data[11] = attributes & 0xFF
    struct.pack_into("<H", data, 26, start_cluster)
    struct.pack_into("<I", data, 28, size_bytes)
    return bytes(data)


def parse_dir_entry(raw: bytes, *, offset: int | None = None) -> DirectoryEntry | None:
    if len(raw) != 32:
        raise ValueError("directory entry must be 32 bytes")
    if raw[0] in (0x00, 0xE5):
        return None
    return DirectoryEntry(
        name=decode_name(raw[0:11]),
        attributes=raw[11],
        size_bytes=struct.unpack_from("<I", raw, 28)[0],
        start_cluster=struct.unpack_from("<H", raw, 26)[0],
        offset=offset,
    )


def list_directory_entries(image: bytes | bytearray) -> list[DirectoryEntry]:
    start = ROOT_DIR_START * SECTOR_SIZE
    end = start + ROOT_DIR_SECTORS * SECTOR_SIZE
    entries: list[DirectoryEntry] = []
    for offset in range(start, end, 32):
        entry = parse_dir_entry(bytes(image[offset : offset + 32]), offset=offset)
        if entry is not None:
            entries.append(entry)
    return entries


def list_subdir_entries(image: bytes | bytearray, start_cluster: int) -> list[DirectoryEntry]:
    """List entries in a subdirectory starting at start_cluster."""
    fat_off = FAT1_START * SECTOR_SIZE
    fat = image[fat_off : fat_off + SECTORS_PER_FAT * SECTOR_SIZE]
    entries: list[DirectoryEntry] = []
    for cluster in cluster_chain(fat, start_cluster):
        sector = cluster_to_sector(cluster)
        base = sector * SECTOR_SIZE
        for offset in range(base, base + SECTOR_SIZE, 32):
            entry = parse_dir_entry(bytes(image[offset : offset + 32]), offset=offset)
            if entry is not None:
                entries.append(entry)
    return entries


def set_fat_entry(fat: bytearray, cluster: int, value: int) -> None:
    offset = cluster + (cluster // 2)
    packed = fat[offset] | (fat[offset + 1] << 8)
    if cluster & 1:
        packed = (packed & 0x000F) | ((value & 0x0FFF) << 4)
    else:
        packed = (packed & 0xF000) | (value & 0x0FFF)
    fat[offset] = packed & 0xFF
    fat[offset + 1] = (packed >> 8) & 0xFF


def get_fat_entry(fat: bytes | bytearray, cluster: int) -> int:
    offset = cluster + (cluster // 2)
    value = fat[offset] | (fat[offset + 1] << 8)
    if cluster & 1:
        return (value >> 4) & 0x0FFF
    return value & 0x0FFF


def cluster_to_sector(cluster: int) -> int:
    return DATA_START + (cluster - 2)


def cluster_chain(fat: bytes | bytearray, start_cluster: int) -> list[int]:
    if start_cluster == 0:
        return []
    chain = []
    current = start_cluster
    while current < 0xFF8:
        chain.append(current)
        current = get_fat_entry(fat, current)
    return chain


def find_directory_entry(image: bytes | bytearray, name: str) -> DirectoryEntry:
    """Find a root entry, or a path like BIN\\DIR.COM / BIN/DIR.COM."""
    path = name.replace("/", "\\").upper().strip("\\")
    parts = [p for p in path.split("\\") if p]
    if not parts:
        raise FileNotFoundError(name)

    entries = list_directory_entries(image)
    current: DirectoryEntry | None = None
    for i, part in enumerate(parts):
        target = encode_name(part)
        found = None
        for entry in entries:
            if entry.name in (".", ".."):
                continue
            if encode_name(entry.name) == target:
                found = entry
                break
        if found is None:
            raise FileNotFoundError(name)
        current = found
        if i + 1 < len(parts):
            if (found.attributes & ATTR_DIRECTORY) == 0:
                raise FileNotFoundError(name)
            entries = list_subdir_entries(image, found.start_cluster)
    assert current is not None
    return current


def read_loader_info(image: bytes | bytearray) -> tuple[int, int]:
    off = LOADER_INFO_SECTOR * SECTOR_SIZE
    sector = image[off : off + SECTOR_SIZE]
    if sector[0:5] != LOADER_MAGIC:
        raise ValueError("missing RFAT1 loader sector")
    start, count = struct.unpack_from("<HH", sector, LOADER_BOOT_KERNEL_START)
    return start, count
