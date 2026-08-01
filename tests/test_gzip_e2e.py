"""E2E: GZIP.COM / GUNZIP.COM file and pipe round-trips on SAMPLE.TXT."""

from __future__ import annotations

import gzip
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
IMAGE = BUILD / "os-gzip.img"
SERIAL = BUILD / "serial-gzip.log"
MARKER = "GZIP OK"


def _env() -> dict[str, str]:
    env = os.environ.copy()
    env["K8086_U18_ROM"] = str(BUILD / "u18.bin")
    env["K8086_U19_ROM"] = str(BUILD / "u19.bin")
    return env


def _wait_serial(proc: subprocess.Popen, needles: tuple[str, ...], timeout: float) -> str:
    deadline = time.time() + timeout
    text = ""
    while time.time() < deadline:
        if SERIAL.is_file():
            text = SERIAL.read_text(errors="replace")
            if all(n in text for n in needles):
                return text
        if proc.poll() is not None:
            break
        time.sleep(0.25)
    text = SERIAL.read_text(errors="replace") if SERIAL.is_file() else ""
    raise AssertionError(f"serial gate failed (need {needles!r}).\n---\n{text}\n---")


def _boot(image: Path, needles: tuple[str, ...], timeout: float = 120.0) -> str:
    SERIAL.write_text("")
    proc = subprocess.Popen(
        launcher_argv(image, "--quiet", "--headless", "--serial-log", SERIAL),
        cwd=str(ROOT / "emulator" / "k8086"),
        env=_env(),
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    try:
        return _wait_serial(proc, needles, timeout)
    finally:
        terminate_emulator(proc)


def test_gzip_tools_on_image() -> None:
    raw = IMAGE.read_bytes()
    for name in ("GZIP.COM", "GUNZIP.COM"):
        assert fat12.find_directory_entry(raw, f"BIN\\{name}").size_bytes > 0


def test_gzip_e2e() -> None:
    with tempfile.NamedTemporaryFile(suffix=".img", delete=False) as tmp:
        image = Path(tmp.name)
    shutil.copyfile(IMAGE, image)
    try:
        text = _boot(image, (MARKER, "gzipped", "gunzipped"))
        assert "GZIP failed" not in text
        assert "GUNZIP failed" not in text
        raw = image.read_bytes()
        sample = fat12.read_file(raw, "TEST\\SAMPLE.TXT")
        out = fat12.read_file(raw, "OUT.TXT")
        assert out == sample, f"round-trip mismatch: {out!r} vs {sample!r}"
        pipe_out = fat12.read_file(raw, "PIPEOUT.TXT")
        assert pipe_out == sample, f"pipe round-trip mismatch: {pipe_out!r} vs {sample!r}"
        gz = fat12.read_file(raw, "SAMPLE.GZ")
        assert gzip.decompress(gz) == sample
    finally:
        unlink_retry(image)


if __name__ == "__main__":
    test_gzip_tools_on_image()
    test_gzip_e2e()
    print("test_gzip_e2e: OK")
