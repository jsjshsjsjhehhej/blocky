// Test del Place Launcher e dei JobId (PlaceInstance.h, PlaceLauncher.h e le
// aggiunte a LanNet/ServerBrowser). Prima la logica pura (formato JobId,
// inviti, scelta del server), poi la rete vera in locale: un server dedicato
// su 127.0.0.1, la ricerca per JobId, l'ingresso, il JobId nel Welcome, e i
// casi "pieno" e "non trovato".

#include "engine/PlaceLauncher.h"

#include <cstdio>
#include <set>

#ifdef _WIN32
#include <windows.h>
#endif

static int g_fail = 0;

static void check(bool ok, const char* what) {
    std::printf("[place] %s %s\n", ok ? "OK  " : "FAIL", what);
    if (!ok) ++g_fail;
}

static void sleepMs(int ms) {
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    netSleepMs(ms);
#endif
}

using blk::placelauncher::Status;
using blk::serverbrowser::Entry;

static Entry entry(const char* job, int players, int maxPlayers) {
    Entry e;
    e.ip = "10.0.0.1";
    e.jobId = job;
    e.players = players;
    e.maxPlayers = maxPlayers;
    e.dedicated = true;
    return e;
}

// Fa girare il launcher finche' ha finito, tenendo vivi i server (devono
// rispondere alla ricerca mentre il thread di scansione aspetta).
static Status runLauncher(blk::placelauncher::Launcher& l, LanNet* a, LanNet* b, float& clock) {
    for (int i = 0; i < 400 && l.busy(); ++i) {
        clock += 0.01f;
        if (a) a->poll(clock);
        if (b) b->poll(clock);
        l.tick();
        sleepMs(10);
    }
    return l.status();
}

static bool pollUntil(LanNet& a, LanNet& b, float& clock, bool (*cond)(LanNet&, LanNet&)) {
    for (int i = 0; i < 200; ++i) {
        clock += 0.02f;
        a.poll(clock);
        b.poll(clock);
        if (cond(a, b)) return true;
        sleepMs(10);
    }
    return false;
}

