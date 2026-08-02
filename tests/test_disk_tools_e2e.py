"""E2E gate for the rmDOS disk-hygiene COM utilities, including CHKDSK."""

from __future__ import annotations

import os
import shutil
import subprocess
import tempfile
import time
from pathlib import Path

from scripts import fat12
from tests.k8086_util import launcher_argv, terminate_emulator, unlink_retry

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "firmware" / "build"
IMAGE = BUILD / "os-disk.img"
SERIAL = BUILD / "serial.log"
MARKERS = (
    "XCOPYS OK",
    "-A-- ONE.TXT",
    "moved",
    "RA-- MOVED.TXT",
    "Volume label set",
    "CHKDSK OK",
    "DISK OK",
)


def _env() -> dict[str, str]:
    env = os.environ.copy()
    env["K8086_U18_ROM"] = str(BUILD / "u18.bin")
    env["K8086_U19_ROM"] = str(BUILD / "u19.bin")
    return env


def _wait_serial(proc: subprocess.Popen, needles: tuple[str, ...], timeout: float) -> str:
    deadline = time.time() + timeout
    text = ""
    while time.time() < deadline:
        if SERIAL.is_file():
            text = SERIAL.read_text(errors="replace")
            if all(n in text for n in needles):
                return text
        if proc.poll() is not None:
            break
        time.sleep(0.25)
    text = SERIAL.read_text(errors="replace") if SERIAL.is_file() else ""
    raise AssertionError(f"serial gate failed (need {needles!r}).\n---\n{text}\n---")


def _boot(image: Path, needles: tuple[str, ...], timeout: float = 90.0) -> str:
    SERIAL.write_text("")
    proc = subprocess.Popen(
        launcher_argv(image, "--quiet", "--headless", "--serial-log", SERIAL),
        cwd=str(ROOT / "emulator" / "k8086"),
        env=_env(),
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    try:
        return _wait_serial(proc, needles, timeout)
    finally:
        terminate_emulator(proc)


def test_disk_tools_on_image() -> None:
    raw = IMAGE.read_bytes()
    for name in ("ATTRIB.COM", "LABEL.COM", "MOVE.COM", "XCOPY.COM", "CHKDSK.COM", "SYS.COM"):
        assert fat12.find_directory_entry(raw, f"BIN\\{name}").size_bytes > 0


def test_disk_tools_e2e() -> None:
    with tempfile.NamedTemporaryFile(suffix=".img", delete=False) as tmp:
        image = Path(tmp.name)
    shutil.copyfile(IMAGE, image)
    try:
        text = _boot(image, MARKERS)
        assert "CHKDSK OK" in text
        assert "errors found" not in text
    finally:
        unlink_retry(image)


def test_chkdsk_detects_orphan() -> None:
    """Inject a lost cluster; CHKDSK (no /F) must report errors, not OK."""
    with tempfile.NamedTemporaryFile(suffix=".img", delete=False) as tmp:
        image = Path(tmp.name)
    raw = bytearray(IMAGE.read_bytes())
    fat12.inject_orphan_cluster(raw)
    fat12.patch_root_file(
        raw,
        "AUTOEXEC.BAT",
        b"BIN\\CHKDSK\r\nECHO CHKERR DONE\r\n",
    )
    image.write_bytes(raw)
    try:
        text = _boot(image, ("errors found", "CHKERR DONE"))
        assert "lost clusters" in text
        assert "CHKDSK OK" not in text
    finally:
        unlink_retry(image)


def test_chkdsk_fix_orphan_and_mirror() -> None:
    """Inject orphan + FAT mirror mismatch; CHKDSK /F then clean re-check."""
    with tempfile.NamedTemporaryFile(suffix=".img", delete=False) as tmp:
        image = Path(tmp.name)
    raw = bytearray(IMAGE.read_bytes())
    fat12.inject_orphan_cluster(raw)
    fat12.inject_fat_mirror_mismatch(raw)
    fat12.patch_root_file(
        raw,
        "AUTOEXEC.BAT",
        b"BIN\\LABEL RMDOS\r\nBIN\\CHKDSK /F\r\nBIN\\CHKDSK\r\nECHO DISK FIX OK\r\n",
    )
    image.write_bytes(raw)
    try:
        text = _boot(image, ("fixes applied", "CHKDSK OK", "DISK FIX OK"), timeout=120.0)
        assert text.count("CHKDSK OK") >= 1
        assert "fixes applied" in text
        assert "DISK FIX OK" in text
    finally:
        unlink_retry(image)


if __name__ == "__main__":
    test_disk_tools_on_image()
    test_disk_tools_e2e()
    test_chkdsk_detects_orphan()
    test_chkdsk_fix_orphan_and_mirror()
    print("test_disk_tools_e2e: OK")
