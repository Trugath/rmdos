"""E2E smoke: 1987 CGA Elite EXE loads under streaming EXEC (optional binary).

Skips when fixtures/guest/elite/ELITE.EXE is absent (not redistributed).
With the binary present: pack lean image, boot headless/turbo, require that
AH=4Bh does not fail (no 'Bad command') and the guest stays in the game
(does not drop back to A:> within the smoke window).
"""

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
SERIAL = BUILD / "serial-elite.log"
IMAGE = BUILD / "os-elite.img"
ELITE = ROOT / "fixtures" / "guest" / "elite" / "ELITE.EXE"
MIN_EXE = 50_000


def test_elite_on_image() -> None:
    if not ELITE.is_file():
        print("test_elite_on_image: SKIP (no ELITE.EXE)")
        return
    assert IMAGE.is_file(), "run make test-elite first"
    raw = IMAGE.read_bytes()
    ent = fat12.find_directory_entry(raw, "ELITE.EXE")
    assert ent.size_bytes >= MIN_EXE, ent.size_bytes


def test_elite_loads_e2e() -> None:
    if not ELITE.is_file():
        print("test_elite_loads_e2e: SKIP (no ELITE.EXE)")
        return

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
                "--turbo",
                "--serial-log",
                SERIAL,
            ),
            cwd=str(ROOT / "emulator" / "k8086"),
            env=env,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        try:
            # Allow boot + EXEC; Elite then sits in CGA loop (little serial).
            time.sleep(8)
            text = SERIAL.read_text(errors="replace") if SERIAL.is_file() else ""
            assert "Bad command" not in text, text[-500:]
            # Still in game: no interactive prompt after AUTOEXEC ELITE.
            assert "A:>" not in text, (
                "dropped to prompt — Elite EXEC likely failed\n" + text[-500:]
            )
            assert proc.poll() is None, "emulator exited early"
        finally:
            terminate_emulator(proc)
    finally:
        unlink_retry(tmp_path)


if __name__ == "__main__":
    if not ELITE.is_file():
        print("test_elite_e2e: SKIP (no ELITE.EXE)")
    else:
        test_elite_on_image()
        test_elite_loads_e2e()
        print("test_elite_e2e: OK")
