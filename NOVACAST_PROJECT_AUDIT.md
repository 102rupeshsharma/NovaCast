# NovaCast Project Audit: Master Requirements, Phase 1 and Phase 2

**Audit date:** 2026-11-25
**Scope:** whole repository at `D:\MyWorkspace\CastSphere` (no version control, so there is no commit hash to cite)
**Audit machine:** Windows 11 Home 10.0.26200, Intel Core 5 120U, Intel Graphics, 60 Hz 1920x1200 display
**Toolchain:** WinLibs GCC 16.2.0 (MinGW-w64 UCRT 14.0), CMake 4.4.4, Ninja 1.13.2
**Source changes made during the audit:** none. All extra builds (sanitizer, coverage, from scratch) were done in a separate scratch folder using compiler flags only.

---

## 1. Executive summary

NovaCast is a **project skeleton**. It contains a working, well-tested *foundation*: a build system, error handling, logging, file helpers, command-line parsing and an SDL2 window. It contains **no Dreamcast hardware emulation of any kind**.

- The six hardware modules (`cpu`, `gpu`, `audio`, `memory`, `disc`, `controller`) are **14 lines each**. Each one only returns a description that says it is not implemented. There is no SH-4 interpreter, no memory map, no BIOS loading, no disc parser, no PowerVR2, no AICA, no Maple bus, no VMU, no JIT and no scheduler.
- The code is honest about this. Every module reports `NotImplemented`, the app prints a warning for each one, and loading a game returns "not implemented" instead of pretending to work. I found **no fake or simulated hardware behaviour**.
- What exists works: **41/41 tests pass** in Debug and Release, Release builds with warnings as errors, 10 repeated test runs and 50 app start/stop cycles all passed, and a 20-second run showed flat memory use.
- **Phase 1 is not complete.** Only the "foundation" sub-step (Phase 1, Step 1) is done. **Phase 2 has not started.** No game can be booted, so real-game testing and performance benchmarking are impossible.

**Final verdict: 1. NOT READY.**

---

## 2. Master requirements audit (traceability matrix)

Status key: COMPLETE / PARTIALLY COMPLETE / INCOMPLETE / FAILED / NOT TESTED.

