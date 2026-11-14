#pragma once
// Facts about the supported discs that both the recompiler and the runtime must agree on.
#include <cstring>

#include "types.h"

namespace vc2 {

struct GameModuleSpec {
    const char* discPath;
    u32 base;  // load address chosen for the relocatable module
    // Encrypted on retail discs: this project contains no console keys, so the player supplies the
    // decrypted executable, dumped from their own disc with PPSSPP ("Dump decrypted EBOOT.BIN on game
    // boot", which names it <DISC_ID>_EBOOT.BIN). sha1 is that of the decrypted ELF, trimmed to the size
    // the disc's header gives. Both null for modules that are plain on the disc.
    const char* dumpName = nullptr;
    const char* sha1 = nullptr;
};

struct GameProfile {
    const char* discId;        // PARAM.SFO DISC_ID
    const char* name;          // display name (plain text, used for window titles / logs)
    const char* exeName;       // file name of the built executable
    GameModuleSpec modules[4]; // first entry is the main executable; {nullptr} terminates
    // Guest functions whose recompiled bodies call psp_hook(c, addr) on entry, so the runtime can
    // adjust arguments (e.g. the engine's frame-rate request). 0 terminates.
    u32 hooks[4];
};

// The main executable is loaded where the PSP kernel would place it. Extra game-supplied PRX
// modules are pinned directly after it (the runtime reserves that range before the game starts
// so sceKernelLoadModule can return it).
inline constexpr GameProfile kGameProfiles[] = {
    // Valkyria Chronicles II (USA). EBOOT.BIN is encrypted (BOOT.BIN is blanked); LIBSUPPREACC.PRX is a
    // plain ELF and loads straight from the disc.
    {"ULUS10515", "Valkyria Chronicles II", "VC2Recompiled.exe",
     {{"PSP_GAME/SYSDIR/EBOOT.BIN", 0x08804000, "ULUS10515_EBOOT.BIN", "1a5729186d2a27285826c9f7ecd0fcdf780f2559"},
      {"PSP_GAME/USRDIR/MODULE/LIBSUPPREACC.PRX", 0x08D80000},
      {nullptr, 0}},
     // 0x08bd320c: engine "set frame rate" (a0 = vblanks per frame; it also derives the float time step
     // game logic scales by). Hooked for the 60 FPS gameplay option.
     {0x08bd320c, 0}},
};

inline const GameProfile* findGameProfile(const char* discId) {
    for (const auto& p : kGameProfiles)
        if (discId && strcmp(p.discId, discId) == 0) return &p;
    return nullptr;
}

inline size_t profileModuleCount(const GameProfile& p) {
    size_t n = 0;
    while (n < 4 && p.modules[n].discPath) n++;
    return n;
}

}  // namespace vc2
