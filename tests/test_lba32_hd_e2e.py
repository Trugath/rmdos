"""E2E: FORMAT C: on a primary partition starting at LBA 65536."""

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
FLOPPY = BUILD / "os-format-hd.img"
SERIAL = BUILD / "serial-lba32-hd.log"

# 80 MiB → HdGeometry grows heads so cylinders stay ≤1023 (10×17×963).
HD_SIZE = 80 * 1024 * 1024
START_LBA = 65536
HEADS = 10
SPT = 17


def _lba_to_chs(lba: int) -> tuple[int, int, int]:
    temp = lba // SPT
    sector = (lba % SPT) + 1
    head = temp % HEADS
    cyl = temp // HEADS
    return cyl, head, sector


def _pack_chs(cyl: int, head: int, sector: int) -> tuple[int, int, int]:
    return head, ((cyl >> 2) & 0xC0) | (sector & 0x3F), cyl & 0xFF


def _seed_mbr(path: Path) -> None:
    img = bytearray(HD_SIZE)
    img[510:512] = b"\x55\xaa"
    cyl0, head0, sec0 = _lba_to_chs(START_LBA)
    end_lba = HD_SIZE // 512 - 1
    cyl1, head1, sec1 = _lba_to_chs(end_lba)
    h0, s0, c0 = _pack_chs(cyl0, head0, sec0)
    h1, s1, c1 = _pack_chs(cyl1, head1, sec1)
    sectors = (HD_SIZE // 512) - START_LBA
    # Active primary FAT16 (06h); FORMAT rewrites type to match FS.
    part = struct.pack(
        "<BBBBBBBBII",
        0x80,
        h0,
        s0,
        c0,
        0x06,
        h1,
        s1,
        c1,
        START_LBA,
        sectors,
    )
    img[0x1BE : 0x1CE] = part
    path.write_bytes(img)


def _run() -> tuple[str, bytes]:
    env = os.environ.copy()
    env["K8086_U18_ROM"] = str(BUILD / "u18.bin")
    env["K8086_U19_ROM"] = str(BUILD / "u19.bin")
    SERIAL.write_text("")
    with tempfile.NamedTemporaryFile(suffix=".img", delete=False) as tmp:
        floppy = Path(tmp.name)
    with tempfile.NamedTemporaryFile(suffix=".hd.img", delete=False) as tmp:
        hd = Path(tmp.name)
    shutil.copyfile(FLOPPY, floppy)
    _seed_mbr(hd)
    try:
        proc = subprocess.Popen(
            launcher_argv(
                floppy,
                hd,
                "--quiet",
                "--headless",
                "--serial-log",
                SERIAL,
                floppy_int13_shim=False,
            ),
            cwd=str(ROOT / "emulator" / "k8086"),
            env=env,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        try:
            deadline = time.time() + 420.0
            text = ""
            while time.time() < deadline:
                text = SERIAL.read_text(errors="replace") if SERIAL.is_file() else ""
                if "HD OK" in text and "Format complete" in text:
                    break
                if proc.poll() is not None:
                    break
                time.sleep(0.25)
            text = SERIAL.read_text(errors="replace") if SERIAL.is_file() else ""
            if "HD OK" not in text or "Format complete" not in text:
                raise AssertionError(f"LBA32 FORMAT gate failed.\n---\n{text}\n---")
        finally:
            terminate_emulator(proc)
        return text, hd.read_bytes()
    finally:
        unlink_retry(floppy)
        unlink_retry(hd)


def test_format_partition_past_64k_lba() -> None:
    _, img = _run()
    part = img[0x1BE : 0x1CE]
    start, sectors = struct.unpack_from("<II", part, 8)
    assert start == START_LBA, f"partition start {start}, want {START_LBA}"
    assert sectors > 0
    vbr = img[start * 512 : (start + 1) * 512]
    assert vbr[510:512] == b"\x55\xaa"
    assert struct.unpack_from("<H", vbr, 11)[0] == 512
    hidden = struct.unpack_from("<I", vbr, 28)[0]
    assert hidden == START_LBA, f"HiddenSectors={hidden:#x}, want {START_LBA:#x}"
    assert vbr[54:62] in (b"FAT16   ", b"FAT12   ")
    # DIR C: succeeded (HD OK) — volume at LBA≥65536 mounted and listed.


if __name__ == "__main__":
    assert FLOPPY.is_file(), f"missing {FLOPPY}; build os-format-hd.img first"
    test_format_partition_past_64k_lba()
    print("test_lba32_hd_e2e: OK")
