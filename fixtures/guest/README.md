# Image fixtures and AUTOEXEC variants

`SAMPLE.TXT` and AUTOEXEC scripts for packing `os*.img`. FIND/CHOICE/MORE are
**rmDOS** tools built from `firmware/src/dos/` into `BIN\`.

| File | Role |
|------|------|
| `SAMPLE.TXT` | Input for FIND → `TEST\SAMPLE.TXT` |
| `AUTOEXEC.BAT` | Empty by default so interactive boots drop to `A:\>` |
| `AUTOEXEC.TEST.BAT` | Compat-gate script for `os-compat.img` (`DEMO\COMPAT`, `BIN\FIND`, `BIN\CHOICE`) |
| `AUTOEXEC.PING.BAT` | `BIN\DHCP` then `BIN\PING` for `os-ping.img` |
| `AUTOEXEC.DHCP.BAT` | `BIN\DHCP` for `os-dhcp.img` |
| `AUTOEXEC.TELNET.BAT` | `BIN\DHCP` then `BIN\TELNET localhost 2323` for `os-telnet.img` |
| `CONFIG.NET.SYS` | `INSTALL=A:\BIN\NET.COM` for resident stack (`os-net.img`) |
| `AUTOEXEC.NET.BAT` | `NETTEST` → `DHCP` → `PING` → `NET /U` after CONFIG loads NET (no `LEASE.DAT`) |
| `CONFIG.ANSI.SYS` | `DEVICE=A:\BIN\ANSI.SYS` for CON CSI filter (`os-ansi.img`) |
| `AUTOEXEC.ANSI.BAT` | `DEMO\ANSITST` + `PROMPT $e…` gate for `os-ansi.img` |
| `CONFIG.EMS.SYS` | `DEVICE=A:\BIN\EMM.SYS` for LIM EMS (`os-ems.img`; needs `ems-window` card) |
| `AUTOEXEC.EMS.BAT` | `DEMO\EMSTST` gate for `os-ems.img` |
| `AUTOEXEC.STAR.BAT` | `DEMO\STAR` for `os-star.img` |
| `AUTOEXEC.BIGEXE.BAT` | `DEMO\BIGEXE.EXE` streaming MZ gate (`os-bigexe.img`) |
| `AUTOEXEC.ELITE.BAT` | `ELITE` for lean `os-elite.img` (see `fixtures/guest/elite/`) |
| `AUTOEXEC.DIR.BAT` | `DIR` / `DIR BIN` for `os-dir.img` |
| `AUTOEXEC.FORMAT.BAT` | `BIN\FORMAT A: /S /Y` for `os-format.img` |
| `AUTOEXEC.FORMAT.HD.BAT` | `BIN\FORMAT C: /Y` then `DIR C:` for `os-format-hd.img` |
| `AUTOEXEC.SYS.BAT` | Partition + format + `SYS C:` transfer for the SYS boot e2e |
| `AUTOEXEC.FAT16.HD.BAT` | PARTEDIT + `FORMAT C: /S` + multi-cluster/subdir I/O for `os-fat16-hd.img` |
| `AUTOEXEC.PARTEDIT.BAT` | `PARTEDIT /CREATE` + `/LIST` then `FORMAT C: /S /Y` for partitioned-HD e2e |
| `AUTOEXEC.MULTILET.BAT` | Two primaries → `FORMAT C:`/`D:` lettering gate |
| `AUTOEXEC.EXTPART.BAT` | Primary + extended + logical → `FORMAT C:`/`D:` |
| `AUTOEXEC.SUBST.BAT` | `SUBST E: A:\TEST` round-trip + FIND via E: |
| `AUTOEXEC.BATCH.BAT` | Batch language / redirection / pipe / ERRORLEVEL / CTTY gate |
| `AUTOEXEC.DISK.BAT` | ATTRIB/LABEL/MOVE/XCOPY/CHKDSK gate for `os-disk.img` |
| `AUTOEXEC.GZIP.BAT` | `BIN\GZIP` / `BIN\GUNZIP` file and pipe round-trips for `os-gzip.img` |
| `AUTOEXEC.UTILS.BAT` | `MEM` / `FC` / `TREE` / `SORT` / `EDIT` / `DEBUG` / `MODE` smoke for `os-utils.img` |
| `AUTOEXEC.DISKCOPY.BAT` | `DISKCOPY A: B: /Y` for `os-diskcopy.img` |
| `AUTOEXEC.DISKCOMP.BAT` | `DISKCOPY` then `DISKCOMP A: B: /Y` for `os-diskcomp.img` |
| `DBG.SCR` / `BIG.TXT` / `SHIFT.BAT` | DEBUG script, >4 KiB EDIT fixture, SHIFT `%1` helper |
| `INSTALL.BAT` | Hard-disk install helper: PARTEDIT /CREATE → FORMAT C: /S → DIR C: (on every `os*.img`) |
| `AUTOEXEC.INSTALL.BAT` | Calls `INSTALL.BAT` for `os-install.img` / HD install e2e |

## Image layout

```
A:\
  KERNEL.SYS
  COMMAND.COM
  INSTALL.BAT
  AUTOEXEC.BAT
  BIN\     DIR TYPE COPY DEL ATTRIB LABEL MOVE XCOPY CHKDSK SYS PARTEDIT
           FORMAT FIND CHOICE MORE MEM FC TREE SORT EDIT DEBUG DISKCOPY
           DISKCOMP MODE SUBST PING DHCP TELNET NET GZIP GUNZIP ANSI.SYS
  DEMO\    HELLO.COM HELLO.EXE COMPAT.COM ANSITST.COM STAR.COM
  TEST\    SAMPLE.TXT DBG.SCR BIG.TXT
  SHIFT.BAT
```

`PATH=A:\BIN` is set in the kernel environment so tools work from `A:\>`.
Optional `CONFIG.SYS` (not on default images) can `INSTALL=` `BIN\NET.COM` for a
resident NE2000 stack, or `DEVICE=` `BIN\ANSI.SYS` for ANSI CON filtering.

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
