"""Headless boot of k8086 disks/fd.img (rmDOS) on rmDOS ROMs."""

from __future__ import annotations

import os
import subprocess
from pathlib import Path

from tests.k8086_util import launcher_argv

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "firmware" / "build"
FD_IMG = ROOT / "emulator" / "k8086" / "disks" / "fd.img"

# Match BootIntegrationTest budget (80M steps).
MAX_INSTRUCTIONS = "80000000"


def test_fd_img_prompt() -> None:
    if not FD_IMG.is_file():
        raise SystemExit(f"rmDOS floppy missing: {FD_IMG}")
    u18 = BUILD / "u18.bin"
    u19 = BUILD / "u19.bin"
    if not u18.is_file() or not u19.is_file():
        raise SystemExit("rmDOS ROMs missing; run make bios")

    env = os.environ.copy()
    env["K8086_U18_ROM"] = str(u18)
    env["K8086_U19_ROM"] = str(u19)

    # Force a screen dump via a never-match expect, then assert boot markers.
    proc = subprocess.run(
        launcher_argv(
            FD_IMG,
            "--quiet",
            "--headless",
            "--cga-expect",
            "___NEVER_MATCH_RMDOS___",
            "--max-instructions",
            MAX_INSTRUCTIONS,
        ),
        cwd=str(ROOT / "emulator" / "k8086"),
        env=env,
        capture_output=True,
        text=True,
        timeout=600,
    )
    screen = proc.stderr or ""
    missing = [m for m in ("rmDOS", "A:>") if m not in screen]
    if missing:
        raise AssertionError(
            f"rmDOS fd.img boot incomplete; missing {missing} (exit={proc.returncode}).\n"
            f"--- screen ---\n{screen[-4000:]}\n"
        )


if __name__ == "__main__":
    test_fd_img_prompt()
    print("test_fd_img_e2e: OK")
