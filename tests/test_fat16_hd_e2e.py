"""E2E: FAT16 partitioned HD — /S boot, multi-cluster R/W, multi-sector subdir."""

from __future__ import annotations

import os
import shutil
import struct
import subprocess
import tempfile
import time
from pathlib import Path

from tests.k8086_util import launcher_argv, terminate_emulator, unlink_retry

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "firmware" / "build"
FLOPPY = BUILD / "os-fat16-hd.img"
SERIAL = BUILD / "serial-fat16-hd.log"
HD_20M = 20 * 1024 * 1024


def _env() -> dict[str, str]:
    env = os.environ.copy()
    env["K8086_U18_ROM"] = str(BUILD / "u18.bin")
    env["K8086_U19_ROM"] = str(BUILD / "u19.bin")
    return env


def _wait_serial(proc: subprocess.Popen, needles: tuple[str, ...], timeout: float) -> str:
    deadline = time.time() + timeout
    text = ""
    while time.time() < deadline:
        text = SERIAL.read_text(errors="replace") if SERIAL.is_file() else ""
        if all(n in text for n in needles):
            return text
        if proc.poll() is not None:
            break
        time.sleep(0.25)
    text = SERIAL.read_text(errors="replace") if SERIAL.is_file() else ""
    missing = [n for n in needles if n not in text]
    raise AssertionError(
        f"FAT16 HD gate missing {missing!r} within {timeout}s.\n---\n{text}\n---"
    )


def _bpb_totsec(vbr: bytes) -> int:
    tot16 = struct.unpack_from("<H", vbr, 19)[0]
    if tot16:
        return tot16
    return struct.unpack_from("<I", vbr, 32)[0]


def _read_file_bytes(img: bytes, start_lba: int, vbr: bytes, start_cluster: int, size: int) -> bytes:
    spc = vbr[13]
    reserved = struct.unpack_from("<H", vbr, 14)[0]
    fats = vbr[16]
    root_ents = struct.unpack_from("<H", vbr, 17)[0]
    spf = struct.unpack_from("<H", vbr, 22)[0]
    root_secs = (root_ents * 32 + 511) // 512
    fat1 = start_lba + reserved
    data_lba = fat1 + fats * spf + root_secs
    fat = img[fat1 * 512 : (fat1 + spf) * 512]

    def next_clust(c: int) -> int:
        return struct.unpack_from("<H", fat, c * 2)[0]

    out = bytearray()
    c = start_cluster
    while len(out) < size:
        assert 2 <= c < 0xFFF8, f"bad cluster {c:#x}"
        lba = data_lba + (c - 2) * spc
        chunk = img[lba * 512 : (lba + spc) * 512]
        need = size - len(out)
        out.extend(chunk[:need])
        c = next_clust(c)
    return bytes(out)


def _find_dirent(sector: bytes, name83: bytes) -> tuple[int, int] | None:
    for i in range(0, 512, 32):
        ent = sector[i : i + 32]
        if ent[0] in (0x00, 0xE5):
            continue
        if ent[0:11] == name83:
            cluster = struct.unpack_from("<H", ent, 26)[0]
            size = struct.unpack_from("<I", ent, 28)[0]
            return cluster, size
    return None


def _dir_sectors(img: bytes, start_lba: int, vbr: bytes, dir_cluster: int) -> list[bytes]:
    """Return raw directory sectors for root (cluster 0) or a subdir."""
    spc = vbr[13]
    reserved = struct.unpack_from("<H", vbr, 14)[0]
    fats = vbr[16]
    root_ents = struct.unpack_from("<H", vbr, 17)[0]
    spf = struct.unpack_from("<H", vbr, 22)[0]
    root_secs = (root_ents * 32 + 511) // 512
    fat1 = start_lba + reserved
    root_lba = fat1 + fats * spf
    data_lba = root_lba + root_secs
    if dir_cluster == 0:
        return [img[(root_lba + i) * 512 : (root_lba + i + 1) * 512] for i in range(root_secs)]

    fat = img[fat1 * 512 : (fat1 + spf) * 512]

    def next_clust(c: int) -> int:
        return struct.unpack_from("<H", fat, c * 2)[0]

    secs: list[bytes] = []
    c = dir_cluster
    while 2 <= c < 0xFFF8:
        lba = data_lba + (c - 2) * spc
        for i in range(spc):
            secs.append(img[(lba + i) * 512 : (lba + i + 1) * 512])
        c = next_clust(c)
    return secs


