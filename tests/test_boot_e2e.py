"""Headless E2E: boot os.img to an interactive COMMAND prompt."""

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
BANNER = "rmDOS 0.8"
PROMPT = "A:>"
IMAGE = BUILD / "os.img"


def test_fat12_image_layout() -> None:
    raw = IMAGE.read_bytes()
    assert len(raw) == fat12.TOTAL_SECTORS * fat12.SECTOR_SIZE
    assert raw[0x36:0x3E] == b"FAT12   "
    assert raw[3:11] == b"RMDOS   "
    entry = fat12.find_directory_entry(raw, "KERNEL.SYS")
    assert entry.size_bytes > 0
    lba, sectors = fat12.read_loader_info(raw)
    assert lba == fat12.cluster_to_sector(entry.start_cluster)
    assert sectors == fat12.sectors_for_size(entry.size_bytes)
    for name in (
        "COMMAND.COM",
        "INSTALL.BAT",
        "BIN\\DIR.COM",
        "BIN\\TYPE.COM",
        "BIN\\COPY.COM",
        "BIN\\DEL.COM",
        "BIN\\ATTRIB.COM",
        "BIN\\LABEL.COM",
        "BIN\\MOVE.COM",
        "BIN\\XCOPY.COM",
        "BIN\\CHKDSK.COM",
        "BIN\\SYS.COM",
        "BIN\\PARTEDIT.COM",
        "BIN\\FORMAT.COM",
        "BIN\\FIND.COM",
        "BIN\\CHOICE.COM",
        "BIN\\MORE.COM",
        "BIN\\MEM.COM",
        "BIN\\FC.COM",
        "BIN\\TREE.COM",
        "BIN\\SORT.COM",
        "BIN\\PING.COM",
        "BIN\\DHCP.COM",
        "BIN\\GZIP.COM",
        "BIN\\GUNZIP.COM",
        "DEMO\\HELLO.COM",
        "DEMO\\HELLO.EXE",
        "DEMO\\COMPAT.COM",
        "DEMO\\STAR.COM",
        "TEST\\SAMPLE.TXT",
    ):
        ent = fat12.find_directory_entry(raw, name)
        assert ent.size_bytes > 0, name


def test_boot_to_prompt() -> None:
    env = os.environ.copy()
    env["K8086_U18_ROM"] = str(BUILD / "u18.bin")
    env["K8086_U19_ROM"] = str(BUILD / "u19.bin")

    # Emulator persists disk writes; use a temp copy so os.img stays pristine.
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
                "--cga-expect",
                PROMPT,
                floppy_int13_shim=False,
            ),
            cwd=str(ROOT / "emulator" / "k8086"),
            env=env,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        try:
            deadline = time.time() + 30
            while time.time() < deadline:
                if SERIAL.is_file():
                    text = SERIAL.read_text(errors="replace")
                    if BANNER in text and PROMPT in text:
                        return
                if proc.poll() is not None:
                    break
                time.sleep(0.1)
            text = SERIAL.read_text(errors="replace") if SERIAL.is_file() else ""
            raise AssertionError(
                f"banner/prompt not seen in serial log.\n---\n{text[-2000:]}\n---"
            )
        finally:
            terminate_emulator(proc)
    finally:
        unlink_retry(tmp_path)


if __name__ == "__main__":
    test_fat12_image_layout()
    test_boot_to_prompt()
    print("test_boot_e2e: OK")
