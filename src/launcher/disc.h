#pragma once
// Quick validation of the user's disc image (no decryption): PARAM.SFO, disc ID, module presence, size sanity.
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace vc2l {

struct DiscInfo {
    bool ok = false;           // supported disc, looks complete
    bool isPspDisc = false;    // PARAM.SFO parsed (even if it is another game)
    std::string error;         // user-facing reason when !ok
    std::string title, discId, discVersion, volumeId;
    std::string gameName;      // plain-text name from the game profile (e.g. "Valkyria Chronicles II")
    std::string exeName;       // executable name for this game (e.g. "VC2Recompiled.exe")
    uint64_t fileSize = 0;
    std::vector<uint8_t> iconPng;  // PSP_GAME/ICON0.PNG, kept in memory only
};

DiscInfo probeDisc(const std::filesystem::path& iso);

// The decrypted executable an encrypted module needs (Valkyria Chronicles II's EBOOT.BIN; this project has
// no console keys). PPSSPP dumps it from the player's disc as ULUS10515_EBOOT.BIN.
struct DumpCheck {
    bool needed = false;          // the disc has an encrypted module
    bool ok = false;              // a verified copy was found at `path`
    bool missing = false;         // no file was found at all (as opposed to one that was turned down)
    std::filesystem::path path;
    std::string name;             // the name PPSSPP gives it
    std::string message;          // status for the UI
};

// Looks for a verified copy: the player's choice first, then the install folder, next to the disc image
// and PPSSPP's dump folders.
DumpCheck findDump(const std::filesystem::path& iso, const std::filesystem::path& chosen,
                   const std::filesystem::path& installDir);
// Copies the verified copy into the install folder, where the recompiler and the game read it.
bool installDump(const DumpCheck& dump, const std::filesystem::path& installDir, std::string& error);
// PPSSPP's dump folders (PSP/SYSTEM/DUMP in its memory stick folder) that exist on this PC.
std::vector<std::filesystem::path> ppssppDumpDirs();

}  // namespace vc2l
