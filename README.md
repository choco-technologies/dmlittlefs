# dmlittlefs

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![CI](https://github.com/choco-technologies/dmlittlefs/actions/workflows/ci.yml/badge.svg)](https://github.com/choco-technologies/dmlittlefs/actions/workflows/ci.yml)

dmlittlefs - [littlefs](https://github.com/littlefs-project/littlefs) for DMOD: a small, power-loss resilient file system with wear levelling for NOR flash.

## Description

dmlittlefs implements the dmfsi file system interface, so it is mounted through dmvfs like any other
file system. It lives on a block device that can be erased - a dmdevfs node of a flash chip such as
`/dev/dmspiflashx/flash` (dmspiflash, the N25Q128A on the STM32F746G-DISCO) - or on an image file on the host.

```
mklittlefs device=/dev/dmspiflashx/flash          # once: create an empty file system (ALL DATA IS LOST)
mount -t dmlittlefs -o device=/dev/dmspiflashx/flash /mnt/flash
```

Why littlefs and not FAT on a NOR flash: it never overwrites in place (copy-on-write), survives a power loss
at any moment, spreads the erases over the whole chip and needs only a few hundred bytes of RAM. See
[docs/README.md](docs/README.md) for the mount options, what is and is not supported, and the geometry rules.

Contents: the module (`dmlittlefs`), the formatting tool (`mklittlefs`), and the unmodified littlefs 2.10.2
sources in `lib/littlefs` (BSD-3-Clause, see `lib/littlefs/LICENSE.md`).

## Building

### Using CMake

```bash
mkdir -p build
cd build
cmake ..
cmake --build .
```

Pass `-DDMOD_DIR=/path/to/local/dmod` to build against a local dmod checkout
instead of fetching `develop` from GitHub.

### Using Make

```bash
make DMOD_MODE=DMOD_MODULE DMOD_DIR=/path/to/dmod
```

## Testing

Tests are built automatically alongside the module (see `tests/`). Once built,
run them with `ctest`:

```bash
cd build
ctest --output-on-failure
```

`ctest` installs the test module's dependencies with `dmf-get` and then runs
it through `dmod_loader`. To run it manually instead:

```bash
export DMOD_DMF_DIR=$(pwd)/build/dmf
dmf-get install -d ${DMOD_DMF_DIR}/test_dmlittlefs-local.dmd -y
dmod_loader build/dmf/test_dmlittlefs.dmf
```

## Usage

<TBD>

This library module provides functions that can be used by other modules:

```c
#include "dmlittlefs.h"
```

## API

| Function | Description |
|----------|-------------|
| `dmlittlefs_create()` | Create a new `dmlittlefs_t` instance. |
| `dmlittlefs_destroy()` | Destroy an instance created by `_create()`. |
| `dmlittlefs_is_valid()` | Check whether a handle is a valid instance. |

See [include/dmlittlefs.h](include/dmlittlefs.h) for the full
declarations and [docs/api-reference.md](docs/api-reference.md) for the
complete reference.

## Documentation

See the `docs/` directory:

- **[api-reference.md](docs/api-reference.md)** - Complete API documentation

View documentation using `dmf-man dmlittlefs`.
## Project Structure

```
dmlittlefs/
├── docs/              # Documentation (markdown format)
├── include/           # Public headers
│   └── dmlittlefs.h
├── src/
│   └── dmlittlefs.c
├── tests/
│   ├── CMakeLists.txt
│   └── dmlittlefs_test.c
├── CMakeLists.txt
├── Makefile
├── dmlittlefs.dmr
└── manifest.dmm
```

## Author

Patryk Kubiak

## License

MIT
