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


if __name__ == "__main__":
    unittest.main()
