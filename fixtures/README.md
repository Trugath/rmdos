# Image fixtures

Host-side files packed into `os*.img` / `test.img`. Layout:

| Dir | Contents |
|-----|----------|
| [`boot/`](boot/) | Default `AUTOEXEC.BAT`, `INSTALL.BAT`, and per-gate `AUTOEXEC.*.BAT` |
| [`config/`](config/) | `CONFIG.*.SYS` variants packed as `CONFIG.SYS` |
| [`testdata/`](testdata/) | → `TEST\` on harness images (`SAMPLE.TXT`, `BIG.TXT`, `DBG.SCR`) |
| [`batch/`](batch/) | Root-of-`A:\` helpers (`SHIFT.BAT`, `CALLTST*.BAT`) |
| [`elite/`](elite/) | Optional 1987 CGA Elite drop-in (gitignored binary) |

FIND/CHOICE/MORE and the rest of `BIN\` are **rmDOS** tools built from
`firmware/src/dos/`.

`boot/*.BAT`, `batch/*.BAT`, and `config/*.SYS` use **CRLF** line endings
(`COMMAND.COM` only terminates batch lines on CR).

| File | Role |
|------|------|
| `testdata/SAMPLE.TXT` | Input for FIND → `TEST\SAMPLE.TXT` |
| `boot/AUTOEXEC.BAT` | Empty by default so interactive boots drop to `A:\>` |
| `boot/AUTOEXEC.TEST.BAT` | Compat-gate script for `os-compat.img` (`DEMO\COMPAT`, `BIN\FIND`, `BIN\CHOICE`) |
| `boot/AUTOEXEC.PING.BAT` | `BIN\DHCP` then `BIN\PING` for `os-ping.img` |
| `boot/AUTOEXEC.DHCP.BAT` | `BIN\DHCP` for `os-dhcp.img` |
| `boot/AUTOEXEC.TELNET.BAT` | `BIN\DHCP` then `BIN\TELNET localhost 2323` for `os-telnet.img` |
| `config/CONFIG.NET.SYS` | `INSTALL=A:\BIN\NET.COM` for resident stack (`os-net.img`) |
| `boot/AUTOEXEC.NET.BAT` | `NETTEST` → `DHCP` → `PING` → `NET /U` after CONFIG loads NET (no `LEASE.DAT`) |
| `config/CONFIG.ANSI.SYS` | `DEVICE=A:\BIN\ANSI.SYS` for CON CSI filter (`os-ansi.img`) |
| `boot/AUTOEXEC.ANSI.BAT` | `DEMO\ANSITST` + `PROMPT $e…` gate for `os-ansi.img` |
| `config/CONFIG.EMS.SYS` | `DEVICE=A:\BIN\EMM.SYS` for LIM EMS (`os-ems.img`; needs `ems-window` card) |
| `boot/AUTOEXEC.EMS.BAT` | `DEMO\EMSTST` gate for `os-ems.img` |
| `config/CONFIG.STUB.SYS` / `boot/AUTOEXEC.STUB.BAT` | Advisory CONFIG no-ops + LASTDRIVE (`os-stubcfg.img`) |
| `boot/AUTOEXEC.MOUSE.BAT` | `BIN\MOUSE` then `DEMO\MOUSETST` for `os-mouse.img` / `make test-mouse` (INT 33h after COM1 inject `0x8903`) |
| `boot/AUTOEXEC.STAR.BAT` | `DEMO\STAR` for `os-star.img` |
| `boot/AUTOEXEC.BIGEXE.BAT` | `DEMO\BIGEXE.EXE` streaming MZ gate (`os-bigexe.img`) |
| `boot/AUTOEXEC.ELITE.BAT` | `ELITE` for lean `os-elite.img` (see [`elite/`](elite/)) |
| `boot/AUTOEXEC.DIR.BAT` | `DIR` / `DIR BIN` for `os-dir.img` |
| `boot/AUTOEXEC.FORMAT.BAT` | `BIN\FORMAT A: /S /Y` for `os-format.img` |
| `boot/AUTOEXEC.FORMAT.HD.BAT` | `BIN\FORMAT C: /Y` then `DIR C:` for `os-format-hd.img` |
| `boot/AUTOEXEC.SYS.BAT` | Partition + format + `SYS C:` transfer for the SYS boot e2e |
| `boot/AUTOEXEC.FAT16.HD.BAT` | PARTEDIT + `FORMAT C: /S` + multi-cluster/subdir I/O for `os-fat16-hd.img` |
| `boot/AUTOEXEC.PARTEDIT.BAT` | `PARTEDIT /CREATE` + `/LIST` then `FORMAT C: /S /Y` for partitioned-HD e2e |
| `boot/AUTOEXEC.MULTILET.BAT` | Two primaries → `FORMAT C:`/`D:` lettering gate |
| `boot/AUTOEXEC.EXTPART.BAT` | Primary + extended + logical → `FORMAT C:`/`D:` |
| `boot/AUTOEXEC.SUBST.BAT` | `SUBST E: A:\TEST` round-trip + FIND via E: |
| `boot/AUTOEXEC.BATCH.BAT` | Batch language / redirection / pipe / ERRORLEVEL / CTTY gate |
| `boot/AUTOEXEC.DISK.BAT` | ATTRIB/LABEL/MOVE/XCOPY/CHKDSK gate for `os-disk.img` |
| `boot/AUTOEXEC.GZIP.BAT` | `BIN\GZIP` / `BIN\GUNZIP` file and pipe round-trips for `os-gzip.img` |
| `boot/AUTOEXEC.UTILS.BAT` | `MEM` / `FC` / `TREE` / `SORT` / `EDIT` / `DEBUG` / `MODE` smoke for `os-utils.img` |
| `boot/AUTOEXEC.DISKCOPY.BAT` | `DISKCOPY A: B: /Y` for `os-diskcopy.img` |
| `boot/AUTOEXEC.DISKCOMP.BAT` | `DISKCOPY` then `DISKCOMP A: B: /Y` for `os-diskcomp.img` |
| `testdata/DBG.SCR` / `testdata/BIG.TXT` / `batch/SHIFT.BAT` | DEBUG script, >4 KiB EDIT fixture, SHIFT `%1` helper |
| `batch/CALLTST.BAT` / `batch/CALLTST2.BAT` | Nested `CALL` helpers for the batch gate |
| `boot/INSTALL.BAT` | Hard-disk install helper: PARTEDIT /CREATE → FORMAT C: /S → DIR C: (on every `os*.img`) |
| `boot/AUTOEXEC.INSTALL.BAT` | Calls `INSTALL.BAT` for `os-install.img` / HD install e2e |

## Image layout

Default packed `os.img` / k8086 `disks/fd.img` (720 KB FAT12) — keep in sync with
[`docs/architecture.md`](../docs/architecture.md):

```
A:\
  KERNEL.SYS
  COMMAND.COM
  INSTALL.BAT
  AUTOEXEC.BAT
  BIN\     DIR TYPE COPY DEL ATTRIB LABEL MOVE XCOPY CHKDSK SYS PARTEDIT
           FORMAT FIND CHOICE MORE MEM FC TREE SORT EDIT DEBUG DISKCOPY
           DISKCOMP MODE SUBST COMP ASSIGN PING DHCP TELNET NET GZIP GUNZIP
           ANSI.SYS EMM.SYS MOUSE.COM CLOCK.COM
  DEMO\    STAR.COM
```

`test.img` and specialized e2e images (`os-compat.img`, `os-ansi.img`, …) add the
harness on top of that base:

```
  DEMO\    HELLO.COM HELLO.EXE COMPAT.COM INT21X.COM ANSITST.COM EMSTST.COM
           MOUSETST.COM STAR.COM
  TEST\    SAMPLE.TXT DBG.SCR BIG.TXT
  SHIFT.BAT
            (os-net.img also: BIN\NETTEST)
```

`PATH=A:\BIN` is set in the kernel environment so tools work from `A:\>`.
Optional `CONFIG.SYS` (not on default images) can `INSTALL=` `BIN\NET.COM` for a
resident NE2000 stack, `DEVICE=` `BIN\ANSI.SYS` for ANSI CON filtering, or
`DEVICE=` `BIN\EMM.SYS` for LIM EMS.

`FORMAT [d:] [/S] [/Y] [/V[:label]] [/F:360|720|1200|1.2|1440] [/1] [/4] [/8]`
builds a FAT12 or FAT16 filesystem from INT 13h geometry (floppy or HDD up to
128 MB), optionally installing a bootable rmDOS system (`/S`).
`SYS [src:] dest:` copies `KERNEL.SYS` and `COMMAND.COM` from the optional
source drive and installs boot metadata on an rmDOS-formatted filesystem
(FORMAT reserves the RFAT sector even without `/S`). Drive letters follow A:/B:
floppies then DOS primaries and extended
logicals on each HD (`80h`…). `PARTEDIT` (`/CREATE` `/CREATEEXT` `/CREATELOG`
`/LIST`) edits primary and extended/logical partitions; FORMAT targets the
letter’s volume. Without a DOS partition table, FORMAT retains its whole-disk
HDD behavior.

To install onto an attached hard disk from a bootable floppy, run `INSTALL`
(or `INSTALL.BAT`). That script runs PARTEDIT, formats `C:` with `/S`, and prints
`INSTALL OK` only when each step succeeds. Workstation VMs need the Fixed Disk
option ROM (`fdrom.bin`) snapshotted with U18/U19 — recreate or Edit ROMs if an
older VM was created without it.

The same image is installed into the k8086 submodule as `disks/fd.img`
(`make install-floppy` / `make os`).
