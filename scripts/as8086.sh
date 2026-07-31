#!/usr/bin/env bash
# Assemble with GNU as, prepending 8086-safe Jcc macros so GAS cannot emit
# 386 Jcc-rel16 (`0F 8x`), which is POP CS on real 8088 hardware.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MACRO_INC="firmware/src/inc/jcc_8086.inc"
OUT=""
SRC=""
AS_ARGS=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        -o)
            OUT="${2:-}"
            shift 2
            ;;
        --32)
            # Always passed through; kept for drop-in compatibility with `as --32`.
            shift
            ;;
        --defsym)
            AS_ARGS+=("$1" "${2:-}")
            shift 2
            ;;
        -*)
            AS_ARGS+=("$1")
            shift
            ;;
        *)
            SRC="$1"
            shift
            ;;
    esac
done

if [[ -z "$OUT" || -z "$SRC" ]]; then
    echo "usage: as8086.sh --32 -o <obj> <source.s>" >&2
    exit 2
fi

if [[ ! -f "$ROOT_DIR/$MACRO_INC" ]]; then
    echo "as8086: missing $MACRO_INC" >&2
    exit 1
fi

mkdir -p "$ROOT_DIR/firmware/build"
TMP="$ROOT_DIR/firmware/build/as8086.$$.$RANDOM.s"
trap 'rm -f "$TMP"' EXIT

{
    printf '.include "%s"\n' "$MACRO_INC"
    # Support absolute or cwd-relative sources from Make rules.
    if [[ "$SRC" = /* ]]; then
        cat "$SRC"
    else
        cat "$ROOT_DIR/$SRC"
    fi
} >"$TMP"

cd "$ROOT_DIR"
as --32 -I "$ROOT_DIR" "${AS_ARGS[@]}" -o "$OUT" "$TMP"
