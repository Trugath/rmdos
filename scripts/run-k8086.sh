#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
K8086_DIR="$ROOT_DIR/emulator/k8086"
LAUNCHER="$K8086_DIR/k8086-emulator/build/install/k8086-emulator/bin/k8086-emulator"
DEFAULT_IMAGE="$ROOT_DIR/firmware/build/os.img"
IMAGE="$DEFAULT_IMAGE"
SERIAL_LOG="$ROOT_DIR/firmware/build/serial.log"
EMU_LOG="$ROOT_DIR/firmware/build/k8086.log"
HEADLESS=1

# Prefer the vendored i686-elf host tools (make/as/ld/objcopy) when present.
HOST_BIN="$ROOT_DIR/tools/host/bin"
if [[ -d "$HOST_BIN" ]]; then
    export PATH="$HOST_BIN:$PATH"
fi

# Prefer freshly built firmware; fall back to shipped copies in the k8086 tree.
U18_ROM="$ROOT_DIR/firmware/build/u18.bin"
U19_ROM="$ROOT_DIR/firmware/build/u19.bin"
SHIPPED_U18="$K8086_DIR/roms/u18.bin"
SHIPPED_U19="$K8086_DIR/roms/u19.bin"

usage() {
    cat <<'USAGE'
Usage: run-k8086.sh [OPTIONS]

Boot an rmDOS floppy image in k8086 using rmDOS U18/U19 by default.

Options:
  --image PATH          Floppy image to boot (default: firmware/build/os.img)
  --hd PATH             Attach hard-disk image as C: (second launcher arg)
  --serial-log PATH     Serial (COM1) output log file
  --emu-log PATH        Host emulator stdout/stderr log
  --display NAME        Open the video window (interactive). Headless is default.
  --cpu MODEL           Motherboard CPU: 8088 (default), 8086, or 80286
  --mhz N               Guest clock MHz (8088: 4.77; 286: 6/8/10/12.5)
  --no-cga              Disable built-in CGA (pair with a VGA ISA --card)
  --initial-video MODE  SW1 video: cga80 (default) or special (card BIOS)
  --card SPEC           ISA card jar[,k=v...] (repeatable)
  --turbo               Free-run CPU (fast boot; click toolbar to return to realtime)
  --no-floppy-int13-shim  Guest BIOS owns floppy INT 13h (FDC); default is host shim
  --u18 PATH            Override U18 system ROM
  --u19 PATH            Override U19 system ROM
  --help                Show this help message

Also honors K8086_U18_ROM / K8086_U19_ROM if set in the environment before launch
(the --u18/--u19 flags take precedence). K8086_FLOPPY_INT13_SHIM=0 disables the
host floppy INT 13h shim (same as --no-floppy-int13-shim).
USAGE
}

TURBO=0
NO_FLOPPY_INT13_SHIM=0
HD_IMAGE=""
CPU=""
MHZ=""
NO_CGA=0
INITIAL_VIDEO=""
CARDS=()
while [[ $# -gt 0 ]]; do
    case "$1" in
        --help|-h) usage; exit 0 ;;
        --image) IMAGE="$2"; shift 2 ;;
        --hd) HD_IMAGE="$2"; shift 2 ;;
        --serial-log) SERIAL_LOG="$2"; shift 2 ;;
        --emu-log) EMU_LOG="$2"; shift 2 ;;
        --display) HEADLESS=0; shift 2 ;;
        --cpu) CPU="$2"; shift 2 ;;
        --mhz) MHZ="$2"; shift 2 ;;
        --no-cga) NO_CGA=1; shift ;;
        --initial-video) INITIAL_VIDEO="$2"; shift 2 ;;
        --card) CARDS+=("$2"); shift 2 ;;
        --turbo) TURBO=1; shift ;;
        --no-floppy-int13-shim) NO_FLOPPY_INT13_SHIM=1; shift ;;
        --u18) U18_ROM="$2"; shift 2 ;;
        --u19) U19_ROM="$2"; shift 2 ;;
        *)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

