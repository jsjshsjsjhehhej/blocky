#pragma once


#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "zip.h"

namespace ZipArchive {

struct ExtractResult {
    bool ok = false;
    int entries = 0;
    std::string error;
};

inline ExtractResult extract(const std::string& zipPath, const std::string& destDir) {
    ExtractResult r;

    auto onEntry = [](const char*, void* arg) -> int {
        int* count = static_cast<int*>(arg);
        ++(*count);
        return 0;
    };

    const int rc = zip_extract(zipPath.c_str(), destDir.c_str(), onEntry, &r.entries);
    if (rc < 0) {
        r.ok = false;
        r.error = "zip_extract fallita (codice " + std::to_string(rc) + ")";
        return r;
    }
    r.ok = true;
    return r;
}

inline ExtractResult extractFromMemory(const std::vector<std::uint8_t>& bytes,
                                       const std::string& destDir) {
    ExtractResult r;
    if (bytes.empty()) {
        r.error = "archivio vuoto";
        return r;
    }

    int entries = 0;
    auto onEntry = [](const char*, void* arg) -> int {
        ++(*static_cast<int*>(arg));
        return 0;
    };

    const int rc = zip_stream_extract(reinterpret_cast<const char*>(bytes.data()),
                                      bytes.size(), destDir.c_str(), onEntry, &entries);
    if (rc < 0) {
        r.error = "estrazione fallita (codice " + std::to_string(rc) + ")";
        return r;
    }
    r.ok = true;
    r.entries = entries;
    return r;
}


/**
 * Impacchetta una cartella in un archivio zip tenuto in memoria.
 *
 * Serve per mandare un gioco appena pubblicato al server: blocky_hostd accetta
 * "PUT /publish/<id>" con un archivio nel corpo della richiesta, ma finora qui
 * c'era solo il lato che ESTRAE — un gioco si poteva ricevere, non spedire.
 *
 * In memoria e non su file perche' l'archivio serve solo il tempo di una
 * richiesta HTTP: scriverlo su disco vorrebbe dire inventare un percorso
 * temporaneo e poi ricordarsi di cancellarlo.
 *
 * I percorsi dentro l'archivio sono relativi alla cartella di partenza e usano
 * sempre "/", come vuole il formato zip anche quando lo si crea su Windows.
 */
inline bool createFromFolder(const std::string& folder, std::vector<std::uint8_t>& out,
                             std::string& error) {
    namespace fs = std::filesystem;
    out.clear();
    error.clear();

    std::error_code ec;
    if (!fs::is_directory(folder, ec)) {
        error = "non e' una cartella: " + folder;
        return false;
    }

    zip_t* zip = zip_stream_open(nullptr, 0, ZIP_DEFAULT_COMPRESSION_LEVEL, 'w');
    if (!zip) {
        error = "impossibile creare l'archivio in memoria";
        return false;
    }

    int written = 0;
    const fs::path base(folder);
    for (const auto& entry : fs::recursive_directory_iterator(folder, ec)) {
        if (ec) break;
        if (!entry.is_regular_file(ec)) continue;

        std::string rel = fs::relative(entry.path(), base, ec).generic_string();
        if (ec || rel.empty() || rel.rfind("..", 0) == 0) continue;

        std::ifstream f(entry.path(), std::ios::binary);
        if (!f) continue;
        const std::string bytes((std::istreambuf_iterator<char>(f)),
                                 std::istreambuf_iterator<char>());

        if (zip_entry_open(zip, rel.c_str()) != 0) continue;
        const bool ok = zip_entry_write(zip, bytes.data(), bytes.size()) == 0;
        zip_entry_close(zip);
        if (ok) ++written;
    }

    if (written == 0) {
        zip_stream_close(zip);
        error = "nessun file da inviare in " + folder;
        return false;
    }

    void* buf = nullptr;
    size_t size = 0;
    const ssize_t n = zip_stream_copy(zip, &buf, &size);
    zip_stream_close(zip);
    if (n < 0 || !buf || size == 0) {
        if (buf) std::free(buf);
        error = "lettura dell'archivio fallita";
        return false;
    }

    out.assign(static_cast<std::uint8_t*>(buf), static_cast<std::uint8_t*>(buf) + size);
    std::free(buf);
    return true;
}

}  // namespace ZipArchive
