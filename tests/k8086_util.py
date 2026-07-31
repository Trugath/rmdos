"""Resolve the Gradle installDist launcher across POSIX and Windows hosts."""

from __future__ import annotations

import os
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
_BIN = (
    ROOT
    / "emulator"
    / "k8086"
    / "k8086-emulator"
    / "build"
    / "install"
    / "k8086-emulator"
    / "bin"
)


def launcher_argv(*args: str | Path) -> list[str]:
    """Return a subprocess argv that starts k8086-emulator with *args."""
    posix = _BIN / "k8086-emulator"
    bat = _BIN / "k8086-emulator.bat"
    extra = [str(a) for a in args]

    if os.name == "nt":
        if bat.is_file():
            # cmd.exe re-parses the line; quote anything with shell metacharacters.
            def _cmd_quote(s: str) -> str:
                if not s or any(c in s for c in ' \t&<>()^|%"'):
                    return '"' + s.replace('"', '\\"') + '"'
                return s

            return ["cmd.exe", "/c", str(bat), *(_cmd_quote(a) for a in extra)]
        if posix.is_file():
            return ["bash", str(posix), *extra]
        raise SystemExit("k8086 launcher missing; run ./setup.sh first")

    if not posix.is_file():
        raise SystemExit("k8086 launcher missing; run ./setup.sh first")
    return [str(posix), *extra]


def terminate_emulator(proc: subprocess.Popen) -> None:
    """Stop the emulator, including Windows child Java processes."""
    if proc.poll() is not None:
        return
    if os.name == "nt" and proc.pid:
        subprocess.run(
            ["taskkill", "/F", "/T", "/PID", str(proc.pid)],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=False,
        )
    else:
        proc.terminate()
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            pass


def unlink_retry(path: Path, attempts: int = 20) -> None:
    """Unlink a path, retrying briefly while Windows releases file locks."""
    for i in range(attempts):
        try:
            path.unlink(missing_ok=True)
            return
        except PermissionError:
            if i + 1 >= attempts:
                raise
            time.sleep(0.1)
