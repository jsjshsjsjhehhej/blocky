#pragma once


#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace AssetAudit {

namespace fs = std::filesystem;

enum class Severity { Ok = 0, Warn = 1, Error = 2 };

inline const char* severityName(Severity s) {
    switch (s) {
    case Severity::Error: return "ERRORE";
    case Severity::Warn:  return "AVVISO";
    default:              return "OK";
    }
}

struct Finding {
    Severity severity = Severity::Ok;
    std::string path;
    std::string rule;
    std::string detail;
};

struct AssetRecord {
    std::string id;
    std::string format;
    std::uint64_t bytes = 0;
    std::uint64_t checksum = 0;
    std::int64_t modified = 0;
};

struct Options {
    std::uint64_t heavyBytes = 8ull * 1024 * 1024;
    std::vector<std::string> devExtensions = {".py", ".bak", ".tmp", ".orig", ".md"};
    std::vector<std::string> reviewNames = {"roblox", "noob", "hellod", "character"};
};

struct Report {
    std::vector<AssetRecord> assets;
    std::vector<Finding> findings;
    int errors = 0;
    int warnings = 0;
    std::uint64_t totalBytes = 0;

    void add(Severity sev, const std::string& path, const std::string& rule,
             const std::string& detail) {
        findings.push_back({sev, path, rule, detail});
        if (sev == Severity::Error) ++errors;
        else if (sev == Severity::Warn) ++warnings;
    }

    bool releaseReady() const { return errors == 0; }
};


inline std::string lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

inline std::string normalizeSlashes(std::string s) {
    for (char& c : s) if (c == '\\') c = '/';
    return s;
}

inline std::uint64_t hashBytes(const std::vector<unsigned char>& data) {
    std::uint64_t h = 1469598103934665603ull;
    for (unsigned char b : data) {
        h ^= (std::uint64_t)b;
        h *= 1099511628211ull;
    }
    return h;
}

inline bool readAll(const fs::path& p, std::vector<unsigned char>& out) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    f.seekg(0, std::ios::end);
    const std::streamoff n = f.tellg();
    if (n < 0) return false;
    f.seekg(0, std::ios::beg);
    out.resize((size_t)n);
    if (n > 0) f.read((char*)out.data(), n);
    return f.good() || f.eof();
}

inline bool startsWith(const std::vector<unsigned char>& d, const char* sig, size_t n) {
    if (d.size() < n) return false;
    for (size_t i = 0; i < n; ++i)
        if (d[i] != (unsigned char)sig[i]) return false;
    return true;
}

inline std::string sniffFormat(const std::vector<unsigned char>& d) {
    if (d.empty()) return "vuoto";

    if (startsWith(d, "\x89PNG\r\n\x1a\n", 8)) return "png";
    if (startsWith(d, "glTF", 4)) return "glb";
    if (startsWith(d, "Kaydara FBX Binary", 18)) return "fbx";
    if (startsWith(d, "; FBX", 5)) return "fbx-ascii";
    if (startsWith(d, "RIFF", 4) && d.size() > 12 && startsWith({d.begin() + 8, d.begin() + 12}, "WAVE", 4))
        return "wav";
    if (startsWith(d, "OggS", 4)) return "ogg";
    if (startsWith(d, "\xFF\xD8\xFF", 3)) return "jpg";
    if (d.size() >= 4 && d[0] == 0 && d[1] == 0 && d[2] == 1 && d[3] == 0) return "ico";
    if (startsWith(d, "\x1b" "Lua", 4)) return "lua-bytecode";

    const size_t probe = std::min<size_t>(d.size(), 512);
    for (size_t i = 0; i < probe; ++i) {
        const unsigned char c = d[i];
        if (c == 0) return "binario";
        if (c < 9 || (c > 13 && c < 32)) return "binario";
    }
    return "testo";
}

inline std::vector<std::string> expectedFormats(const std::string& ext) {
    if (ext == ".png")  return {"png"};
    if (ext == ".jpg" || ext == ".jpeg") return {"jpg"};
    if (ext == ".glb")  return {"glb"};
    if (ext == ".fbx")  return {"fbx", "fbx-ascii"};
    if (ext == ".wav")  return {"wav"};
    if (ext == ".ogg")  return {"ogg"};
    if (ext == ".ico")  return {"ico"};
    if (ext == ".lua")  return {"testo", "lua-bytecode"};
    if (ext == ".json" || ext == ".txt" || ext == ".html" || ext == ".rc" || ext == ".md")
        return {"testo"};
    return {};
}

inline bool containsAny(const std::string& haystack, const std::vector<std::string>& needles) {
    for (const std::string& n : needles)
        if (haystack.find(n) != std::string::npos) return true;
    return false;
}


