#pragma once


#include "lua.h"
#include "lualib.h"
#include <Luau/Compiler.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace SafeBytecode {

inline constexpr uint16_t kVersion = 1;
inline constexpr char kMagic[4] = { 'B', 'B', 'C', 'S' };
inline constexpr uint32_t kSealKey = 0xB10C5AFEU;

inline uint32_t fnv(const void* data, size_t n, uint32_t seed) {
    const auto* p = static_cast<const uint8_t*>(data);
    uint32_t h = seed;
    for (size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

inline uint32_t sealPayload(const std::string& gameId,
                            const uint8_t* payload, size_t len) {
    uint32_t h = 0x811C9DC5u;
    const char* tag = "BlockySafeBytecodeV1";
    h = fnv(tag, std::strlen(tag), h);
    h ^= kSealKey;
    h = fnv(gameId.data(), gameId.size(), h);
    h = fnv(&kVersion, sizeof(kVersion), h);
    if (payload && len)
        h = fnv(payload, len, h);
    h ^= 0xC0DEC0DEu;
    return h;
}

inline bool compileLuaToBytecode(const std::string& source,
                                 const std::string& chunkName,
                                 std::vector<uint8_t>& outBc,
                                 std::string& err) {
    outBc.clear();
    // Luau::compile produce gia' il bytecode finale (niente passaggio di
    // "dump" separato come con lua_dump/Lua 5.4) — ma non segnala un
    // errore di sintassi con un codice di ritorno: lo si scopre provando a
    // caricarlo con luau_load, che sa interpretare il marcatore di errore
    // che Luau::compile incorpora nel bytecode stesso.
    const std::string bc = Luau::compile(source);
    lua_State* L = luaL_newstate();
    if (!L) {
        err = "luaL_newstate fallita";
        return false;
    }
    const std::string name = chunkName.empty() ? "=(blocky)" : chunkName;
    if (luau_load(L, name.c_str(), bc.data(), bc.size(), 0) != 0) {
        err = lua_tostring(L, -1) ? lua_tostring(L, -1) : "compile error";
        lua_close(L);
        return false;
    }
    lua_close(L);
    if (bc.empty()) {
        err = "bytecode vuoto";
        return false;
    }
    outBc.assign(bc.begin(), bc.end());
    return true;
}

inline bool packSealed(const std::string& gameId,
                       const std::vector<uint8_t>& luaBc,
                       std::vector<uint8_t>& outFile,
                       std::string& err) {
    if (luaBc.empty()) {
        err = "payload vuoto";
        return false;
    }
    if (gameId.size() > 200) {
        err = "gameId troppo lungo";
        return false;
    }
    uint16_t idLen = (uint16_t)gameId.size();
    uint16_t flags = 0;
    uint32_t payLen = (uint32_t)luaBc.size();
    uint32_t seal = sealPayload(gameId, luaBc.data(), luaBc.size());

    outFile.clear();
    outFile.reserve(4 + 2 + 2 + 2 + idLen + 4 + payLen + 4);
    outFile.insert(outFile.end(), kMagic, kMagic + 4);
    auto put16 = [&](uint16_t v) {
        outFile.push_back((uint8_t)(v & 0xff));
        outFile.push_back((uint8_t)((v >> 8) & 0xff));
    };
    auto put32 = [&](uint32_t v) {
        outFile.push_back((uint8_t)(v & 0xff));
        outFile.push_back((uint8_t)((v >> 8) & 0xff));
        outFile.push_back((uint8_t)((v >> 16) & 0xff));
        outFile.push_back((uint8_t)((v >> 24) & 0xff));
    };
    put16(kVersion);
    put16(flags);
    put16(idLen);
    outFile.insert(outFile.end(), gameId.begin(), gameId.end());
    put32(payLen);
    outFile.insert(outFile.end(), luaBc.begin(), luaBc.end());
    put32(seal);
    return true;
}

struct Unpacked {
    std::string gameId;
    std::vector<uint8_t> luaBc;
    bool ok = false;
    std::string err;
};

inline uint16_t rd16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
inline uint32_t rd32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

inline Unpacked unpackSealed(const uint8_t* data, size_t n) {
    Unpacked u;
    if (!data || n < 4 + 2 + 2 + 2 + 4 + 4) {
        u.err = "file BBCS troppo corto";
        return u;
    }
    if (std::memcmp(data, kMagic, 4) != 0) {
        u.err = "magic BBCS non valido";
        return u;
    }
    size_t o = 4;
    uint16_t ver = rd16(data + o); o += 2;
    if (ver != kVersion) {
        u.err = "versione BBCS non supportata";
        return u;
    }
    o += 2;
    uint16_t idLen = rd16(data + o); o += 2;
    if (o + idLen + 4 + 4 > n) {
        u.err = "header BBCS corrotti";
        return u;
    }
    u.gameId.assign((const char*)data + o, (const char*)data + o + idLen);
    o += idLen;
    uint32_t payLen = rd32(data + o); o += 4;
    if (o + payLen + 4 > n) {
        u.err = "payload BBCS troncato";
        return u;
    }
    u.luaBc.assign(data + o, data + o + payLen);
    o += payLen;
    uint32_t seal = rd32(data + o);
    uint32_t expect = sealPayload(u.gameId, u.luaBc.data(), u.luaBc.size());
    if (seal != expect) {
        u.err = "SIGILLO ROTTO — bytecode manomesso o non firmato da Blocky";
        u.luaBc.clear();
        return u;
    }
    u.ok = true;
    return u;
}

inline Unpacked unpackSealedFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        Unpacked u;
        u.err = "impossibile aprire " + path;
        return u;
    }
    std::vector<uint8_t> buf((std::istreambuf_iterator<char>(f)),
                             std::istreambuf_iterator<char>());
    return unpackSealed(buf.data(), buf.size());
}

inline bool writeFile(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(data.data()), (std::streamsize)data.size());
    return (bool)f;
}

inline bool compileSourceToSealedFile(const std::string& source,
                                      const std::string& gameId,
                                      const std::string& outPath,
                                      std::string& err) {
    std::vector<uint8_t> bc;
    if (!compileLuaToBytecode(source, "@" + gameId, bc, err))
        return false;
    std::vector<uint8_t> packed;
    if (!packSealed(gameId, bc, packed, err))
        return false;
    if (!writeFile(outPath, packed)) {
        err = "scrittura fallita: " + outPath;
        return false;
    }
    return true;
}

inline bool isBbcPath(const std::string& path) {
    if (path.size() < 4) return false;
    auto ext = path.substr(path.size() - 4);
    for (char& c : ext)
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    return ext == ".bbc";
}

inline bool looksLikeBbcMagic(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    char m[4] = {};
    f.read(m, 4);
    return f.gcount() == 4 && std::memcmp(m, kMagic, 4) == 0;
}

}
