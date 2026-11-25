# Testing NovaCast

Every result here was produced by actually running the tests. Anything not
run is listed as **not tested**.

## Test suites

| Suite | Where | Runs on | How to run |
|-------|-------|---------|------------|
| Engine unit and integration tests (GoogleTest) | `tests/common`, `tests/cpu`, `tests/memory`, `tests/system` | Windows (host) | `ctest --preset debug` / `ctest --preset release` |
| Desktop command line | `tests/desktop` | Windows | same |
| Desktop app smoke tests (real executable) | `tests/CMakeLists.txt` (`app.*`) | Windows | same |
| Android instrumented tests: engine through JNI | `mobile/app/src/androidTest` | Android device or emulator | `cd mobile && ./gradlew connectedDebugAndroidTest` |

The engine tests use only synthetic data: hand-assembled SH-4 programs and a
synthetic 2 MB ROM that stores a value and loops. No Sega BIOS or game is
needed or used.

## Results (2026-11-25)

### Shared engine and Windows app

Machine: Windows 11 Home, Intel Core 5 120U, 16 GB. Toolchain: WinLibs GCC 16.2.

| Check | Result |
|-------|--------|
| Debug build | OK, no warnings in project code |
| Release build with warnings as errors | OK |
| `ctest --preset debug` | 164/164 passed |
| `ctest --preset release` | 164/164 passed |
| UBSan (trap mode) build, before the restructure | 164/164 passed, no undefined behaviour |
| Every SH-4 opcode (65,536 × 3 FPU modes) executed without a host crash | passed |
| Packaged `NovaCast-Windows.exe` runs outside the build tree | passed (`--status`, exit 0) |

### Android app

Device: **Android Emulator** (not a physical phone), Pixel 6 profile,
Android 15 (API 35), x86_64 system image, WHPX acceleration.

| Check | Result |
|-------|--------|
| Debug APK build (NDK r27c, Clang) | OK, no warnings in project code |
| Release APK build, minified, signed | OK; `apksigner verify`: valid, APK Signature Scheme v2 |
| APK contents | `libnovacast_jni.so` for arm64-v8a and x86_64; versionName 0.1.0 |
| Instrumented tests (`connectedDebugAndroidTest`) | 5/5 passed: version, module status, BIOS validation, synthetic ROM boots from the reset vector, 100 M cycles keep time consistent |
| Release APK installs and launches | passed; cold start about 1.2 s; native library loads (logcat) |
| Boot ROM chosen with the Android file picker (Storage Access Framework) | passed; CPU at the reset vector (PC=A0000000, SR=700000F0) |
| Start → running | passed; about 200 M instructions per emulated second, as on Windows |
| Pause: instruction count unchanged 5 s later | passed |
| Resume | passed |
| Stop: back to the power-on state (time and instruction count 0) | passed, after fixing the bug described below |
| Home button while running | auto-paused with a message, state kept |
| App force-stopped and relaunched | boot ROM remembered (app-private storage) |
| Rotation to landscape while running | layout adapts; emulation keeps running |
| Crashes (`FATAL` in logcat) over the whole session | 0 |

Interpreter speed in the Android emulator: about 38-48 MIPS (x86_64 guest
on the laptop above). This says nothing about real phones.

### Bugs found by these tests and fixed

- **Stop did not clear the instruction counter (shared engine).** After a
  power-on reset, emulated time was 0 but the instruction count kept its old
  value. Found on Android; Windows had the same behaviour, but no test looked
  at it. `Sh4::reset()` now clears the counter, and
  `Emulator.ResetIsRepeatable` checks it.
- **Stale speed reading while paused (Android UI).** The last MIPS sample
  stayed on screen after pausing; it now shows 0.

## Not tested

| Item | Why |
|------|-----|
| Physical Android phone or tablet, including ARM64 | No device available. The arm64-v8a library builds but has **never run**. |
| Thermal auto-pause | Needs a device that reports thermal status; the emulator does not. The code path is not exercised. |
| Bluetooth / external controllers, touch controls | Not implemented: the engine has no controller emulation yet. |
| Graphics, audio, save states, game loading, cross-platform save compatibility | Not implemented in the engine on any platform. |
| A real Dreamcast BIOS | None available. |
| Visual Studio (MSVC) build | MSVC not installed. |
| GitHub Actions release workflow | Can only run on GitHub; it will first run when a `v*` tag is pushed. |
| Long-duration Android stability (hours) | Only short sessions were run. |
