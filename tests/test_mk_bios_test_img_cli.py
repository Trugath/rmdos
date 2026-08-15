"""Regression tests for mk_bios_test_img.py CLI error handling."""

from __future__ import annotations

import os
import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MK_BIOS_TEST_IMG = ROOT / "scripts" / "mk_bios_test_img.py"


class TestMkBiosTestImgCli(unittest.TestCase):
    def test_missing_boot_source_reports_concise_error(self) -> None:
        """When --boot points to a non-existent file, script exits with concise error, not traceback."""
        result = subprocess.run(
            [
                sys.executable,
                str(MK_BIOS_TEST_IMG),
                "--output",
                "/tmp/test.img",
                "--boot",
                "nonexistent/path/boot.bin",
            ],
            capture_output=True,
            text=True,
            cwd=ROOT,
            env={**os.environ, "PYTHONPATH": str(ROOT)},
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn("Traceback", result.stderr)
        self.assertNotIn("Traceback", result.stdout)
        self.assertEqual(result.stderr, "source file not found: nonexistent/path/boot.bin\n")


if __name__ == "__main__":
    unittest.main()
