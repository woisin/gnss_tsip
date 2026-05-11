# GNSS

This directory contains a small standalone TSIP utility for the Protempis/Trimble
`RES SMT 360` / `ICM SMT 360` GNSS timing module.

Main files:

- [src/gnss_tsip.h](src/gnss_tsip.h)
- [src/gnss_tsip.c](src/gnss_tsip.c)
- [src/gnss_set_overdetermined_position.c](src/gnss_set_overdetermined_position.c)

The executable:

- opens the GNSS serial port in `115200 8O1`
- sends a TSIP `0x32` double-precision LLA position
- can force receiver mode `0xBB = 7` for overdetermined clock mode
- can query `0x8F-AC` receiver status
- can save settings with `0x8E-26`

## Building

This section describes how to build for both ARM and native x86-64 architectures.

Target selection is controlled with:

- `-DGNSS_BUILD_TARGET=auto`: try ARM64 cross-compile if available, otherwise host
- `-DGNSS_BUILD_TARGET=x86_64`: force native host build
- `-DGNSS_BUILD_TARGET=aarch64`: force ARM64 cross-compile (fails if compiler is missing)

### Build for Native x86-64

To build for your local machine (x86-64):

```bash
cmake -S . -B build-x86-64 -DGNSS_BUILD_TARGET=x86_64
cmake --build build-x86-64 -j8
```

Verify the binary:
```bash
file build-x86-64/gnss_set_overdetermined_position
```

Expected output: `ELF 64-bit LSB pie executable, x86-64, ...`

### Build for ARM (aarch64)

#### Option 1: Using System Cross-Compiler (Recommended)

Install the ARM cross-compiler if not already present:

**Ubuntu/Debian:**
```bash
sudo apt-get update && sudo apt-get install -y gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
```

Then build:
```bash
cmake -S . -B build-arm -DGNSS_BUILD_TARGET=aarch64
cmake --build build-arm -j8
```

Verify the binary:
```bash
file build-arm/gnss_set_overdetermined_position
```

Expected output: `ELF 64-bit LSB executable, ARM aarch64, ...`

#### Option 2: Using Custom Cross-Compiler Path

If you have a cross-compiler installed at a custom location:

```bash
cmake -S . -B build-arm \
  -DGNSS_BUILD_TARGET=aarch64 \
  -DCMAKE_C_COMPILER=/path/to/aarch64-linux-gnu-gcc \
  -DCMAKE_CXX_COMPILER=/path/to/aarch64-linux-gnu-g++ \
  -DCMAKE_SYSTEM_NAME=Linux \
  -DCMAKE_SYSTEM_PROCESSOR=aarch64
cmake --build build-arm -j8
```

#### Option 3: Using Original GRAND Cross-Compiler

When `gnss/` is configured standalone, the default build target is the local
ARM64 Zynq/Linux cross-compiler if it exists at:

`/home/grand/cross-compiler/gcc-linaro-7.5.0-2019.12-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-gcc`

It will be auto-detected and used by default when running:
```bash
cmake -S . -B build-arm -DGNSS_BUILD_TARGET=aarch64
cmake --build build-arm -j8
```

### Clean Build

To remove old build artifacts and start fresh:

```bash
rm -rf build-arm build-x86-64
```

### Troubleshooting

**Issue:** CMake complains about stale cache
```
The current CMakeCache.txt directory is different than the directory where CMakeCache.txt was created.
```

**Solution:** Clean the build directory and reconfigure:
```bash
rm -rf build-arm
cmake -S . -B build-arm -DGNSS_BUILD_TARGET=aarch64
cmake --build build-arm -j8
```

**Issue:** CMake reports missing AArch64 compiler
```
GNSS_BUILD_TARGET=aarch64 but no aarch64 compiler was found.
```

**Solution:** Install the cross-compiler and verify it is in PATH:
```bash
sudo apt-get update && sudo apt-get install -y gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
which aarch64-linux-gnu-gcc
aarch64-linux-gnu-gcc --version
```

Then retry from a clean build directory:
```bash
rm -rf build-arm
cmake -S . -B build-arm -DGNSS_BUILD_TARGET=aarch64
cmake --build build-arm -j8
```

If needed, force compiler paths explicitly:
```bash
cmake -S . -B build-arm \
  -DGNSS_BUILD_TARGET=aarch64 \
  -DCMAKE_C_COMPILER=/usr/bin/aarch64-linux-gnu-gcc \
  -DCMAKE_CXX_COMPILER=/usr/bin/aarch64-linux-gnu-g++
cmake --build build-arm -j8
```

## Usage

Show help:

```bash
./build-arm/gnss_set_overdetermined_position --help
```

Read receiver status only:

```bash
./build-arm/gnss_set_overdetermined_position /dev/ttyPS1 --query-only
```

Send an accurate fixed position and force overdetermined mode:

```bash
./build-arm/gnss_set_overdetermined_position \
  /dev/ttyPS1 \
  45.123456 \
  -1.234567 \
  1324.5 \
  --force-mode \
  --save
```

Arguments:

- `tty`: serial device connected to the GNSS module, for example `/dev/ttyPS1`
- `lat_deg`: latitude in degrees
- `lon_deg`: longitude in degrees
- `alt_m`: altitude in meters

Options:

- `--query-only`: request and print `0x8F-AC` only
- `--force-mode`: additionally send TSIP `0xBB` with receiver mode `7`
- `--save`: send TSIP `0x8E-26` to store configuration in flash

## Notes

- The receiver may ignore TSIP commands during the first ~10 seconds after boot.
  The program waits about 11 seconds before sending configuration commands.
- `0x32` already causes the receiver to switch to overdetermined timing mode
  according to the vendor documentation, so `--force-mode` is mainly a safety
  option.
- The GNSS protocol is binary TSIP, not plain-text NMEA.

## Reference Docs

See [doc/](doc/) for the vendor manuals.

Most useful references:

- Protempis_RES+ICM-SMT360_UserGuide_v1.8-C.pdf
- Trimble 97975-00.pdf
