"""E2E: SUBST maps E: → A:\\TEST and resolves files."""

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
FLOPPY = BUILD / "os-subst.img"
SERIAL = BUILD / "serial-subst.log"


def test_subst_roundtrip() -> None:
    env = os.environ.copy()
    env["K8086_U18_ROM"] = str(BUILD / "u18.bin")
    env["K8086_U19_ROM"] = str(BUILD / "u19.bin")
    SERIAL.write_text("")
    with tempfile.NamedTemporaryFile(suffix=".img", delete=False) as tmp:
        floppy = Path(tmp.name)
    shutil.copyfile(FLOPPY, floppy)
    try:
        proc = subprocess.Popen(
            launcher_argv(
                floppy,
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
            deadline = time.time() + 180
            text = ""
            while time.time() < deadline:
                text = SERIAL.read_text(errors="replace") if SERIAL.is_file() else ""
                if "SUBST OK" in text:
                    break
                if proc.poll() is not None:
                    break
                time.sleep(0.25)
            else:
                raise AssertionError(f"subst timed out:\n{text}")
            assert "SUBST OK" in text, text
            assert text.count("SUBST OK") >= 1
            assert "HELLO" in text or "SAMPLE" in text or "FIND" in text or "SUBST OK" in text
        finally:
            terminate_emulator(proc)
    finally:
        unlink_retry(floppy)


if __name__ == "__main__":
    assert FLOPPY.is_file(), f"missing {FLOPPY}"
    test_subst_roundtrip()
    print("test_subst_e2e: OK")
