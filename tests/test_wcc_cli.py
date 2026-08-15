"""Regression tests for wcc.py CLI error handling."""

from __future__ import annotations

import os
import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WCC = ROOT / "scripts" / "wcc.py"


class TestWccCli(unittest.TestCase):
    def test_missing_input_source_reports_concise_error(self) -> None:
        """When the input .c path does not exist, script exits with concise error, not traceback."""
        result = subprocess.run(
            [
                sys.executable,
                str(WCC),
                "nonexistent/path/hello.c",
                "-o",
                "/tmp/out.s",
            ],
            capture_output=True,
            text=True,
            cwd=ROOT,
            env={**os.environ, "PYTHONPATH": str(ROOT)},
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn("Traceback", result.stderr)
        self.assertNotIn("Traceback", result.stdout)
        self.assertEqual(result.stderr, "source file not found: nonexistent/path/hello.c\n")


if __name__ == "__main__":
    unittest.main()
