"""E2E smoke: Wolfenstein 3D on 80286 + VGA HD under streaming EXEC (optional).

Skips when fixtures/wolf3d/WOLF3D.EXE is absent (not redistributed).
With the binary present: pack lean boot floppy + ~10MB HD with the shareware
drop-in, boot headless/turbo on 80286 with the VGA ISA card, require that
AH=4Bh does not fail (no 'Bad command'), the guest stays in the game, and a
bounded instruction smoke shows Mode Y with non-black pixels and guest IRQ0.
"""

from __future__ import annotations

import os
import re
import shutil
import struct
import subprocess
import tempfile
import time
from pathlib import Path

from tests.k8086_util import launcher_argv, terminate_emulator, unlink_retry

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "firmware" / "build"
SERIAL = BUILD / "serial-wolf3d.log"
FLOPPY = BUILD / "os-wolf3d.img"
HD = BUILD / "hd-wolf3d.img"
WOLF3D = ROOT / "fixtures" / "wolf3d" / "WOLF3D.EXE"
K8086 = ROOT / "emulator" / "k8086"
VGA_JAR_GLOB = "cards/vga/build/libs/vga-*.jar"
MIN_EXE = 100_000
# Sign-on MungePic + blit + SD_Startup needs tens of millions of guest insns.
VISIBLE_MAX_INSTRUCTIONS = "80000000"


def _gradlew_argv(*tasks: str) -> list[str]:
    if os.name == "nt":
        return ["cmd.exe", "/c", "gradlew.bat", *tasks]
    return ["./gradlew", *tasks]


def _ensure_vga_jar() -> Path:
    subprocess.check_call(
        _gradlew_argv(":cards:vga:jar", ":k8086-emulator:installDist", "-q"),
        cwd=str(K8086),
    )
    jars = sorted(K8086.glob(VGA_JAR_GLOB))
    if not jars:
        raise SystemExit("vga JAR missing after gradle build")
    return jars[-1]


def _root_file_size(img: bytes, name: str) -> int:
    """Return size of an 8.3 root entry on the first primary FAT volume."""
    assert img[510:512] == b"\x55\xaa"
    part = img[0x1BE : 0x1CE]
    assert part[4] in (0x01, 0x04, 0x06), part[4]
    part_start = struct.unpack_from("<I", part, 8)[0]
    vbr = img[part_start * 512 : (part_start + 1) * 512]
    reserved = struct.unpack_from("<H", vbr, 14)[0]
    fats = vbr[16]
    spf = struct.unpack_from("<H", vbr, 22)[0]
    root_ents = struct.unpack_from("<H", vbr, 17)[0]
    root_lba = part_start + reserved + fats * spf
    root = img[root_lba * 512 : root_lba * 512 + root_ents * 32]
    stem, _, suf = name.upper().partition(".")
    target = (stem.ljust(8) + suf.ljust(3)).encode("ascii")
    for i in range(root_ents):
        ent = root[i * 32 : (i + 1) * 32]
        if ent[0] in (0x00, 0xE5):
            break
        if ent[0:11] == target:
            return struct.unpack_from("<I", ent, 28)[0]
    raise FileNotFoundError(name)


def test_wolf3d_on_hd() -> None:
    if not WOLF3D.is_file():
        print("test_wolf3d_on_hd: SKIP (no WOLF3D.EXE)")
        return
    assert FLOPPY.is_file(), "run make test-wolf3d first"
    assert HD.is_file(), "run make test-wolf3d first"
    raw = HD.read_bytes()
    assert raw[510:512] == b"\x55\xaa"
    part = raw[0x1BE : 0x1CE]
    assert part[0] == 0x80
    assert part[4] in (0x01, 0x04, 0x06)
    start = struct.unpack_from("<I", part, 8)[0]
    vbr = raw[start * 512 : (start + 1) * 512]
    assert vbr[510:512] == b"\x55\xaa"
    assert vbr[54:62] in (b"FAT12   ", b"FAT16   ")
    assert _root_file_size(raw, "WOLF3D.EXE") >= MIN_EXE
    assert _root_file_size(raw, "VSWAP.WL1") > 100_000


