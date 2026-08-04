# Wolfenstein 3D drop-in

Place the original id Software Wolfenstein 3D files here (not redistributed by
rmDOS):

- `WOLF3D.EXE` (required) — typical size ~110 KiB (shareware v1.4)
- Companion data (`.WL1` / `.WL6`, maps, audio, etc.) — packed onto the HD

Then:

```bash
make run-wolf3d
```

This packs:

- `firmware/build/os-wolf3d.img` — lean boot floppy (`SHELL=WOLFGO.COM` → `C:\WOLF3D.EXE`)
- `firmware/build/hd-wolf3d.img` — XT ~10 MiB HD (MBR + FAT12 primary) with the drop-in

`WOLFGO.COM` is a tiny shell so COMMAND.COM (~58 KiB) is not resident while the
game runs — that frees conventional heap for the sign-on MAIN gauge / InitGame.

Boots k8086 as **80286** with built-in CGA off, SW1 “special” video, VGA + AdLib
ISA cards (Mode 13h / Mode Y; OPL2 at 388h). `run-wolf3d` is **realtime**
(~8 MHz 286 pacing); use the toolbar Fast Forward for bring-up. Headless
`make test-wolf3d` uses `--turbo`.

Long `REP STOS`/`MOVS` fills yield every 256 iterations when IF=1 so guest IRQ0
can service the AdLib timer; CLI paths (HD INT 13h) do not yield. Realtime play
advances the guest clock via IRQ0 only — no host TimeCount or key-injection
shims.

`make test-wolf3d` skips if no binary.

Copyright: Wolfenstein 3D belongs to its rights holders. Keep binaries out of
git (see `.gitignore` in this directory).
