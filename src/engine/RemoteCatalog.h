#pragma once

/**
 * Il catalogo degli item condiviso fra i dispositivi.
 *
 * Stesso problema dei giochi, stessa soluzione: gli item che costruisci nel
 * Catalog vivono in shop.json dentro i dati locali del computer, quindi
 * aprendo Blocky dal telefono non esistono. Qui il PC li manda al server e
 * ogni dispositivo se li riprende.
 *
 * Le mesh viaggiano insieme al catalogo, non separatamente: un item che
 * rimanda a un file che il telefono non ha sarebbe un item invisibile, cioe'
 * peggio che non averlo.
 *
 * L'indirizzo del server e' lo stesso dei giochi (RemoteGames::loadHost): e'
 * lo stesso computer, non ha senso chiederlo due volte.
 */

#include <atomic>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "engine/GameHub.h"
#include "engine/HttpClient.h"
#include "engine/RemoteGames.h"
#include "engine/ZipArchive.h"
#include "platform/Log.h"

namespace blk {
namespace remotecatalog {

/** Dove finiscono le mesh scaricate dal server. */
inline std::string localMeshDir() { return blockyDataDir() + "/catalog_remote"; }

inline std::mutex& msgMutex() { static std::mutex m; return m; }
inline std::string& msgSlot() { static std::string s; return s; }
inline std::atomic<bool>& busy() { static std::atomic<bool> b{false}; return b; }

inline bool isBusy() { return busy().load(); }

inline void setMessage(const std::string& s) {
    std::lock_guard<std::mutex> lock(msgMutex());
    msgSlot() = s;
}

/** Ultimo esito da mostrare nell'interfaccia; si svuota leggendolo. */
inline std::string takeMessage() {
    std::lock_guard<std::mutex> lock(msgMutex());
    std::string s = std::move(msgSlot());
    msgSlot().clear();
    return s;
}

// ---- Invio del catalogo (dal PC che lo possiede) -----------------------

/**
 * Manda al server il catalogo locale con le sue mesh.
 *
 * Si costruisce prima una cartella temporanea con dentro catalog.json e una
 * copia delle mesh, poi la si impacchetta. Copiare invece di elencare percorsi
 * sparsi per il disco e' l'unico modo per essere sicuri che dall'altra parte
 * arrivi tutto: i sourcePath degli item puntano dove capita sul computer di
 * chi li ha creati, e quei percorsi sull'altro dispositivo non esistono.
 *
 * Nell'archivio i sourcePath vengono riscritti col solo nome del file, cosi'
 * chi scarica sa cosa chiedere a GET /catalog/<file> senza doversi inventare
 * niente.
 */
inline void publish(const std::string& hostPort, const std::string& catalogJson,
                    const std::vector<std::string>& meshPaths) {
    if (hostPort.empty() || catalogJson.empty()) return;
    bool expected = false;
    if (!busy().compare_exchange_strong(expected, true)) {
        setMessage("Invio gia' in corso");
        return;
    }

    std::thread([hostPort, catalogJson, meshPaths]() {
        namespace fs = std::filesystem;
        std::error_code ec;

        const fs::path staging = fs::path(blockyDataDir()) / "catalog_upload";
        fs::remove_all(staging, ec);
        fs::create_directories(staging, ec);

        {
            std::ofstream f(staging / "catalog.json", std::ios::binary);
            if (f) f.write(catalogJson.data(), (std::streamsize)catalogJson.size());
        }

        int copied = 0;
        for (const auto& p : meshPaths) {
            if (p.empty()) continue;
            const fs::path src(p);
            if (!fs::is_regular_file(src, ec)) continue;
            fs::copy_file(src, staging / src.filename(),
                          fs::copy_options::overwrite_existing, ec);
            if (!ec) ++copied;
        }

        std::vector<std::uint8_t> zipBytes;
        std::string error;
        const bool zipped = ZipArchive::createFromFolder(staging.string(), zipBytes, error);
        fs::remove_all(staging, ec);

        if (!zipped) {
            setMessage("Invio catalogo fallito: " + error);
            busy().store(false);
            return;
        }

        std::string host;
        int port = blk::remotegames::kDefaultPort;
        blk::remotegames::splitHostPort(hostPort, host, port);

        std::string reply;
        const bool ok = blk::http::put(host, port, "/catalog", zipBytes, reply);
        setMessage(ok ? ("Catalogo inviato a " + host + " (" + std::to_string(copied) + " mesh)")
                      : ("Il server " + host + " non ha accettato il catalogo"));
        LOG_INFO("catalog", "invio a %s:%d %s", host.c_str(), port, ok ? "ok" : "fallito");
        busy().store(false);
    }).detach();
}

// ---- Ricezione (su ogni dispositivo) -----------------------------------

inline std::mutex& fetchMutex() { static std::mutex m; return m; }
inline std::string& fetchedJson() { static std::string s; return s; }
inline std::atomic<bool>& fetchReady() { static std::atomic<bool> b{false}; return b; }

/**
 * Scarica il catalogo e le mesh che nomina.
 *
 * Le mesh si scaricano subito e non alla prima volta che servono: sono poche e
 * piccole, e un item che compare nell'elenco ma resta invisibile finche' non
 * finisce un download in sottofondo sarebbe solo confusione.
 */
inline void startFetch(const std::string& hostPort) {
    if (hostPort.empty()) return;

    std::thread([hostPort]() {
        std::string host;
        int port = blk::remotegames::kDefaultPort;
        blk::remotegames::splitHostPort(hostPort, host, port);

        std::string json;
        if (!blk::http::get(host, port, "/catalog", json) || json.empty()) return;
        if (json.find("\"name\"") == std::string::npos) return;   // elenco vuoto

        plat::makeDirs(localMeshDir());

        // Ogni "sourcePath" non vuoto e' il nome di un file da chiedere al
        // server. Si scorre il JSON invece di interpretarlo tutto: e' lo stesso
        // approccio usato nel resto del motore per file scritti da noi.
        int meshes = 0;
        size_t pos = 0;
        while ((pos = json.find("\"sourcePath\"", pos)) != std::string::npos) {
            const std::string file = jsonFindStr(json, "sourcePath", pos);
            pos += 12;
            if (file.empty()) continue;

            const std::string local = localMeshDir() + "/" + file;
            if (plat::fileExists(local) && plat::fileSize(local) > 0) { ++meshes; continue; }

            std::string body;
            if (!blk::http::get(host, port, "/catalog/" + file, body, 8000) || body.empty())
                continue;
            std::ofstream f(local, std::ios::binary);
            if (!f) continue;
            f.write(body.data(), (std::streamsize)body.size());
            ++meshes;
        }

        {
            std::lock_guard<std::mutex> lock(fetchMutex());
            fetchedJson() = json;
        }
        fetchReady().store(true);
        LOG_INFO("catalog", "%d mesh dal server %s", meshes, host.c_str());
    }).detach();
}

/** true una sola volta, quando un catalogo scaricato e' pronto da fondere. */
inline bool takeFetched(std::string& json) {
    if (!fetchReady().exchange(false)) return false;
    std::lock_guard<std::mutex> lock(fetchMutex());
    json = std::move(fetchedJson());
    fetchedJson().clear();
    return true;
}

} // namespace remotecatalog
} // namespace blk
