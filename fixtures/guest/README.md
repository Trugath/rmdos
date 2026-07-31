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

## Image layout

```
A:\
  KERNEL.SYS
  COMMAND.COM
  AUTOEXEC.BAT
  BIN\     DIR TYPE COPY DEL FIND CHOICE MORE PING DHCP
  DEMO\    HELLO.COM HELLO.EXE COMPAT.COM STAR.COM
  TEST\    SAMPLE.TXT
```

`PATH=A:\BIN` is set in the kernel environment so tools work from `A:\>`.

The same image is installed into the k8086 submodule as `disks/fd.img`
(`make install-floppy` / `make os`).
