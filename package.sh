#!/bin/bash
set -e

# Package release script for metamod-vconsole
# Usage: ./package.sh [x86|x64|both]

ARCH="${1:-both}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RELEASE_DIR="$SCRIPT_DIR/release"

package_arch() {
    local arch=$1
    local so_path

    if [ "$arch" = "x86" ]; then
        so_path="build-x86/Debug/bin/libmetamod-vconsole.so"
    else
        so_path="build-x64/Debug/bin/libmetamod-vconsole.so"
    fi

    if [ ! -f "$so_path" ]; then
        echo "Error: $so_path not found. Build first with:"
        echo "  METAMOD_VCONSOLE_ARCH=$arch ./build.sh"
        return 1
    fi

    local out_dir="$RELEASE_DIR/metamod-vconsole_$arch"
    echo "Packaging $arch release to $out_dir..."

    # Clean and create structure
    rm -rf "$out_dir"
    mkdir -p "$out_dir/addons/metamod-vconsole/dlls"

    # Copy native plugin
    cp "$so_path" "$out_dir/addons/metamod-vconsole/dlls/"

    # Copy config.ini
    cp "$SCRIPT_DIR/config.ini" "$out_dir/addons/metamod-vconsole/"

    # Create tarball
    cd "$out_dir"
    tar -czvf "../metamod-vconsole_$arch.tar.gz" addons
    cd "$SCRIPT_DIR"

    # Clean up temp directory
    rm -rf "$out_dir"

    echo "Created: $RELEASE_DIR/metamod-vconsole_$arch.tar.gz"
}

mkdir -p "$RELEASE_DIR"

case "$ARCH" in
    x86|ia32)
        package_arch x86
        ;;
    x64)
        package_arch x64
        ;;
    both|all)
        package_arch x86
        package_arch x64
        ;;
    *)
        echo "Usage: $0 [x86|x64|both]"
        exit 1
        ;;
esac

echo "Done!"
