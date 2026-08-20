#pragma once

/**
 * Estrazione/creazione ZIP, sopra la libreria kuba--/zip già vendorizzata
 * dentro il sorgente di assimp (contrib/zip, pinnato alla v5.4.3 in
 * CMakeLists.txt). Non è una scelta definitiva: è quella che non aggiunge
 * una dipendenza nuova stanotte. Se un domani assimp toglie quel contrib,
 * questo è l'unico file da aggiornare — il resto del motore non sa che
 * libreria c'è sotto.
 */

#include <cstdint>
#include <string>
#include <vector>

#include "zip.h"   // contrib/zip/src/zip.h, aggiunto all'include path dal CMake

namespace ZipArchive {

struct ExtractResult {
    bool ok = false;
    int entries = 0;
    std::string error;
};

/** Estrae `zipPath` dentro `destDir` (già esistente). */
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

/**
 * Estrae direttamente da un buffer in memoria: evita di dover scrivere il
 * .zip su disco solo per poi rileggerlo.
 *
 * zip_stream_extract passa da zip_archive_extract, la stessa routine di
 * zip_extract: ogni nome viene normalizzato scartando i segmenti "." e
 * ".." prima di essere concatenato a destDir (zip_name_normalize in
 * zip.c). Zip-slip è quindi bloccato dentro la libreria, non da un
 * controllo mio che potrei aver scritto male.
 */
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

} // namespace ZipArchive