int main() {
    // ---- JobId ----
    {
        std::set<std::string> seen;
        bool allValid = true;
        for (int i = 0; i < 2000; ++i) {
            const std::string j = blk::jobid::generate();
            allValid &= blk::jobid::isValid(j) && j[14] == '4';
            seen.insert(j);
        }
        check(allValid, "2000 JobId nel formato GUID v4");
        check(seen.size() == 2000, "2000 JobId tutti diversi");

        const std::string j = "3f2a9c1e-5b7d-4e8f-9a0b-1c2d3e4f5a6b";
        check(blk::jobid::normalize("  {3F2A9C1E-5B7D-4E8F-9A0B-1C2D3E4F5A6B}\n") == j,
              "normalize: maiuscole, graffe e spazi");
        check(blk::jobid::normalize("3f2a9c1e5b7d4e8f9a0b1c2d3e4f5a6b") == j, "normalize: senza trattini");
        check(blk::jobid::normalize("3f2a9c1e-5b7d-4e8f-9a0b-1c2d3e4f5a6").empty(), "normalize: troppo corto");
        check(blk::jobid::normalize("zz2a9c1e-5b7d-4e8f-9a0b-1c2d3e4f5a6b").empty(), "normalize: non esadecimale");
    }

    // ---- Inviti ----
    {
        const std::string job = blk::jobid::generate();
        blk::placeinvite::Target t;
        check(blk::placeinvite::parse(blk::placeinvite::make("obby_party", job), t)
              && t.placeId == "obby_party" && t.jobId == job && t.poolCode.empty(),
              "invito blocky://place/<id>?job=<jobId> andata e ritorno");
        check(blk::placeinvite::parse("blocky://place/obby_party", t) && t.placeId == "obby_party"
              && t.jobId.empty(), "invito senza job = qualsiasi server");
        check(blk::placeinvite::parse(job, t) && t.jobId == job && t.placeId.empty(), "JobId da solo");
        check(blk::placeinvite::parse("blk://abc234", t) && t.poolCode == "ABC234", "codice blk:// (minuscolo)");
        check(blk::placeinvite::parse("XYZ789", t) && t.poolCode == "XYZ789", "codice da solo");
        check(!blk::placeinvite::parse("blocky://place/../segreti?job=" + job, t), "invito con '/' rifiutato");
        check(!blk::placeinvite::parse("blocky://place/obby?job=nonvalido", t), "invito con job sbagliato rifiutato");
        check(!blk::placeinvite::parse("ABC10O", t), "codice con 0/O/1 rifiutato");
        check(!blk::placeinvite::parse("ciao a tutti", t), "testo qualsiasi rifiutato");
    }

    // ---- Scelta del server ----
    {
        const std::string a = blk::jobid::generate(), b = blk::jobid::generate(), c = blk::jobid::generate();
        std::vector<Entry> s = {entry(a.c_str(), 2, 8), entry(b.c_str(), 7, 8), entry(c.c_str(), 8, 8)};
        auto d = blk::placelauncher::choose(s, "");
        check(d.status == Status::Join && d.serverIndex == 1, "senza JobId: il piu' pieno con posto (7/8)");
        d = blk::placelauncher::choose(s, a);
        check(d.status == Status::Join && d.serverIndex == 0, "con JobId: proprio quel server");
        d = blk::placelauncher::choose(s, c);
        check(d.status == Status::GameFull && d.serverIndex == 2, "JobId di un server pieno: GameFull");
        d = blk::placelauncher::choose(s, blk::jobid::generate());
        check(d.status == Status::JobNotFound, "JobId che non risponde: JobNotFound");
        std::vector<Entry> fullOnly = {entry(c.c_str(), 8, 8)};
        check(blk::placelauncher::choose(fullOnly, "").status == Status::HostNew, "tutti pieni: si apre un server");
        check(blk::placelauncher::choose({}, "").status == Status::HostNew, "nessun server: si apre un server");

        // Solo server dedicati: un giocatore che ospita non conta.
        Entry phone = entry(a.c_str(), 3, 8);
        phone.dedicated = false;
        std::vector<Entry> mixed = {phone, entry(b.c_str(), 1, 8)};
        d = blk::placelauncher::choose(mixed, "", true);
        check(d.status == Status::Join && d.serverIndex == 1, "solo server: sceglie il dedicato anche se meno pieno");
        check(blk::placelauncher::choose({phone}, "", true).status == Status::NoServer,
              "solo server: un telefono che ospita non basta (NoServer)");
        check(blk::placelauncher::choose({phone}, a, true).status == Status::JobNotFound,
              "solo server: il JobId di un telefono non vale");
        check(blk::placelauncher::choose({}, "", true).status == Status::NoServer,
              "solo server, nessuno in rete: NoServer, non si ospita");
        check(blk::placelauncher::isFailure(Status::NoServer), "NoServer riporta al menu");
    }

    // ---- Coda teleport degli script ----
    {
        const std::string job = blk::jobid::generate();
        check(blk::teleport::request("obby_party", job) && blk::teleport::queue().size() == 1
              && blk::teleport::queue()[0].jobId == job, "Teleport con JobId in coda");
        check(blk::teleport::request("obby_party", "") && blk::teleport::queue().size() == 1,
              "un secondo Teleport sostituisce il primo");
        check(!blk::teleport::request("obby party", "") , "PlaceId con spazio rifiutato");
        check(!blk::teleport::request("obby", "non-un-job"), "JobId non valido rifiutato");
        blk::teleport::queue().clear();
    }

    // ---- Rete vera su 127.0.0.1 ----
    {
        const int port = 27961;
        float clock = 0.f;
        LanNet server, client, extra;
        if (!server.startDedicated(port, -1, 1, "JobTest")) {
            std::printf("[place] FAIL startDedicated sulla porta %d\n", port);
            return 1;
        }
        check(blk::jobid::isValid(server.jobId), "il server dedicato ha un JobId");

        blk::placelauncher::Launcher l;
        blk::placelauncher::Request r;
        r.gameIndex = 0;
        r.placeId = "obby_party";
        r.jobId = server.jobId;
        r.port = port;
        l.start(r);
        check(runLauncher(l, &server, nullptr, clock) == Status::Join && l.chosen()
              && l.chosen()->jobId == server.jobId, "il launcher trova il server per JobId");

        const Entry target = *l.chosen();
        client.startClient(target.ip.c_str(), target.port, 0, "Amico");
        check(pollUntil(server, client, clock, [](LanNet&, LanNet& c) { return c.connected; }),
              "il client entra nel server scelto");
        check(std::string(client.jobId) == server.jobId, "il client riceve il JobId nel Welcome");
        check(client.hostDedicated, "il client sa di essere su un server dedicato");
        check(l.chosen() && l.chosen()->dedicated, "l'annuncio dice che e' un dedicato");

        // Server da 1 posto ora pieno: chi lo cerca per JobId sa che e' pieno.
        l.reset();
        l.start(r);
        check(runLauncher(l, &server, &client, clock) == Status::GameFull, "server pieno: GameFull, non 'non trovato'");

        // Senza JobId e con l'unico server pieno: si apre una partita nuova.
        r.jobId.clear();
        l.reset();
        l.start(r);
        check(runLauncher(l, &server, &client, clock) == Status::HostNew, "unico server pieno: HostNew");

        r.jobId = blk::jobid::generate();
        l.reset();
        l.start(r);
        check(runLauncher(l, &server, &client, clock) == Status::JobNotFound, "JobId di una partita finita: JobNotFound");

        // Un host di gioco normale (non dedicato) ha il suo JobId, diverso.
        extra.startHost(port + 1, 0, "Altro", 8);
        check(blk::jobid::isValid(extra.jobId) && std::string(extra.jobId) != server.jobId,
              "ogni server ha un JobId diverso");

        // Un giocatore che ospita, cercato davvero in rete: con "solo server"
        // non si entra, senza si'.
        blk::placelauncher::Request rp;
        rp.gameIndex = 0;
        rp.placeId = "obby_party";
        rp.port = port + 1;
        rp.serverOnly = true;
        l.reset();
        l.start(rp);
        check(runLauncher(l, &extra, nullptr, clock) == Status::NoServer, "rete vera: host-giocatore ignorato (NoServer)");
        rp.serverOnly = false;
        l.reset();
        l.start(rp);
        check(runLauncher(l, &extra, nullptr, clock) == Status::Join && l.chosen() && !l.chosen()->dedicated,
              "rete vera: senza solo-server l'host-giocatore va bene");
        client.shutdown();
        check(client.jobId[0] == '\0', "uscendo dalla partita il JobId si azzera");
    }

    std::printf(g_fail ? "[place] %d FALLITI\n" : "[place] TUTTO OK\n", g_fail);
    return g_fail ? 1 : 0;
}
