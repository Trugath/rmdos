# rmDOS

Real-mode XT stack: **motherboard system ROMs (U18/U19)** plus a **DOS-compatible OS**,
built from scratch for IBM PC/XT-class machines (8088/8086, ≤1 MiB conventional memory).

A later DOS (protected mode / extender era) may be derived from this project;
**rmDOS itself stays real mode only**. Cassette BASIC is intentionally omitted.

## Layout

```
rmdos/
|-- emulator/k8086/     # Git submodule: IBM 5155/5160 Kotlin emulator
|-- firmware/
|   |-- bios/           # Clean-room XT system BIOS → u18.bin / u19.bin
|   |-- src/            # Boot sector + kernel (16-bit x86)
|   |-- linker/         # OS link scripts
|   |-- build/          # ROMs, os.img, logs
|-- scripts/            # Assembler wrapper, mkimg, run-k8086
|-- docs/               # Architecture
|-- tests/              # Host-side / E2E tests
|-- setup.sh            # Init submodule + build k8086 CLI
|-- Makefile
```

## Goals

- Clean-room **5155/5160-compatible** system BIOS (U18 32 KB + U19 8 KB), no ROM BASIC
- Real-mode 8088/8086 OS (`INT 21h`, `.COM` / `.EXE`, FAT12, `COMMAND.COM`)
- Develop and boot under [k8086](https://github.com/Trugath/k8086)

Architecture: [`docs/architecture.md`](docs/architecture.md).

## Chosen stack

- Emulator: k8086 in `emulator/k8086/` (JDK 21+)
- Language: 16-bit x86 assembly (GAS + `scripts/as8086.sh`); freestanding C via vendored wcc where useful
- Image: raw 720 KB FAT12 floppy

## Quick start

```bash
./setup.sh          # submodule + k8086 installDist
make                # firmware/build/{u18.bin,u19.bin,os.img}
make run            # boot OS on our chips (CGA window)
```

Headless (serial log):

```bash
./scripts/run-k8086.sh
```

BIOS service unit tests (boot-sector images) and k8086 default floppy gate:

```bash
make test           # roms + bios service units + os.img e2e + ping gate
make test-fd-img    # disks/fd.img (rmDOS) → A:> with rmDOS U18/U19
make test-ping      # PING.COM → virtual gateway (DE-220 NIC)
make test-dhcp      # DHCP.COM → virtual DHCP lease (DE-220 NIC)
make run-fd         # interactive disks/fd.img on our ROMs
```

Built `u18.bin` / `u19.bin` are installed into `emulator/k8086/roms/` as the
emulator defaults (`make bios` / `make install-roms`). Override at runtime with
`K8086_U18_ROM` / `K8086_U19_ROM`, `run-k8086.sh --u18/--u19`, or the workstation
**New…** / **Edit…** ROM dialogs (per-VM immutable snapshots under `~/.k8086/vms/`).

## Boot flow

1. CPU reset at `0xFFFF0` far-jumps to `F000:E05B` (our POST).
2. POST initializes chipset/BDA/IVT, scans option ROMs, then INT 19h.
3. INT 19h loads the floppy boot sector to `0000:7C00`.
4. Boot reads the FAT12 `RFAT1` loader sector, loads `KERNEL.SYS` into `0070:0000`.
5. Kernel installs INT 20h/21h, runs a quiet FAT R/W self-check, then starts
   `COMMAND.COM` (empty `AUTOEXEC.BAT` → interactive `A:\>` prompt). The image
   layout is `BIN\` (tools including FIND/CHOICE/MORE), `DEMO\` (HELLO/COMPAT),
   `TEST\` (SAMPLE.TXT), with `PATH=A:\BIN`. Interactive `PING`/`DHCP` need the DE-220 card:
   `--card cards/de220/build/libs/de220-*.jar,base=0x300,irq=3,network=default`
   (e.g. `DHCP` then `PING 10.0.2.2`).