inline Report run(const std::string& rootDir, const Options& opt = {}) {
    Report report;
    const fs::path root(rootDir);

    if (!fs::exists(root)) {
        report.add(Severity::Error, rootDir, "cartella-mancante",
                   "la cartella degli asset non esiste");
        return report;
    }

    std::map<std::uint64_t, std::string> seen;

    std::error_code ec;
    for (fs::recursive_directory_iterator it(root, ec), end; it != end; it.increment(ec)) {
        if (ec) break;
        if (!it->is_regular_file(ec)) continue;

        const fs::path& p = it->path();
        const std::string rel = normalizeSlashes(fs::relative(p, root, ec).string());
        const std::string ext = lower(p.extension().string());
        const std::string name = lower(p.filename().string());

        std::vector<unsigned char> data;
        if (!readAll(p, data)) {
            report.add(Severity::Error, rel, "illeggibile",
                       "il file esiste ma non si riesce ad aprirlo");
            continue;
        }

        AssetRecord rec;
        rec.id = rel;
        rec.bytes = (std::uint64_t)data.size();
        rec.checksum = hashBytes(data);
        rec.format = sniffFormat(data);

        const auto ft = fs::last_write_time(p, ec);
        rec.modified = ec ? 0 : (std::int64_t)ft.time_since_epoch().count();

        report.totalBytes += rec.bytes;

        if (rec.bytes == 0) {
            report.add(Severity::Error, rel, "vuoto", "0 byte: il caricamento fallirà");
        }

        const std::vector<std::string> want = expectedFormats(ext);
        if (!want.empty() && rec.bytes > 0 &&
            std::find(want.begin(), want.end(), rec.format) == want.end()) {
            report.add(Severity::Error, rel, "formato-errato",
                       "estensione " + ext + " ma i byte dicono '" + rec.format + "'");
        }

        if (rec.bytes > opt.heavyBytes) {
            report.add(Severity::Warn, rel, "pesante",
                       std::to_string(rec.bytes / 1024) + " KB in un solo asset");
        }

        auto dup = seen.find(rec.checksum);
        if (dup != seen.end() && rec.bytes > 0) {
            report.add(Severity::Warn, rel, "duplicato",
                       "identico a " + dup->second);
        } else {
            seen.emplace(rec.checksum, rel);
        }

        if (std::find(opt.devExtensions.begin(), opt.devExtensions.end(), ext)
            != opt.devExtensions.end()) {
            report.add(Severity::Warn, rel, "residuo-sviluppo",
                       "file di lavoro: non serve agli utenti");
        }
        if (name.find("_orig") != std::string::npos ||
            name.rfind("fix_", 0) == 0 ||
            name.rfind("gen_", 0) == 0 ||
            name.rfind("rebuild_", 0) == 0) {
            report.add(Severity::Warn, rel, "residuo-sviluppo",
                       "sembra una versione di lavoro o uno script di build");
        }

        if (containsAny(name, opt.reviewNames)) {
            report.add(Severity::Warn, rel, "origine-da-verificare",
                       "il nome rimanda a materiale di terzi: controlla prima di pubblicare");
        }

        report.assets.push_back(std::move(rec));
    }

    if (report.assets.empty())
        report.add(Severity::Warn, rootDir, "vuota", "nessun asset trovato");

    return report;
}

inline bool validateFile(const std::string& path, std::string& why,
                         std::uint64_t maxBytes = 64ull * 1024 * 1024) {
    std::error_code ec;
    if (!fs::exists(path, ec)) { why = "file inesistente"; return false; }

    const auto size = fs::file_size(path, ec);
    if (ec) { why = "dimensione illeggibile"; return false; }
    if (size == 0) { why = "file vuoto"; return false; }
    if (size > maxBytes) { why = "file troppo grande"; return false; }

    std::vector<unsigned char> head;
    if (!readAll(path, head)) { why = "apertura fallita"; return false; }

    const std::string ext = lower(fs::path(path).extension().string());
    const std::vector<std::string> want = expectedFormats(ext);
    if (!want.empty()) {
        const std::string got = sniffFormat(head);
        if (std::find(want.begin(), want.end(), got) == want.end()) {
            why = "contenuto '" + got + "' non coerente con " + ext;
            return false;
        }
    }
    return true;
}


inline std::string jsonEscape(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += c; }
        else if (c == '\n') o += "\\n";
        else o += c;
    }
    return o;
}

inline std::string toJson(const Report& r) {
    std::ostringstream j;
    j << "{\n  \"format\": \"BlockyAssetManifest\",\n  \"version\": 1,\n";
    j << "  \"assetCount\": " << r.assets.size() << ",\n";
    j << "  \"totalBytes\": " << r.totalBytes << ",\n";
    j << "  \"errors\": " << r.errors << ",\n";
    j << "  \"warnings\": " << r.warnings << ",\n";
    j << "  \"assets\": [\n";

    for (size_t i = 0; i < r.assets.size(); ++i) {
        const AssetRecord& a = r.assets[i];
        j << "    {\"id\":\"" << jsonEscape(a.id) << "\""
          << ",\"format\":\"" << a.format << "\""
          << ",\"bytes\":" << a.bytes
          << ",\"checksum\":\"" << std::hex << a.checksum << std::dec << "\""
          << ",\"modified\":" << a.modified << "}";
        if (i + 1 < r.assets.size()) j << ",";
        j << "\n";
    }
    j << "  ]\n}\n";
    return j.str();
}

}
