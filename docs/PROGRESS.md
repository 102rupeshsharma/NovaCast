# NovaCast progress report

## Current state at a glance

| Module | Status | What exists |
|--------|--------|-------------|
| cpu | Partial | SH-4 interpreter (full integer and FPU instruction set), exceptions, interrupt acceptance, store queues. No MMU, no on-chip peripherals |
| memory | Partial | Physical address map: RAM, VRAM (both paths), boot ROM, flash reads. No flash writes, no device register blocks |
| core | — | Scheduler (single 200 MHz timebase), Emulator wiring, BIOS/flash loading, reset, `run_for` |
| gpu, audio, disc, controller | Not implemented | — |

NovaCast still **cannot run Dreamcast games or boot a real BIOS to its menu**.
See "Not implemented yet" below.

## Multi-platform restructure: Windows + Android (2026-11-25)

- The engine moved to `core/` (the former `src/core/` system module is now
  `core/system/`), and the Windows app to `desktop/`. `core/CMakeLists.txt`
  builds the engine on its own, with no SDL or GoogleTest. All 164 desktop
  tests pass unchanged after the move.
- New Android app in `mobile/` (Kotlin, Jetpack Compose, NDK r27c). It builds
  the same `core/` through a thin JNI bridge. No emulation code is duplicated.
- One version number for every platform: the `VERSION` file.
- Release workflow `.github/workflows/release.yml`: on a `v*` tag it builds
  `NovaCast-Windows.exe`/`.zip` and a signed `NovaCast-Android.apk` and
  publishes them as a GitHub pre-release. It has not run yet.
- Android was tested on the **Android Emulator only** (Android 15, x86_64),
  not on a physical device. Results are in [testing.md](testing.md).
- A shared-engine bug found through Android testing was fixed: a power-on
  reset did not clear the CPU's instruction counter.

## Phase 1, Step 2 – scheduler, memory map, BIOS loading, SH-4 interpreter

### Implemented