| # | Requirement | Expected behaviour | Source files | Status | Test performed | Result | Missing | Evidence |
|---|---|---|---|---|---|---|---|---|
| M1 | Modular architecture | Separate CPU/GPU/audio/memory/disc/controller modules | `src/*/`, `src/CMakeLists.txt` | PARTIALLY COMPLETE | Checked the build graph; each module links only `novacast_common` | Structure correct | Modules are empty, so the boundaries between them are untested | `src/CMakeLists.txt` lines 17-22 |
| M2 | C++20 / CMake / SDL2 | Builds with C++20, CMake, SDL2 | `CMakeLists.txt`, `cmake/*` | COMPLETE | Debug + Release builds; compile commands inspected | `-std=c++20` in use; SDL 2.30.9 linked | — | `compile_commands.json`; app log "SDL 2.30.9" |
| M3 | Graphics API (Vulkan/OpenGL) | Renderer on OpenGL or Vulkan | — | INCOMPLETE | — | — | No renderer. The window uses `SDL_Renderer` (Direct3D) only to clear the screen | `src/app/sdl_window.cpp` |
| M4 | Dear ImGui UI | Modern UI | — | INCOMPLETE | — | — | Phase 3; not started (correctly) | — |
| M5 | SH-4 CPU emulation | Interpreter + JIT | `src/cpu/cpu_module.*` | INCOMPLETE | Read source | Status-only, 14 lines | Everything | `cpu_module.cpp` |
| M6 | GPU (PowerVR2) | TA, ISP/TSP, textures, rendering | `src/gpu/gpu_module.*` | INCOMPLETE | Read source | Status-only | Everything | `gpu_module.cpp` |
| M7 | Audio (AICA) | ARM7, channels, mixing, output | `src/audio/audio_module.*` | INCOMPLETE | Read source | Status-only | Everything | `audio_module.cpp` |
| M8 | Memory management | RAM/VRAM/ROM, address map | `src/memory/memory_module.*` | INCOMPLETE | Read source | Status-only | Everything | `memory_module.cpp` |
| M9 | BIOS integration | Load, validate, boot BIOS | — | INCOMPLETE | grep for BIOS loading code | None found | Everything | — |
| M10 | GD-ROM support | GDI/CDI parsing, drive, DMA | `src/disc/disc_module.*`, `src/core/emulator.cpp` | INCOMPLETE | `Emulator.LoadGame*` tests; ran app with a fake `.gdi` | Path and extension checked, then returns `NotImplemented` (correct and honest) | All parsing and drive emulation | `emulator.cpp:36-53`; app exit 1 with "not implemented" |
| M11 | Controller hardware | Maple bus controller | `src/controller/*`, `src/app/sdl_window.cpp` | INCOMPLETE | Read source | Host gamepads only detected and logged; no Dreamcast controller emulation | Maple bus, device protocol, input mapping | `sdl_window.cpp:50-62` |
| M12 | VMU support | VMU storage | — | INCOMPLETE | — | — | Everything | — |
| M13 | Save functionality | VMU saves, save states | — | INCOMPLETE | — | — | Everything | — |
| M14 | Error handling and logging | Structured errors, logs, clean failure | `src/common/error.*`, `log.*`, `src/app/main.cpp` | COMPLETE (for current scope) | 14 unit tests; bad-argument and missing-file app runs | All pass; exit codes 2 / 1 as designed | Will need extending to hardware faults | `ctest` output; section 7 |
| M15 | Performance optimisation | JIT, efficient rendering | — | INCOMPLETE | — | — | Nothing to optimise yet | — |
| M16 | Automated testing | Unit + integration tests per component | `tests/` | PARTIALLY COMPLETE | Ran the full suite | 41/41 pass | No hardware tests possible: no hardware code | Section 7 |
| M17 | Compatibility testing | Game database, boot tests | — | INCOMPLETE | — | — | No database; no game can run | — |
| M18 | Customisable controls, opacity, styles, library, save states (Phase 3) | — | — | INCOMPLETE | — | — | Phase 3 not started (correct per sequence) | — |
| M19 | Documentation | Architecture, build, progress | `README.md`, `docs/*` | PARTIALLY COMPLETE | Read and compared with code | Accurate on the whole | `ARCHITECTURE.md` still says Step 1 is "pending build verification" (stale) | `docs/ARCHITECTURE.md` "Plan" section |

---

## 3. Phase 1 audit

### A. Emulator foundation

| Item | Status | Evidence |
|---|---|---|
| Application initialisation | COMPLETE | App starts; SDL init, window, Direct3D renderer logged |
| Emulator initialisation | PARTIALLY COMPLETE | `Emulator::create()` works (100 create/destroy cycles in `Emulator.CreateAndDestroyRepeatedly`), but it starts no hardware because there is none |
| Emulator shutdown / resource cleanup | COMPLETE for current scope | RAII for SDL session, window, renderer, controllers; 50 app start/stop cycles, 0 failures; handle count constant at 418 over a 20 s run |
| Logging | COMPLETE | Levels, sink, file output, truncation, lazy argument evaluation all tested |
| Error handling | COMPLETE | `Result<T>`/`Error`; top-level exception handler in `main` |
| Configuration handling | INCOMPLETE | Command-line options only; no persistent configuration file |

### B. SH-4 CPU foundation: **INCOMPLETE**
No register file, CPU state, decoder, execution loop, memory interface or exception model exists. `src/cpu/cpu_module.cpp` only returns a status record.

### C. Memory system: **INCOMPLETE**
No address space, RAM, read/write paths, register mapping, bounds handling, alignment or endianness handling. The only memory-related code is a `static_assert` requiring a little-endian host (`src/common/types.h:20`). The Dreamcast memory map appears in `docs/ARCHITECTURE.md` as documentation only.

### D. BIOS and boot: **INCOMPLETE**
No BIOS loading, validation or boot sequence. Missing-BIOS and invalid-BIOS error handling cannot be tested.

### E. Disc reader: **INCOMPLETE**
No GDI, CDI or CUE parsing, metadata, track model or sector reads. What does work: missing files are rejected (`FileNotFound`), and wrong extensions are rejected (`Unsupported`).