def test_fat16_partition_io_and_boot() -> None:
    SERIAL.write_text("")
    with tempfile.NamedTemporaryFile(suffix=".img", delete=False) as tmp_fd:
        floppy = Path(tmp_fd.name)
    with tempfile.NamedTemporaryFile(suffix=".hd.img", delete=False) as tmp_hd:
        hd = Path(tmp_hd.name)
    shutil.copyfile(FLOPPY, floppy)
    with hd.open("wb") as f:
        f.truncate(HD_20M)
    env = _env()
    try:
        proc = subprocess.Popen(
            launcher_argv(floppy, hd, "--quiet", "--headless", "--serial-log", SERIAL, floppy_int13_shim=False, hd_int13_bios=False),
            cwd=str(ROOT / "emulator" / "k8086"),
            env=env,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        try:
            _wait_serial(proc, ("FAT16IO OK", "Format complete"), timeout=300.0)
        finally:
            terminate_emulator(proc)

        image = hd.read_bytes()
        assert image[510:512] == b"\x55\xaa"
        part = image[0x1BE : 0x1CE]
        assert part[0] == 0x80
        assert part[4] in (0x04, 0x06), f"expected FAT16 partition type, got {part[4]:#x}"
        start, sectors = struct.unpack_from("<II", part, 8)
        assert start > 0 and sectors > 0
        vbr = image[start * 512 : (start + 1) * 512]
        assert vbr[510:512] == b"\x55\xaa"
        assert vbr[54:62] == b"FAT16   "
        spc = vbr[13]
        assert spc >= 1
        tot = _bpb_totsec(vbr)
        reserved = struct.unpack_from("<H", vbr, 14)[0]
        fats = vbr[16]
        root_ents = struct.unpack_from("<H", vbr, 17)[0]
        spf = struct.unpack_from("<H", vbr, 22)[0]
        root_secs = (root_ents * 32 + 511) // 512
        data_lba = reserved + fats * spf + root_secs
        clusters = (tot - data_lba) // max(spc, 1)
        assert clusters >= 4085, f"expected FAT16 cluster count, got {clusters}"

        root_secs_data = _dir_sectors(image, start, vbr, 0)
        big = None
        big2 = None
        sub = None
        for sec in root_secs_data:
            if big is None:
                big = _find_dirent(sec, b"BIG     SYS")
            if big2 is None:
                big2 = _find_dirent(sec, b"BIG2    SYS")
            if sub is None:
                sub = _find_dirent(sec, b"SUB        ")
        assert big is not None and big2 is not None and sub is not None
        assert big[1] > spc * 512, f"BIG.SYS should span >1 cluster ({big[1]} bytes, spc={spc})"
        assert big[1] == big2[1]
        data1 = _read_file_bytes(image, start, vbr, big[0], big[1])
        data2 = _read_file_bytes(image, start, vbr, big2[0], big2[1])
        assert data1 == data2

        # Subdir should have enough entries that sector index > 0 is used (SPC>1).
        sub_secs = _dir_sectors(image, start, vbr, sub[0])
        found = 0
        for sec in sub_secs:
            for i in range(0, 512, 32):
                ent = sec[i : i + 32]
                if ent[0] in (0x00, 0xE5):
                    continue
                name = ent[0:11]
                if name.startswith(b"F") and name.endswith(b"TXT"):
                    found += 1
        assert found >= 20, f"expected >=20 Fxx.TXT in SUB, found {found}"
        if spc > 1:
            assert len(sub_secs) >= 2

        SERIAL.write_text("")
        proc = subprocess.Popen(
            launcher_argv(
                floppy, "@" + str(hd), "--quiet", "--headless", "--serial-log", SERIAL,
                floppy_int13_shim=False, hd_int13_bios=False,
            ),
            cwd=str(ROOT / "emulator" / "k8086"),
            env=env,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        try:
            text = _wait_serial(proc, ("rmDOS 0.8",), timeout=60.0)
        finally:
            terminate_emulator(proc)
        assert "rmDOS 0.8" in text
    finally:
        unlink_retry(floppy)
        unlink_retry(hd)


if __name__ == "__main__":
    assert FLOPPY.is_file(), f"missing {FLOPPY}; build it first"
    test_fat16_partition_io_and_boot()
    print("test_fat16_hd_e2e: OK")
