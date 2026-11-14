# Valkyria Chronicles II — PC Recompilation

<a href="https://recomplauncher.com"><picture><source media="(prefers-color-scheme: dark)" srcset="https://recomplauncher.com/assets/press/launcher/badge-light.svg"><img alt="Get it on RecompLauncher" src="https://recomplauncher.com/assets/press/launcher/badge-dark.svg" height="48"></picture></a>

A native Windows port of the PSP game *Valkyria Chronicles II* (North America, **ULUS10515**),
produced by **static recompilation**: the game's MIPS code is translated to C on your machine and
compiled into a native executable that runs on a re-implementation of the PSP system software. It
adds modern PC features: rendering at up to 8× the PSP resolution, keyboard and gamepad controls,
borderless or exclusive fullscreen, V-Sync, anisotropic filtering, texture upscaling, FXAA, MSAA
and an optional 60 FPS mode for battles.

> **You must own the game.** This repository contains no game code, data, artwork, audio or
> console keys. The launcher reads everything from *your* ISO dump of the UMD (and a decrypted
> copy of its executable that you make with PPSSPP) and builds the executable locally. Nothing
> derived from the game (generated code, icons, objects) is ever written into the repo.

## How to play

1. Dump your UMD to an `.iso` (for example with a PSP running custom firmware). Compressed `.cso`
   images work too; CHD images don't yet (convert them to `.iso`).
