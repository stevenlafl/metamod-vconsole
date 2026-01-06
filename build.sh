#!/bin/bash

# Build script for metamod-vconsole Docker container
# Builds x86 and/or x64 architectures based on METAMOD_VCONSOLE_ARCH env var
# Usage: METAMOD_VCONSOLE_ARCH=x86|x64|all ./build.sh

set -e

IMAGE_NAME="metamod-vconsole"
IMAGE_TAG="latest"
CONTAINER_NAME="metamod-vconsole-build"

echo "Running Docker container: ${CONTAINER_NAME}"

# Remove existing container if it exists
docker rm -f "${CONTAINER_NAME}" 2>/dev/null || true

# Run the container and build the code
docker run --name "${CONTAINER_NAME}" \
    -v "$(pwd):/app" \
    -e METAMOD_VCONSOLE_ARCH="${METAMOD_VCONSOLE_ARCH:-all}" \
    "${IMAGE_NAME}:${IMAGE_TAG}" \
    bash -c "
        cd /app

        # Initialize vcpkg submodule if needed
        if [ ! -d 'deps/vcpkg' ]; then
            echo 'Cloning vcpkg...'
            git clone https://github.com/microsoft/vcpkg.git deps/vcpkg
        fi

        echo 'Bootstrapping vcpkg...'
        cd /app/deps/vcpkg && ./bootstrap-vcpkg.sh
        cd /app

        # Build x86
        if [ \"\$METAMOD_VCONSOLE_ARCH\" = \"all\" ] || [ \"\$METAMOD_VCONSOLE_ARCH\" = \"x86\" ] || [ \"\$METAMOD_VCONSOLE_ARCH\" = \"ia32\" ]; then
            echo ''
            echo '=============================================='
            echo 'Building metamod-vconsole for x86...'
            echo '=============================================='
            cmake --preset linux-x86-debug
            cmake --build build-x86 --config Debug
        fi

        # Build x64
        if [ \"\$METAMOD_VCONSOLE_ARCH\" = \"all\" ] || [ \"\$METAMOD_VCONSOLE_ARCH\" = \"x64\" ]; then
            echo ''
            echo '=============================================='
            echo 'Building metamod-vconsole for x64...'
            echo '=============================================='
            cmake --preset linux-x64-debug
            cmake --build build-x64 --config Debug
        fi
    "

echo "Container run completed."

# Show output paths based on what was built
ARCH="${METAMOD_VCONSOLE_ARCH:-all}"
if [ "$ARCH" = "all" ] || [ "$ARCH" = "x86" ] || [ "$ARCH" = "ia32" ]; then
    echo "  x86: ./build-x86/Debug/bin/libmetamod-vconsole.so"
fi
if [ "$ARCH" = "all" ] || [ "$ARCH" = "x64" ]; then
    echo "  x64: ./build-x64/Debug/bin/libmetamod-vconsole.so"
fi

# Copy the built .so file to the HLDS installation (x86 for Half-Life)
HLDS_ADDON_DIR="/home/stevenlafl/Containers/hlds/hlds/ts/addons/metamod-vconsole"
if [ -f "./build-x86/Debug/bin/libmetamod-vconsole.so" ]; then
    echo "Copying x86 libmetamod-vconsole.so to HLDS installation..."
    mkdir -p "${HLDS_ADDON_DIR}/dlls/"
    cp "./build-x86/Debug/bin/libmetamod-vconsole.so" "${HLDS_ADDON_DIR}/dlls/"

    # Copy config.ini if it doesn't exist (don't overwrite user config)
    if [ ! -f "${HLDS_ADDON_DIR}/config.ini" ]; then
        echo "Copying default config.ini..."
        cp "./config.ini" "${HLDS_ADDON_DIR}/"
    fi

    echo "Done"
fi
