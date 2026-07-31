"""DOS compatibility gate: COMPAT.COM + rmDOS FIND/CHOICE via AUTOEXEC."""

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
IMAGE = BUILD / "os-compat.img"
COMPAT_OK = "COMPAT OK"
UTILS_OK = "UTILS OK"
FIND_NEEDLE = "HELLO rmDOS"


def test_compat_on_image() -> None:
    raw = IMAGE.read_bytes()
    for name in (
        "DEMO\\COMPAT.COM",
        "BIN\\FIND.COM",
        "BIN\\CHOICE.COM",
        "BIN\\MORE.COM",
        "TEST\\SAMPLE.TXT",
        "AUTOEXEC.BAT",
    ):
        ent = fat12.find_directory_entry(raw, name)
        assert ent.size_bytes > 0, name


def test_compat_e2e() -> None:
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
            ),
            cwd=str(ROOT / "emulator" / "k8086"),
            env=env,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
    except Exception:
        unlink_retry(tmp_path)
        raise
    deadline = time.time() + 120
    text = ""
    ok = False
    try:
        while time.time() < deadline:
            if SERIAL.is_file():
                text = SERIAL.read_text(errors="replace")
                if (
                    COMPAT_OK in text
                    and FIND_NEEDLE in text
                    and UTILS_OK in text
                ):
                    ok = True
                    break
                if "COMPAT FAIL" in text:
                    break
            if proc.poll() is not None:
                break
            time.sleep(0.25)
        terminate_emulator(proc)
    finally:
        unlink_retry(tmp_path)

    if "COMPAT FAIL" in text:
        raise AssertionError(f"COMPAT FAIL\n---\n{text}\n---")
    if not ok:
        raise AssertionError(
            f"compat/FIND/CHOICE gate failed (need {COMPAT_OK!r}, "
            f"{FIND_NEEDLE!r}, {UTILS_OK!r}).\n---\n{text}\n---"
        )
    print("test_dos_compat: OK")


if __name__ == "__main__":
    test_compat_on_image()
    test_compat_e2e()
