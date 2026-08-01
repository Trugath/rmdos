"""E2E: DISKCOPY A: B: /Y copies the boot floppy onto a second drive."""

from __future__ import annotations

import os
import shutil
import subprocess
import tempfile
import time
from pathlib import Path

from scripts import fat12
from scripts.disk import SECTOR_SIZE, TOTAL_SECTORS
from tests.k8086_util import launcher_argv, terminate_emulator, unlink_retry

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "firmware" / "build"
SERIAL = BUILD / "serial-diskcopy.log"
IMAGE = BUILD / "os-diskcopy.img"
MARKERS = ("DISKCOPY OK", "DISKCOPY DONE")
COMPARE_SECTORS = 32


def test_diskcopy_on_image() -> None:
    raw = IMAGE.read_bytes()
    assert fat12.find_directory_entry(raw, "BIN\\DISKCOPY.COM").size_bytes > 0


def test_diskcopy_e2e() -> None:
    env = os.environ.copy()
    env["K8086_U18_ROM"] = str(BUILD / "u18.bin")
    env["K8086_U19_ROM"] = str(BUILD / "u19.bin")

    SERIAL.write_text("")
    with tempfile.NamedTemporaryFile(suffix=".img", delete=False) as tmp_a:
        path_a = Path(tmp_a.name)
    with tempfile.NamedTemporaryFile(suffix=".img", delete=False) as tmp_b:
        path_b = Path(tmp_b.name)

    shutil.copyfile(IMAGE, path_a)
    # Patterned blank B: so a successful copy is obvious.
    path_b.write_bytes(bytes((i * 17) & 0xFF for i in range(TOTAL_SECTORS * SECTOR_SIZE)))

    try:
        proc = subprocess.Popen(
            launcher_argv(
                path_a,
                "--floppy",
                path_b,
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
            deadline = time.time() + 600
            text = ""
            while time.time() < deadline:
                if SERIAL.is_file():
                    # Serial may contain NULs from prior failed runs; keep text decode safe.
                    text = SERIAL.read_text(errors="replace")
                    if all(marker in text for marker in MARKERS):
                        break
                if proc.poll() is not None:
                    break
                time.sleep(0.5)
            else:
                raise AssertionError(
                    f"diskcopy gate timed out (need {MARKERS!r}).\n---\n{text[-4000:]}\n---"
                )
            if not all(marker in text for marker in MARKERS):
                raise AssertionError(
                    f"diskcopy gate failed (need {MARKERS!r}).\n---\n{text[-4000:]}\n---"
                )
        finally:
            terminate_emulator(proc)

        a = path_a.read_bytes()
        b = path_b.read_bytes()
        n = COMPARE_SECTORS * SECTOR_SIZE
        assert a[:n] == b[:n], "B: first sectors should match A: after DISKCOPY"
    finally:
        unlink_retry(path_a)
        unlink_retry(path_b)


if __name__ == "__main__":
    test_diskcopy_on_image()
    test_diskcopy_e2e()
    print("test_diskcopy_e2e: OK")
