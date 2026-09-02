"""pack_mz maxalloc must not force parents to own all free RAM."""

from __future__ import annotations

import struct
import unittest

from scripts.pack_mz import pack_mz


class TestPackMzAlloc(unittest.TestCase):
    def test_default_maxalloc_takes_all(self) -> None:
        mz = pack_mz(b"\xC3")
        self.assertEqual(mz[:2], b"MZ")
        self.assertEqual(struct.unpack_from("<H", mz, 12)[0], 0xFFFF)

    def test_limited_maxalloc_for_child_exec(self) -> None:
        # DESK.EXE needs this so STAR.COM can AH=4Bh-load beside it.
        mz = pack_mz(b"\xC3" * 1000, maxalloc=0x100)
        self.assertEqual(struct.unpack_from("<H", mz, 10)[0], 0x10)
        self.assertEqual(struct.unpack_from("<H", mz, 12)[0], 0x100)

    def test_reloc_table_emitted(self) -> None:
        mz = pack_mz(b"\x00\x00", relocs=[(0, 0), (2, 1)])
        self.assertEqual(struct.unpack_from("<H", mz, 6)[0], 2)
        # 0x1C + 8 reloc bytes → 3 paragraphs of header
        self.assertEqual(struct.unpack_from("<H", mz, 8)[0], 3)
        self.assertEqual(struct.unpack_from("<H", mz, 24)[0], 0x1C)
        self.assertEqual(struct.unpack_from("<HH", mz, 0x1C), (0, 0))
        self.assertEqual(struct.unpack_from("<HH", mz, 0x20), (2, 1))
        hdr = struct.unpack_from("<H", mz, 8)[0] * 16
        self.assertEqual(mz[hdr : hdr + 2], b"\x00\x00")


if __name__ == "__main__":
    unittest.main()
