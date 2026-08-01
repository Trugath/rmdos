"""E2E: boot install floppy + blank 10MB HD, run INSTALL.BAT, reboot from HD."""

from __future__ import annotations

import os
import shutil
import struct
import subprocess
import tempfile
import time
from pathlib import Path

from scripts import fat12
from tests.k8086_util import launcher_argv, terminate_emulator, unlink_retry

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "firmware" / "build"
FLOPPY = BUILD / "os-install.img"
SERIAL = BUILD / "serial-install-hd.log"

# k8086 default XT geometry (~10 MiB): 306 cyl × 4 heads × 17 spt
HD_10M = 306 * 4 * 17 * 512

MARKERS = ("PARTEDIT OK", "Format complete", "INSTALL OK")


def _blank_hd(path: Path) -> None:
    with path.open("wb") as f:
        f.truncate(HD_10M)


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
        f"install gate missing {missing!r} within {timeout}s.\n---\n{text}\n---"
    )


def test_install_bat_on_floppy() -> None:
    raw = FLOPPY.read_bytes()
    assert fat12.find_directory_entry(raw, "INSTALL.BAT").size_bytes > 0
    assert fat12.find_directory_entry(raw, "BIN\\PARTEDIT.COM").size_bytes > 0
    assert fat12.find_directory_entry(raw, "BIN\\FORMAT.COM").size_bytes > 0
    ae = fat12.find_directory_entry(raw, "AUTOEXEC.BAT")
    assert ae.size_bytes > 0


def test_install_from_floppy_to_10mb_hd() -> None:
    """User path: boot A:, INSTALL.BAT partitions/formats C: /S, then HD boots."""
    SERIAL.write_text("")
    with tempfile.NamedTemporaryFile(suffix=".img", delete=False) as tmp_fd:
        floppy = Path(tmp_fd.name)
    with tempfile.NamedTemporaryFile(suffix=".hd.img", delete=False) as tmp_hd:
        hd = Path(tmp_hd.name)
    shutil.copyfile(FLOPPY, floppy)
    _blank_hd(hd)
    env = _env()
    try:
        # Phase 1: boot floppy with blank 10MB disk attached; AUTOEXEC runs INSTALL.BAT
        proc = subprocess.Popen(
            launcher_argv(floppy, hd, "--quiet", "--headless", "--serial-log", SERIAL),
            cwd=str(ROOT / "emulator" / "k8086"),
            env=env,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        try:
            text = _wait_serial(proc, MARKERS, timeout=240.0)
            assert "KERNEL" in text.upper() or "COMMAND" in text.upper() or "DIR" in text
        finally:
            terminate_emulator(proc)

        image = hd.read_bytes()
        assert len(image) == HD_10M
        assert image[510:512] == b"\x55\xaa"
        part = image[0x1BE : 0x1CE]
        assert part[0] == 0x80, "partition not active"
        assert part[4] == 0x01, f"10MB install partition type should be FAT12 (01h), got {part[4]:#x}"
        start, sectors = struct.unpack_from("<II", part, 8)
        assert start > 0 and sectors > 0
        vbr = image[start * 512 : (start + 1) * 512]
        assert vbr[510:512] == b"\x55\xaa"
        assert struct.unpack_from("<I", vbr, 28)[0] == start
        root_lba = start + struct.unpack_from("<H", vbr, 14)[0]
        root_lba += vbr[16] * struct.unpack_from("<H", vbr, 22)[0]
        root = image[root_lba * 512 : (root_lba + 1) * 512]
        assert b"KERNEL  SYS" in root
        assert b"COMMAND COM" in root

        # Phase 2: reboot with HD as IPL (@ prefix); OS should start from C:
        SERIAL.write_text("")
        proc = subprocess.Popen(
            launcher_argv(
                floppy, "@" + str(hd), "--quiet", "--headless", "--serial-log", SERIAL
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
    test_install_bat_on_floppy()
    test_install_from_floppy_to_10mb_hd()
    print("test_install_hd_e2e: OK")
