"""Regression tests for pack_mz.py CLI error handling."""

from __future__ import annotations

import os
import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PACK_MZ = ROOT / "scripts" / "pack_mz.py"


class TestPackMzCli(unittest.TestCase):
    def test_missing_com_source_reports_concise_error(self) -> None:
        """When --com points to a non-existent file, script exits with concise error, not traceback."""
        result = subprocess.run(
            [
                sys.executable,
                str(PACK_MZ),
                "--com",
                "nonexistent/path/app.com",
                "--out",
                "/tmp/out.exe",
            ],
            capture_output=True,
            text=True,
            cwd=ROOT,
            env={**os.environ, "PYTHONPATH": str(ROOT)},
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn("Traceback", result.stderr)
        self.assertNotIn("Traceback", result.stdout)
        self.assertEqual(result.stderr, "source file not found: nonexistent/path/app.com\n")


if __name__ == "__main__":
    unittest.main()
