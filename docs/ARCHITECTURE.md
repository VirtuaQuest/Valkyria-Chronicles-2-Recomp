# Architecture

This project turns the PSP executable of *Valkyria Chronicles II* (ULUS10515, North America)
into a native Windows program. It is a **static recompilation**: the game's MIPS machine code is translated
ahead of time into C, compiled by MSVC, and linked against a runtime that re-implements the
PSP's operating system and hardware at a high level (HLE). No game code or data is stored in
this repository; everything game-specific is derived from the user's own disc (and a decrypted copy
of its executable, dumped by the user with PPSSPP) on their machine.

```
 user's ISO ──► vc2core (load, analyze, translate) ──► generated C (≈1.2M MIPS instrs)
                                                                      │  MSVC /O2
                                                                      ▼
                             VC2Recompiled.exe  ──calls──►  vc2runtime.dll (HLE PSP)
                                                                SDL3 · OpenGL 3.3 · ImGui
```

## 1. Disc and executable handling (`src/common`, `src/crypto`, `src/loader`)

* `iso9660.*` reads the UMD image directly, `.iso` or compressed `.cso` (CISO v1: an index of 2 KB
  blocks, each raw DEFLATE or stored, decoded by `inflate.*`); nothing is extracted to disk.
* `prx_decrypt.*` takes plain executables from the disc and recognises encrypted `~PSP` ones.
  The project contains no console keys or decryption.
