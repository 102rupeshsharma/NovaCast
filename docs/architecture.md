# NovaCast architecture

## Principles

- **One engine, two apps.** `core/` is the emulator engine. The Windows app
  (`desktop/`) and the Android app (`mobile/`) both build this same code;
  there is no second implementation of any hardware.
- **One library per hardware block.** `cpu`, `gpu`, `audio`, `memory`, `disc`
  and `controller` are separate static libraries. Each depends only on
  `common`. Modules talk to each other only through interfaces wired up by
  `system`, never by including each other's internals.
- **The engine has no host dependencies.** Nothing under `core/` uses SDL,
  Windows, Android or a UI framework, only standard C++20. Host services are
  provided by the apps:

  | Service | Windows (`desktop/`) | Android (`mobile/`) |
  |---------|----------------------|---------------------|
  | Window / UI | SDL2 window | Jetpack Compose |
  | Log output | stderr and optional file (engine default) | logcat sink installed in `JNI_OnLoad` |
  | Files | `std::filesystem` paths from the command line | Storage Access Framework; bytes passed to the engine |
  | Lifecycle | process start/exit | pause when the app is backgrounded or overheating |
  | Threading | main thread | one dedicated engine thread |
  | Video, audio, input | not implemented yet | not implemented yet |

  Video, audio and input adapters will follow the same rule once the engine
  has GPU, AICA and Maple emulation to connect them to.
- **One clock.** All timing is expressed in SH-4 cycles (200 MHz) on a single
  scheduler owned by `system`.
- **Interpreter first, JIT per platform later.** The SH-4 interpreter is
  shared. A future JIT needs separate x86-64 (Windows) and ARM64 (Android)
  back-ends, with the interpreter as the fallback and the reference to
  compare against. Android restricts executable memory, so its JIT will be
  designed separately. No JIT exists yet.
- **Honest status.** Every module reports `NotImplemented`, `Partial` or
  `Implemented`. A status goes up only together with tests that prove it. A
  unit test (`Modules.StatusMatchesImplementation`) enforces this.
- **Errors are values.** Recoverable failures return `nc::Result<T>`.
  Exceptions are only for programming errors and out-of-memory, and are caught
  at the top of `main`.
- **RAII everywhere.** SDL objects, files and the emulator itself release
  their resources in destructors. No owning raw pointers.

## Dependency graph

```
  desktop: novacast.exe ── SDL2        mobile: libnovacast_jni.so ── Kotlin/Compose
              │      └── cli                       │
              └──────────────┬─────────────────────┘
                           system   (Emulator, Scheduler)
         ┌────┬─────┬──────┼──────┬──────┬────────────┐
        cpu  gpu  audio  memory  disc  controller
         └────┴─────┴──────┴──────┴──────┴────────────┘
                           common
```

## Source layout

```
VERSION                 single version number for every platform
CMakeLists.txt          desktop build: core + desktop + tests
CMakePresets.json       debug / release (Ninja), vs2022 (Debug + Release)
cmake/                  Dependencies.cmake (SDL2, GoogleTest), CompilerWarnings.cmake
core/                   shared emulator engine (core/CMakeLists.txt builds it alone)
  common/               types, error/Result, log, fileio, module_info
  cpu/                  SH-4 interpreter
  gpu/                  PowerVR2 (not implemented)
  audio/                AICA (not implemented)
  memory/               RAM, VRAM, ROM, address map
  disc/                 GD-ROM drive, GDI/CDI/CUE images (not implemented)
  controller/           Maple bus, controller, VMU (not implemented)
  system/               Emulator (owns and wires the modules), Scheduler
desktop/                Windows app: main, command line, SDL window
mobile/                 Android app (Gradle project)
  app/src/main/cpp/     JNI bridge; builds ../../../../../core with the NDK
  app/src/main/java/    Kotlin: engine wrapper, ViewModel, Compose UI
tests/                  GoogleTest suites (common, cpu, memory, system, desktop)
.github/workflows/      release.yml: builds both apps on a v* tag
docs/                   this file, PROGRESS.md, desktop/android build guides, testing
```

Planned modules from the master plan that do not exist yet (DMA, VMU, save
states) will be added under `core/` when they are implemented, not as empty
folders.

## Hardware reference: physical memory map (address & 0x1FFFFFFF)

RAM, VRAM, boot ROM and flash are implemented (`core/memory/bus.cpp`); the
device register blocks are not yet.

| Range | Device | Module |
|-------|--------|--------|
| 0x00000000-0x001FFFFF | Boot ROM (2 MB) | memory |
| 0x00200000-0x0021FFFF | Flash ROM (128 KB) | memory |
| 0x005F6800-0x005F7FFF | Holly system bus, Maple, GD-ROM, G1/G2 | core, controller, disc |
| 0x005F8000-0x005F9FFF | PowerVR2 registers, fog table, palette | gpu |
| 0x00700000-0x00710FFF | AICA registers and RTC | audio |
| 0x00800000-0x009FFFFF | AICA wave RAM (2 MB) | audio |
| 0x04000000-0x05FFFFFF | VRAM (8 MB), 64-bit and 32-bit access paths | memory |
| 0x0C000000-0x0FFFFFFF | Main RAM (16 MB, mirrored) | memory |
| 0x10000000-0x11FFFFFF | Tile accelerator FIFO, YUV, direct texture | gpu |
| 0x1C000000-0x1FFFFFFF | SH-4 on-chip registers (also via P4) | cpu |

## Plan

1. **Phase 1, Step 1 (done, verified):** foundation. See PROGRESS.md.
2. **Phase 1, Step 2 (done, verified with synthetic programs):** scheduler,
   memory map, BIOS/flash loading, SH-4 interpreter with exceptions and
   interrupt acceptance.
3. **Phase 1, next steps:** SH-4 peripherals (TMU, INTC, DMAC, SCIF) and the
   MMU; Holly interrupt controller and DMA; flash command protocol;
   GDI/CDI/CUE parsing and the GD-ROM drive; Maple controller; boot sequence
   up to the BIOS running.
4. **Phase 2:** PowerVR2 (tile accelerator, OpenGL renderer), AICA (ARM7,
   channels, DSP, SDL audio output), VMU, SH-4 JIT checked against the
   interpreter, timing and synchronisation.
5. **Phase 3:** Dear ImGui interface, game library, input mapping and
   on-screen controls, settings, save states.
6. **Phase 4:** profiling and optimisation, compatibility database,
   benchmarks against Flycast.

## Testing strategy

- Every module gets a GoogleTest suite under `tests/<module>/`. No test may
  depend on copyrighted data: CPU tests use hand-assembled SH-4 code, and disc
  tests generate synthetic images in a temporary directory.
- Integration tests drive several modules through `core`.
- CTest also runs the real executable (`app.*` tests) to check start-up, error
  exit codes and clean shutdown.
- Checks that need user-owned BIOS or disc images are run by hand and
  recorded in PROGRESS.md with the exact result.
