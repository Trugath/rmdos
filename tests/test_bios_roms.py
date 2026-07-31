"""Verify built U18/U19 ROM sizes and pinned entry bytes."""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "firmware" / "build"
U18 = BUILD / "u18.bin"
U19 = BUILD / "u19.bin"


def test_rom_sizes_and_reset() -> None:
    assert U18.is_file(), "run make bios first"
    assert U19.is_file(), "run make bios first"
    u18 = U18.read_bytes()
    u19 = U19.read_bytes()
    assert len(u18) == 32768
    assert len(u19) == 8192
    assert u19 == b"\xFF" * 8192
    # File offset 0x7FF0 = linear 0xFFFF0
    assert u18[0x7FF0:0x7FF5] == bytes([0xEA, 0x5B, 0xE0, 0x00, 0xF0])
    # POST trampoline at file offset 0x605B = F000:E05B
    assert u18[0x605B] in (0xE9, 0xEB, 0xEA)  # near/short/far jmp


def test_pinned_stubs_present() -> None:
    u18 = U18.read_bytes()
    # F1 at E842 → offset 0x6842
    assert u18[0x6842] != 0x00 or u18[0x6843] != 0x00
    # CAD at EA82 → offset 0x6A82
    assert u18[0x6A82] in (0xE9, 0xEB, 0xEA)
    # Font at FA6E → offset 0x7A6E; digit '0' glyph area non-empty somewhere in 0x30*
    font = u18[0x7A6E : 0x7A6E + 1024]
    assert any(b != 0 for b in font)


if __name__ == "__main__":
    test_rom_sizes_and_reset()
    test_pinned_stubs_present()
    print("test_bios_roms: OK")
