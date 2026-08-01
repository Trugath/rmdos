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
| `AUTOEXEC.STAR.BAT` | `DEMO\STAR` for `os-star.img` |
| `AUTOEXEC.DIR.BAT` | `DIR` / `DIR BIN` for `os-dir.img` |
| `AUTOEXEC.FORMAT.BAT` | `BIN\FORMAT A: /S /Y` for `os-format.img` |
| `AUTOEXEC.FORMAT.HD.BAT` | `BIN\FORMAT C: /Y` then `DIR C:` for `os-format-hd.img` |
| `AUTOEXEC.FDISK.BAT` | `FDISK /AUTO` then `FORMAT C: /S /Y` for partitioned-HD e2e |
| `AUTOEXEC.BATCH.BAT` | Batch language / redirection gate for `os-batch.img` |
| `AUTOEXEC.DISK.BAT` | ATTRIB/LABEL/MOVE/XCOPY/CHKDSK gate for `os-disk.img` |

## Image layout

```
A:\
  KERNEL.SYS
  COMMAND.COM
  AUTOEXEC.BAT
  BIN\     DIR TYPE COPY DEL ATTRIB LABEL MOVE XCOPY CHKDSK SYS FDISK
           FORMAT FIND CHOICE MORE PING DHCP
  DEMO\    HELLO.COM HELLO.EXE COMPAT.COM STAR.COM
  TEST\    SAMPLE.TXT
```

`PATH=A:\BIN` is set in the kernel environment so tools work from `A:\>`.

`FORMAT [d:] [/S] [/Y]` builds a FAT12 or FAT16 filesystem from INT 13h geometry
(floppy or HDD up to 40 MB), optionally installing a bootable rmDOS system
(`/S`). `FDISK /AUTO` creates an active primary DOS partition; FORMAT detects
it, preserves the MBR, and writes the VBR with BPB hidden sectors. Without a DOS
partition table, FORMAT retains its whole-disk HDD behavior.

The same image is installed into the k8086 submodule as `disks/fd.img`
(`make install-floppy` / `make os`).
