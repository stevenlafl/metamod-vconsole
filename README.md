# metamod-vconsole

A Metamod plugin that provides a VConsole server for GoldSrc HLDS, enabling remote console access using the Source 2 VConsole protocol.

## Features

- VConsole protocol server compatible with [CS2RemoteConsole](https://github.com/theokyr/CS2RemoteConsole) clients
- Captures all server console output including engine commands (`status`, `stats`, etc.)
- Remote command execution
- Configurable port and bind address
- Connection limiting (default: 1 connection, port closes when connected)
- Optional logging

## Building

Requires Docker for cross-platform builds.

```bash
# Build Docker image (first time only)
./build-docker.sh

# Build the plugin
./build.sh

# Build for specific architecture
METAMOD_VCONSOLE_ARCH=x86 ./build.sh    # 32-bit (for classic HLDS)
METAMOD_VCONSOLE_ARCH=x64 ./build.sh    # 64-bit
```

Output: `./build-x86/Debug/bin/libmetamod-vconsole.so`

## Installation

1. Copy `libmetamod-vconsole.so` to `<game>/addons/metamod-vconsole/dlls/`
2. Copy `config.ini` to `<game>/addons/metamod-vconsole/`
3. Add to `<game>/addons/metamod/plugins.ini`:
   ```
   linux addons/metamod-vconsole/dlls/libmetamod-vconsole.so
   ```

## Configuration

Edit `config.ini`:

```ini
[vconsole]
# Port for VConsole server (default: 29000)
port=29000

# Bind address (default: 0.0.0.0 for all interfaces)
# Use 127.0.0.1 to only allow local connections
bind=0.0.0.0

# Maximum concurrent connections (default: 1)
# Set to 0 for unlimited
max_connections=1

# Enable logging to server console (default: 1)
# Set to 0 to disable [VConsole] log messages
logging=1
```

## Packaging

```bash
./package.sh x86      # Creates release/metamod-vconsole_x86.tar.gz
./package.sh x64      # Creates release/metamod-vconsole_x64.tar.gz
./package.sh both     # Creates both
```

## Testing

A test client is included:

```bash
cd tests
./run_test.sh -p 29000 -c "status"
./run_test.sh --help
```

## License

This project is licensed under the [GNU General Public License v3.0](LICENSE).
