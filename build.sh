#!/bin/bash
# Build CRTower Defence with pspdev.
# Linux/WSL:  ./build.sh
# Windows:    MSYS_NO_PATHCONV=1 wsl.exe -d Ubuntu --exec bash "/mnt/c/.../build.sh"
set -e
set -o pipefail

export PSPDEV="${PSPDEV:-$HOME/pspdev}"
export PATH="$PSPDEV/bin:$PATH"

if ! command -v psp-gcc >/dev/null 2>&1; then
    echo "psp-gcc not found. Install pspdev and set PSPDEV." >&2
    exit 1
fi

cd "$(dirname "$0")"
make "$@" 2>&1 | tee build.log
if grep -q "stubs out of order" build.log; then
    echo "BUILD ERROR: stubs out of order" >&2
    exit 1
fi
