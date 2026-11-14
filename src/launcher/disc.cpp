#include "disc.h"

#include <windows.h>
#include <shlobj.h>

#include "common/game_layout.h"
#include "common/game_module.h"
#include "common/iso9660.h"
#include "common/sfo.h"
#include "util.h"

namespace vc2l {

DiscInfo probeDisc(const std::filesystem::path& isoPath) {
    DiscInfo d;
    std::error_code ec;
    if (isoPath.empty()) {
        d.error = "No disc image selected.";
        return d;
    }
    if (!std::filesystem::is_regular_file(isoPath, ec)) {
        d.error = "The file does not exist: " + pathUtf8(isoPath);
        return d;
    }
    vc2::IsoImage iso;
    std::string err;
    if (!iso.open(isoPath, &err)) {
        d.error = "This file is not a readable ISO disc image (" + err + ").\nMake sure you selected an .iso dump "
                  "of your UMD (CSO/CHD compressed images are not supported).";
        return d;
    }
    d.fileSize = iso.totalSize();
    d.volumeId = iso.volumeId();
    Bytes sfoRaw;
    vc2::SfoData sfo;
    if (!iso.readFile("PSP_GAME/PARAM.SFO", sfoRaw) || !vc2::parseSfo(sfoRaw, sfo)) {
        d.error = "This disc image is not a PSP game (PSP_GAME/PARAM.SFO is missing or damaged).";
        return d;
    }
    d.isPspDisc = true;
    d.title = sfo.get("TITLE");
    d.discId = sfo.get("DISC_ID");
    d.discVersion = sfo.get("DISC_VERSION");
    iso.readFile("PSP_GAME/ICON0.PNG", d.iconPng);
    const vc2::GameProfile* profile = vc2::findGameProfile(d.discId.c_str());
    if (!profile) {
        d.error = "Unsupported disc: this is \"" + d.title + "\" (" + d.discId +
                  ").\nThis launcher builds Valkyria Chronicles II: ULUS10515 (North America).";
        return d;
    }
    d.gameName = profile->name;
    d.exeName = profile->exeName;
    // Truncated or damaged dumps: every recompiled module must be fully inside the image.
    for (size_t mi = 0; mi < vc2::profileModuleCount(*profile); mi++) {
        const auto& m = profile->modules[mi];
        const vc2::IsoEntry* e = iso.find(m.discPath);
        if (!e || e->size == 0) {
            d.error = std::string("The disc image is incomplete: ") + m.discPath + " is missing.";
            return d;
        }
        if ((uint64_t)e->lba * 2048 + e->size > d.fileSize) {
            d.error = std::string("The disc image is truncated (") + m.discPath +
                      " lies beyond the end of the file). Please re-dump your UMD.";
            return d;
        }
        uint8_t probe[16];
        if (!iso.readBytes((uint64_t)e->lba * 2048, probe, sizeof probe)) {
            d.error = std::string("Could not read ") + m.discPath + " from the disc image.";
            return d;
        }
    }
    d.ok = true;
    return d;
}

std::vector<std::filesystem::path> ppssppDumpDirs() {
    // An installed PPSSPP keeps its memory stick in Documents\PPSSPP (a portable one in "memstick" next to
    // PPSSPP itself, which the player picks by hand).
    std::vector<std::filesystem::path> found;
    std::error_code ec;
    PWSTR docs = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &docs))) {
        auto d = std::filesystem::path(docs) / "PPSSPP" / "PSP" / "SYSTEM" / "DUMP";
        if (std::filesystem::is_directory(d, ec)) found.push_back(d);
        CoTaskMemFree(docs);
    }
    return found;
}

DumpCheck findDump(const std::filesystem::path& isoPath, const std::filesystem::path& chosen,
                   const std::filesystem::path& installDir) {
    DumpCheck r;
    vc2::IsoImage iso;
    Bytes sfoRaw;
    vc2::SfoData sfo;
    if (isoPath.empty() || !iso.open(isoPath) || !iso.readFile("PSP_GAME/PARAM.SFO", sfoRaw) || !vc2::parseSfo(sfoRaw, sfo))
        return r;
    const vc2::GameProfile* profile = vc2::findGameProfile(sfo.get("DISC_ID").c_str());
    if (!profile) return r;
    const vc2::GameModuleSpec* spec = nullptr;
    for (size_t i = 0; i < vc2::profileModuleCount(*profile) && !spec; i++)
        if (vc2::moduleNeedsDump(iso, profile->modules[i])) spec = &profile->modules[i];
    if (!spec) return r;
    r.needed = true;
    r.name = spec->dumpName;

    std::vector<std::filesystem::path> candidates;
    if (!chosen.empty()) candidates.push_back(chosen);
    if (!installDir.empty()) candidates.push_back(installDir / r.name);
    candidates.push_back(isoPath.parent_path() / r.name);
    for (auto& d : ppssppDumpDirs()) candidates.push_back(d / r.name);

    // The first reason a file was turned down (the player's own choice first), when none is valid.
    std::string firstError;
    std::error_code ec;
    for (size_t i = 0; i < candidates.size(); i++) {
        const auto& c = candidates[i];
        const bool isChosen = i == 0 && !chosen.empty();
        if (!std::filesystem::is_regular_file(c, ec)) {
            if (isChosen) firstError = "File not found: " + pathUtf8(c);
            continue;
        }
        Bytes elf;
        std::string err;
        if (vc2::checkModuleDump(iso, *spec, c, elf, err)) {
            r.ok = true;
            r.path = c;
            r.message = "Verified: " + pathUtf8(c);
            return r;
        }
        if (firstError.empty()) firstError = err;
    }
    r.missing = firstError.empty();
    r.message = !r.missing ? firstError
                           : "Needed: " + r.name + ", the decrypted game executable dumped from your disc with "
                             "PPSSPP (the launcher's \"How do I get it?\" and the README explain how).";
    return r;
}

bool installDump(const DumpCheck& dump, const std::filesystem::path& installDir, std::string& error) {
    if (!dump.ok) {
        error = dump.message;
        return false;
    }
    const auto target = installDir / dump.name;
    std::error_code ec;
    if (std::filesystem::exists(target, ec) && std::filesystem::equivalent(dump.path, target, ec)) return true;
    std::filesystem::create_directories(installDir, ec);
    if (!std::filesystem::copy_file(dump.path, target, std::filesystem::copy_options::overwrite_existing, ec)) {
        error = "Could not copy " + dump.name + " into the install folder: " + ec.message();
        return false;
    }
    return true;
}

}  // namespace vc2l
