"""E2E gate for COMMAND.COM batch, redirection, SET, GOTO, and REN."""

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
SERIAL = BUILD / "serial.log"
IMAGE = BUILD / "os-batch.img"
MARKERS = (
    "EXISTS",
    "RENAMED",
    "BAR",
    "rmDOS DOS 3.31",
    "PROMPT OK",
    "Current date is",
    "VERIFY is ON",
    "ONE",
    "TWO",
    "FOR OK",
    "BREAK is ON",
    "IFSTR OK",
    "IFNOT OK",
    "SHIFT OK",
    "EXIT OK",
    "ERASED",
    "AUTOEXEC.BAT",
    "CMD OK",
    "ENV OK",
    "PIPE OK",
    "ELCD OK",
    "CTTY OK",
    "BATCH OK",
)


def test_batch_on_image() -> None:
    raw = IMAGE.read_bytes()
    entry = fat12.find_directory_entry(raw, "AUTOEXEC.BAT")
    assert entry.size_bytes >= 20


def test_batch_e2e() -> None:
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
                tmp_path, "--quiet", "--headless", "--serial-log", SERIAL
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
                    if all(marker in text for marker in MARKERS):
                        return
                if proc.poll() is not None:
                    break
                time.sleep(0.25)
            raise AssertionError(
                f"batch gate failed (need {MARKERS!r}).\n---\n{text}\n---"
            )
        finally:
            terminate_emulator(proc)
    finally:
        unlink_retry(tmp_path)


if __name__ == "__main__":
    test_batch_on_image()
    test_batch_e2e()
    print("test_batch_e2e: OK")
