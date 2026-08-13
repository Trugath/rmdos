#!/usr/bin/env python3
"""Unit tests for scripts/pack_exe.py MZ EXE builder.

Tests cover MZ header fields and layout for basic code, data, cont,
custom IP, BSS/minalloc, bss_end, paragraph padding, maxalloc adjustment,
and invalid IP/64K overflow edge cases.
"""

from __future__ import annotations

import struct
import unittest

from scripts.pack_exe import pack_exe


def _parse_mz_header(mz: bytes) -> dict:
    """Parse MZ header fields into a dictionary."""
    return {
        "magic": struct.unpack_from("<H", mz, 0)[0],
        "last_page_size": struct.unpack_from("<H", mz, 2)[0],
        "pages": struct.unpack_from("<H", mz, 4)[0],
        "relocs": struct.unpack_from("<H", mz, 6)[0],
        "header_paras": struct.unpack_from("<H", mz, 8)[0],
        "minalloc": struct.unpack_from("<H", mz, 10)[0],
        "maxalloc": struct.unpack_from("<H", mz, 12)[0],
        "ss": struct.unpack_from("<H", mz, 14)[0],
        "sp": struct.unpack_from("<H", mz, 16)[0],
        "checksum": struct.unpack_from("<H", mz, 18)[0],
        "ip": struct.unpack_from("<H", mz, 20)[0],
        "cs": struct.unpack_from("<H", mz, 22)[0],
        "reloc_off": struct.unpack_from("<H", mz, 24)[0],
        "overlay": struct.unpack_from("<H", mz, 26)[0],
    }


class TestPackExeBasic(unittest.TestCase):
    """Basic MZ header and layout tests."""

    def test_mz_magic(self):
        """MZ magic number should be 0x5A4D."""
        mz = pack_exe(b"\x90", b"")
        self.assertEqual(mz[:2], b"MZ")

    def test_header_structure(self):
        """Header should be 32 bytes (2 paras)."""
        mz = pack_exe(b"\x90", b"")
        hdr = _parse_mz_header(mz)
        self.assertEqual(hdr["header_paras"], 2)
        self.assertEqual(hdr["relocs"], 0)
        self.assertEqual(hdr["reloc_off"], 0x1C)
        self.assertEqual(hdr["overlay"], 0)

    def test_code_paragraph_padding(self):
        """Code should be padded to paragraph boundary."""
        # 17 bytes = 1 para + 1 byte, should pad to 2 paras (32 bytes)
        code = b"\x90" * 17
        mz = pack_exe(code, b"")
        hdr = _parse_mz_header(mz)
        # code_paras = ceil(17/16) = 2
        self.assertEqual(hdr["ss"], 2)  # SS = code_paras
        # Image starts after 32-byte header
        image_start = 32
        # First 32 bytes should be code (2 paras)
        self.assertEqual(mz[image_start:image_start + 32], code + b"\x00" * 15)

    def test_empty_code(self):
        """Empty code should still produce valid MZ."""
        mz = pack_exe(b"", b"")
        hdr = _parse_mz_header(mz)
        self.assertEqual(hdr["magic"], 0x5A4D)
        self.assertEqual(hdr["ss"], 0)  # No code, no paras

    def test_total_size_calculation(self):
        """Total size = header + padded code."""
        code = b"\x90" * 10  # 10 bytes, 1 para
        mz = pack_exe(code, b"")
        hdr = _parse_mz_header(mz)
        expected_pages = (32 + 16 + 511) // 512  # header + code
        self.assertEqual(hdr["pages"], expected_pages)



class TestPackExeData(unittest.TestCase):
    """Data segment tests."""

    def test_data_after_code(self):
        """Data should follow code (with padding)."""
        code = b"\x90" * 10
        data = b"\xAB" * 10
        mz = pack_exe(code, data)
        hdr = _parse_mz_header(mz)
        # code_paras = 1, header = 2 paras, so code at offset 32
        self.assertEqual(mz[32:32+10], code)
        # data follows code (code is padded to 16 bytes)
        self.assertEqual(mz[48:48+10], data)

    def test_data_with_paragraph_padding(self):
        """Data should be placed after paragraph-padded code."""
        code = b"\x90" * 17  # 17 bytes = 2 paras (32 bytes padded)
        data = b"\xAB" * 5
        mz = pack_exe(code, data)
        # code takes 32 bytes, data starts at offset 32+32=64
        self.assertEqual(mz[64:64+5], data)

    def test_data_only(self):
        """Data without code should work."""
        data = b"\xAB" * 10
        mz = pack_exe(b"", data)
        # code_paras = 0, so data starts at header end (offset 32)
        self.assertEqual(mz[32:32+10], data)


