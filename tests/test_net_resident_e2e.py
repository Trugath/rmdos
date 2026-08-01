"""E2E: CONFIG.SYS INSTALL=NET.COM — resident stack, lease in TSR (no LEASE.DAT)."""

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
SERIAL = BUILD / "serial-net.log"
IMAGE = BUILD / "os-net.img"
K8086 = ROOT / "emulator" / "k8086"
DE220_JAR_GLOB = "cards/de220/build/libs/de220-*.jar"


def _ensure_de220_jar() -> Path:
    jars = sorted(K8086.glob(DE220_JAR_GLOB))
    if jars:
        return jars[-1]
    subprocess.check_call(
        ["./gradlew", ":cards:de220:jar", "-q"],
        cwd=str(K8086),
    )
    jars = sorted(K8086.glob(DE220_JAR_GLOB))
    if not jars:
        raise SystemExit("de220 JAR missing after gradle build")
    return jars[-1]


def test_net_image_has_config() -> None:
    raw = IMAGE.read_bytes()
    assert fat12.find_directory_entry(raw, "BIN\\NET.COM").size_bytes > 0
    assert fat12.find_directory_entry(raw, "BIN\\NETTEST.COM").size_bytes > 0
    assert fat12.find_directory_entry(raw, "CONFIG.SYS").size_bytes > 0
    try:
        fat12.find_directory_entry(raw, "LEASE.DAT")
        raise AssertionError("LEASE.DAT should not be packed on os-net.img")
    except FileNotFoundError:
        pass


def test_net_resident_e2e() -> None:
    env = os.environ.copy()
    env["K8086_U18_ROM"] = str(BUILD / "u18.bin")
    env["K8086_U19_ROM"] = str(BUILD / "u19.bin")

    jar = _ensure_de220_jar()
    card_rel = jar.relative_to(K8086).as_posix()
    card_spec = f"{card_rel},base=0x300,irq=3,network=default"

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
                "--card",
                card_spec,
            ),
            cwd=str(K8086),
            env=env,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        deadline = time.time() + 120
        text = ""
        ok = False
        while time.time() < deadline:
            if SERIAL.is_file():
                text = SERIAL.read_text(errors="replace")
                if (
                    "NET resident" in text
                    and "NETTEST OK" in text
                    and "Lease acquired" in text
                    and "Reply from" in text
                    and "NETSTACK OK" in text
                    and "NET unloaded" in text
                    and "no DHCP lease" not in text
                ):
                    ok = True
                    break
                if "NET init failed" in text or "CONFIG: INSTALL failed" in text:
                    break
                if "NETTEST:" in text and "NETTEST OK" not in text:
                    break
            if proc.poll() is not None:
                break
            time.sleep(0.25)
        terminate_emulator(proc)
        if ok:
            raw = tmp_path.read_bytes()
            try:
                fat12.find_directory_entry(raw, "LEASE.DAT")
                raise AssertionError("LEASE.DAT created under TSR path")
            except FileNotFoundError:
                pass
    finally:
        unlink_retry(tmp_path)

    if not ok:
        raise AssertionError(f"resident NET stack gate failed.\n---\n{text}\n---")
    print("test_net_resident_e2e: OK")


if __name__ == "__main__":
    test_net_image_has_config()
    test_net_resident_e2e()
