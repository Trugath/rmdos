"""Headless BIOS service unit tests via boot-sector images."""

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
BIOS_TESTS = BUILD / "bios_tests"
SERIAL_DIR = BIOS_TESTS / "serial"

# These tests write the floppy image; run from a temp copy so the pristine
# build artifact (especially the boot sector) is not persisted-over.
DESTRUCTIVE_DISK_TESTS = frozenset({
    "bt_fdc_rw",
    "bt_fdc_fmt",
    "bt_hd_rw",
    "bt_hd_verify",
    "bt_int19_hd",
})

# Attach a blank XT ~10MB HD so C800 Fixed Disk ROM is exercised (DL=80).
HD_BIOS_TESTS = frozenset({
    "bt_hd_params",
    "bt_hd_rw",
    "bt_hd_verify",
    "bt_int13_err",
    "bt_hd_svc",
    "bt_hd_fmt",
    "bt_int19_hd",
})
HD_SIZE = 306 * 4 * 17 * 512

TESTS = [
    "bt_equip",
    "bt_bda",
    "bt_video",
    "bt_scroll",
    "bt_disk",
    "bt_disk144",
    "bt_disk120",
    "bt_disk360",
    "bt_disk_stat",
    "bt_disk_upgrade",
    "bt_timer",
    "bt_int1c",
    "bt_kbd_flags",
    "bt_kbd_ext",
    "bt_modes_text",
    "bt_modes_gfx",
    "bt_mode4",
    "bt_mode6",
    "bt_serial",
    "bt_int15",
    "bt_pixel",
    "bt_misc",
    "bt_ctype",
    "bt_gfx_scroll",
    "bt_pixel6",
    "bt_prtsc",
    "bt_ident",
    "bt_entry",
    "bt_cad",
    "bt_fdc_rw",
    "bt_fdc_fmt",
    "bt_fdc_type",
    "bt_page",
    "bt_palette",
    "bt_bel",
    "bt_int1a_set",
    "bt_hd_params",
    "bt_hd_rw",
    "bt_kbd_irq",
    "bt_kbd_prtsc",
    "bt_brk",
    "bt_int18",
    "bt_chgline",
    "bt_str",
    "bt_cfg",
    "bt_readchar",
    "bt_writech",
    "bt_kbd_read",
    "bt_kbd_shift",
    "bt_int13_err",
    "bt_hd_verify",
    "bt_motor",
    "bt_timer_of",
    "bt_hd_svc",
    "bt_hd_fmt",
    "bt_kbd_locks",
    "bt_kbd_full",
    "bt_int19_hd",
]


def _run_one(
    name: str,
    timeout_s: float = 20.0,
    *,
    floppy_int13_shim: bool = False,
    hd_int13_bios: bool | None = None,
) -> None:
    img = BIOS_TESTS / f"{name}.img"
    if not img.is_file():
        raise AssertionError(f"missing {img}; run make bios-tests")

    SERIAL_DIR.mkdir(parents=True, exist_ok=True)
    suffix = "_shim" if floppy_int13_shim else ""
    if hd_int13_bios:
        suffix = suffix + "_hosthd"
    serial = SERIAL_DIR / f"{name}{suffix}.log"
    serial.write_text("")

    env = os.environ.copy()
    env["K8086_U18_ROM"] = str(BUILD / "u18.bin")
    env["K8086_U19_ROM"] = str(BUILD / "u19.bin")

    tmp_path: Path | None = None
    hd_path: Path | None = None
    run_img = img
    if name in DESTRUCTIVE_DISK_TESTS:
        with tempfile.NamedTemporaryFile(suffix=".img", delete=False) as tmp:
            tmp_path = Path(tmp.name)
        shutil.copyfile(img, tmp_path)
        run_img = tmp_path

    launch_args: list = [run_img]
    if name in HD_BIOS_TESTS:
        with tempfile.NamedTemporaryFile(suffix=".hd.img", delete=False) as tmp:
            hd_path = Path(tmp.name)
        with hd_path.open("wb") as f:
            f.truncate(HD_SIZE)
        launch_args.append(hd_path)

    expect = f"PASS {name}"
    kwargs: dict = {}
    if not floppy_int13_shim:
        kwargs["floppy_int13_shim"] = False
    if hd_int13_bios is True:
        kwargs["hd_int13_bios"] = True
    proc = subprocess.Popen(
        launcher_argv(
            *launch_args,
            "--quiet",
            "--headless",
            "--serial-log",
            serial,
            **kwargs,
        ),
        cwd=str(ROOT / "emulator" / "k8086"),
        env=env,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    try:
        deadline = time.time() + timeout_s
        while time.time() < deadline:
            text = serial.read_text(errors="replace") if serial.is_file() else ""
            if expect in text:
                proc.wait(timeout=5)
                return
            if "FAIL " in text:
                raise AssertionError(f"{name} failed:\n{text}")
            if proc.poll() is not None:
                break
            time.sleep(0.05)
        text = serial.read_text(errors="replace") if serial.is_file() else ""
        raise AssertionError(
            f"{name}: expected {expect!r} in serial (exit={proc.poll()}).\n---\n{text}\n---"
        )
    finally:
        terminate_emulator(proc)
        if tmp_path is not None:
            unlink_retry(tmp_path)
        if hd_path is not None:
            unlink_retry(hd_path)


def test_bios_services() -> None:
    for name in TESTS:
        _run_one(name)
    # One smoke with the legacy host floppy INT 13h shim still enabled.
    _run_one("bt_disk", floppy_int13_shim=True)
    # Host Fixed Disk BIOS still services AH=08 for the same guest boot sector.
    _run_one("bt_hd_params", hd_int13_bios=True)


if __name__ == "__main__":
    test_bios_services()
    print("test_bios_services: OK")
