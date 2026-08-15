"""Regression tests for pack_exe.py CLI error handling."""

from __future__ import annotations

import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PACK_EXE = ROOT / "scripts" / "pack_exe.py"


class TestPackExeCli(unittest.TestCase):
    def test_missing_code_source_reports_concise_error(self) -> None:
        """When --code points to a non-existent file, script exits with concise error, not traceback."""
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            data = tmp_path / "data.bin"
            data.write_bytes(b"DATA")
            result = subprocess.run(
                [
                    sys.executable,
                    str(PACK_EXE),
                    "--code",
                    "nonexistent/path/code.bin",
                    "--data",
                    str(data),
                    "--out",
                    str(tmp_path / "out.exe"),
                ],
                capture_output=True,
                text=True,
                cwd=ROOT,
                env={**os.environ, "PYTHONPATH": str(ROOT)},
            )
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn("Traceback", result.stderr)
        self.assertNotIn("Traceback", result.stdout)
        self.assertEqual(result.stderr, "source file not found: nonexistent/path/code.bin\n")

    def test_missing_data_source_reports_concise_error(self) -> None:
        """When --data points to a non-existent file, script exits with concise error, not traceback."""
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            code = tmp_path / "code.bin"
            code.write_bytes(b"CODE")
            result = subprocess.run(
                [
                    sys.executable,
                    str(PACK_EXE),
                    "--code",
                    str(code),
                    "--data",
                    "nonexistent/path/data.bin",
                    "--out",
                    str(tmp_path / "out.exe"),
                ],
                capture_output=True,
                text=True,
                cwd=ROOT,
                env={**os.environ, "PYTHONPATH": str(ROOT)},
            )
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn("Traceback", result.stderr)
        self.assertNotIn("Traceback", result.stdout)
        self.assertEqual(result.stderr, "source file not found: nonexistent/path/data.bin\n")

    def test_missing_const_source_reports_concise_error(self) -> None:
        """When --const points to a non-existent file, script exits with concise error, not traceback."""
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            code = tmp_path / "code.bin"
            code.write_bytes(b"CODE")
            data = tmp_path / "data.bin"
            data.write_bytes(b"DATA")
            result = subprocess.run(
                [
                    sys.executable,
                    str(PACK_EXE),
                    "--code",
                    str(code),
                    "--data",
                    str(data),
                    "--const",
                    "nonexistent/path/const.bin",
                    "--out",
                    str(tmp_path / "out.exe"),
                ],
                capture_output=True,
                text=True,
                cwd=ROOT,
                env={**os.environ, "PYTHONPATH": str(ROOT)},
            )
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn("Traceback", result.stderr)
        self.assertNotIn("Traceback", result.stdout)
        self.assertEqual(result.stderr, "source file not found: nonexistent/path/const.bin\n")


if __name__ == "__main__":
    unittest.main()
