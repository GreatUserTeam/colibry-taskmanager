#!/bin/sh
# Build and install colibry-taskmanager from source.
# Usage: ./install.sh [--prefix=/usr/local]
set -eu

PREFIX="${PREFIX:-/usr/local}"
BUILD_DIR="${BUILD_DIR:-build}"

# The only supported flag; environment variables work too.
case "${1:-}" in
    --prefix=*) PREFIX="${1#--prefix=}" ;;
    "") ;;
    *) echo "Unknown option: $1 (use --prefix=/path)" >&2; exit 1 ;;
esac

if command -v ninja >/dev/null 2>&1; then
    GENERATOR="Ninja"
else
    GENERATOR="Unix Makefiles"
fi

cmake -S . -B "$BUILD_DIR" -G "$GENERATOR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DColibry_WARNINGS_AS_ERRORS=ON
cmake --build "$BUILD_DIR"

if command -v ctest >/dev/null 2>&1; then
    QT_QPA_PLATFORM=offscreen ctest --test-dir "$BUILD_DIR" --output-on-failure
fi

if [ "$(id -u)" -eq 0 ]; then
    cmake --install "$BUILD_DIR"
elif command -v sudo >/dev/null 2>&1; then
    sudo cmake --install "$BUILD_DIR"
elif command -v doas >/dev/null 2>&1; then
    doas cmake --install "$BUILD_DIR"
else
    echo "Built in $BUILD_DIR; rerun as root (or install sudo/doas) to install to $PREFIX" >&2
    exit 1
fi
