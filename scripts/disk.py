from __future__ import annotations

from pathlib import Path
from tempfile import NamedTemporaryFile

SECTOR_SIZE = 512
TOTAL_SECTORS = 1440  # 720 KB DD: 80×2×9
TOTAL_SECTORS_1440K = 2880  # 1.44 MB HD: 80×2×18
TOTAL_SECTORS_1200K = 2400  # 1.2 MB HD: 80×2×15
TOTAL_SECTORS_360K = 720  # 360 KB DD: 40×2×9

ATTR_DIRECTORY = 0x10
ATTR_READONLY = 0x01


def encode_name(name: str) -> bytes:
    upper = name.upper()
    stem, dot, suffix = upper.partition(".")
    if not stem or len(stem) > 8 or len(suffix) > 3 or (dot and not suffix):
        raise ValueError(f"invalid 8.3 filename: {name}")
    return stem.ljust(8).encode("ascii") + suffix.ljust(3).encode("ascii")


def decode_name(raw: bytes) -> str:
    stem = raw[:8].decode("ascii").rstrip(" ")
    suffix = raw[8:11].decode("ascii").rstrip(" ")
    if not stem:
        return ""
    if suffix:
        return f"{stem}.{suffix}"
    return stem


def sectors_for_size(size_bytes: int) -> int:
    return max(1, -(-size_bytes // SECTOR_SIZE))


def atomic_write(path: Path, data: bytes | bytearray) -> None:
    tmp_path: Path | None = None
    try:
        with NamedTemporaryFile(dir=path.parent, delete=False, suffix=".tmp") as tmp:
            tmp_path = Path(tmp.name)
            tmp.write(data)
        tmp_path.replace(path)
    except BaseException:
        if tmp_path is not None:
            tmp_path.unlink(missing_ok=True)
        raise
