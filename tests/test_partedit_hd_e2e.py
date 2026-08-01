"""E2E: PARTEDIT /CREATE followed by FORMAT C: /S creates a bootable partition."""

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
FLOPPY = BUILD / "os-partedit-hd.img"
SERIAL = BUILD / "serial-partedit-hd.log"
HD_SIZE = 306 * 4 * 17 * 512


def _run() -> bytes:
    env = os.environ.copy()
    env["K8086_U18_ROM"] = str(BUILD / "u18.bin")
    env["K8086_U19_ROM"] = str(BUILD / "u19.bin")
    SERIAL.write_text("")
    with tempfile.NamedTemporaryFile(suffix=".img", delete=False) as tmp_fd:
        floppy = Path(tmp_fd.name)
    with tempfile.NamedTemporaryFile(suffix=".hd.img", delete=False) as tmp_hd:
        hd = Path(tmp_hd.name)
    shutil.copyfile(FLOPPY, floppy)
    hd.write_bytes(b"")
    with hd.open("r+b") as f:
        f.truncate(HD_SIZE)
    try:
        proc = subprocess.Popen(
            launcher_argv(floppy, hd, "--quiet", "--headless", "--serial-log", SERIAL, floppy_int13_shim=False),
            cwd=str(ROOT / "emulator" / "k8086"),
            env=env,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        try:
            deadline = time.time() + 240
            while time.time() < deadline:
                text = SERIAL.read_text(errors="replace") if SERIAL.is_file() else ""
                if "PARTEDIT OK" in text and "PARTEDITFMT OK" in text and "HD 80" in text:
                    break
                if proc.poll() is not None:
                    break
                time.sleep(0.25)
            else:
                raise AssertionError(f"PARTEDIT/FORMAT timed out:\n{text}")
            assert "PARTEDIT OK" in text and "PARTEDITFMT OK" in text, text
            assert "HD 80" in text, text
            assert "C:" in text, text
        finally:
            terminate_emulator(proc)
        # k8086's @ prefix selects the fixed disk as INT 19h boot media.
        SERIAL.write_text("")
        proc = subprocess.Popen(
            launcher_argv(floppy, "@" + str(hd), "--quiet", "--headless", "--serial-log", SERIAL, floppy_int13_shim=False),
            cwd=str(ROOT / "emulator" / "k8086"),
            env=env,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        try:
            deadline = time.time() + 30
            while time.time() < deadline:
                text = SERIAL.read_text(errors="replace") if SERIAL.is_file() else ""
                if "rmDOS 0.8" in text:
                    break
                if proc.poll() is not None:
                    break
                time.sleep(0.25)
            assert "rmDOS 0.8" in text, f"HD boot failed:\n{text}"
        finally:
            terminate_emulator(proc)
        return hd.read_bytes()
    finally:
        unlink_retry(floppy)
        unlink_retry(hd)


def test_partedit_then_format_partition() -> None:
    image = _run()
    assert image[510:512] == b"\x55\xaa"
    part = image[0x1BE : 0x1CE]
    assert part[0] == 0x80
    assert part[4] == 0x01, f"10MB partition type should be FAT12 (01h), got {part[4]:#x}"
    start, sectors = struct.unpack_from("<II", part, 8)
    assert start == 17 and sectors > 0
    vbr = image[start * 512 : (start + 1) * 512]
    assert vbr[510:512] == b"\x55\xaa"
    assert struct.unpack_from("<H", vbr, 11)[0] == 512
    assert struct.unpack_from("<I", vbr, 28)[0] == start
    root_lba = start + struct.unpack_from("<H", vbr, 14)[0]
    root_lba += vbr[16] * struct.unpack_from("<H", vbr, 22)[0]
    root = image[root_lba * 512 : (root_lba + 1) * 512]
    assert b"KERNEL  SYS" in root


if __name__ == "__main__":
    assert FLOPPY.is_file(), f"missing {FLOPPY}; build it first"
    test_partedit_then_format_partition()
    print("test_partedit_hd_e2e: OK")
