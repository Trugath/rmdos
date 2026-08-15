# rmdos Coding Agent Guidelines

## What this repo is
rmdos is a DOS-like operating system built for an 8086 emulator, featuring an assembler-based kernel, BIOS modules, and a suite of DOS-compatible command-line tools (both assembly and C). The repository includes Python scripts for build tooling, image creation, and testing, along with emulator integration for E2E validation.

## Preferred validation commands
For quick host-side validation before running heavy emulator E2E tests:

```bash
# Compile-check all Python scripts and tests
python3 -m compileall scripts tests

# Run host CLI tests only (no emulator)
make test-cli
```

## Note on full test suite
The full `make test` target builds emulator images and runs E2E tests inside the k8086 emulator. This is heavy and time-consuming. Prefer the host checks above first to catch syntax/import issues before running emulator tests.

## Error handling convention
When a Python CLI script receives a missing source path argument, it must exit with a concise error message of the form:

```
source file not found: <path>
```

No traceback should be printed. Tests expect this exact format for assertion purposes.

## CLI error-handling verification
All CLI scripts now handle missing source paths gracefully. Verified with `python3 -m unittest discover -s tests -p 'test_*_cli.py' -v`.
