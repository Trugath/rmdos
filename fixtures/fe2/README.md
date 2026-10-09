# Frontier Elite II drop-in

Place the original GameTek / Konami PC Frontier files here (not redistributed by
rmDOS):

- `FRONTIER.EXE` (required)
- Overlays (`EL2*.OVL`), graphics (`EL2G.GRF`), AdLib ROL tunes, etc.

Then:

```bash
make run-fe2
```

This packs:

- `firmware/build/os-fe2.img` — lean boot floppy (`DEVICE=EMM.SYS`, `SHELL=FE2GO.COM` → `C:\FRONTIER.EXE`)
- `firmware/build/hd-fe2.img` — XT ~10 MiB HD (MBR + FAT primary) with the drop-in

`FE2GO.COM` is a tiny shell so COMMAND.COM (~58 KiB) is not resident while the
game runs. `EMM.SYS` plus the k8086 `ems-window` card (`pages=80` hex → 128 ×
16 KiB = 2 MiB) provide LIM EMS.

Boots k8086 as **80386 @ 16 MHz** with built-in CGA off, SW1 “special” video,
VGA + AdLib + EMS ISA cards. `run-fe2` uses **`--turbo`** so the large EXE/HD
load is usable; clear turbo from the toolbar for paced play.

`EL2SETUP.OVL` in this drop-in is patched to auto-select AdLib (option `2`) so
bring-up is not stuck on the sound menu’s silent key wait.

Copyright: Frontier Elite II belongs to its rights holders. Keep binaries out of
git (see `.gitignore` in this directory).
