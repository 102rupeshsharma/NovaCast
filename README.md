# NovaCast

NovaCast is an independent Sega Dreamcast emulator written in C++20, built
from scratch. One shared emulator engine powers two apps: a **Windows**
desktop app and an **Android** app.

> **Status: early development (version 0.1.0, pre-alpha).** The SH-4 CPU
> interpreter, the memory map, BIOS loading and the system scheduler work and
> are tested. There is no video, audio, GD-ROM, controller or SH-4 peripheral
> emulation yet, so NovaCast **cannot play games or boot a real BIOS to its
> menu** on either platform. See [docs/PROGRESS.md](docs/PROGRESS.md) for exactly
> what exists.

## Download NovaCast

Download the version for your device: the Windows file does not run on
Android, and the Android file does not run on Windows.

| Platform | Download | Version | Requirements |
|----------|----------|---------|--------------|
| **Windows** | Not published yet: see [Releases](https://github.com/102rupeshsharma/NovaCast/releases) | 0.1.0 pre-alpha | Windows 10 or 11, 64-bit |
| **Android** | Not published yet: see [Releases](https://github.com/102rupeshsharma/NovaCast/releases) | 0.1.0 pre-alpha | Android 8.0 or newer, 64-bit ARM (arm64-v8a) or x86_64 |

Direct download links will appear here once the first release has been
published. Every release lists SHA-256 checksums so you can verify your
download.

### Installing on Windows

1. Download `NovaCast-Windows.exe` (or `NovaCast-Windows.zip` and extract it).
2. Run `novacast.exe`. No installer or extra DLLs are needed.
3. Windows SmartScreen may warn about an unrecognised app because the build is
   not code-signed. Choose **More info → Run anyway** only if you downloaded it
   from this repository's Releases page.

### Installing on Android

1. Download `NovaCast-Android.apk` on your phone or tablet.
2. Open it. If Android asks, allow your browser or file manager to
   **install unknown apps** (Settings → Apps → Special app access).
3. Open NovaCast and select a boot ROM (`dc_boot.bin`) dumped from your own
   console. The app runs the shared SH-4 CPU core; there is no video, audio or
   game support yet.

NovaCast does not include, and will never include, a Dreamcast BIOS or games.

## What works today

| Feature | Windows | Android |
|---------|---------|---------|
| Shared SH-4 CPU interpreter and memory map | Yes | Yes (same code) |
| Load a boot ROM and run it (no video) | Yes, `--bios … --run-cycles N` | Yes, file picker plus Start / Pause / Resume / Stop |
| Hardware module status | `--status` | Shown in the app |
| Pause when backgrounded or overheating | n/a | Yes |
| Video, audio, games, controllers, save states | No | No |

## Repository layout

| Path | Contents |
|------|----------|
| `core/` | Shared emulator engine (plain C++20, no platform code): `common`, `cpu`, `gpu`, `audio`, `memory`, `disc`, `controller`, `system` |
| `desktop/` | Windows app (SDL2 window, command line) |
| `mobile/` | Android app (Kotlin, Jetpack Compose, NDK; JNI bridge in `app/src/main/cpp`) |
| `tests/` | GoogleTest suites for the engine and the desktop command line |
| `.github/workflows/release.yml` | Builds both apps and publishes a release when a `v*` tag is pushed |
| `docs/` | [Architecture](docs/architecture.md), [Windows build](docs/desktop-build.md), [Android build](docs/android-build.md), [Testing](docs/testing.md), [Progress](docs/PROGRESS.md) |

## Building from source

- Windows: see [docs/desktop-build.md](docs/desktop-build.md). In short:
  `cmake --preset release && cmake --build --preset release && ctest --preset release`
- Android: see [docs/android-build.md](docs/android-build.md). In short:
  `cd mobile && gradle assembleDebug`

## Legal

NovaCast contains no Sega code, BIOS or game data, and none will be added.
To run software you need a Dreamcast BIOS and disc images that you dumped
yourself from hardware you own.
