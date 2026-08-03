"""Layout smoke: test.img packs the DEMO/TEST harness omitted from lean os.img."""

from __future__ import annotations

from pathlib import Path

from scripts import fat12

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "firmware" / "build"
IMAGE = BUILD / "test.img"


def test_test_img_harness_layout() -> None:
    assert IMAGE.is_file(), f"missing {IMAGE}; build test.img first"
    raw = IMAGE.read_bytes()
    assert len(raw) == fat12.TOTAL_SECTORS * fat12.SECTOR_SIZE
    for name in (
        "COMMAND.COM",
        "BIN\\FIND.COM",
        "DEMO\\STAR.COM",
        "DEMO\\HELLO.COM",
        "DEMO\\HELLO.EXE",
        "DEMO\\COMPAT.COM",
        "DEMO\\INT21X.COM",
        "DEMO\\ANSITST.COM",
        "DEMO\\EMSTST.COM",
        "DEMO\\MOUSETST.COM",
        "TEST\\SAMPLE.TXT",
        "TEST\\DBG.SCR",
        "TEST\\BIG.TXT",
        "SHIFT.BAT",
    ):
        ent = fat12.find_directory_entry(raw, name)
        assert ent.size_bytes > 0, name


if __name__ == "__main__":
    test_test_img_harness_layout()
    print("test_test_img_layout: OK")
