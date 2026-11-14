#include "game_module.h"

#include <cstdio>
#include <fstream>

#include "../crypto/prx_decrypt.h"
#include "iso9660.h"

namespace vc2 {

std::string sha1Hex(const Bytes& data) {
    u8 d[20];
    sha1(data.data(), data.size(), d);
    char hex[41];
    for (int i = 0; i < 20; i++) snprintf(hex + i * 2, 3, "%02x", d[i]);
    return hex;
}

static bool readHead(IsoImage& iso, const GameModuleSpec& spec, u8 (&head)[0x40]) {
    const IsoEntry* e = iso.find(spec.discPath);
    return e && e->size >= sizeof head && iso.readBytes((u64)e->lba * 2048, head, sizeof head);
}

bool moduleNeedsDump(IsoImage& iso, const GameModuleSpec& spec) {
    u8 head[0x40];
    return spec.dumpName && readHead(iso, spec, head) && memcmp(head, "~PSP", 4) == 0;
}

bool checkModuleDump(IsoImage& iso, const GameModuleSpec& spec, const std::filesystem::path& dump, Bytes& elf,
                     std::string& error) {
    u8 head[0x40];
    if (!readHead(iso, spec, head) || memcmp(head, "~PSP", 4) != 0) {
        error = std::string(spec.discPath) + " on this disc is not an encrypted executable.";
        return false;
    }
    const u32 elfSize = rd32(head + 0x28);  // ~PSP header: size of the decrypted ELF
    std::ifstream f(dump, std::ios::binary | std::ios::ate);
    if (!f) {
        error = "Cannot open " + dump.filename().string() + ".";
        return false;
    }
    const auto size = (u64)f.tellg();
    if (size < elfSize) {
        error = dump.filename().string() + " is too small to be the decrypted " + spec.discPath + ".";
        return false;
    }
    elf.resize(elfSize);
    f.seekg(0);
    f.read((char*)elf.data(), elfSize);
    if (!f || memcmp(elf.data(), "\x7F" "ELF", 4) != 0) {
        error = dump.filename().string() + " is not a decrypted executable (no ELF header).";
        return false;
    }
    if (spec.sha1 && sha1Hex(elf) != spec.sha1) {
        error = dump.filename().string() + " is not the decrypted " + spec.discPath +
                " of this disc (its contents don't match).";
        return false;
    }
    return true;
}

ModuleRead readGameModule(IsoImage& iso, const GameModuleSpec& spec,
                          const std::vector<std::filesystem::path>& dumpDirs, Bytes& elf) {
    ModuleRead r;
    if (!moduleNeedsDump(iso, spec)) {
        Bytes raw;
        if (!iso.readFile(spec.discPath, raw)) {
            r.error = std::string("Missing file on disc: ") + spec.discPath;
            return r;
        }
        DecryptResult dr = decryptPspExecutable(raw, elf);
        if (dr != DecryptResult::Ok && dr != DecryptResult::NotEncrypted) {
            r.error = std::string("Could not load ") + spec.discPath + ": " + decryptResultString(dr);
            return r;
        }
        r.ok = true;
        return r;
    }
    std::string firstError;
    std::error_code ec;
    for (const auto& dir : dumpDirs) {
        const auto path = dir / spec.dumpName;
        if (!std::filesystem::is_regular_file(path, ec)) continue;
        std::string err;
        if (checkModuleDump(iso, spec, path, elf, err)) {
            r.ok = r.fromDump = true;
            r.dumpPath = path;
            return r;
        }
        if (firstError.empty()) firstError = err;
    }
    r.error = !firstError.empty() ? firstError
                                  : std::string(spec.discPath) + " is encrypted on the disc and its decrypted copy (" +
                                        spec.dumpName + ") was not found. Dump it from your disc with PPSSPP and pick "
                                        "it in the launcher.";
    return r;
}

}  // namespace vc2
