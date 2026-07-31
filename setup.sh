#!/usr/bin/env bash
# Initialize the k8086 submodule and build its CLI installDist for rmDOS.
# Run from the repository root. Requires JDK 21+.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
K8086_DIR="$ROOT_DIR/emulator/k8086"
LAUNCHER="$K8086_DIR/k8086-emulator/build/install/k8086-emulator/bin/k8086-emulator"

usage() {
    cat <<'USAGE'
Usage: setup.sh [--skip-deps]

Initialize emulator/k8086 (git submodule) and build the k8086 CLI via Gradle
installDist. JDK 21+ is required (Gradle can download a toolchain if needed).

  --skip-deps   Skip package-manager JDK install attempts.
USAGE
}

SKIP_DEPS=0
for arg in "$@"; do
    case "$arg" in
        --help|-h) usage; exit 0 ;;
        --skip-deps) SKIP_DEPS=1 ;;
        *) echo "Unknown option: $arg" >&2; usage >&2; exit 2 ;;
    esac
done

have_jdk21() {
    if ! command -v java >/dev/null 2>&1; then
        return 1
    fi
    local ver
    ver="$(java -version 2>&1 | head -n1 || true)"
    if [[ "$ver" =~ \"(1\.)?(2[1-9]|[3-9][0-9]) ]]; then
        return 0
    fi
    return 1
}

install_jdk_hint() {
    if [[ $SKIP_DEPS -eq 1 ]]; then
        return 0
    fi
    if [[ ! -f /etc/os-release ]]; then
        echo "No /etc/os-release; install a JDK 21+ manually and re-run with --skip-deps if needed." >&2
        return 0
    fi
    # shellcheck source=/dev/null
    source /etc/os-release
    local id_lower="${ID:-}"
    id_lower="${id_lower,,}"
    case "$id_lower" in
        ubuntu|debian|linuxmint|pop)
            if command -v apt-get >/dev/null 2>&1; then
                echo "Installing OpenJDK 21 (apt)..."
                sudo apt-get update -qq
                sudo apt-get install -y openjdk-21-jdk
                return 0
            fi
            ;;
        fedora|rhel|centos|rocky|almalinux)
            if command -v dnf >/dev/null 2>&1; then
                echo "Installing OpenJDK 21 (dnf)..."
                sudo dnf install -y java-21-openjdk-devel
                return 0
            fi
            ;;
        arch|artix)
            if command -v pacman >/dev/null 2>&1; then
                echo "Installing JDK 21 (pacman)..."
                sudo pacman -S --needed --noconfirm jdk21-openjdk
                return 0
            fi
            ;;
        *)
            echo "Unsupported distro for auto JDK install (ID=$ID). Install JDK 21+ and re-run with --skip-deps." >&2
            ;;
    esac
}

if ! have_jdk21; then
    echo "JDK 21+ not found on PATH."
    install_jdk_hint
    if ! have_jdk21; then
        echo "JDK 21+ is still required. Gradle may download a toolchain during the build," >&2
        echo "but a host java is recommended. Continuing..." >&2
    fi
fi

echo "Initializing k8086 submodule..."
# Do not --recursive: k8086's SingleStepTests corpora are multi-GB and unused for run.
git -C "$ROOT_DIR" submodule update --init emulator/k8086

if [[ ! -x "$K8086_DIR/gradlew" ]]; then
    echo "k8086 gradlew not found at $K8086_DIR" >&2
    exit 1
fi

echo "Building k8086 CLI (installDist)..."
(cd "$K8086_DIR" && ./gradlew :k8086-emulator:installDist)

if [[ ! -x "$LAUNCHER" ]]; then
    echo "Build failed: launcher not found at $LAUNCHER" >&2
    exit 1
fi

echo "Done. k8086 launcher is at $LAUNCHER"
echo "Run 'make' to build system ROMs + OS image, then 'make run'."
