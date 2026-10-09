"""Regression tests for mkimg.py CLI error handling."""

from __future__ import annotations

import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MKIMG = ROOT / "scripts" / "mkimg.py"


class TestMkimgCli(unittest.TestCase):
    def test_missing_boot_source_reports_concise_error(self) -> None:
        """When --boot points to a non-existent file, script exits with concise error, not traceback."""
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            output = tmp_path / "test.img"
            kernel = tmp_path / "kernel.sys"
            kernel.write_bytes(b"KERNEL")
            result = subprocess.run(
                [
                    sys.executable,
                    str(MKIMG),
                    "--output",
                    str(output),
                    "--boot",
                    "nonexistent/path/boot.bin",
                    "--kernel",
                    str(kernel),
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

    def test_missing_kernel_source_reports_concise_error(self) -> None:
        """When --kernel points to a non-existent file, script exits with concise error, not traceback."""
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            output = tmp_path / "test.img"
            boot = tmp_path / "boot.bin"
            boot_data = b"\x00" * 510 + b"\x55\xaa"
            boot.write_bytes(boot_data)
            result = subprocess.run(
                [
                    sys.executable,
                    str(MKIMG),
                    "--output",
                    str(output),
                    "--boot",
                    str(boot),
                    "--kernel",
                    "nonexistent/path/kernel.sys",
                ],
                capture_output=True,
                text=True,
                cwd=ROOT,
                env={**os.environ, "PYTHONPATH": str(ROOT)},
            )
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn("Traceback", result.stderr)
        self.assertNotIn("Traceback", result.stdout)
        self.assertEqual(result.stderr, "source file not found: nonexistent/path/kernel.sys\n")


if __name__ == "__main__":
    unittest.main()
