"""Headless BIOS service unit tests via boot-sector images."""

from __future__ import annotations

import os
import subprocess
import time
from pathlib import Path

from tests.k8086_util import launcher_argv, terminate_emulator

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "firmware" / "build"
BIOS_TESTS = BUILD / "bios_tests"
SERIAL_DIR = BIOS_TESTS / "serial"

TESTS = [
    "bt_equip",
    "bt_bda",
    "bt_video",
    "bt_scroll",
    "bt_disk",
    "bt_timer",
    "bt_int1c",
    "bt_kbd_flags",
    "bt_modes_text",
    "bt_modes_gfx",
    "bt_mode4",
    "bt_mode6",
    "bt_serial",
    "bt_int15",
    "bt_pixel",
    "bt_misc",
    "bt_ctype",
    "bt_gfx_scroll",
    "bt_pixel6",
]


def _run_one(name: str, timeout_s: float = 20.0) -> None:
    img = BIOS_TESTS / f"{name}.img"
    if not img.is_file():
        raise AssertionError(f"missing {img}; run make bios-tests")

    SERIAL_DIR.mkdir(parents=True, exist_ok=True)
    serial = SERIAL_DIR / f"{name}.log"
    serial.write_text("")

    env = os.environ.copy()
    env["K8086_U18_ROM"] = str(BUILD / "u18.bin")
    env["K8086_U19_ROM"] = str(BUILD / "u19.bin")

    expect = f"PASS {name}"
    proc = subprocess.Popen(
        launcher_argv(
            img,
            "--quiet",
            "--headless",
            "--serial-log",
            serial,
        ),
        cwd=str(ROOT / "emulator" / "k8086"),
        env=env,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    try:
        deadline = time.time() + timeout_s
        while time.time() < deadline:
            text = serial.read_text(errors="replace") if serial.is_file() else ""
            if expect in text:
                proc.wait(timeout=5)
                return
            if "FAIL " in text:
                raise AssertionError(f"{name} failed:\n{text}")
            if proc.poll() is not None:
                break
            time.sleep(0.05)
        text = serial.read_text(errors="replace") if serial.is_file() else ""
        raise AssertionError(
            f"{name}: expected {expect!r} in serial (exit={proc.poll()}).\n---\n{text}\n---"
        )
    finally:
        terminate_emulator(proc)


def test_bios_services() -> None:
    for name in TESTS:
        _run_one(name)


if __name__ == "__main__":
    test_bios_services()
    print("test_bios_services: OK")
