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

## Image layout

```
A:\
  KERNEL.SYS
  COMMAND.COM
  AUTOEXEC.BAT
  BIN\     DIR TYPE COPY DEL FORMAT FIND CHOICE MORE PING DHCP
  DEMO\    HELLO.COM HELLO.EXE COMPAT.COM STAR.COM
  TEST\    SAMPLE.TXT
```

`PATH=A:\BIN` is set in the kernel environment so tools work from `A:\>`.

`FORMAT [d:] [/S] [/Y]` builds a FAT12 or FAT16 filesystem from INT 13h geometry
(floppy or whole-disk HDD up to 40 MB), optionally installing a bootable rmDOS
system (`/S`). The kernel mounts volumes via the on-disk BPB and can switch to
`C:` when a hard disk is attached.

The same image is installed into the k8086 submodule as `disks/fd.img`
(`make install-floppy` / `make os`).
