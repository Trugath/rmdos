"""Regression tests for pack_fdrom.py CLI error handling."""

from __future__ import annotations

import os
import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PACK_FDROM = ROOT / "scripts" / "pack_fdrom.py"


class TestPackFdromCli(unittest.TestCase):
    def test_missing_input_source_reports_concise_error(self) -> None:
        """When --input points to a non-existent file, script exits with concise error, not traceback."""
        result = subprocess.run(
            [
                sys.executable,
                str(PACK_FDROM),
                "--input",
                "nonexistent/path/rom.bin",
                "--output",
                "/tmp/out.bin",
            ],
            capture_output=True,
            text=True,
            cwd=ROOT,
            env={**os.environ, "PYTHONPATH": str(ROOT)},
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn("Traceback", result.stderr)
        self.assertNotIn("Traceback", result.stdout)
        self.assertEqual(result.stderr, "source file not found: nonexistent/path/rom.bin\n")


if __name__ == "__main__":
    unittest.main()
