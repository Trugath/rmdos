"""COM loader SP must stay in-segment when PSP+image+stack > 64KiB."""

from __future__ import annotations

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DESK_COM = ROOT.parent / "firmware" / "build" / "desk.com"


def com_exe_sp(allocated_paragraphs: int) -> int:
    """Mirror load_setup_com SP math after the 64K cap fix."""
    bx = allocated_paragraphs
    if bx > 0x1000:
        bx = 0x1000
    return ((bx << 4) - 2) & 0xFFFF


def com_alloc_paragraphs(com_size: int) -> int:
    """Mirror load_setup_com: 0x10 PSP + ceil(size/16) + 0x200 stack."""
    return 0x10 + ((com_size + 15) >> 4) + 0x200


class TestComStackPointer(unittest.TestCase):
    def test_small_com_sp_near_image_top(self) -> None:
        paras = com_alloc_paragraphs(4096)
        sp = com_exe_sp(paras)
        self.assertLess(paras, 0x1000)
        self.assertEqual(sp, (paras << 4) - 2)

    def test_large_desk_sized_com_sp_does_not_wrap(self) -> None:
        # firmware/build/desk.com is ~60KiB; without the cap, shl wraps to ~0x0D3E.
        paras = com_alloc_paragraphs(60467)
        self.assertGreater(paras, 0x1000)
        broken = ((paras << 4) - 2) & 0xFFFF
        self.assertLess(broken, 0x1000)  # documents the overflow bug
        sp = com_exe_sp(paras)
        self.assertEqual(sp, 0xFFFE)
        self.assertGreater(sp, 0xF000)

    def test_built_desk_com_needs_sp_cap(self) -> None:
        # Historical: desk shipped as ~60KiB COM-style body; SP math must cap.
        # DESK is now small-model MZ (no desk.com); keep the size gate synthetic.
        size = 60467
        paras = com_alloc_paragraphs(size)
        self.assertGreater(
            paras,
            0x1000,
            f"desk-sized COM ({size} bytes) should exceed 64K paras with 8K stack",
        )
        broken = ((paras << 4) - 2) & 0xFFFF
        self.assertNotEqual(broken, 0xFFFE)
        self.assertEqual(com_exe_sp(paras), 0xFFFE)

    def test_exactly_64k_block(self) -> None:
        self.assertEqual(com_exe_sp(0x1000), 0xFFFE)


if __name__ == "__main__":
    unittest.main()