def test_wolf3d_loads_e2e() -> None:
    if not WOLF3D.is_file():
        print("test_wolf3d_loads_e2e: SKIP (no WOLF3D.EXE)")
        return

    env = os.environ.copy()
    env["K8086_U18_ROM"] = str(BUILD / "u18.bin")
    env["K8086_U19_ROM"] = str(BUILD / "u19.bin")

    jar = _ensure_vga_jar()
    card_rel = jar.relative_to(K8086).as_posix() + ",window=false"

    SERIAL.write_text("")
    with tempfile.NamedTemporaryFile(suffix=".img", delete=False) as tmp:
        floppy = Path(tmp.name)
    with tempfile.NamedTemporaryFile(suffix=".hd.img", delete=False) as tmp:
        hd = Path(tmp.name)
    shutil.copyfile(FLOPPY, floppy)
    shutil.copyfile(HD, hd)
    try:
        proc = subprocess.Popen(
            launcher_argv(
                floppy,
                hd,
                "--quiet",
                "--headless",
                "--turbo",
                "--cpu",
                "80286",
                "--mhz",
                "10",
                "--no-cga",
                "--initial-video",
                "special",
                "--card",
                card_rel,
                "--serial-log",
                SERIAL,
                floppy_int13_shim=False,
            ),
            cwd=str(K8086),
            env=env,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        try:
            # Boot floppy, mount C:, EXEC Wolf3D (may sit in VGA loop).
            time.sleep(14)
            text = SERIAL.read_text(errors="replace") if SERIAL.is_file() else ""
            assert "Bad command" not in text, text[-500:]
            # Still in game: no interactive prompt after AUTOEXEC C: / WOLF3D.
            assert "A:>" not in text, (
                "dropped to A: prompt — Wolf3D EXEC likely failed\n" + text[-500:]
            )
            assert "C:>" not in text, (
                "dropped to C: prompt — Wolf3D EXEC likely failed\n" + text[-500:]
            )
            assert proc.poll() is None, "emulator exited early"
        finally:
            terminate_emulator(proc)
    finally:
        unlink_retry(floppy)
        unlink_retry(hd)


def test_wolf3d_visible_signon() -> None:
    """Bounded run: Mode Y sign-on has lit pixels; guest owns IRQ0 (no host TimeCount)."""
    if not WOLF3D.is_file():
        print("test_wolf3d_visible_signon: SKIP (no WOLF3D.EXE)")
        return

    env = os.environ.copy()
    env["K8086_U18_ROM"] = str(BUILD / "u18.bin")
    env["K8086_U19_ROM"] = str(BUILD / "u19.bin")
    env["K8086_CARD_SNAPSHOT"] = "1"

    jar = _ensure_vga_jar()
    card_rel = jar.relative_to(K8086).as_posix() + ",window=false"
    serial = BUILD / "serial-wolf3d-visible.log"
    serial.write_text("")

    with tempfile.NamedTemporaryFile(suffix=".img", delete=False) as tmp:
        floppy = Path(tmp.name)
    with tempfile.NamedTemporaryFile(suffix=".hd.img", delete=False) as tmp:
        hd = Path(tmp.name)
    shutil.copyfile(FLOPPY, floppy)
    shutil.copyfile(HD, hd)
    try:
        proc = subprocess.run(
            launcher_argv(
                floppy,
                hd,
                "--quiet",
                "--headless",
                "--turbo",
                "--cpu",
                "80286",
                "--mhz",
                "10",
                "--no-cga",
                "--initial-video",
                "special",
                "--card",
                card_rel,
                "--max-instructions",
                VISIBLE_MAX_INSTRUCTIONS,
                "--serial-log",
                serial,
                floppy_int13_shim=False,
            ),
            cwd=str(K8086),
            env=env,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
            text=True,
            timeout=120,
            check=False,
        )
        err = proc.stderr or ""
        snap_lines = [ln for ln in err.splitlines() if ln.startswith("CARD_SNAPSHOT")]
        assert snap_lines, "missing CARD_SNAPSHOT (set K8086_CARD_SNAPSHOT=1)\n" + err[-800:]
        snap = snap_lines[-1]
        lit = re.search(r"litPixels=(\d+)", snap)
        nz = re.search(r"nzVRAM=(\d+)", snap)
        start = re.search(r"start=(\d+)", snap)
        ivt = re.search(r"IVT8=([0-9A-Fa-f]+):", snap)
        assert lit and int(lit.group(1)) > 0, snap
        assert nz and int(nz.group(1)) > 0, snap
        assert start and int(start.group(1)) != 32768, "CRTC stuck on empty page 0x8000\n" + snap
        assert ivt and int(ivt.group(1), 16) < 0xF000, "IRQ0 still BIOS\n" + snap
        text = serial.read_text(errors="replace") if serial.is_file() else ""
        assert "Bad command" not in text, text[-500:]
    finally:
        unlink_retry(floppy)
        unlink_retry(hd)


if __name__ == "__main__":
    if not WOLF3D.is_file():
        print("test_wolf3d_e2e: SKIP (no WOLF3D.EXE)")
    else:
        test_wolf3d_on_hd()
        test_wolf3d_loads_e2e()
        test_wolf3d_visible_signon()
        print("test_wolf3d_e2e: OK")