### F. Phase 1 testing

| Check | Result |
|---|---|
| Debug build | PASS (0 warnings in project code) |
| Release build, `-Werror` | PASS |
| Unit tests (Debug / Release) | PASS 41/41 / PASS 41/41 |
| Memory tests | NOT TESTED: no memory system |
| CPU foundation tests | NOT TESTED: no CPU |
| Disc parser tests | NOT TESTED: no parser |
| Initialisation / shutdown tests | PASS (`Emulator.*`, `app.*`) |

**Phase 1 conclusion:** of the ten Phase 1 components in the master plan, only error handling/logging is complete and initialisation/shutdown is partial. **Phase 1 is INCOMPLETE.**

---

## 4. Phase 2 audit

Every Phase 2 area is **INCOMPLETE**: there is no implementation to inspect.

| Area | Status | Evidence |
|---|---|---|
| SH-4 interpreter (integer, FPU, branches, exceptions, interrupts, timing) | INCOMPLETE | No code. Instruction coverage: 0 instructions |
| PowerVR2 (TA, polygons, textures, blending, depth, tiles, framebuffer, RTT) | INCOMPLETE | No code. The window draws a solid colour (RGB 18,20,28) and **does not pretend to be game output**: the title says "no emulation core yet" |
| AICA (channels, PCM/ADPCM, mixing, buffering, output) | INCOMPLETE | No code. SDL audio is not even initialised (`SDL_INIT_AUDIO` absent) |
| GD-ROM and DMA | INCOMPLETE | No code |
| Controller and VMU | INCOMPLETE | No code |
| JIT | INCOMPLETE | Not started |
| Timing and synchronisation | INCOMPLETE | No scheduler |

---

## 5. Project architecture review

**Strengths**
- Clean dependency graph: hardware modules depend only on `common`; only `src/app` links SDL2. The core could be reused by another frontend or by tests.
- Errors are returned as values with a clear code set; exceptions are caught once at the top of `main`.
- RAII throughout. The only `new` is passed straight into a `unique_ptr` (`src/core/emulator.cpp:19`). Nothing calls `malloc`, `free` or `delete`.
- A test (`Modules.NoModuleClaimsUnbuiltHardware`) fails if a module claims more progress than exists.

**Weaknesses**
- No version control (`git rev-parse` reports no repository). Nothing ties progress reports to a specific commit, and no change history exists.
- Dependencies are downloaded without a content hash (see S1).
- The Visual Studio 2022 preset has never been built: MSVC is not installed.
- Building with the WinLibs CMake needs a machine-specific `CMAKE_TLS_CAINFO` path. This is documented in the README.

---

## 6. Source code review

Scanned all 30 source/header files in `src/` and `tests/` (about 1,450 lines).

| Check | Finding |
|---|---|
| Empty files | None |
| TODO / FIXME / HACK / stub / placeholder / fake / mock | None |
| Unimplemented functions | None that pretend to work. `Emulator::load_game` deliberately returns `NotImplemented` |
| Placeholder hardware | The six `*_module.cpp` files are status-only. They are clearly labelled `NotImplemented` and do not fake behaviour |
| Duplicate implementations | None. (An earlier duplicate compile of `command_line.cpp` was fixed before this audit) |
| Hardcoded test values | Test inputs are synthetic and generated at run time; no expected value is tuned to make a test pass |
| Fake or simulated hardware | **None found** |
| Unused modules | All modules are linked into `novacast_core` and queried at run time, but contain no functionality |

---

## 7. Automated test results

### 7.1 Suite results

| Run | Result |
|---|---|
| `ctest --preset debug` | 41/41 PASS |
| `ctest --preset release` | 41/41 PASS |
| `ctest --preset release --repeat until-fail:10` | 41/41 PASS on all 10 repetitions |
| 50 cycles of `novacast --smoke-test` | 50/50 exit 0 |
| From-scratch Release build in an empty folder (downloads included) | Build OK, 0 warnings, 41/41 PASS |
| UBSan (trap mode) + `_GLIBCXX_ASSERTIONS` Debug build | 41/41 PASS, no UB detected |

