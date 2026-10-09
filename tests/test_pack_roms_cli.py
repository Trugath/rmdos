"""Regression tests for pack_roms.py CLI error handling."""

from __future__ import annotations

import os
import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PACK_ROMS = ROOT / "scripts" / "pack_roms.py"


class TestPackRomsCli(unittest.TestCase):
    def test_missing_u18_source_reports_concise_error(self) -> None:
        """When --u18 points to a non-existent file, script exits with concise error, not traceback."""
        result = subprocess.run(
            [
                sys.executable,
                str(PACK_ROMS),
                "--u18",
                "nonexistent/path/u18.bin",
                "--u19-out",
                "/tmp/out_u19.bin",
            ],
            capture_output=True,
            text=True,
            cwd=ROOT,
            env={**os.environ, "PYTHONPATH": str(ROOT)},
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn("Traceback", result.stderr)
        self.assertNotIn("Traceback", result.stdout)
        self.assertEqual(result.stderr, "source file not found: nonexistent/path/u18.bin\n")


if __name__ == "__main__":
    unittest.main()
