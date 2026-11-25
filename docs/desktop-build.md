# Building NovaCast for Windows

The desktop app is `desktop/` plus the shared engine in `core/`, built with
CMake. The result is a single statically linked `novacast.exe` that needs no
DLLs.

## Requirements

- Windows 10 or 11, 64-bit
- CMake 3.21 or newer and Ninja
- A C++20 compiler: MinGW-w64 GCC 13+ (verified with GCC 16.2), or Visual
  Studio 2022 (preset provided, **not yet verified**: MSVC has not been
  available on the development machine)
- Git and internet access on the first configure: CMake downloads SDL2 2.30.9
  and GoogleTest 1.15.2 into the build folder

## Build and test

From a terminal where `gcc`/`g++`, `cmake` and `ninja` are on `PATH`:

```bash
cmake --preset debug   && cmake --build --preset debug   && ctest --preset debug
cmake --preset release && cmake --build --preset release && ctest --preset release
```

With Visual Studio 2022 (unverified):

```powershell
cmake --preset vs2022
cmake --build --preset vs2022-release
ctest --preset vs2022-release
```

The app is written to `build/<preset>/bin/novacast.exe`.

### Portable toolchain used during development

The project was developed with a portable WinLibs toolchain (GCC 16.2,
CMake 4.4, Ninja 1.13) unpacked into `.toolchain/` (git-ignored):

```bash
export PATH="$PWD/.toolchain/mingw64/bin:$PATH"
CA="C:/Program Files/Git/ucrt64/etc/ssl/certs/ca-bundle.crt"
cmake --preset release -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ "-DCMAKE_TLS_CAINFO=$CA"
cmake --build --preset release && ctest --preset release
```

The WinLibs CMake ships without trusted certificate authorities, so downloads
fail with "certificate signer not trusted" unless `CMAKE_TLS_CAINFO` points at
a CA bundle. Git for Windows provides one. Do not disable TLS verification.

## Build options

| Option | Default | Meaning |
|--------|---------|---------|
| `NOVACAST_BUILD_TESTS` | `ON` | Build the tests (downloads GoogleTest) |
| `NOVACAST_USE_SYSTEM_SDL2` | `OFF` | Use an installed SDL2 through `find_package` |
| `NOVACAST_WARNINGS_AS_ERRORS` | `OFF` | Treat warnings as errors (the release workflow turns this on) |

Debug builds define `NOVACAST_DEBUG=1` and keep assertions. Release builds are
optimised and define `NDEBUG`.

## Packaging

The release workflow (`.github/workflows/release.yml`) builds with MSYS2's
UCRT64 GCC and publishes:

- `NovaCast-Windows.exe`: the app itself (statically linked, no installer)
- `NovaCast-Windows.zip`: the exe plus the progress report
- `SHA256SUMS-windows.txt`: checksums

No installer is produced: a single self-contained exe is simpler and enough
for the current feature set.

## Running

```powershell
novacast --status                      # which hardware modules exist
novacast                               # empty window; Esc closes it
novacast --bios dc_boot.bin --run-cycles 200000000
                                       # run your own boot ROM headless for 1 emulated second
novacast --help                        # all options
```
