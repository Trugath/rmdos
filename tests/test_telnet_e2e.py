"""E2E + image checks for TELNET.COM (TCP + DNS via lease resolver)."""

from __future__ import annotations

import os
import shutil
import socket
import subprocess
import tempfile
import threading
import time
from pathlib import Path

from scripts import fat12
from tests.k8086_util import launcher_argv, terminate_emulator, unlink_retry

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "firmware" / "build"
SERIAL = BUILD / "serial-telnet.log"
IMAGE = BUILD / "os-telnet.img"
K8086 = ROOT / "emulator" / "k8086"
DE220_JAR_GLOB = "cards/de220/build/libs/de220-*.jar"
BANNER = b"TELNET-E2E\r\n"
PORT = 2323


def _gradlew_argv(*tasks: str) -> list[str]:
    if os.name == "nt":
        return ["cmd.exe", "/c", "gradlew.bat", *tasks]
    return ["./gradlew", *tasks]


def _ensure_emulator_and_de220() -> Path:
    """Rebuild emulator (NAT TCP + DNS) and ensure DE-220 card JAR exists."""
    subprocess.check_call(
        _gradlew_argv(":k8086-emulator:installDist", ":cards:de220:jar", "-q"),
        cwd=str(K8086),
    )
    jars = sorted(K8086.glob(DE220_JAR_GLOB))
    if not jars:
        raise SystemExit("de220 JAR missing after gradle build")
    return jars[-1]


class _BannerServer:
    def __init__(self, host: str, port: int) -> None:
        self.host = host
        self.port = port
        self._sock: socket.socket | None = None
        self._thread: threading.Thread | None = None
        self.error: str | None = None
        self.accepted = False

    def start(self) -> None:
        srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        srv.bind((self.host, self.port))
        srv.listen(1)
        srv.settimeout(90.0)
        self._sock = srv

        def _run() -> None:
            try:
                assert self._sock is not None
                conn, _ = self._sock.accept()
                self.accepted = True
                with conn:
                    conn.sendall(BANNER)
                    # Hold the connection open so the guest session can RX.
                    time.sleep(0.35)
                    try:
                        conn.shutdown(socket.SHUT_WR)
                    except OSError:
                        pass
            except Exception as exc:  # noqa: BLE001 — surface in assertion
                self.error = str(exc)

        self._thread = threading.Thread(target=_run, daemon=True)
        self._thread.start()

    def stop(self) -> None:
        if self._sock is not None:
            try:
                self._sock.close()
            except OSError:
                pass
            self._sock = None
        if self._thread is not None:
            self._thread.join(timeout=5)
            self._thread = None


def test_telnet_on_image() -> None:
    raw = IMAGE.read_bytes()
    ent = fat12.find_directory_entry(raw, "BIN\\TELNET.COM")
    assert ent.size_bytes > 0
    ae = fat12.find_directory_entry(raw, "AUTOEXEC.BAT")
    assert ae.size_bytes >= 4
    # Packed AUTOEXEC must use DNS hostname path (not raw IPv4 only).
    auto = fat12.read_file(raw, "AUTOEXEC.BAT")
    assert b"TELNET" in auto.upper()
    assert b"localhost" in auto.lower()


def test_telnet_com_contains_dns_strings() -> None:
    """Unit-ish: TELNET.COM image embeds DNS / resolve UX strings."""
    raw = IMAGE.read_bytes()
    com = fat12.read_file(raw, "BIN\\TELNET.COM")
    assert b"Resolving hostname..." in com
    assert b"could not resolve host" in com
    assert b"Connected." in com
    assert b"Usage: TELNET host" in com


def _run_telnet_session() -> str:
    env = os.environ.copy()
    env["K8086_U18_ROM"] = str(BUILD / "u18.bin")
    env["K8086_U19_ROM"] = str(BUILD / "u19.bin")

    jar = _ensure_emulator_and_de220()
    card_rel = jar.relative_to(K8086).as_posix()
    card_spec = f"{card_rel},base=0x300,irq=3,network=default"

    server = _BannerServer("127.0.0.1", PORT)
    server.start()

    SERIAL.write_text("")
    with tempfile.NamedTemporaryFile(suffix=".img", delete=False) as tmp:
        tmp_path = Path(tmp.name)
    shutil.copyfile(IMAGE, tmp_path)
    text = ""
    ok = False
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
        while time.time() < deadline:
            if SERIAL.is_file():
                text = SERIAL.read_text(errors="replace")
                if (
                    "Lease acquired" in text
                    and "Resolving hostname..." in text
                    and "Connected." in text
                    and "no DHCP lease" not in text
                    and "NIC init failed" not in text
                    and "connect failed" not in text
                    and "could not resolve host" not in text
                ):
                    ok = True
                    break
                if (
                    "no DHCP lease" in text
                    or "NIC init failed" in text
                    or "connect failed" in text
                    or "ARP failed" in text
                    or "could not resolve host" in text
                ):
                    break
            if proc.poll() is not None:
                if (
                    "Lease acquired" in text
                    and "Resolving hostname..." in text
                    and "Connected." in text
                    and "connect failed" not in text
                    and "could not resolve host" not in text
                ):
                    ok = True
                break
            time.sleep(0.25)
        terminate_emulator(proc)
    finally:
        server.stop()
        unlink_retry(tmp_path)

    if server.error:
        raise AssertionError(f"banner server error: {server.error}\n---\n{text}\n---")
    if not server.accepted:
        raise AssertionError(f"banner server never accepted a connection.\n---\n{text}\n---")
    if not ok:
        raise AssertionError(f"TELNET e2e connect not seen.\n---\n{text}\n---")
    return text


def test_telnet_e2e_dns_localhost() -> None:
    text = _run_telnet_session()
    assert "Resolving hostname..." in text
    assert "Connected." in text
    # Payload is drawn via B800 (con_putc); serial may not show TELNET-E2E.
    print("test_telnet_e2e_dns_localhost: OK")


# Back-compat alias for make / older runners
def test_telnet_e2e() -> None:
    test_telnet_e2e_dns_localhost()


if __name__ == "__main__":
    test_telnet_on_image()
    test_telnet_com_contains_dns_strings()
    test_telnet_e2e()