* `game_module.*` reads each module the profile names. The disc's `EBOOT.BIN` is encrypted,
  so it comes from the decrypted copy the player dumps from their own disc with PPSSPP
  (`ULUS10515_EBOOT.BIN`, which PPSSPP writes when "Dump decrypted EBOOT.BIN on game boot" is
  on). The copy is trimmed to the ELF size in the disc's `~PSP` header (PPSSPP writes a few
  hundred bytes more) and must match the SHA-1 in the profile. The launcher finds it (its own
  path, the install folder, next to the ISO, PPSSPP's dump folder), checks it and copies it
  into the install folder, where the game reads it at boot. A copy with another SHA-1 is accepted
  when its module name matches the disc's `~PSP` header (other dumps or printings may differ); the
  game is built from it as it is, and the engine hooks, whose addresses belong to the known copy,
  are switched off.
* Each module's SHA-1 (Windows CNG) is embedded in the generated code and re-checked at boot,
  so a different disc revision can never run against mismatched translated code.
* `elf.*` loads the relocatable PRX: applies `R_MIPS_32/26/HI16/LO16` relocations, parses
  `SceModuleInfo`, import stubs (NIDs) and exports, and records every relocated pointer.

Two modules are recompiled: `EBOOT.BIN` (loaded at `0x08804000`) and the game's own
`LIBSUPPREACC.PRX` (a plain ELF on the disc, pinned at `0x08D80000`). The game profile in
`src/common/game_layout.h` names them, gives the dump name and SHA-1 of the encrypted one, and
lists engine functions hooked at entry (the frame-rate request `0x08bd320c` used by the 60 FPS
option). Sony's `LIBFONT.PRX` is replaced by an HLE implementation
(`sceLibFont`), as are all firmware libraries.

## 2. Analysis (`src/recomp/analysis.*`)

* **Function discovery** uses facts rather than heuristics. Because the module is relocatable,
  every absolute code address appears in the relocation table. Function starts are: the entry
  point, exports, all `jal` targets ("strong" starts), and relocated code pointers ("weak"
  starts, e.g. vtables and callbacks).
* **Jump tables** are recovered from the `lui/addiu + lw + jr` pattern (the `lui` carries a
  HI16 relocation to the table, `sltiu` gives the bound) and truncated at the first entry that
  leaves the enclosing function.
* Weak starts that are entered from the middle of another function (they are really switch
  labels) are merged iteratively until no inter-function branch lands mid-function.

Result for this game: 18,883 functions, 583 jump tables, **0 unresolved indirect jumps and 0
mid-function external branches**, which means every function can be emitted as a
self-contained C function with `goto` labels.

## 3. Code generation (`src/recomp/codegen.*`)

* One C function per MIPS function: `void f_08804000(PspContext* c)`.
* GPRs, HI/LO, FPU and VFPU registers live in `PspContext` (`src/runtime/psp_recomp.h`).
* Memory accesses are `LW(addr)` etc. on a 512 MB host reservation mirroring the PSP physical
  map (`addr & 0x1FFFFFFF`, so cached, uncached and kernel mirrors all alias). The PSP is
  little-endian like x86, so no byte swapping is needed.
* Control flow: delay slots are emitted in order with the branch condition captured first;
  "likely" branches nullify the slot. `jal` is a C call, `jr $ra` is a C `return`, `j` or a
  branch to another function is a tail call, jump tables become `switch` + `goto`, and
  `jalr` or unknown `jr` targets go through `psp_call_indirect`, a table lookup.
* Calls to imported functions become `psp_hle_call(c, importIndex)`.
* VFPU instructions (about 4,000 static sites in this game) call the runtime VFPU interpreter
  with the raw instruction word. VFPU branches are emitted inline.
* Output is split into about 96 files of similar size for parallel compilation (about 2 minutes
  at `/O2` on 8 cores).

## 4. Runtime (`src/runtime`, built as `vc2runtime.dll`)

| Area | Files | Notes |
|---|---|---|
| Boot | `core/boot.cpp` | opens the ISO, verifies, loads and relocates modules into PSP memory, binds imports, starts `user_main` |
| CPU glue | `core/cpu.cpp`, `cpu/vfpu.cpp` | indirect dispatch table, traps, VFPU interpreter |
| Kernel | `kernel/threads.cpp`, `threadman.cpp` | **one Windows fiber per PSP thread**, strict-priority scheduler, waits with timeouts, callbacks, sub-interrupt handlers |
| | `kernel/sysmem.cpp` | user partition allocator (low/high/addr/aligned) |
| | `kernel/io.cpp` | `disc0:`/`umd0:` served from the ISO (including raw `sce_lbn` sector paths), `ms0:` mapped to `memstick/` |
| | `kernel/modules.cpp`, `misc.cpp` | module manager, power, RTC, UMD, utils, network (multiplayer stubs) |
| HLE | `hle/display.cpp` | 59.94 Hz vblank, framebuffer latch, controller sampling |
| | `hle/ge.cpp`, `gpu/*` | GE display lists and an OpenGL 3.3 renderer with internal-resolution upscaling |
| | `hle/audio.cpp`, `hle/sas*.cpp` | PCM channels mixed into SDL3, SAS sound-effect synthesizer |
| | `hle/utility*.cpp`, `hle/font*.cpp` | savedata and on-screen keyboard dialogs (ImGui), font rendering through system fonts |
| Host | `host/*` | SDL3 window, OpenGL context, keyboard and gamepad mapping, ImGui settings overlay (F1), config file |

### Threads and blocking

A guest thread runs on its fiber until it blocks (`waitCurrent`), and the scheduler fiber then
picks the highest-priority ready thread. Kernel calls are the only preemption points: after each
HLE call, the runtime switches threads if a higher-priority thread became ready, or if a periodic
timer (vblank, audio mixer) is overdue and interrupts are enabled. The overdue-timer check models
the hardware interrupt that would preempt a spinning thread on a real PSP. Time-based events (vblank, delays, audio mixing) are driven by the
scheduler loop. GE and vblank interrupt handlers run on a private interrupt stack.

### Why a DLL

The game EXE contains only generated code plus a 20-line `main.c`. Fixes to the runtime ship as
a new `vc2runtime.dll` without re-translating or recompiling the game (the ABI version is
checked at startup).

## 5. Launcher (`src/launcher`)

The launcher validates the ISO and runs the translation in-process. It then drives MSVC
(found through `vswhere`) with one `cl.exe` per generated file in parallel, so the progress bar
tracks real compile progress. It builds an icon resource from the disc's `ICON0.PNG` in the
output folder, links against `vc2runtime.lib`, and copies the runtime next to the EXE. Object
files are cached by content hash, so updates only relink.

## Legal notes

* No Sony or Sega code, console keys, game assets,
  or firmware files are included. The game must be supplied by the user from their own disc.
* PPSSPP's source was consulted as documentation of PSP behaviour; no PPSSPP code is copied.
