"""E2E: CONFIG.SYS DEVICE=EMM.SYS with ems-window card — EMSTST marker."""

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
SERIAL = BUILD / "serial-ems.log"
IMAGE = BUILD / "os-ems.img"
K8086 = ROOT / "emulator" / "k8086"
EMS_JAR_GLOB = "cards/ems-window/build/libs/ems-window-*.jar"
MARKERS = ("EMS OK",)


def _ensure_ems_jar() -> Path:
    """Always rebuild so a stale JAR cannot miss FFh-unmap semantics EMM.SYS probes."""
    subprocess.check_call(
        ["./gradlew", ":cards:ems-window:jar", "-q"],
        cwd=str(K8086),
    )
    jars = sorted(K8086.glob(EMS_JAR_GLOB))
    if not jars:
        raise SystemExit("ems-window JAR missing after gradle build")
    return jars[-1]


def test_ems_image_has_config() -> None:
    raw = IMAGE.read_bytes()
    assert fat12.find_directory_entry(raw, "BIN\\EMM.SYS").size_bytes > 0
    assert fat12.find_directory_entry(raw, "DEMO\\EMSTST.COM").size_bytes > 0
    assert fat12.find_directory_entry(raw, "CONFIG.SYS").size_bytes > 0


def test_ems_e2e() -> None:
    env = os.environ.copy()
    env["K8086_U18_ROM"] = str(BUILD / "u18.bin")
    env["K8086_U19_ROM"] = str(BUILD / "u19.bin")

    jar = _ensure_ems_jar()
    card_rel = jar.relative_to(K8086).as_posix()

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
                "--card",
                card_rel,
            ),
            cwd=str(K8086),
            env=env,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        deadline = time.time() + 90
        text = ""
        ok = False
        while time.time() < deadline:
            if SERIAL.exists():
                text = SERIAL.read_text(errors="replace")
                if all(m in text for m in MARKERS):
                    ok = True
                    break
            if proc.poll() is not None:
                break
            time.sleep(0.25)
        terminate_emulator(proc)
        if not ok:
            raise AssertionError(
                f"missing markers {MARKERS!r} in serial log:\n{text[-2000:]}"
            )
    finally:
        unlink_retry(tmp_path)


if __name__ == "__main__":
    test_ems_image_has_config()
    test_ems_e2e()
    print("ok")
