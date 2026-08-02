"""E2E: SYS copies rmDOS system files and installs boot metadata."""

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
SERIAL = BUILD / "serial-sys.log"
HD_10M = 306 * 4 * 17 * 512


def _env() -> dict[str, str]:
    env = os.environ.copy()
    env["K8086_U18_ROM"] = str(BUILD / "u18.bin")
    env["K8086_U19_ROM"] = str(BUILD / "u19.bin")
    return env


def _run(floppy: Path, hd: Path | str, marker: str, timeout: float = 120) -> str:
    SERIAL.write_text("")
    proc = subprocess.Popen(
        launcher_argv(
            floppy,
            hd,
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
        deadline = time.time() + timeout
        while time.time() < deadline:
            text = SERIAL.read_text(errors="replace") if SERIAL.is_file() else ""
            if marker in text:
                return text
            if proc.poll() is not None:
                break
            time.sleep(0.25)
        raise AssertionError(SERIAL.read_text(errors="replace"))
    finally:
        terminate_emulator(proc)


def _make_source(path: Path) -> None:
    subprocess.run(
        [
            sys.executable,
            "-m",
            "scripts.mkfs_fat12",
            "--output",
            str(path),
            "--boot",
            str(BUILD / "boot.bin"),
            "--kernel",
            str(BUILD / "kernel.bin"),
            "--file",
            f"COMMAND.COM={BUILD / 'command.com'}",
            "--file",
            f"BIN/FORMAT.COM={BUILD / 'format.com'}",
            "--file",
            f"BIN/PARTEDIT.COM={BUILD / 'partedit.com'}",
            "--file",
            f"BIN/SYS.COM={BUILD / 'sys.com'}",
            "--file",
            f"AUTOEXEC.BAT={ROOT / 'fixtures/guest/AUTOEXEC.SYS.BAT'}",
        ],
        cwd=ROOT,
        check=True,
    )


def _read_root_file(volume: bytes, root: bytes, entry_off: int) -> bytes:
    spc = volume[13]
    reserved = struct.unpack_from("<H", volume, 14)[0]
    fats = volume[16]
    root_entries = struct.unpack_from("<H", volume, 17)[0]
    spf = struct.unpack_from("<H", volume, 22)[0]
    root_secs = (root_entries * 32 + 511) // 512
    data_lba = reserved + fats * spf + root_secs
    fat = volume[reserved * 512 : (reserved + spf) * 512]
    cluster = struct.unpack_from("<H", root, entry_off + 26)[0]
    size = struct.unpack_from("<I", root, entry_off + 28)[0]
    clusters = (_bpb_total(volume) - data_lba) // spc
    fat16 = clusters >= 4085

    out = bytearray()
    while len(out) < size:
        assert cluster >= 2
        lba = data_lba + (cluster - 2) * spc
        out.extend(volume[lba * 512 : (lba + spc) * 512])
        if fat16:
            cluster = struct.unpack_from("<H", fat, cluster * 2)[0]
        else:
            off = cluster + cluster // 2
            value = fat[off] | (fat[off + 1] << 8)
            cluster = (value >> 4) & 0x0FFF if cluster & 1 else value & 0x0FFF
    return bytes(out[:size])


def _bpb_total(volume: bytes) -> int:
    total = struct.unpack_from("<H", volume, 19)[0]
    return total or struct.unpack_from("<I", volume, 32)[0]


def test_sys_copy_and_boot() -> None:
    with tempfile.NamedTemporaryFile(suffix=".img", delete=False) as fd_tmp:
        floppy = Path(fd_tmp.name)
    with tempfile.NamedTemporaryFile(suffix=".hd.img", delete=False) as hd_tmp:
        hd = Path(hd_tmp.name)
    try:
        _make_source(floppy)
        with hd.open("r+b") as f:
            f.truncate(HD_10M)
        text = _run(floppy, hd, "SYS COPY OK")
        raw = hd.read_bytes()
        part = raw[0x1BE : 0x1CE]
        base = struct.unpack_from("<I", part, 8)[0]
        volume = raw[base * 512 :]
        assert "SYS OK" in text, (text, base, volume[:64])
        assert volume[512:517] == b"RFAT1"

        reserved = struct.unpack_from("<H", volume, 14)[0]
        fats = volume[16]
        root_entries = struct.unpack_from("<H", volume, 17)[0]
        spf = struct.unpack_from("<H", volume, 22)[0]
        root_start = (reserved + fats * spf) * 512
        root = volume[root_start : root_start + root_entries * 32]
        kernel_off = root.index(b"KERNEL  SYS")
        command_off = root.index(b"COMMAND COM")
        assert _read_root_file(volume, root, kernel_off) == (BUILD / "kernel.bin").read_bytes()
        assert _read_root_file(volume, root, command_off) == (BUILD / "command.com").read_bytes()

        cluster = struct.unpack_from("<H", root, kernel_off + 26)[0]
        size = struct.unpack_from("<I", root, kernel_off + 28)[0]
        root_secs = (root_entries * 32 + 511) // 512
        data_lba = reserved + fats * spf + root_secs
        expect_lba = data_lba + (cluster - 2) * volume[13]
        rfat_lba = struct.unpack_from("<H", volume, 512 + 0x1C)[0]
        rfat_secs = struct.unpack_from("<H", volume, 512 + 0x1E)[0]
        assert rfat_lba == expect_lba
        assert rfat_secs == (size + 511) // 512

        text = _run(floppy, "@" + str(hd), "rmDOS 0.8", timeout=60)
        assert "rmDOS 0.8" in text
    finally:
        unlink_retry(floppy)
        unlink_retry(hd)


if __name__ == "__main__":
    test_sys_copy_and_boot()
    print("test_sys_e2e: OK")