if [[ ! -x "$LAUNCHER" ]]; then
    echo "k8086 launcher not found at $LAUNCHER" >&2
    echo "Run ./setup.sh from the repository root first." >&2
    exit 1
fi

if [[ "$IMAGE" == "$DEFAULT_IMAGE" ]]; then
    if ! command -v make >/dev/null 2>&1; then
        echo "make not found on PATH." >&2
        echo "Install GNU make, or unpack the host toolchain into tools/host/bin" >&2
        echo "(so tools/host/bin/make exists), then retry." >&2
        exit 1
    fi
    echo "Building..."
    (cd "$ROOT_DIR" && make) || exit 1
fi

if [[ ! -f "$IMAGE" ]]; then
    echo "Build failed: $IMAGE not found." >&2
    exit 1
fi

if [[ -n "$HD_IMAGE" && ! -f "$HD_IMAGE" ]]; then
    echo "Hard-disk image not found: $HD_IMAGE" >&2
    exit 1
fi

if [[ ! -f "$U18_ROM" ]]; then
    U18_ROM="$SHIPPED_U18"
fi
if [[ ! -f "$U19_ROM" ]]; then
    U19_ROM="$SHIPPED_U19"
fi

if [[ ! -f "$U18_ROM" || ! -f "$U19_ROM" ]]; then
    echo "System ROMs not found:" >&2
    echo "  $U18_ROM" >&2
    echo "  $U19_ROM" >&2
    echo "Run 'make bios' (or 'make install-roms') first, or pass --u18/--u19." >&2
    exit 1
fi

mkdir -p "$(dirname "$SERIAL_LOG")" "$(dirname "$EMU_LOG")"
: >"$SERIAL_LOG"
: >"$EMU_LOG"

ARGS=("$IMAGE")
if [[ -n "$HD_IMAGE" ]]; then
    ARGS+=("$HD_IMAGE")
fi
ARGS+=(--quiet --serial-log "$SERIAL_LOG")
if [[ $HEADLESS -eq 1 ]]; then
    ARGS+=(--headless)
fi
if [[ -n "$CPU" ]]; then
    ARGS+=(--cpu "$CPU")
fi
if [[ -n "$MHZ" ]]; then
    ARGS+=(--mhz "$MHZ")
fi
if [[ $NO_CGA -eq 1 ]]; then
    ARGS+=(--no-cga)
fi
if [[ -n "$INITIAL_VIDEO" ]]; then
    ARGS+=(--initial-video "$INITIAL_VIDEO")
fi
for card in "${CARDS[@]+"${CARDS[@]}"}"; do
    ARGS+=(--card "$card")
done
if [[ $TURBO -eq 1 ]]; then
    ARGS+=(--turbo)
fi
if [[ $NO_FLOPPY_INT13_SHIM -eq 1 ]]; then
    ARGS+=(--no-floppy-int13-shim)
fi

export K8086_U18_ROM="$U18_ROM"
export K8086_U19_ROM="$U19_ROM"

set +e
if [[ $HEADLESS -eq 1 ]]; then
    (
        cd "$K8086_DIR"
        "$LAUNCHER" "${ARGS[@]}"
    ) >"$EMU_LOG" 2>&1
    status=$?
else
    (
        cd "$K8086_DIR"
        "$LAUNCHER" "${ARGS[@]}"
    ) 2>&1 | tee "$EMU_LOG"
    status=${PIPESTATUS[0]}
fi
set -e

if [[ $status -ne 0 ]]; then
    echo "k8086 exited with code $status. Log: $EMU_LOG" >&2
    if [[ $HEADLESS -eq 1 && -s "$EMU_LOG" ]]; then
        tail -n 40 "$EMU_LOG" >&2 || true
    fi
fi

exit "$status"
