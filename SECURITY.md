# Security Policy

## Supported versions

Security fixes are applied on the latest `main` and on the most recent
`v*` release when practical. Older tags are not patched.

## Reporting a vulnerability

Please **do not** open a public issue for security-sensitive reports.

Prefer one of:

1. [GitHub private vulnerability reporting](https://github.com/Trugath/rmdos/security/advisories/new)
   for this repository (if enabled), or
2. Email the maintainer via the address on their
   [GitHub profile](https://github.com/Trugath).

Include enough detail to reproduce (host OS, k8086 revision, guest image/ROMs,
and steps). You should hear back within a reasonable time; there is no bug
bounty.

## Scope notes

rmDOS and its XT BIOS are hobby / clean-room real-mode software for emulators
and period hardware. Reports about intentional period limitations (no Cassette
BASIC, real-mode only, etc.) are not vulnerabilities.
