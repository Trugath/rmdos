# Elite (1987 CGA) drop-in

Place the original Firebird PC Elite files here (not redistributed by rmDOS):

- `ELITE.EXE` (required) — typical size ~74 KiB
- Optional: `ELITETRN.COM` or other companions from your disk/zip

Then:

```bash
make run-elite
```

This packs a lean `firmware/build/os-elite.img` (system + Elite only) and boots
k8086 with the CGA display in **nonturbo** realtime.

Copyright: Elite belongs to its rights holders. Keep binaries out of git
(see `.gitignore` in this directory).
