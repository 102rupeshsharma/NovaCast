# NovaCast

NovaCast is an independent Sega Dreamcast emulator written in C++20, being
built from scratch.

> **Status: Phase 1, Step 2.** The SH-4 CPU interpreter, the memory map, BIOS
> loading and the system scheduler work and are tested with synthetic
> programs. There is no video, audio, GD-ROM, controller or SH-4 peripheral
> emulation yet, so NovaCast **cannot run games or boot a real BIOS to its
> menu**. See [docs/PROGRESS.md](docs/PROGRESS.md) for exactly what exists.

## Download

| Platform | Download | Status |
|----------|----------|--------|
| **Windows 10/11 (64-bit)** | [**Download NovaCast for Windows (.zip)**](https://github.com/102rupeshsharma/NovaCast/releases/latest/download/NovaCast-windows-x64.zip) · [SHA-256](https://github.com/102rupeshsharma/NovaCast/releases/latest/download/NovaCast-windows-x64.zip.sha256) | Pre-alpha, version 0.1.0 |
| **Android** | Not available yet | Planned. No Android build exists yet |
| **iOS** | Not available yet | Planned. No iOS build exists yet |

All versions and release notes: [Releases](https://github.com/102rupeshsharma/NovaCast/releases)

> **Read before downloading:** this is an early development build. It
> **cannot play Dreamcast games yet**: it opens an empty window and can run a
> boot ROM headless for testing. There is no video, audio, disc or controller
> emulation yet.

**Installing on Windows**

1. Click the Windows link above. The `.zip` file downloads straight away.
2. Right-click the zip, choose **Extract All**, and open the extracted
   `NovaCast-0.1.0-windows-x64` folder.
3. Double-click `novacast.exe`, or run it from a terminal (see [Running](#running)).
   No installer and no extra DLLs are needed.

Windows SmartScreen may warn about an unrecognised app, because the build
is not code-signed. Choose **More info → Run anyway** only if you trust where
you downloaded it from. To check that the file was not altered, compare its
hash with the published SHA-256 value:

```powershell
Get-FileHash .\NovaCast-windows-x64.zip -Algorithm SHA256
```

NovaCast does not include, and will never include, a Dreamcast BIOS or any games.

## What is in the repository

| Path | Contents |
|------|----------|
| `src/common/` | Fixed-width types, `Result<T>`/`Error` error handling, thread-safe logging (console + file), file helpers |
| `src/cpu/` | SH-4 interpreter (partial: no MMU or on-chip peripherals) |
| `src/gpu/` | PowerVR2 GPU module (not implemented) |
| `src/audio/` | AICA audio module (not implemented) |
| `src/memory/` | Physical address map: RAM, VRAM, boot ROM, flash (partial) |
| `src/disc/` | GD-ROM drive and GDI/CDI/CUE image module (not implemented) |
| `src/controller/` | Maple bus, controller and VMU module (not implemented) |
| `src/core/` | `Emulator` (owns and wires the modules), `Scheduler` (the single 200 MHz timebase) |
| `src/app/` | Entry point, command line, SDL2 window and controller hot-plug detection |
| `tests/` | GoogleTest unit tests plus smoke tests that run the real executable |
| `cmake/` | Dependency setup (SDL2, GoogleTest) and compiler flags |
| `docs/` | Architecture and progress report |

Each hardware module is its own static library that depends only on
`common`. Only `src/app` links SDL2, so the emulation code stays independent of
the frontend.

## Requirements (Windows 11)

- CMake 3.21 or newer
- A C++20 compiler, either:
  - Visual Studio 2022 (or Build Tools 2022) with the "Desktop development with C++" workload, or
  - MinGW-w64 GCC 13 or newer (for example the WinLibs UCRT build)
- Ninja (included with Visual Studio; needed for the `debug`/`release` presets)
- Git and internet access the first time you configure. CMake downloads
  SDL2 2.30.9 and GoogleTest 1.15.2 into the build folder.

## Building

With **Visual Studio 2022** (Debug and Release in one build tree):

```powershell
cmake --preset vs2022
cmake --build --preset vs2022-debug
cmake --build --preset vs2022-release
ctest --preset vs2022-debug
ctest --preset vs2022-release
```

With **Ninja** (run from a "Developer PowerShell for VS 2022", or with MinGW `g++` on `PATH`):

```powershell
cmake --preset debug
cmake --build --preset debug
ctest --preset debug

cmake --preset release
cmake --build --preset release
ctest --preset release
```

With the **portable WinLibs toolchain** used on the development machine
(GCC 16.2, CMake 4.4 and Ninja 1.13 unpacked into `.toolchain/`, which is
git-ignored):

```bash
export PATH="$PWD/.toolchain/mingw64/bin:$PATH"
CA="C:/Program Files/Git/ucrt64/etc/ssl/certs/ca-bundle.crt"
cmake --preset debug   -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ "-DCMAKE_TLS_CAINFO=$CA"
cmake --preset release -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ "-DCMAKE_TLS_CAINFO=$CA"
cmake --build --preset debug   && ctest --preset debug
cmake --build --preset release && ctest --preset release
```

The WinLibs CMake does not come with a list of trusted certificate authorities,
so downloads fail with "certificate signer not trusted" unless
`CMAKE_TLS_CAINFO` points at one. Git for Windows ships one. Don't use
`CMAKE_TLS_VERIFY=0`.

Executables are placed in `build/<preset>/bin/` (for Visual Studio, `build/vs2022/bin/<Config>/`).

### Build options

| Option | Default | Meaning |
|--------|---------|---------|
| `NOVACAST_BUILD_TESTS` | `ON` | Build the tests (downloads GoogleTest) |
| `NOVACAST_USE_SYSTEM_SDL2` | `OFF` | Use an installed SDL2 through `find_package` instead of downloading it |
| `NOVACAST_WARNINGS_AS_ERRORS` | `OFF` | Treat warnings as errors |

Debug builds define `NOVACAST_DEBUG=1` and keep assertions. Release builds are optimised and define `NDEBUG`.

## Running

```powershell
novacast --status        # which hardware modules exist
novacast                 # opens an empty window; Esc closes it
novacast --frames 300    # closes after 300 frames
novacast --log-level debug --log-file novacast.log
novacast game.gdi        # validates the path, then reports that loading is not implemented
novacast --bios dc_boot.bin --run-cycles 200000000   # run your boot ROM headless for 1 emulated second
```

With a real boot ROM, the headless run prints where the CPU stopped. The
BIOS needs hardware that is not emulated yet (Holly, PVR, GD-ROM, SH-4
timers), so it will not get far.

## Legal

NovaCast contains no Sega code, BIOS or game data, and none will be added.
To run software you will need a Dreamcast BIOS and disc images that you dumped
yourself from hardware you own.
