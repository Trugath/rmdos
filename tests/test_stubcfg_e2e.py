"""CONFIG.SYS advisory no-ops + LASTDRIVE parse."""

from __future__ import annotations

import os
import shutil
import subprocess
import tempfile
import time
from pathlib import Path

from tests.k8086_util import launcher_argv, terminate_emulator, unlink_retry

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "firmware" / "build"
SERIAL = BUILD / "serial.log"
IMAGE = BUILD / "os-stubcfg.img"
MARKERS = ("COM1 set: 9600,N,8,1", "STUBCFG OK")


def test_stubcfg_e2e() -> None:
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
            deadline = time.time() + 60
            text = ""
            while time.time() < deadline:
                if SERIAL.is_file():
                    text = SERIAL.read_text(errors="replace")
                    if all(m in text for m in MARKERS):
                        assert "CONFIG: ignored" not in text
                        return
                if proc.poll() is not None:
                    break
                time.sleep(0.25)
            raise AssertionError(
                f"stubcfg gate failed (need {MARKERS!r}).\n---\n{text}\n---"
            )
        finally:
            terminate_emulator(proc)
    finally:
        unlink_retry(tmp_path)


if __name__ == "__main__":
    test_stubcfg_e2e()
    print("test_stubcfg_e2e: OK")