### 7.2 Per-test results (identical in Debug and Release)

| Test group | Tests | Expected | Actual | Status | Source |
|---|---|---|---|---|---|
| `Result.*`, `Error.*` | 6 | Values, errors, move-only types, names | As expected | PASS | `src/common/error.*` |
| `LogTest.*`, `LogLevelNames.*` | 8 | Formatting, filtering, `off`, lazy evaluation, 1023-char truncation, file output, bad path | As expected | PASS | `src/common/log.*` |
| `FileIo.*` | 8 | Round trip, empty, missing, directory, size limit, write failure, non-ASCII path, extension | As expected | PASS | `src/common/fileio.*` |
| `Modules.*` | 3 | All six modules described; all `NotImplemented` | As expected | PASS | `src/*/…_module.cpp` |
| `Emulator.*` | 6 | 100 create/destroy cycles, coexistence, cannot run software, load_game errors | As expected | PASS | `src/core/emulator.*` |
| `CommandLine.*` | 5 | Defaults, flags, values, 8 error cases, usage text | As expected | PASS | `src/app/command_line.*` |
| `app.version`, `app.status` | 2 | Version string; "Can run Dreamcast software: no" | As expected | PASS | `src/app/main.cpp` |
| `app.bad_option`, `app.missing_game` | 2 | Non-zero exit | Non-zero exit | PASS | `src/app/main.cpp` |
| `app.window_smoke` | 1 | Hidden window, 3 frames, clean exit | "shut down cleanly after 3 frames" | PASS | `src/app/sdl_window.cpp` |

### 7.3 Required test categories from the audit brief

| # | Category | Status | Reason |
|---|---|---|---|
| 1 | Unit tests per hardware component | NOT TESTED | No hardware components exist |
| 2 | CPU instruction tests | NOT TESTED | No CPU |
| 3 | Memory read/write tests | NOT TESTED | No memory system |
| 4 | GPU rendering tests | NOT TESTED | No GPU |
| 5 | Audio processing tests | NOT TESTED | No audio |
| 6 | Disc image tests | NOT TESTED | No parser (only path/extension rejection is tested) |
| 7 | DMA tests | NOT TESTED | No DMA |
| 8 | Controller tests | NOT TESTED | No Maple bus |
| 9 | VMU tests | NOT TESTED | No VMU |
| 10 | CPU/GPU integration | NOT TESTED | Neither exists |
| 11 | Audio synchronisation | NOT TESTED | No audio |
| 12 | Emulator initialisation | PASS (partial scope) | `Emulator.*`, `app.*`; no hardware is initialised |
| 13 | Long-duration stability | PASS (app shell only) | 1,200-frame run, flat memory; 50 start/stop cycles. Not meaningful for emulation |

### 7.4 Reproducibility, sanitizer and coverage builds

**From-scratch Release build (reproducibility).** Empty build folder, outside the source tree, fresh downloads of SDL2 and GoogleTest, `NOVACAST_WARNINGS_AS_ERRORS=ON`:
configure OK, build OK, **0 warnings** in the whole build log including dependencies, **41/41 tests pass**. It took 648 s, sharing the CPU with the instrumented build below. The build is reproducible on this machine. Exact reproducibility over time is not guaranteed while downloads are unhashed (S1) and nothing is under version control.

**Sanitizer build.** Debug, `-fsanitize=undefined -fsanitize-trap=all -D_GLIBCXX_ASSERTIONS -O0`, which aborts on any detected undefined behaviour or standard-library bounds/precondition violation:
build OK, **41/41 tests pass**, including the five tests that run the real app. **No undefined behaviour or assertion failure detected.** AddressSanitizer could not be used (not supported by this toolchain, see section 10).

**Line coverage (gcov, same build, full suite including app tests).** Project `.cpp` files only; headers are excluded because gcov counts them separately in every file that includes them.

