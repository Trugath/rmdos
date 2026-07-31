# Contributing

## Setup

- JDK 21+ (for the k8086 submodule)
- Host `as` / `ld` / `objcopy` capable of `elf_i386` (or the toolchain under `tools/host/` on Windows)
- Python 3
- `./setup.sh` then `make` / `make test`

## Style

Match existing assembly and script style in the area you touch. Prefer small,
focused changes with a host or E2E test when behavior changes.

## License

By contributing, you agree your contributions are licensed under the MIT License
(see [LICENSE](LICENSE)).
