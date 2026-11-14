# Status

## Valkyria Chronicles II (ULUS10515)

The recompilation targets the North American release. Its `EBOOT.BIN` is encrypted on the disc
and this project contains no console keys: the player dumps the decrypted executable from their
own disc with PPSSPP (`ULUS10515_EBOOT.BIN`), and the launcher verifies it (SHA-1 of the ELF,
trimmed to the size in the disc's header) and copies it into the install folder. The game's own
`LIBSUPPREACC.PRX` is a plain ELF and is read from the disc.

| | |
|---|---|
| Analysis | 18,861 functions in `EBOOT.BIN`, 583 jump tables, 0 unresolved jumps (18,883 with `LIBSUPPREACC.PRX`) |
| Build | `VC2Recompiled.exe` via the launcher (about 1:40 from scratch, a few seconds when only the runtime changed) |
| Verified | boots from the disc plus the dump, notice and autosave screens, save creation, logo and opening movies with sound, difficulty choice, story scenes, the first battle's briefing, squad placement and turn-1 tactical map; steady 30 FPS in story scenes and on the battle map and 60 FPS in menus, as on a PSP; the experimental 60 FPS gameplay mode (engine hook `0x08bd320c`) |
| Not yet verified | later chapters, long play sessions |

## Recompiler and runtime

The recompiler and runtime are shared with the Valkyria Chronicles III recompilation; the table
below was verified with both games. Last verified on:
Windows 11, MSVC 14.44, GeForce GTX 1660 SUPER.

## Works (verified)

| Area | State | How it was verified |
|---|---|---|
| Executable loading, analysis | ✅ complete | SHA-1 check of each module (and of the player's dump); 18,861 functions (VC2), 0 unresolved jumps |
| MIPS → C translation | ✅ complete | every instruction handled (0 `psp_unhandled` sites); compiles warning-free |
| Launcher | ✅ complete | full build 1:48, incremental ~4 s, cancel, icon, settings |
| Kernel (threads, sync, memory, I/O) | ✅ working | game runs its 60 fps loop, streams from disc, creates and reloads saves |
| VFPU | ✅ complete | self-test (`vfpu_test`) passes; all 481 encodings the game uses |
| GE renderer | ✅ working | menus, movies, story scenes, tactical map and 3D battles rendered and compared against expected output |
| Upscaling and graphics options | ✅ | 1–8× internal resolution switchable live; MSAA, FXAA, xBR-style texture upscaling, anisotropy |
| Movies (video) | ✅ | all 28 PMF movies decode via Windows Media Foundation (H.264) |
| Sound effects (SAS) | ✅ | bit-exact path from SAS to the mixer; no underruns at steady state |
| Music and movie audio (ATRAC3+) | ✅ | the runtime's own decoder (`src/runtime/media/atrac3plus/`); `atrac_test` decodes `SND0.AT3`, all movie soundtracks and VC3's BGM files at ~300× real time, matching FFmpeg's decoder at ~135 dB SNR (≤ 1 LSB) |
| Savedata, OSK, fonts | ✅ | game creates its save on a fresh memory stick and reloads it on the next boot |
| Input | ✅ | keyboard and SDL gamepads; scripted input drives the game from boot to battles |

## Known issues

1. Music and movie audio are ATRAC3+. The game's BGM (`.atx`) plays through CRI's ADX+ATRAC
   layer and `sceAtrac3plus`; the first cue is BGM 38, about 2 minutes 20 seconds into a new
   game. The runtime decodes it with its own ATRAC3plus decoder (`src/runtime/media/atrac3plus/`,
   installed at boot; its constant tables are LGPL-2.1+, see `LICENSE-NOTICE.md` there).
   ATRAC3 (non-plus) is not implemented and plays as silence; neither disc contains ATRAC3
   streams. Decoded loud passages clip slightly (up to ~0.2% of samples in a few movies), exactly
   as the reference decoder's output does.
2. Sound effects (SAS) and the CRI ADX path (`OD_STREAM.AFS` / `OD_VOICE.AFS`) work. ADX
   streams are used only by some scenes. Debug aids used for this investigation are in
   `core/crash.cpp`: `VC2_WATCH`, `VC2_BREAK`/`VC2_BREAK2` and `VC2_PRINTF` (hardware watch and
   break points on guest code; `VC2_PRINTF=08bbb4f8` captures the game's sound and file log).
3. A few rare GE blend modes are approximated (logged once when used), and dithering is not
   emulated.
4. Local multiplayer (ad hoc) is not supported; the game reports no connection.
5. Saves are stored unencrypted (compatible only with this port, not with a real PSP).

## Debug environment variables

| Variable | Purpose |
|---|---|
| `VC2_DUMP_DIR`, `VC2_DUMP_EVERY`, `VC2_DUMP_AT` | PNG frame dumps |
| `VC2_INPUT=frame:button[:len],...` | scripted pad input (`from-to/period:button` repeats a press) |
| `VC2_INPUT_AFTER=text[#n]`, `VC2_INPUT_THEN=...` | second input script, timed from the n-th log message containing `text` (e.g. a movie's duration) |
| `VC2_THREAD_DUMP=1` | log kernel thread states every 5 s (with CPU time, kernel calls and the last call per thread, and the scheduler's timer/idle time) |
| `VC2_SYNC_PRESENT=1`, `VC2_PRESENT_CHECK=1` | swap on the emulation thread (old behaviour) / log the window image brightness every 300 swaps |
| `VC2_AUDIO_DUMP`, `VC2_SAS_DUMP` | WAV dumps of the final mix and the SAS output |
| `VC2_WATCH=addr`, `VC2_BREAK=func`, `VC2_PRINTF=func` | hardware data watch, function call counter, and capture of the game's debug printf |
| `VC2_GE_TRACE`, `VC2_GE_LOG`, `VC2_GPU_STATS`, `VC2_GE_SELFTEST` | GPU diagnostics |
| `VC2_FPS_STATS=1` | log the game's frame rate every second and which code waits for vblanks |
| `VC2_FAST_FORWARD=from-to` | hold fast-forward over a range of vblanks (as holding ` does) |
| `VC2_DUMPS=folder` | where `vc2tool` looks for `ULUS10515_EBOOT.BIN` (default: the current folder) |
| `--trace`, `--debug` | HLE call trace / debug logging |

### Verification notes (2026-11-14)

* Built from the disc plus a stand-in for PPSSPP's dump (the decrypted ELF followed by the extra
  bytes PPSSPP writes, 6,840,592 bytes in all): the launcher trims and verifies it, copies it
  into the install folder, and the game boots from it ("EBOOT.BIN from the decrypted copy" in the
  log). Without a dump, or with a wrong file, the build stops with a message saying what is
  needed; `--prepare` exits with code 24.
* Scripted input reaches the first battle's tactical map in about 70 seconds and stays there.
* Frame dumps (`VC2_DUMP_DIR`) are encoded on a worker thread. While movies play they can still
  cost a frame or two, so frame rates are measured without dumps.
* Scripted input: confirmation dialogs that default to "No" need a cursor press that lands after
  the dialog's opening animation (e.g. `...:left`, `+30:left`, `+60:cross`).

### Frame rate (2026-11-14)

* VC2 paces itself with the same engine "set frame rate" function as VC3 (`0x08bd320c` here,
  found by its instruction pattern): a0 is vblanks per frame, and it also derives the float time
  step that game logic scales by. The profile hooks it (`GameProfile::hooks` -> `psp_hook`).
* Measured on an idle PC (4-minute runs to the battle map, one sample per second, no dumps):
  * As on PSP: 188 samples at 30 FPS (story scenes, map) and 32 at 60 (menus); the few in
    between are one-second windows that straddle a switch. No audio underruns.
  * With **60 FPS gameplay** (`HighFrameRate=1`): 230 of 238 samples at 60 FPS, the rest 56-61
    during the logo and opening. No audio underruns.
* Game speed: the battle map's pulsing marker repeats every 60-62 vblanks in both modes (dumps
  every other frame, 9000-9120), so the 60 FPS mode draws the same motion twice as smoothly
  rather than running it twice as fast. The option stays **off by default**, as in VC3, until it
  has had more hands-on play testing.

### PC options (2026-11-14)

Each option was set in `vc2recomp.ini` and the game run to the same screen (frame dumps are the
final window image, before the overlay):

* Internal resolution 1x / 6x, texture upscaling, FXAA, stretch and integer-scale aspect,
  borderless and exclusive fullscreen (1920x1080 output on a 1920x1080 display), the FPS overlay
  (window screenshot), V-Sync on/off and 60 FPS gameplay all change what they should. MSAA 4x
  is active (logged); the screens tested were 2D, which have no polygon edges to smooth.
* **Texture filtering "Nearest"** only changed the final scaling of the picture to the window,
  which at the default 3x (1440x816 in a 1440x816 window) is 1:1, so it did nothing. It now
  samples the game's textures as sharp pixels (render-to-texture effects keep the game's own
  filter); 5.3% of the pixels of the tested screen change.
* **Frame limit** was read from the settings but used nowhere: the game runs on PSP vblank timing.
  The slider was removed (the frame rate comes from the game, the 60 FPS option and V-Sync); the
  ini key is still read so older settings files load.
* **Fast-forward** (hold `) set a flag nothing used. It now runs the vblank timer 3x faster while
  held: the battle map ran at 89.9 FPS instead of 30 (1,800 vblanks in 10 s), returned to a
  steady 30 when released, with no audio underruns.
* Measuring the window's present rate needs a display that is on: with the monitor off, Windows
  counted 1 refresh in 2 s, a vsync'd swap took ~79 ms and the window showed 12-13 frames per
  second (exclusive fullscreen ~1), while the game itself kept its exact 30/60 FPS. With V-Sync
  off it presented 60. `VC2_FPS_STATS` logs the present rate and the presentation thread's
  timings for this.

### Presentation thread (2026-11-13)

* The final image (game + overlay) is drawn into one of three textures shared with a second
  GL context, and a dedicated thread blits the newest one to the window and swaps. Only that
  thread waits for vsync, so a 59.94 Hz PSP vblank drifting against a 60 Hz monitor never stalls
  the game threads (found with VC3, whose boot loader crawled when the swap blocked them).