| File | Line coverage | Notes |
|---|---|---|
| `src/core/emulator.cpp` | 100% of 26 | |
| six `src/*/…_module.cpp` | 100% (2 lines each) | Status-only modules |
| `src/app/command_line.cpp` | 98.1% of 52 | |
| `src/common/log.cpp` | 95.5% of 67 | |
| `src/common/error.cpp` | 92.9% of 28 | |
| `src/common/fileio.cpp` | 90.5% of 42 | Uncovered: rare I/O failure branches |
| `src/common/module_info.cpp` | 88.9% of 9 | |
| `src/app/main.cpp` | 75.0% of 68 | Uncovered: exception handlers, log-file failure path |
| `src/app/sdl_window.cpp` | 54.9% of 82 | Uncovered: controller connect/disconnect, Esc/quit events, software-renderer fallback, SDL failure paths |
| **Total** | **about 83% (321 of 386 lines)** | Covers the foundation only; there is no hardware code to cover |

No source file or test expectation was changed, and no test was disabled.

---

## 8. Real game compatibility results

**NOT TESTED. Real games cannot run.**

- NovaCast has no CPU, memory, BIOS, disc or GPU emulation, so no game can boot. Trying to load a disc image returns `not implemented: disc image loading is not implemented yet` and exits with code 1. I confirmed this with a dummy `.gdi` file.
- No BIOS or game images exist in the workspace (searched for `.gdi`, `.cdi`, `.chd`, `.cue`, `dc_boot*`, `.bin`).

| Game | Format | Boot | Gameplay | Avg FPS | 1% low | Frame-time variance | Audio | Graphics | Controller | Issues |
|---|---|---|---|---|---|---|---|---|---|---|
| (none) | — | NOT TESTED | NOT TESTED | — | — | — | — | — | — | Emulation core does not exist |

No compatibility entries were created. Doing so would be fabrication.

---

## 9. Performance benchmarks

**No emulation benchmarks are possible**, and none are reported. A comparison with Flycast cannot be made.

For reference only, here is the cost of the empty application shell (Release, visible window, vsync, 60 Hz display). These numbers say **nothing** about emulation performance.

| Measurement | Value |
|---|---|
| Start-up to first frame | about 1.1 s (mostly Direct3D/driver initialisation) |
| 1,200 frames | 21.15 s wall = about 1.1 s start-up + 20.0 s, i.e. 60 frames/s locked to vsync |
| Private memory after start-up | 45.0 MB at 2 s, 45.1 MB at 20 s (flat; no leak trend) |
| Peak working set | 56.7 MB |
| Handle count | 418, constant |
| Process CPU time over a 600-frame run (11.1 s) | 0.94 s (about 8% of one core, mostly start-up) |

Not measured: GPU usage, audio latency, input latency, loading times and CPU/GPU synchronisation. None of these exist to measure yet.

---

## 10. Security findings

| ID | Severity | Finding | Evidence | Recommendation |
|---|---|---|---|---|
| S1 | Medium | SDL2 and GoogleTest are downloaded **without a content hash** (`URL` without `URL_HASH`). TLS protects the transfer, but a replaced release archive would be accepted without notice. | `cmake/Dependencies.cmake` lines 26-27 and 51-52 | Add `URL_HASH SHA256=…` to both `FetchContent_Declare` calls |
| S2 | Low | The log sink is called **while the log mutex is held**. A sink that itself logs would deadlock: `std::mutex` cannot be locked twice by the same thread. The header does not warn about this. | `src/common/log.cpp`, `log_write` | Document "sinks must not log", or copy the sink pointer and call it after unlocking |
| S3 | Low | SDL headers are included with `-I`, not `-isystem`. A warning in a future SDL header would break `-Werror` builds. | Release `compile_commands.json`, `sdl_window.cpp` entry | Mark the SDL include directories as `SYSTEM` |
| S4 | Low | `main(char**)` receives arguments in the ANSI code page. A game path containing characters outside it will be mangled on Windows. Already documented in PROGRESS.md. | `src/app/main.cpp` | Use `GetCommandLineW`/`CommandLineToArgvW` before disc loading is implemented |
| S5 | Info | `read_file` checks the size, then reads. If the file grows in between, only the original size is read; if it shrinks, the short read is detected and reported. Safe, but worth knowing for disc images on removable media. | `src/common/fileio.cpp` | None needed now |
| S6 | Info | `app.bad_option` and `app.missing_game` accept **any** non-zero exit. I confirmed that a crash is still caught: CTest reports an aborting program as an exception and the `WILL_FAIL` test fails. But a wrong exit code (for example 1 instead of 2) would not be caught. | `tests/CMakeLists.txt` | Check the error message with `PASS_REGULAR_EXPRESSION` as well |

