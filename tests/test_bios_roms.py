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
    # Identity: FFF5 date, FFFE=XT, top-8K checksum
    assert u18[0x7FF5:0x7FFD] == b"08/01/26"
    assert u18[0x7FFE] == 0xFE
    assert sum(u18[0x6000:0x8000]) & 0xFF == 0


def test_pinned_stubs_present() -> None:
    u18 = U18.read_bytes()
    # F1 at E842 → offset 0x6842
    assert u18[0x6842] != 0x00 or u18[0x6843] != 0x00
    # CAD at EA82 → offset 0x6A82
    assert u18[0x6A82] in (0xE9, 0xEB, 0xEA)
    # Font at FA6E → offset 0x7A6E; digit '0' glyph area non-empty somewhere in 0x30*
    font = u18[0x7A6E : 0x7A6E + 1024]
    assert any(b != 0 for b in font)
    # Soft-compat IBM trampolines (file offset = F000_off - 0x8000)
    for off in (
        0xE6F2,  # INT 19
        0xE739,  # INT 14
        0xE82E,  # INT 16
        0xEC59,  # INT 13
        0xEFD2,  # INT 17
        0xF841,  # INT 12
        0xF84D,  # INT 11
        0xF859,  # INT 15
        0xFE6E,  # INT 1A
        0xFEA5,  # IRQ0
        0xFF54,  # INT 5
    ):
        assert u18[off - 0x8000] in (0xE9, 0xEB, 0xEA), f"missing jmp at {off:04X}"
    # Baud table at E729 — first divisor 110 baud = 1047
    assert u18[0xE729 - 0x8000 : 0xE729 - 0x8000 + 2] == (1047).to_bytes(2, "little")


if __name__ == "__main__":
    test_rom_sizes_and_reset()
    test_pinned_stubs_present()
    print("test_bios_roms: OK")
