# Contributing

## Setup

- JDK 21+ (for the k8086 submodule)
- Host `as` / `ld` / `objcopy` capable of `elf_i386` (or the toolchain under
  `tools/host/` on Windows — put `tools/host/bin` and `tools/host/i686-elf/bin`
  on `PATH`)
- Python 3
- `./setup.sh` then `make` / `make test`

On Windows use Git Bash or MSYS2 for `setup.sh` and Make; WSL2 is fine with a
native Linux toolchain. See the **Windows host** section in [README.md](README.md).

Compatibility expectations for contributions: [docs/compatibility.md](docs/compatibility.md).

## Releases

Ship a build by tagging `vX.Y.Z` and pushing the tag; CI publishes `os.img` +
ROMs with an auto-generated changelog (see README **Releases**).

## Style

Match existing assembly and script style in the area you touch. Prefer small,
focused changes with a host or E2E test when behavior changes.

## License

By contributing, you agree your contributions are licensed under the MIT License
(see [LICENSE](LICENSE)).