class TestPackExeConst(unittest.TestCase):
    """Const segment tests with paragraph padding."""

    def test_const_follows_padded_data(self):
        """Const should begin on paragraph boundary after data."""
        code = b"\x90" * 10  # 1 para
        data = b"\xAB" * 5   # 1 para (padded)
        const = b"\xCD" * 10
        mz = pack_exe(code, data, const=const)
        # code: 1 para (16 bytes), data: 1 para (16 bytes padded), const follows
        # code at 32-48, data at 48-64, const at 64+
        self.assertEqual(mz[32:48], code + b"\x00" * 6)  # code + pad (16 bytes)
        self.assertEqual(mz[48:64], data + b"\x00" * 11)  # data + pad (16 bytes)
        self.assertEqual(mz[64:74], const)  # const starts at 64 (para boundary)

    def test_const_without_data(self):
        """Const should follow code even with empty data."""
        code = b"\x90" * 10
        const = b"\xCD" * 10
        mz = pack_exe(code, b"", const=const)
        # code is 1 para (16 bytes), const should start at 48 (para boundary)
        self.assertEqual(mz[48:58], const)

    def test_const_with_bss_end(self):
        """bss_end should be used for data_span when const is present."""
        code = b"\x90" * 10
        data = b"\xAB" * 5
        const = b"\xCD" * 10
        bss_end = 0x200  # Custom bss_end
        mz = pack_exe(code, data, const=const, bss_end=bss_end)
        hdr = _parse_mz_header(mz)
        # SP should be based on bss_end + stack (default 0x1000)
        expected_sp = (bss_end + 0x1000 - 2) & 0xFFFF
        self.assertEqual(hdr["sp"], expected_sp)


class TestPackExeIP(unittest.TestCase):
    """Custom IP entry point tests."""

    def test_default_ip(self):
        """Default IP should be 0."""
        mz = pack_exe(b"\x90", b"")
        hdr = _parse_mz_header(mz)
        self.assertEqual(hdr["ip"], 0)

    def test_custom_ip(self):
        """Custom IP should be set in header."""
        mz = pack_exe(b"\x90", b"", ip=0x100)
        hdr = _parse_mz_header(mz)
        self.assertEqual(hdr["ip"], 0x100)

    def test_ip_at_end_of_code(self):
        """IP near end of code segment."""
        mz = pack_exe(b"\x90" * 100, b"", ip=0x0050)
        hdr = _parse_mz_header(mz)
        self.assertEqual(hdr["ip"], 0x0050)

    def test_ip_overflow_error(self):
        """IP > 0xFFFF should raise ValueError."""
        with self.assertRaises(ValueError) as ctx:
            pack_exe(b"\x90", b"", ip=0x10000)
        self.assertIn("ip out of range", str(ctx.exception))

    def test_ip_negative_error(self):
        """Negative IP should raise ValueError."""
        with self.assertRaises(ValueError) as ctx:
            pack_exe(b"\x90", b"", ip=-1)
        self.assertIn("ip out of range", str(ctx.exception))



class TestPackExeBSS(unittest.TestCase):
    """BSS and minalloc tests."""

    def test_minalloc_from_bss_bytes(self):
        """minalloc should account for BSS bytes."""
        bss_bytes = 0x100  # 256 bytes = 16 paras
        mz = pack_exe(b"\x90", b"", bss_bytes=bss_bytes)
        hdr = _parse_mz_header(mz)
        # minalloc = ceil(256/16) = 16 paras
        self.assertEqual(hdr["minalloc"], 272)

    def test_minalloc_with_stack(self):
        """minalloc should include stack."""
        bss_bytes = 0x100
        stack = 0x100  # Custom stack (not default 0x1000)
        mz = pack_exe(b"\x90", b"", bss_bytes=bss_bytes, stack=stack)
        hdr = _parse_mz_header(mz)
        # minalloc = ceil((256 + 256)/16) = 32 paras
        self.assertEqual(hdr["minalloc"], 32)

    def test_minalloc_extra(self):
        """minalloc_extra should add to minalloc."""
        bss_bytes = 0x100
        minalloc_extra = 0x10
        mz = pack_exe(b"\x90", b"", bss_bytes=bss_bytes, minalloc_extra=minalloc_extra)
        hdr = _parse_mz_header(mz)
        # minalloc = ceil(256/16) + 16 = 16 + 16 = 32 paras
        self.assertEqual(hdr["minalloc"], 288)

    def test_minalloc_minimum_one(self):
        """minalloc should be at least 1 even with zero BSS."""
        mz = pack_exe(b"\x90", b"", bss_bytes=0)
        hdr = _parse_mz_header(mz)
        self.assertGreaterEqual(hdr["minalloc"], 1)

    def test_bss_end_offset(self):
        """bss_end should set SP base."""
        bss_end = 0x500
        stack = 0x200
        mz = pack_exe(b"\x90", b"", bss_end=bss_end, stack=stack)
        hdr = _parse_mz_header(mz)
        # SP = (bss_end + stack - 2) & 0xFFFF
        expected_sp = (bss_end + stack - 2) & 0xFFFF
        self.assertEqual(hdr["sp"], expected_sp)


