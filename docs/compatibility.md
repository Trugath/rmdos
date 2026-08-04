# rmDOS compatibility matrix

Quick expectations for apps, disks, and adapters. Detail and ABI notes live in
[`architecture.md`](architecture.md).

**Baseline:** IBM PC/XT-class (5155/5160), real mode only, DOS **3.31-ish**,
CGA, FAT12/FAT16 volumes **≤128 MiB**. Developed/tested on
[k8086](https://github.com/Trugath/k8086).

## In scope (expected to work)

| Area | Support |
|------|---------|
| BIOS video | CGA text + graphics modes 0–6; INT 10h AH=00–0F (light-pen stub) |
| BIOS disk | Floppy via onboard FDC; HD via C800 Fixed Disk option ROM (`fdrom.bin`) |
| BIOS I/O | COM1/COM2, LPT1/LPT2 (BDA probe); timer; keyboard; INT 15h wait/config |
| Memory | Conventional RAM; optional UMB (`mem-expansion`); LIM EMS 3.2 (`EMM.SYS` + `ems-window`) |
| Filesystem | FAT12/FAT16; 8.3 names; partition bases and HiddenSectors are **32-bit** |
| DOS API | Broad INT 21h / FCB / handles / MCB / EXEC (COM + MZ stream) / batch shell |
| Drivers | Character `DEVICE=` `.SYS` (≤8 KiB), e.g. `ANSI.SYS`, `EMM.SYS` |
| Net tools | `PING` / `DHCP` / `TELNET` / optional `NET.COM` (rmDOS INT 60h mux, DE-220) |

## Honest stubs (accepted; not full DOS)

| Surface | Behavior |
|---------|----------|
| `BUFFERS=` | LoL buffer-header chain only; FAT I/O stays windowed |
| `STACKS=` / `FCBS=` / `DRIVPARM=` | Parsed, no-op |
| File lock `AH=5Ch` | CF, AX=1 (SHARE not installed) |
| Network `AH=5Dh`/`5Eh`/`5Fh` | CF, AX=1 (redirector not installed) |
| INT 2Fh SHARE/PRINT/APPEND/XMS | Install-check stubs only |
| IOCTL AL=04/05/0Dh | Unsupported control channels |
| `SHELL=` in CONFIG | Path only (`/P` `/E:` discarded there; COMMAND honors `/E:n`) |
| Pipes | Sequential temp files, not concurrent DOS pipes |
| INT 17h floating LPT | Status forced ready/selected |
| GZIP compress | Stored DEFLATE blocks (inflate accepts fixed/dynamic) |

## Intentional out of scope

| Item | Notes |
|------|-------|
| Cassette BASIC / ROM BASIC | U19 is pad; INT 18h prints “no BASIC” |
| XMS / HIMEM / A20 / protected mode / extenders | Later project may grow beyond real mode |
| FAT32, LFN, volumes >128 MiB | Hard ceiling |
| Block `DEVICE=` drivers | Rejected; character-only |
| MDA / EGA / VGA | Motherboard BIOS is CGA-only; k8086 `cards/vga` adds mode 03h/13h/Mode Y (WM0 rotate/set-reset/bitmask, WM1/WM2 + latches, ATC pel pan, INT 10h overscan AH=10h/AL=01). CPU `MOV` to memory is write-only (no dest read) so VGA WM1 latch copies work. AdLib card: OPL2 2-op FM at 388h (timer detect). Wolf3D uses `SHELL=WOLFGO.COM` (~50 KiB more MAIN heap). Mid-REP string quanta (IF=1 only) let IRQ0 run during long fills; CLI disk transfers stay atomic. Still OOS on VGA: WM3, split-screen compose; no XMS/SB |
| COM3–4 / LPT3 | |
| Crynwr packet driver / INT 2Fh redirector | Use rmDOS INT 60h `AH=B8h` or standalone COMs |
| JOIN, full SHARE/PRINT/APPEND bodies | |
| Classic externals | No PRINT/BACKUP/RESTORE/REPLACE/RECOVER/APPEND/JOIN |

## Media and partitions

- Floppies: 360K / 720K / 1.2M / 1.44M via BDA / INT 1Eh.
- HD: DOS primaries `01h`/`04h`/`06h` and extended logicals `05h`/`0Fh`; whole-disk FAT VBR at LBA 0.
- Partition start LBA may be **≥65536**; boot uses dword HiddenSectors.
- Malformed BPBs fail mount with serial `fat fail` (no divide-by-zero).
- `PARTEDIT` create paths are still oriented at typical small XT images; pre-seeded high-LBA MBRs are supported by kernel/FORMAT/boot.

## Known false-success / emulator notes

- Validation is **k8086-first**; no second-emulator/hardware gate in CI.
- Floating LPT can look “ready” with no printer attached.
- Elite / Wolf3D / third-party EXEs are optional smoke only when the binary is present.
