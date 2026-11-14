#pragma once
// Reading the game's executable modules, shared by the recompiler, the launcher and the runtime.
// Plain modules come straight from the disc. Encrypted ones (Valkyria Chronicles II's EBOOT.BIN) come
// from the decrypted copy the player dumped from their own disc with PPSSPP: this project contains no
// console keys.
#include <filesystem>
#include <string>
#include <vector>

#include "game_layout.h"
#include "types.h"

namespace vc2 {

class IsoImage;

struct ModuleRead {
    bool ok = false;
    bool fromDump = false;          // read from the player's decrypted copy instead of the disc
    bool known = true;              // the exact executable the profile was made from (its SHA-1)
    std::filesystem::path dumpPath; // the copy used
    std::string error;              // user-facing reason when !ok
};

// Whether the module is encrypted on this disc (needs the player's decrypted copy).
bool moduleNeedsDump(IsoImage& iso, const GameModuleSpec& spec);

// Checks a decrypted copy against the disc's encrypted module: an ELF, trimmed to the size in the disc's
// header (PPSSPP writes a little more). The expected SHA-1 identifies the exact copy the profile was made
// from; another copy of the same module (the name in the disc's header matches the ELF's) is accepted
// with *known = false, as discs of other dumps or printings may differ. On success `elf` holds the
// trimmed ELF.
bool checkModuleDump(IsoImage& iso, const GameModuleSpec& spec, const std::filesystem::path& dump, Bytes& elf,
                     std::string& error, bool* known = nullptr);

// Reads a module as an ELF: from the disc, or for encrypted ones from spec.dumpName in the first of
// `dumpDirs` that has a valid copy.
ModuleRead readGameModule(IsoImage& iso, const GameModuleSpec& spec,
                          const std::vector<std::filesystem::path>& dumpDirs, Bytes& elf);

// Lowercase hex SHA-1.
std::string sha1Hex(const Bytes& data);

}  // namespace vc2
