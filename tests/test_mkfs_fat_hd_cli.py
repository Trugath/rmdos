"""Regression tests for mkfs_fat_hd.py CLI error handling."""

from __future__ import annotations

import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MKFS_FAT_HD = ROOT / "scripts" / "mkfs_fat_hd.py"


class TestMkfsFatHdCli(unittest.TestCase):
    def test_missing_file_source_reports_concise_error(self) -> None:
        """When --file points to a non-existent source, script exits with concise error, not traceback."""
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            output = tmp_path / "test.img"
            # Create a minimal valid file to use for --file (mkfs_fat_hd requires at least one file)
            valid_file = tmp_path / "valid.bin"
            valid_file.write_bytes(b"VALID")
            # Use a non-existent source file first so it's the one that fails
            result = subprocess.run(
                [
                    sys.executable,
                    str(MKFS_FAT_HD),
                    "--output",
                    str(output),
                    "--file",
                    "NONEXIST.COM=nonexistent/path/hello.com",
                    "--file",
                    "VALID.BIN=valid.bin",
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