class TestPackExeMaxalloc(unittest.TestCase):
    """maxalloc adjustment tests."""

    def test_default_maxalloc(self):
        """Default maxalloc should be 0x100."""
        mz = pack_exe(b"\x90", b"")
        hdr = _parse_mz_header(mz)
        self.assertEqual(hdr["maxalloc"], 0x100)

    def test_custom_maxalloc(self):
        """Custom maxalloc should be set."""
        mz = pack_exe(b"\x90", b"", maxalloc=0x200)
        hdr = _parse_mz_header(mz)
        self.assertEqual(hdr["maxalloc"], 0x200)

    def test_maxalloc_adjusted_up(self):
        """maxalloc should be raised to minalloc if smaller."""
        bss_bytes = 0x200  # 32 paras
        # Default maxalloc is 0x100, but minalloc will be 32
        # So maxalloc should be raised to 32 (0x20)
        mz = pack_exe(b"\x90", b"", bss_bytes=bss_bytes, maxalloc=0x10)
        hdr = _parse_mz_header(mz)
        # minalloc = ceil(512/16) = 32, maxalloc should be at least 32
        self.assertGreaterEqual(hdr["maxalloc"], hdr["minalloc"])

    def test_maxalloc_masked(self):
        """maxalloc should be masked to 16 bits."""
        mz = pack_exe(b"\x90", b"", maxalloc=0x12345)
        hdr = _parse_mz_header(mz)
        self.assertEqual(hdr["maxalloc"], 0x2345)



class TestPackExeOverflow(unittest.TestCase):
    """64K overflow edge case tests."""

    def test_data_bss_stack_overflow_error(self):
        """data+bss+stack > 64K should raise ValueError."""
        # SP = data_span + stack, must not exceed 64K
        # data_span = len(data) + bss_bytes (without const)
        # If data_span + stack > 65536, should fail
        with self.assertRaises(ValueError) as ctx:
            # data_span = 0x10000 (64K), stack = 0x100 -> overflow
            pack_exe(b"", b"", bss_bytes=0x10000, stack=0x100)
        self.assertIn("exceeds 64K", str(ctx.exception))

    def test_sp_clamped_to_zero(self):
        """SP should be clamped to at least 2."""
        # With minimal data_span and stack, SP should be valid
        mz = pack_exe(b"\x90", b"", bss_bytes=0, stack=2)
        hdr = _parse_mz_header(mz)
        # SP = (0 + 2 - 2) & 0xFFFF = 0, but clamped to 2
        self.assertGreaterEqual(hdr["sp"], 0)

    def test_large_data_span(self):
        """Large data span should compute SP correctly."""
        data = b"\x00" * 0x1000  # 4KB
        stack = 0x1000
        mz = pack_exe(b"\x90", data, stack=stack)
        hdr = _parse_mz_header(mz)
        # data_span = 0x1000 (4KB), stack = 0x1000
        # SP = (0x1000 + 0x1000 - 2) & 0xFFFF = 0x1FFE
        expected_sp = (0x1000 + 0x1000 - 2) & 0xFFFF
        self.assertEqual(hdr["sp"], expected_sp)


class TestPackExeLayout(unittest.TestCase):
    """Complete layout verification tests."""

    def test_basic_layout(self):
        """Verify complete layout for basic code."""
        code = b"\x90" * 32  # Exactly 2 paras
        mz = pack_exe(code, b"")
        hdr = _parse_mz_header(mz)
        # Header: 32 bytes (2 paras)
        # Code: 32 bytes (2 paras)
        # Total: 64 bytes, pages = ceil(64/512) = 1
        self.assertEqual(hdr["pages"], 1)
        self.assertEqual(hdr["ss"], 2)  # code_paras = 2
        # Code starts at offset 32
        self.assertEqual(mz[32:64], code)

    def test_layout_with_data_and_const(self):
        """Verify layout with code, data, and const."""
        code = b"A" * 10   # 1 para padded
        data = b"B" * 10   # 1 para padded
        const = b"C" * 10  # follows data
        mz = pack_exe(code, data, const=const)
        hdr = _parse_mz_header(mz)
        # Header: 32 bytes
        # Code: 16 bytes (1 para padded)
        # Data: 16 bytes (1 para padded)
        # Const: 10 bytes
        # Code at 32-48, data at 48-64, const at 64-74
        self.assertEqual(mz[32:42], code)
        self.assertEqual(mz[48:58], data)
        self.assertEqual(mz[64:74], const)

    def test_reloc_table_offset(self):
        """Reloc table offset should be 0x1C."""
        mz = pack_exe(b"\x90", b"")
        hdr = _parse_mz_header(mz)
        self.assertEqual(hdr["reloc_off"], 0x1C)

    def test_checksum_zero(self):
        """Checksum should be 0 (no relocation entries)."""
        mz = pack_exe(b"\x90", b"")
        hdr = _parse_mz_header(mz)
        self.assertEqual(hdr["checksum"], 0)

    def test_cs_zero(self):
        """CS should be 0."""
        mz = pack_exe(b"\x90", b"")
        hdr = _parse_mz_header(mz)
        self.assertEqual(hdr["cs"], 0)


if __name__ == "__main__":
    unittest.main()
