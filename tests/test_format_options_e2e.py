"""E2E gates for FORMAT volume labels and floppy geometry switches."""

from __future__ import annotations

import os
import struct
import subprocess
import sys
import tempfile
import time
from pathlib import Path

from tests.k8086_util import launcher_argv, terminate_emulator, unlink_retry

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "firmware" / "build"
SERIAL = BUILD / "serial-format-options.log"


def _env() -> dict[str, str]:
    env = os.environ.copy()
    env["K8086_U18_ROM"] = str(BUILD / "u18.bin")
    env["K8086_U19_ROM"] = str(BUILD / "u19.bin")
    return env


def _format(args: str) -> bytes:
    with tempfile.NamedTemporaryFile(suffix=".img", delete=False) as image_tmp:
        image = Path(image_tmp.name)
    with tempfile.NamedTemporaryFile(suffix=".bat", delete=False, mode="wb") as bat_tmp:
        autoexec = Path(bat_tmp.name)
        bat_tmp.write(f"BIN\\FORMAT A: /Y {args}\r\n".encode("ascii"))
    try:
        subprocess.run(
            [
                sys.executable,
                "-m",
                "scripts.mkfs_fat12",
                "--output",
                str(image),
                "--boot",
                str(BUILD / "boot.bin"),
                "--kernel",
                str(BUILD / "kernel.bin"),
                "--file",
                f"COMMAND.COM={BUILD / 'command.com'}",
                "--file",
                f"BIN/FORMAT.COM={BUILD / 'format.com'}",
                "--file",
                f"AUTOEXEC.BAT={autoexec}",
            ],
            cwd=ROOT,
            check=True,
        )

        SERIAL.write_text("")
        proc = subprocess.Popen(
            launcher_argv(
                image,
                "--quiet",
                "--headless",
                "--serial-log",
                SERIAL,
                floppy_int13_shim=False,
            ),
            cwd=str(ROOT / "emulator" / "k8086"),
            env=_env(),
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        try:
            deadline = time.time() + 60
            while time.time() < deadline:
                text = SERIAL.read_text(errors="replace") if SERIAL.is_file() else ""
                if "FORMAT OK" in text:
                    break
                if proc.poll() is not None:
                    break
                time.sleep(0.2)
            else:
                raise AssertionError(SERIAL.read_text(errors="replace"))
            text = SERIAL.read_text(errors="replace") if SERIAL.is_file() else ""
            assert "FORMAT OK" in text, text
        finally:
            terminate_emulator(proc)
        return image.read_bytes()
    finally:
        unlink_retry(image)
        unlink_retry(autoexec)


def _totsec(raw: bytes) -> int:
    total = struct.unpack_from("<H", raw, 19)[0]
    return total or struct.unpack_from("<I", raw, 32)[0]


def test_format_f720_and_label() -> None:
    raw = _format("/F:720 /V:PHASE4")
    assert _totsec(raw) == 1440
    assert raw[24:28] == struct.pack("<HH", 9, 2)
    assert raw[43:54] == b"PHASE4     "

    reserved = struct.unpack_from("<H", raw, 14)[0]
    fats = raw[16]
    spf = struct.unpack_from("<H", raw, 22)[0]
    root = (reserved + fats * spf) * 512
    assert raw[root : root + 11] == b"PHASE4     "
    assert raw[root + 11] == 0x08


def test_format_four_preset() -> None:
    raw = _format("/4")
    assert _totsec(raw) == 720
    assert struct.unpack_from("<HH", raw, 24) == (9, 2)
    assert raw[21] == 0xFD


def test_format_one_side() -> None:
    raw = _format("/1")
    assert _totsec(raw) == 720
    assert struct.unpack_from("<HH", raw, 24) == (9, 1)


if __name__ == "__main__":
    test_format_f720_and_label()
    test_format_four_preset()
    test_format_one_side()
    print("test_format_options_e2e: OK")
