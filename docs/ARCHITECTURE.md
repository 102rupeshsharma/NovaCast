# NovaCast architecture

## Principles

- **One library per hardware block.** `cpu`, `gpu`, `audio`, `memory`, `disc`
  and `controller` are separate static libraries. Each depends only on
  `common`. Modules will talk to each other through small interfaces owned by
  `core`, never by including each other's internals.
- **The emulator core has no host dependencies.** Only `src/app` links SDL2.
  The core will receive input as abstract button/axis state and hand video and
  audio out through backend interfaces (OpenGL first, Vulkan later).
- **One clock.** All timing will be expressed in SH-4 cycles (200 MHz) on a
  single scheduler owned by `core`.
- **Honest status.** Every module reports `NotImplemented`, `Partial` or
  `Implemented`. A status goes up only together with tests that prove it. A
  unit test (`Modules.NoModuleClaimsUnbuiltHardware`) enforces this.
- **Errors are values.** Recoverable failures return `nc::Result<T>`.
  Exceptions are only for programming errors and out-of-memory, and are caught
  at the top of `main`.
- **RAII everywhere.** SDL objects, files and the emulator itself release
  their resources in destructors. No owning raw pointers.

## Dependency graph

```
            app (novacast.exe) ── SDL2
             │        └── cli
            core
   ┌────┬────┼─────┬──────┬───────────┐
  cpu  gpu audio memory  disc   controller
   └────┴────┴─────┴──────┴───────────┘
                 common
```

## Source layout

```
CMakeLists.txt          top-level project and options
CMakePresets.json       debug / release (Ninja), vs2022 (Debug + Release)
cmake/                  Dependencies.cmake (SDL2, GoogleTest), CompilerWarnings.cmake
src/common/             types, error/Result, log, fileio, module_info
src/cpu/                SH-4
src/gpu/                PowerVR2
src/audio/              AICA
src/memory/             RAM, VRAM, ROM, address map
src/disc/               GD-ROM drive, GDI/CDI/CUE images
src/controller/         Maple bus, controller, VMU
src/core/               Emulator (system owner)
src/app/                main, command line, SDL window
tests/                  GoogleTest suites mirroring src/, plus app smoke tests
docs/                   this file, PROGRESS.md
```

## Hardware reference: physical memory map (address & 0x1FFFFFFF)

Recorded here to guide the memory module. None of it is implemented yet.

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
