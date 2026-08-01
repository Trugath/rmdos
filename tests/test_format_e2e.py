"""E2E: FORMAT A: /S /Y rebuilds FAT12 and restores system files."""

from __future__ import annotations

import os
import shutil
import subprocess
import struct
import tempfile
import time
from pathlib import Path

from scripts import fat12
from scripts.disk import SECTOR_SIZE
from tests.k8086_util import launcher_argv, terminate_emulator, unlink_retry

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "firmware" / "build"
SERIAL = BUILD / "serial.log"
IMAGE = BUILD / "os-format.img"
MARKER = "FORMAT OK"
COMPLETE = "Format complete"
SYS_MSG = "System transferred"


def test_format_on_image() -> None:
    raw = IMAGE.read_bytes()
    assert fat12.find_directory_entry(raw, "BIN\\FORMAT.COM").size_bytes > 0
    ae = fat12.find_directory_entry(raw, "AUTOEXEC.BAT")
    assert ae.size_bytes >= 4


def test_format_e2e() -> None:
    env = os.environ.copy()
    env["K8086_U18_ROM"] = str(BUILD / "u18.bin")
    env["K8086_U19_ROM"] = str(BUILD / "u19.bin")

    SERIAL.write_text("")
    with tempfile.NamedTemporaryFile(suffix=".img", delete=False) as tmp:
        tmp_path = Path(tmp.name)
    shutil.copyfile(IMAGE, tmp_path)
    try:
        proc = subprocess.Popen(
            launcher_argv(
                tmp_path,
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
            deadline = time.time() + 90
            text = ""
            while time.time() < deadline:
                if SERIAL.is_file():
                    text = SERIAL.read_text(errors="replace")
                    if MARKER in text and COMPLETE in text and SYS_MSG in text:
                        break
                if proc.poll() is not None:
                    break
                time.sleep(0.25)
            else:
                text = SERIAL.read_text(errors="replace") if SERIAL.is_file() else ""
                raise AssertionError(
                    f"FORMAT /S gate timed out.\n---\n{text}\n---"
                )
            text = SERIAL.read_text(errors="replace") if SERIAL.is_file() else ""
            if MARKER not in text or COMPLETE not in text or SYS_MSG not in text:
                raise AssertionError(f"FORMAT /S gate failed.\n---\n{text}\n---")
        finally:
            terminate_emulator(proc)

        raw = tmp_path.read_bytes()
        # BPB
        assert struct.unpack_from("<H", raw, 11)[0] == 512
        assert raw[13] == 1  # SPC
        totsec = struct.unpack_from("<H", raw, 19)[0]
        assert totsec == 1440
        assert raw[21] == fat12.MEDIA_DESCRIPTOR
        assert raw[SECTOR_SIZE : SECTOR_SIZE + 5] == fat12.LOADER_MAGIC
        # System files present
        kern = fat12.find_directory_entry(raw, "KERNEL.SYS")
        cmd = fat12.find_directory_entry(raw, "COMMAND.COM")
        assert kern.size_bytes > 0
        assert cmd.size_bytes > 0
        assert kern.start_cluster == 2
    finally:
        unlink_retry(tmp_path)


if __name__ == "__main__":
    test_format_on_image()
    test_format_e2e()
    print("test_format_e2e: OK")
