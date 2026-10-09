"""E2E: COMMAND.COM DIR lists Directory of / <DIR> / bytes free.

Also gates DIR /O with 90 files under MANY\\ (packed reverse of name order)
so sorted output still includes entries past the old 80-slot cap.
"""

from __future__ import annotations

import os
import re
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
IMAGE = BUILD / "os-dir.img"
MARKER = "DIR OK"
DIRWP = "DIRWP OK"
DIRO = "DIRO OK"
DIRO90 = "DIRO90 OK"


def test_dir_on_image() -> None:
    raw = IMAGE.read_bytes()
    ae = fat12.find_directory_entry(raw, "AUTOEXEC.BAT")
    assert ae.size_bytes >= 4
    assert fat12.find_directory_entry(raw, "BIN\\DIR.COM").size_bytes > 0
    many = fat12.find_directory_entry(raw, "MANY")
    assert many.attributes & 0x10, "MANY should be a directory"
    entries = fat12.list_subdir_entries(raw, many.start_cluster)
    files = [e.name for e in entries if e.name not in (".", "..") and not (e.attributes & 0x10)]
    assert len(files) >= 90, f"expected >=90 MANY files, got {len(files)}"
    assert "F00.TXT" in files and "F89.TXT" in files


def test_dir_e2e() -> None:
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
        try:
            deadline = time.time() + 120
            text = ""
            while time.time() < deadline:
                if SERIAL.is_file():
                    text = SERIAL.read_text(errors="replace")
                    if (
                        MARKER in text
                        and DIRWP in text
                        and DIRO90 in text
                        and DIRO in text
                        and "Directory of" in text
                        and "<DIR>" in text
                        and "bytes free" in text
                    ):
                        _assert_many_sorted(text)
                        return
                if proc.poll() is not None:
                    break
                time.sleep(0.25)
            text = SERIAL.read_text(errors="replace") if SERIAL.is_file() else ""
            raise AssertionError(
                f"DIR gate failed (need {MARKER!r}, {DIRWP!r}, {DIRO90!r}, {DIRO!r}, "
                f"Directory of, <DIR>, bytes free).\n---\n{text}\n---"
            )
        finally:
            terminate_emulator(proc)
    finally:
        unlink_retry(tmp_path)


def _assert_many_sorted(text: str) -> None:
    """MANY /O:N must list F00 before F89 and still include F85 (past old cap)."""
    end = text.find(DIRO90)
    assert end > 0, "DIRO90 OK missing"
    # Slice from the MANY directory header before DIRO90.
    chunk = text[:end]
    start = chunk.rfind("MANY")
    assert start >= 0, "MANY listing header missing before DIRO90"
    section = chunk[start:]
    # Filenames appear as ASCIZ stem+ext from DTA (e.g. F00.TXT).
    names = re.findall(r"\bF(\d{2})\.TXT\b", section, flags=re.IGNORECASE)
    assert "00" in names and "89" in names and "85" in names, (
        f"MANY /O missing expected names (got {names[:5]}…{names[-5:]}).\n{section}"
    )
    assert names.index("00") < names.index("89"), (
        f"MANY /O:N not name-sorted: first F00 at {names.index('00')}, "
        f"F89 at {names.index('89')}"
    )
    assert names.index("00") < names.index("85") < names.index("89")


if __name__ == "__main__":
    test_dir_on_image()
    test_dir_e2e()
    print("test_dir_e2e: OK")
