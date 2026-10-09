"""E2E: CONFIG.SYS INSTALL of large COMs must return to shell (teardown).

Historically, COMs at/above ~6609 bytes hung after child exit because
com_resume_kernel freed on kernel_stack_top and smashed INSTALL's parent frame.
"""

from __future__ import annotations

import os
import shutil
import subprocess
import tempfile
import time
from pathlib import Path

from tests.k8086_util import launcher_argv, terminate_emulator, unlink_retry

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "firmware" / "build"
SERIAL = BUILD / "serial-install-large.log"

# Cliff sizes from the original bisect (941 vs 942 image paragraphs) plus a
# larger MODE-class size.
PAD_SIZES = (6609, 7237)

TINY_C = """\
#include "dos.h"
static char msg[12] = "TINY OK\\r\\n$";
int main(void)
{
    print_dollar(msg);
    return 0;
}
"""


def _env() -> dict[str, str]:
    env = os.environ.copy()
    env["K8086_U18_ROM"] = str(BUILD / "u18.bin")
    env["K8086_U19_ROM"] = str(BUILD / "u19.bin")
    return env


def _build_tiny_com(dest: Path) -> bytes:
    """Compile a minimal COM that prints TINY OK."""
    with tempfile.TemporaryDirectory() as td:
        tdir = Path(td)
        src = tdir / "tiny.c"
        asm = tdir / "tiny.s"
        obj = tdir / "tiny.o"
        elf = tdir / "tiny.elf"
        com = tdir / "tiny.com"
        src.write_text(TINY_C)
        subprocess.check_call(
            [
                "python3",
                "-m",
                "scripts.rmcc",
                str(src),
                "-o",
                str(asm),
                "--com",
                "-I",
                str(ROOT / "firmware" / "src" / "main" / "dos" / "inc"),
            ],
            cwd=str(ROOT),
        )
        subprocess.check_call(
            [str(ROOT / "scripts" / "as8086.sh"), "--32", "-o", str(obj), str(asm)],
            cwd=str(ROOT),
        )
        ld = [
            "ld",
            "-m",
            "elf_i386",
            "-T",
            str(ROOT / "firmware" / "linker" / "com.ld"),
            "--gc-sections",
            "-o",
            str(elf),
            str(obj),
            str(BUILD / "dos_rt.o"),
        ]
        subprocess.check_call(ld, cwd=str(ROOT))
        subprocess.check_call(["objcopy", "-O", "binary", str(elf), str(com)])
        data = com.read_bytes()
    dest.write_bytes(data)
    return data


def _pad_com(base: bytes, size: int) -> bytes:
    if len(base) > size:
        raise AssertionError(f"tiny COM is {len(base)} bytes; cannot pad to {size}")
    return base + b"\0" * (size - len(base))


def _run_install(com_bytes: bytes, size: int) -> str:
    with tempfile.TemporaryDirectory() as td:
        tdir = Path(td)
        com_path = tdir / "TINY.COM"
        com_path.write_bytes(com_bytes)
        cfg = tdir / "CONFIG.SYS"
        cfg.write_text("SHELL=A:\\COMMAND.COM /P\r\nINSTALL=A:\\BIN\\TINY.COM\r\n")
        auto = tdir / "AUTOEXEC.BAT"
        auto.write_text("ECHO LARGEOK\r\nEXIT\r\n")
        img = tdir / "os-il.img"
        subprocess.check_call(
            [
                "python3",
                "-m",
                "scripts.mkfs_fat12",
                "--output",
                str(img),
                "--boot",
                str(BUILD / "boot.bin"),
                "--kernel",
                str(BUILD / "kernel.bin"),
                "--file",
                f"COMMAND.COM={BUILD / 'command.com'}",
                "--file",
                f"BIN/TINY.COM={com_path}",
                "--file",
                f"CONFIG.SYS={cfg}",
                "--file",
                f"AUTOEXEC.BAT={auto}",
            ],
            cwd=str(ROOT),
            stdout=subprocess.DEVNULL,
        )
        SERIAL.write_text("")
        with tempfile.NamedTemporaryFile(suffix=".img", delete=False) as tmp:
            tmp_path = Path(tmp.name)
        shutil.copyfile(img, tmp_path)
        try:
            proc = subprocess.Popen(
                launcher_argv(
                    tmp_path, "--quiet", "--headless", "--serial-log", SERIAL
                ),
                cwd=str(ROOT / "emulator" / "k8086"),
                env=_env(),
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
            try:
                deadline = time.time() + 30
                text = ""
                while time.time() < deadline:
                    text = SERIAL.read_text(errors="replace") if SERIAL.is_file() else ""
                    if "TINY OK" in text and "LARGEOK" in text:
                        return text
                    if proc.poll() is not None:
                        break
                    time.sleep(0.2)
                raise AssertionError(
                    f"INSTALL large COM ({size} bytes) failed.\n---\n{text}\n---"
                )
            finally:
                terminate_emulator(proc)
        finally:
            unlink_retry(tmp_path)


def test_install_large_com_e2e() -> None:
    assert (BUILD / "kernel.bin").is_file()
    assert (BUILD / "command.com").is_file()
    assert (BUILD / "dos_rt.o").is_file()
    base = _build_tiny_com(BUILD / "tiny-install.com")
    for size in PAD_SIZES:
        text = _run_install(_pad_com(base, size), size)
        assert "TINY OK" in text
        assert "LARGEOK" in text
        assert "\x00" not in text.split("TINY OK", 1)[-1][:200]


if __name__ == "__main__":
    test_install_large_com_e2e()
    print("test_install_large_com_e2e: OK")
