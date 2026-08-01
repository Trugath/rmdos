"""E2E: FORMAT C: on XT-era whole-disk images (FAT12 ~10MB, FAT16 ~20/40MB)."""

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
SERIAL = BUILD / "serial-hd.log"
FLOPPY = BUILD / "os-format-hd.img"

# k8086 default XT geometry: 306×4×17
HD_10M = 306 * 4 * 17 * 512
HD_20M = 20 * 1024 * 1024
HD_40M = 40 * 1024 * 1024
MAX_SECS_40M = 40 * 1024 * 1024 // 512


def _blank_hd(path: Path, size: int) -> None:
    with path.open("wb") as f:
        f.truncate(size)


def _run_format_hd(
    hd_path: Path, timeout: float = 180.0, *, hd_int13_bios: bool | None = None
) -> str:
    env = os.environ.copy()
    env["K8086_U18_ROM"] = str(BUILD / "u18.bin")
    env["K8086_U19_ROM"] = str(BUILD / "u19.bin")
    SERIAL.write_text("")
    with tempfile.NamedTemporaryFile(suffix=".img", delete=False) as tmp:
        floppy = Path(tmp.name)
    with tempfile.NamedTemporaryFile(suffix=".hd.img", delete=False) as tmp:
        hd = Path(tmp.name)
    shutil.copyfile(FLOPPY, floppy)
    shutil.copyfile(hd_path, hd)
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
                hd_int13_bios=hd_int13_bios,
            ),
            cwd=str(ROOT / "emulator" / "k8086"),
            env=env,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        try:
            deadline = time.time() + timeout
            text = ""
            while time.time() < deadline:
                if SERIAL.is_file():
                    text = SERIAL.read_text(errors="replace")
                    if "HD OK" in text and "Format complete" in text:
                        break
                if proc.poll() is not None:
                    break
                time.sleep(0.25)
            else:
                text = SERIAL.read_text(errors="replace") if SERIAL.is_file() else ""
                raise AssertionError(f"FORMAT C: gate timed out.\n---\n{text}\n---")
            text = SERIAL.read_text(errors="replace") if SERIAL.is_file() else ""
            if "HD OK" not in text or "Format complete" not in text:
                raise AssertionError(f"FORMAT C: gate failed.\n---\n{text}\n---")
        finally:
            terminate_emulator(proc)
        return hd.read_bytes()
    finally:
        unlink_retry(floppy)
        unlink_retry(hd)


def _bpb_totsec(img: bytes) -> int:
    tot16 = struct.unpack_from("<H", img, 19)[0]
    if tot16:
        return tot16
    return struct.unpack_from("<I", img, 32)[0]


def _geometry_totsec(size_bytes: int) -> int:
    """Match k8086 HdGeometry.fromImageSize (SPT=17, prefer 4 heads)."""
    spt = 17
    heads = 4
    total = size_bytes // 512
    if total == 0:
        return 0
    cyl = total // (heads * spt)
    while cyl > 1023 and heads < 255:
        heads += 1
        cyl = total // (heads * spt)
    if cyl < 1:
        cyl = 1
    cyl = min(cyl, 1024)
    return cyl * heads * spt


def _assert_fat_volume(img: bytes, *, expect_fat16: bool, size_bytes: int) -> None:
    assert struct.unpack_from("<H", img, 11)[0] == 512
    assert img[21] == 0xF8
    tot = _bpb_totsec(img)
    expect = _geometry_totsec(size_bytes)
    assert tot == expect, f"totsec {tot} != geometry {expect}"
    assert 1 <= tot <= MAX_SECS_40M
    spc = img[13]
    reserved = struct.unpack_from("<H", img, 14)[0]
    fats = img[16]
    root_ents = struct.unpack_from("<H", img, 17)[0]
    spf = struct.unpack_from("<H", img, 22)[0]
    root_secs = (root_ents * 32 + 511) // 512
    data_lba = reserved + fats * spf + root_secs
    clusters = (tot - data_lba) // max(spc, 1)
    if expect_fat16:
        assert clusters >= 4085
        assert img[54:62] == b"FAT16   "
    else:
        assert clusters < 4085
        assert img[54:62] == b"FAT12   "


def test_format_hd_10m_fat12() -> None:
    hd = BUILD / "hd-10m.img"
    _blank_hd(hd, HD_10M)
    img = _run_format_hd(hd)
    _assert_fat_volume(img, expect_fat16=False, size_bytes=HD_10M)


def test_format_hd_20m_fat16() -> None:
    hd = BUILD / "hd-20m.img"
    _blank_hd(hd, HD_20M)
    img = _run_format_hd(hd, timeout=240.0)
    _assert_fat_volume(img, expect_fat16=True, size_bytes=HD_20M)


def test_format_hd_40m_fat16_totsec32() -> None:
    hd = BUILD / "hd-40m.img"
    _blank_hd(hd, HD_40M)
    img = _run_format_hd(hd, timeout=300.0)
    _assert_fat_volume(img, expect_fat16=True, size_bytes=HD_40M)
    assert struct.unpack_from("<H", img, 19)[0] == 0
    assert _bpb_totsec(img) == _geometry_totsec(HD_40M)


def test_format_hd_host_bios_smoke() -> None:
    """Host FixedDiskBios still works when explicitly enabled."""
    hd = BUILD / "hd-10m-hostbios.img"
    _blank_hd(hd, HD_10M)
    img = _run_format_hd(hd, hd_int13_bios=True)
    _assert_fat_volume(img, expect_fat16=False, size_bytes=HD_10M)


if __name__ == "__main__":
    assert FLOPPY.is_file(), f"missing {FLOPPY}; build os-format-hd.img first"
    test_format_hd_10m_fat12()
    test_format_hd_20m_fat16()
    test_format_hd_40m_fat16_totsec32()
    test_format_hd_host_bios_smoke()
    print("test_format_hd_e2e: OK")
