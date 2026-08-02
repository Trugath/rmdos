"""E2E: malformed BPBs fail mount cleanly with 'fat fail' (no divide trap)."""

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
IMAGE = BUILD / "os.img"
SERIAL = BUILD / "serial-bpb-bad.log"
FAT_FAIL = "fat fail"
BANNER = "rmDOS 0.8"


def _boot(img: Path, timeout: float = 45.0) -> str:
    env = os.environ.copy()
    env["K8086_U18_ROM"] = str(BUILD / "u18.bin")
    env["K8086_U19_ROM"] = str(BUILD / "u19.bin")
    SERIAL.write_text("")
    with tempfile.NamedTemporaryFile(suffix=".img", delete=False) as tmp:
        floppy = Path(tmp.name)
    shutil.copyfile(img, floppy)
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
            deadline = time.time() + timeout
            text = ""
            while time.time() < deadline:
                text = SERIAL.read_text(errors="replace") if SERIAL.is_file() else ""
                if FAT_FAIL in text or (BANNER in text and "A:>" in text):
                    break
                if proc.poll() is not None:
                    break
                time.sleep(0.2)
            return SERIAL.read_text(errors="replace") if SERIAL.is_file() else text
        finally:
            terminate_emulator(proc)
    finally:
        unlink_retry(floppy)


def _patch_vbr(mutator) -> Path:
    raw = bytearray(IMAGE.read_bytes())
    mutator(raw)
    path = BUILD / "os-bpb-tmp.img"
    path.write_bytes(raw)
    return path


def _expect_fat_fail(label: str, mutator) -> None:
    img = _patch_vbr(mutator)
    try:
        text = _boot(img)
        assert FAT_FAIL in text, f"{label}: expected {FAT_FAIL!r}.\n---\n{text}\n---"
        assert "A:>" not in text, f"{label}: prompt should not appear.\n---\n{text}\n---"
    finally:
        unlink_retry(img)


def test_bpb_spc_zero() -> None:
    def mut(raw: bytearray) -> None:
        raw[13] = 0

    _expect_fat_fail("spc=0", mut)


def test_bpb_spc_not_pow2() -> None:
    def mut(raw: bytearray) -> None:
        raw[13] = 3

    _expect_fat_fail("spc=3", mut)


def test_bpb_reserved_zero() -> None:
    def mut(raw: bytearray) -> None:
        struct.pack_into("<H", raw, 14, 0)

    _expect_fat_fail("reserved=0", mut)


def test_bpb_fats_zero() -> None:
    def mut(raw: bytearray) -> None:
        raw[16] = 0

    _expect_fat_fail("fats=0", mut)


def test_bpb_root_ents_too_small() -> None:
    def mut(raw: bytearray) -> None:
        # <16 → root_secs becomes 0 after /16
        struct.pack_into("<H", raw, 17, 8)

    _expect_fat_fail("root_ents=8", mut)


def test_bpb_spf_zero() -> None:
    def mut(raw: bytearray) -> None:
        struct.pack_into("<H", raw, 22, 0)

    _expect_fat_fail("spf=0", mut)


def test_bpb_totsec_zero() -> None:
    def mut(raw: bytearray) -> None:
        struct.pack_into("<H", raw, 19, 0)
        struct.pack_into("<I", raw, 32, 0)

    _expect_fat_fail("totsec=0", mut)


def test_bpb_no_data_region() -> None:
    def mut(raw: bytearray) -> None:
        # Keep geometry fields; shrink TotSec16 below reserved+FATs+root.
        struct.pack_into("<H", raw, 19, 4)
        struct.pack_into("<I", raw, 32, 0)

    _expect_fat_fail("totsec < data_lba", mut)


def test_bpb_good_still_boots() -> None:
    text = _boot(IMAGE)
    assert BANNER in text and "A:>" in text, f"control boot failed.\n---\n{text}\n---"
    assert FAT_FAIL not in text


if __name__ == "__main__":
    assert IMAGE.is_file(), f"missing {IMAGE}; build os.img first"
    test_bpb_spc_zero()
    test_bpb_spc_not_pow2()
    test_bpb_reserved_zero()
    test_bpb_fats_zero()
    test_bpb_root_ents_too_small()
    test_bpb_spf_zero()
    test_bpb_totsec_zero()
    test_bpb_no_data_region()
    test_bpb_good_still_boots()
    print("test_bpb_mount_e2e: OK")
