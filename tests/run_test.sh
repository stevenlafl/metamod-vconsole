#!/bin/bash

# Build and run VConsole test client
# Usage: ./run_test.sh [options]
# Options are passed directly to vconsole_test

cd "$(dirname "$0")"

if [ ! -f vconsole_test ] || [ vconsole_test.cpp -nt vconsole_test ]; then
    echo "Building test client..."
    make || exit 1
    echo ""
fi

./vconsole_test "$@"