2. Make a decrypted copy of the game executable with [PPSSPP](https://www.ppsspp.org) (free). The
   executable on the disc is encrypted, and this project contains no PSP keys to read it:
   1. In PPSSPP, open **Settings > System > Developer tools** and turn on **Dump decrypted
      EBOOT.BIN on game boot**.
   2. Start Valkyria Chronicles II from your `.iso` and wait for the first screen. PPSSPP shows
      *Dump Decrypted Eboot* with the file's location. Close PPSSPP.
   3. The file is `ULUS10515_EBOOT.BIN` in PPSSPP's memory stick folder, under `PSP/SYSTEM/DUMP`
      (`Documents\PPSSPP\PSP\SYSTEM\DUMP` for an installed PPSSPP). The launcher finds it there,
      or next to your `.iso`; otherwise pick it with **Browse...**. It checks that the file is the
      one from your disc before using it. (A dump from another copy of the disc that differs from the
      one this recomp was tested with is accepted too, with the 60 FPS option turned off for it.)
3. Install the free **Visual Studio 2022 Build Tools** with the *Desktop development with C++*
   workload. The launcher uses it to compile the translated game.
4. Download the latest `VC2Recompiled-<version>-Windows-x64.zip` from
   [Releases](https://github.com/VirtuaQuest/Valkyria-Chronicles-2-Recomp/releases), unzip it and
   run `VC2Launcher.exe`. Pick your ISO (and the dump, if it isn't found) and an install folder and
   press **Recompile**. A progress bar follows the steps: load → analyze → translate → compile →
   link. The first build takes about two minutes; later runtime updates only relink.
5. Press **Play**.

### With RecompLauncher

[RecompLauncher](https://recomplauncher.com) can install this recomp and set it up from your disc
image: it asks for the ISO and the PPSSPP dump, runs the same recompile without a window
(`VC2Launcher.exe --prepare`, with a progress bar on its setup screen), shows the graphics settings
on the game's page, and starts the game with `VC2Launcher.exe --play`. The setup is described in
[`recomplauncher.json`](recomplauncher.json), which ships at the top of every release. You still
need the C++ Build Tools from step 3.

## Controls

| PSP | Keyboard | Gamepad (XInput/SDL) |
|---|---|---|
| Analog stick | W A S D (hold Shift to walk slowly) | Left stick |
| D-pad | Arrow keys | D-pad / right stick |
| ✕ (confirm) | Space / Enter | A (South) |
| ◯ (cancel) | Esc / Backspace | B (East) |
| □ | F | X (West) |
| △ | R | Y (North) |
| L / R | Q / E | LB / RB (or triggers) |
| START / SELECT | Tab / C | Start / Back |

Hotkeys: **F1** opens the settings overlay (or the gamepad Guide button), **F11** or
**Alt+Enter** toggles fullscreen, and holding **`** (backquote) fast-forwards (3× speed). Keys can be
rebound in `vc2recomp.ini` or the launcher's settings tab.

## Graphics settings

* **Internal resolution:** 1× (480×272, the PSP's native resolution) up to 8× (3840×2176). 3D
  scenes render at the higher resolution instead of being stretched.
* **Aspect:** keep 16:9 / stretch / integer scaling.
* **Texture filtering** (smooth, or nearest for sharp pixels), **anisotropic filtering (up to 16×)**, **texture upscaling (2–4×)**,
  **FXAA**, **MSAA**, **V-Sync**, FPS counter.
* **60 FPS gameplay (experimental):** battles and maps run at 30 FPS on the PSP; this draws them
  at 60 FPS through the engine's own time step, without changing the game's speed. Off by default.

## Files in the install folder

| File | What it is |
|---|---|
| `VC2Recompiled.exe` | the game, built from your disc |
| `ULUS10515_EBOOT.BIN` | the decrypted executable you dumped with PPSSPP (read at every start) |
| `vc2runtime.dll` | the PSP system re-implementation the game runs on |
| `vc2recomp.ini` | settings (also edited by the launcher's Settings tab) |
| `vc2recomp.log` | log of the last run (`vc2recomp.1.log` to `.3.log`: the three before); attach it to bug reports |
| `memstick/PSP/SAVEDATA/` | your saves |

## Build information

Every build records its version, git commit, branch, commit date, build date and compiler:

* the launcher shows the version under its title and the full details on the **About** tab
  (**Copy build information** puts them on the clipboard for bug reports);
* `VC2Launcher.exe --version` prints them;
* Explorer shows them under **Properties > Details** of `VC2Launcher.exe` and `vc2runtime.dll`;
* the game writes them at the top of `vc2recomp.log` and shows the version in the F1 overlay.

The version is the release tag for release builds (`v1.2.0`), `git describe` output between
releases (`v1.2.0-3-gabc1234`), or `v<base>-dev+<commit>` before the first tag; `-dirty` marks
builds with uncommitted changes.

## Building from source

Requirements: Windows 10/11 x64, Visual Studio 2022 (or Build Tools) with CMake and Ninja,
internet access on first configure (SDL3 and Dear ImGui are fetched by CMake).

```bat
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Command line: `VC2Launcher.exe --game <disc image> --out <install folder> --build` recompiles without
the window (`--iso` is the same as `--game`; `--dump <file>` names the PPSSPP dump; `--help` lists
the rest).

Outputs in `build/bin/`: `VC2Launcher.exe`, `vc2runtime.dll`, `sdk/` (import library and headers
the launcher compiles against) and `vc2tool.exe` (developer CLI: `info`, `decrypt` (extracts a
module's ELF), `image` (writes the uncompressed image of a `.cso`), `imports`, `analyze`, `dis`, `recomp`; it reads `ULUS10515_EBOOT.BIN` from the
folder `VC2_DUMPS` names, or the current folder).

`tools/package.ps1 -BuildDir build -Version v1.2.3` writes the release zip
(`dist/VC2Recompiled-v1.2.3-Windows-x64.zip`, with `recomplauncher.json` at its top). Pushing a
`v*` tag runs the same build and packaging in GitHub Actions and attaches the zip to the release.

## How it works

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md). In short: the game executable is loaded,
relocated and analyzed. Function boundaries and every jump table are recovered from the
relocation table (18,861 functions and 583 jump tables, with no unresolved indirect jumps), and each MIPS function
becomes a C function. The runtime DLL provides the PSP kernel (threads as fibers), file system
on top of the ISO, GE graphics on OpenGL, audio mixing, input, savedata and the rest of the
system libraries.

## Status

This is a work in progress. See [docs/STATUS.md](docs/STATUS.md) for what works today and what
is still missing.

## Credits

* The recompiler and runtime are shared with the
  [Valkyria Chronicles III recompilation](https://github.com/VirtuaQuest/Valkyria-Chronicles-3-recomp).
* PSP hardware and OS behaviour was studied from public documentation and from the PPSSPP
  project's source as reference. No PPSSPP code is included.
* [SDL3](https://libsdl.org) (zlib license) and [Dear ImGui](https://github.com/ocornut/imgui)
  (MIT license).
* The ATRAC3plus decoder (`src/runtime/media/atrac3plus/`) is the project's own code, but its
  constant tables (`atrac3plus_tables.cpp`) are derived from [FFmpeg](https://ffmpeg.org)'s ATRAC3+
  decoder by Maxim Poliakovski and are therefore LGPL-2.1-or-later. See
  [LICENSE-NOTICE.md](src/runtime/media/atrac3plus/LICENSE-NOTICE.md).