Other checks:
- **Buffer handling:** formatting uses `vsnprintf`/`snprintf` into fixed buffers with size limits; truncation is tested. No `strcpy`, `sprintf`, `memcpy` or manual `new[]`.
- **Integer overflow:** `--frames` uses `std::from_chars` into `u64`; negative and malformed input is rejected (tested).
- **File parsing:** there are no parsers yet, so the main attack surface of an emulator (disc image and BIOS parsing) does not exist yet. It will need fuzzing once written.
- **Threads:** the app is single-threaded; logging is mutex-protected.
- **Sanitizers:** AddressSanitizer and the UBSan runtime are not available with MinGW GCC (`cannot find -lasan` / `-lubsan`, verified). UBSan in trap mode works; results are in 7.4. Leak checking was done by observing process memory only (section 9), not with a leak-checking tool.

---

## 11. Missing features

Everything in the master plan except the foundation:

- **Phase 1:** scheduler/system clock; memory map (RAM, VRAM, ROM, flash, register dispatch); SH-4 interpreter, exceptions, interrupts and on-chip peripherals; Holly interrupt controller and DMA; BIOS loading and boot; GDI/CDI/CUE parsing; GD-ROM drive; Maple controller.
- **Phase 2:** PowerVR2 and the OpenGL renderer; AICA, ARM7 and audio output; VMU; JIT; timing and synchronisation; compatibility database.
- **Phase 3:** ImGui UI, game library, input mapping, on-screen controls, opacity, styles, settings persistence, save states.
- **Phase 4:** optimisation, compatibility testing, Flycast benchmarks.
- **Process:** version control; MSVC build verification; a configuration file.

---

## 12. Critical bugs

**None found in the existing code.** No test failed in any of the four builds (Debug, Release, from-scratch Release, UBSan), no undefined behaviour was detected, and no crash occurred in 50 start/stop cycles.

The critical *gap*, not a bug, is that the emulator does not emulate anything yet.

---

## 13. Recommended fixes (in order)

1. Put the project under git, and commit the current state as the Phase 1 Step 1 baseline.
2. Add `URL_HASH SHA256` to the SDL2 and GoogleTest downloads (S1).
3. Mark SDL include directories `SYSTEM` (S3), and fix the logging re-entrancy note (S2).
4. Update `docs/ARCHITECTURE.md`, which still calls Step 1 "pending build verification".
5. Verify the `vs2022` preset on a machine with MSVC, or remove it until it can be verified.
6. Then continue Phase 1 in order: scheduler, memory map and BIOS loading, SH-4 interpreter with instruction tests, interrupts, disc parsing with fuzz tests, GD-ROM, Maple. Use wide-character command-line handling before disc loading (S4).

---

## 14. Final completion verdict

Percentages are weighted by requirement and based on evidence, not on file counts.

| Area | Completion | How it was weighted |
|---|---|---|
| **Master requirements** | **about 5%** | 11 final-expectation items weighted equally: only "proper error handling and testing" is about half done (the testing half covers no hardware); the modular structure exists but is empty |
| **Phase 1** | **15%** | 10 master-plan components at 10% each: error handling/logging 10%, initialisation/shutdown 5% (works, but nothing to initialise); CPU, memory, bus, BIOS, disc loading, formats, controller and timing 0% |
| **Phase 2** | **0%** | No Phase 2 component has any implementation |
| **Automated test coverage** | **about 4% of required categories** | 13 required categories: initialisation passes at partial scope (0.5); the rest cannot be tested yet. Separately, the code that does exist has about 83% line coverage (7.4) |
| **Real game compatibility** | **0%** | No game can boot; none tested |

### Verdict: **1. NOT READY**

The foundation is solid, honest and well tested, but NovaCast cannot execute a single SH-4 instruction. Phase 1 is incomplete, and Phase 2 has not begun.