| Item | Where | Tests |
|------|-------|-------|
| Scheduler: fixed event table, deadline ordering, tie order, cancel/reschedule from callbacks, overflow-safe deadlines, lateness reporting | `core/system/scheduler.*` | `Scheduler.*` (9) |
| Physical address map with 64 KB pages: boot ROM (read only), flash (read only), VRAM 64-bit path and 32-bit interleaved path with mirrors, RAM with 4 mirrors, device hook (`map_device`), unmapped accesses (read 0, logged once per page, counted) | `core/memory/bus.*` | `BusTest.*` (13) |
| Boot ROM loading: exact size check (2 MB), blank image rejection, file errors; flash ROM loading (128 KB) | `core/system/emulator.cpp`, `core/memory/bus.cpp` | `Emulator.Bios*`, `Emulator.Flash*` |
| Reset vector: CPU starts at 0xA0000000 (boot ROM) and executes it | `core/cpu/sh4.cpp` | `Emulator.BootsFromResetVector` (synthetic ROM) |
| SH-4 decode table: 209 instruction patterns, 65,536-entry table, overlap check | `core/cpu/sh4_interp.cpp` | `Sh4Decoder.*` |
| SH-4 integer, logic, shift, multiply/MAC (including S-bit saturation), division step (DIV0S/DIV0U/DIV1), data transfer in every addressing mode | `core/cpu/sh4_interp.cpp` | `Sh4Test.*` (division checked against host arithmetic using the manual's own routines) |
| Branches with delay slots, slot-illegal detection, branch targets computed before the slot runs, PC-relative addressing in slots | `core/cpu/sh4.cpp`, `sh4_interp.cpp` | `Sh4Test.*Branch*`, `*Slot*` |
| Exceptions: illegal / slot illegal, TRAPA, address errors (alignment, user-mode privileged area), FPU disable / slot FPU disable, SR.BL=1 → manual reset; SPC/SSR/SGR/EXPEVT/TRA/TEA | `core/cpu/sh4.cpp` | `Sh4Test.*` (11 exception tests) |
| Interrupt acceptance (priority vs SR.IMASK, SR.BL), INTEVT, SLEEP and wake-up | `core/cpu/sh4.cpp` | `Sh4Test.Interrupt*`, `SleepWaitsForInterrupt` |
| Register banks (SR.MD/RB), privileged-instruction checks | `core/cpu/sh4.cpp` | `Sh4Test.SrRbSwapsBanks` and others |
| FPU: single and double precision, FPSCR.PR/SZ/FR/RM/DN, paired and XD moves, FIPR, FTRV, FSCA, FSRRA, FTRC saturation, SH-4 NaN value, host rounding set from FPSCR.RM while running | `core/cpu/sh4_interp.cpp` | `Sh4FpuTest.*` (21) |
| P4 area: CCN registers, area-7 alias, store queues and PREF flush via QACR, operand cache RAM (CCR.ORA) | `core/cpu/sh4.cpp` | `Sh4Test.StoreQueue*`, `Ccn*`, `OperandCacheRam` |
| Headless BIOS runner: `--bios`, `--flash`, `--run-cycles` | `desktop/src/main.cpp`, `command_line.*` | `CommandLine.Bios*`, manual run below |

### Not implemented yet

- **SH-4:** MMU (MMUCR.AT=1 is logged, translation not done; LDTLB is a logged no-op); TMU, INTC priority logic (IPR registers), DMAC, BSC, SCIF, RTC, UBC. Registers of these modules read back what was written and log once. Caches (OCBI/OCBP/OCBWB and cache arrays are no-ops). FPU exception flags, cause bits and traps.
- **Timing:** every instruction costs one cycle. Real SH-4 timing (dual issue, pipeline stalls, memory wait states) is not modelled.
- **Memory:** flash programming/erase command protocol (writes are logged and dropped); Holly system bus, PVR, AICA, G1/G2 register blocks and wave RAM are unmapped.
- **Everything else from the master plan:** Holly interrupt controller and DMA, GD-ROM and disc images, PowerVR2, AICA, Maple/controller/VMU, JIT, save states, UI.

### Known accuracy limits of what is implemented

- FSCA uses `sin`/`cos` and FSRRA uses `1/sqrt`. Real hardware uses lookup tables and approximations, so the lowest bits can differ.
- FIPR, FTRV and FMAC compute in double precision and round once. Hardware precision differs slightly.
- FPU instructions whose behaviour is undefined for the current FPSCR.PR (for example FIPR with PR=1) are executed as no-ops with a one-time warning.
- MAC.W with SR.S=1 saturates MACL and leaves MACH unchanged. This follows the manual's description and has not been checked against hardware.
- The instruction tests were written by hand from the SH-4 Software Manual. They have **not** been compared against real hardware or an independent reference emulator.

### Performance (measured, not a target)

Release build on the machine below, synthetic ROM, tight two-instruction loop (BRA + NOP), 400 million cycles, three runs: **50.7, 60.2 and 71.7 MIPS**. Running in real time needs 200 MIPS (one instruction per cycle at 200 MHz), so the interpreter is currently **about 3-4x slower than real time**, and that is on a best-case loop. A JIT, or at least a faster interpreter path, will be needed for full speed.

### Verification (2026-11-25)

| Check | Result |
|-------|--------|
| Build Debug | OK, 0 warnings in project code |
| Build Release with `NOVACAST_WARNINGS_AS_ERRORS=ON` | OK |
| `ctest --preset debug` | 164/164 passed |
| `ctest --preset release` | 164/164 passed, then 164/164 on each of 5 repeats |
| UBSan (trap mode) + `_GLIBCXX_ASSERTIONS` build, full suite | 164/164 passed, no undefined behaviour detected |
| All 65,536 opcodes executed once each in three FPU modes (`EveryOpcodeExecutesWithoutHostFaults`) | Every opcode executes or raises a guest exception; no host crash; no UB under UBSan |
| Line coverage (gcov), whole suite | `sh4_interp.cpp` 99.8%, `sh4.cpp` 74.3%, `bus.cpp` 99.1%, `scheduler.cpp` 98.1%, `emulator.cpp` 90.5% |
| Line coverage of `sh4_interp.cpp` from tests that **check results** (all-opcodes test excluded) | 63.5%. The rest of the handlers have been run but their results have not been checked. Typical unchecked examples: most STC/LDC/STS/LDS variants, GBR logic ops, R0-indexed FMOV |
| `novacast --bios rom.bin --run-cycles 200000000` (synthetic 2 MB ROM) | Ran to completion; RAM written as programmed; PC parked in its loop; 0 unmapped accesses |
| `novacast --bios README.md --run-cycles 10` | Rejected: "boot ROM must be exactly 2097152 bytes", exit 1 |
| Real Dreamcast BIOS | **Not tested**: no BIOS dump is available on this machine |

Two expectations were corrected during this step, and both are recorded here.
- `Emulator.BootsFromResetVector` and `Emulator.ScheduledEventsFireWhileRunning` first expected time to stop exactly on a deadline. A delayed branch and its slot always run together, so time can pass a deadline by one cycle. The tests now check that bound and that the overrun is reported to the event as `cycles_late`.
- The new test `Emulator.ManySmallSlicesKeepTimeConsistent` then found that these overruns accumulated across `run_for` calls. `run_for` now works towards an absolute target, so overruns no longer accumulate.
- The first version of the all-opcodes test only reset the register struct between opcodes. SLEEP (0x001B) left the CPU's sleeping flag set, so every later opcode was silently skipped. The test now does a full CPU reset each time and asserts that every opcode actually executed.

## Phase 1, Step 1 – project foundation

### Implemented

| Item | Where | Covered by tests |
|------|-------|------------------|
| C++20 CMake project, Debug/Release presets, Visual Studio preset | `CMakeLists.txt`, `CMakePresets.json`, `cmake/` | configure and build |
| SDL2 2.30.9 (static, fetched) or system SDL2 | `cmake/Dependencies.cmake` | `app.window_smoke` |
| GoogleTest 1.15.2 (fetched) | `cmake/Dependencies.cmake` | the test suite itself |
| Separate libraries: common, cpu, gpu, audio, memory, disc, controller, core, cli | `core/CMakeLists.txt`, `desktop/CMakeLists.txt` | build |
| `Result<T>` / `Error` error handling | `core/common/error.*` | `Result.*`, `Error.*` |
| Thread-safe logging: levels, custom sink, file output, timestamps | `core/common/log.*` | `LogTest.*` |
| File helpers with size limits and UTF-8 paths | `core/common/fileio.*` | `FileIo.*` |
| Honest module status reporting | `core/*/…_module.*`, `core/system/emulator.*` | `Modules.*`, `Emulator.*` |
| Command-line parsing with input validation | `desktop/src/command_line.*` | `CommandLine.*` |
| Entry point: SDL window (RAII), event loop, Esc to quit, controller hot-plug logging, top-level exception handler | `desktop/src/main.cpp`, `desktop/src/sdl_window.*` | `app.*` smoke tests |

### Not implemented at the end of Step 1 (historical; see Step 2 above for the current state)

- **CPU:** no SH-4 instruction execution, exceptions, interrupts, FPU or on-chip peripherals.
- **GPU:** no PowerVR2 registers, tile accelerator, textures or rendering. The window shows a plain background only.
- **Audio:** no AICA, ARM7 or sound output.
- **Memory:** no RAM, VRAM, BIOS/flash loading or address map.
- **Disc:** no GDI/CDI/CUE parsing or GD-ROM drive. `load_game` checks the path and extension, then returns `NotImplemented`.
- **Controller:** no Maple bus, controller or VMU emulation. Host gamepads are only detected and logged.
- No timing/scheduler, JIT, save states, settings or UI beyond the empty window.

### Known limitations of the foundation

- Command-line arguments arrive through `main(char**)`, so on Windows a game path containing characters outside the system code page will not be passed correctly. This will be fixed by reading `GetCommandLineW` before disc loading is added.
- The first configure needs network access to download SDL2 and GoogleTest.
- The window uses `SDL_Renderer` only to show that windowing works. The OpenGL renderer arrives with PowerVR2 emulation.

- The Visual Studio 2022 preset is written but has not been tested: MSVC is not installed on the development machine. Only the GCC/MinGW path has been verified.

### Verification (2026-11-25)

Machine: Windows 11 Home 10.0.26200, Intel Core 5 120U, Intel Graphics.
Toolchain: WinLibs GCC 16.2.0 (MinGW-w64 UCRT 14.0), CMake 4.4.4, Ninja 1.13.2.

| Check | Result |
|-------|--------|
| Configure `debug` and `release` presets (SDL2 2.30.9 + GoogleTest 1.15.2 fetched with TLS verification on) | OK |
| Build Debug | OK, 0 warnings in project code |
| Build Release with `NOVACAST_WARNINGS_AS_ERRORS=ON` | OK |
| `ctest --preset debug` | 41/41 passed |
| `ctest --preset release` | 41/41 passed |
| `novacast --frames 120` with a visible window | Window opened (SDL "windows" driver, direct3d renderer), 120 frames at vsync, exit 0 |
| `novacast --status` | Lists all six modules as "not implemented", exit 0 |
| `novacast fake.gdi` | Reports "disc image loading is not implemented yet", exit 1 |
| `novacast --frames abc` | Usage error, exit 2 |

Not checked yet: memory-leak tools (AddressSanitizer is not supported by MinGW GCC; Dr. Memory or an MSVC ASan build will be used once there is emulation state worth checking), and controller hot-plug with a physical gamepad (none attached).
