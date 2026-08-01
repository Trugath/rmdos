"""E2E: two primary partitions get C: and D: letters."""

from __future__ import annotations

import os
import shutil
import struct
import subprocess
import tempfile
import time
from pathlib import Path

from tests.k8086_util import launcher_argv, terminate_emulator, unlink_retry

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "firmware" / "build"
FLOPPY = BUILD / "os-multilet-hd.img"
SERIAL = BUILD / "serial-multilet-hd.log"
HD_SIZE = 306 * 4 * 17 * 512


def test_two_primaries_c_and_d() -> None:
    env = os.environ.copy()
    env["K8086_U18_ROM"] = str(BUILD / "u18.bin")
    env["K8086_U19_ROM"] = str(BUILD / "u19.bin")
    SERIAL.write_text("")
    with tempfile.NamedTemporaryFile(suffix=".img", delete=False) as tmp_fd:
        floppy = Path(tmp_fd.name)
    with tempfile.NamedTemporaryFile(suffix=".hd.img", delete=False) as tmp_hd:
        hd = Path(tmp_hd.name)
    shutil.copyfile(FLOPPY, floppy)
    hd.write_bytes(b"")
    with hd.open("r+b") as f:
        f.truncate(HD_SIZE)
    try:
        proc = subprocess.Popen(
            launcher_argv(floppy, hd, "--quiet", "--headless", "--serial-log", SERIAL, floppy_int13_shim=False),
            cwd=str(ROOT / "emulator" / "k8086"),
            env=env,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        try:
            deadline = time.time() + 300
            text = ""
            while time.time() < deadline:
                text = SERIAL.read_text(errors="replace") if SERIAL.is_file() else ""
                if "MULTILET OK" in text:
                    break
                if proc.poll() is not None:
                    break
                time.sleep(0.25)
            else:
                raise AssertionError(f"multilet timed out:\n{text}")
            assert "MULTILET OK" in text, text
            assert "PARTEDIT OK" in text, text
            assert "Format complete" in text, text
        finally:
            terminate_emulator(proc)
        image = hd.read_bytes()
        assert image[510:512] == b"\x55\xaa"
        p1 = image[0x1BE : 0x1CE]
        p2 = image[0x1CE : 0x1DE]
        assert p1[4] in (0x01, 0x04, 0x06)
        assert p2[4] in (0x01, 0x04, 0x06)
        s1, n1 = struct.unpack_from("<II", p1, 8)
        s2, n2 = struct.unpack_from("<II", p2, 8)
        assert s1 == 17 and n1 == 8000
        assert s2 == 17 + 8000 and n2 > 0
        v1 = image[s1 * 512 : (s1 + 1) * 512]
        v2 = image[s2 * 512 : (s2 + 1) * 512]
        assert v1[510:512] == b"\x55\xaa"
        assert v2[510:512] == b"\x55\xaa"
        assert struct.unpack_from("<H", v1, 11)[0] == 512
        assert struct.unpack_from("<H", v2, 11)[0] == 512
    finally:
        unlink_retry(floppy)
        unlink_retry(hd)


if __name__ == "__main__":
    assert FLOPPY.is_file(), f"missing {FLOPPY}; build it first"
    test_two_primaries_c_and_d()
    print("test_multilet_hd_e2e: OK")
