"""Regression tests for mkfs_fat12.py CLI error handling."""

from __future__ import annotations

import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MKFS_FAT12 = ROOT / "scripts" / "mkfs_fat12.py"


class TestMkfsFat12Cli(unittest.TestCase):
    def test_missing_file_source_reports_concise_error(self) -> None:
        """When --file points to a non-existent source, script exits with concise error, not traceback."""
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            output = tmp_path / "test.img"
            # Create a minimal valid 512-byte boot sector with 0x55AA signature
            boot = tmp_path / "boot.bin"
            boot_data = b"\x00" * 510 + b"\x55\xaa"
            boot.write_bytes(boot_data)
            # Create a minimal non-empty kernel file
            kernel = tmp_path / "kernel.sys"
            kernel.write_bytes(b"KERNEL")
            # Use a non-existent source file
            result = subprocess.run(
                [
                    sys.executable,
                    str(MKFS_FAT12),
                    "--output",
                    str(output),
                    "--boot",
                    str(boot),
                    "--kernel",
                    str(kernel),
                    "--file",
                    "HELLO.COM=nonexistent/path/hello.com",
                ],
                capture_output=True,
                text=True,
                cwd=ROOT,
                env={**os.environ, "PYTHONPATH": str(ROOT)},
            )
        self.assertNotEqual(result.returncode, 0)
        # Ensure no Python traceback in stderr
        self.assertNotIn("Traceback", result.stderr)
        self.assertNotIn("Traceback", result.stdout)
        # Assert exact concise stderr line
        self.assertEqual(result.stderr, "source file not found: nonexistent/path/hello.com\n")


if __name__ == "__main__":
    unittest.main()
