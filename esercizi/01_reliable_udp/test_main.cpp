// Banco di prova per ESERCIZIO 01. Non serve capirlo per fare l'esercizio,
// ma leggerlo e' istruttivo: mostra COME si prova un livello di rete senza
// una rete vera (tempo simulato, linea finta, numeri casuali ripetibili).
//
// Compila ed esegui (dalla cartella dell'esercizio):
//   g++ -std=c++17 -O1 -Wall -Wextra test_main.cpp reliable.cpp -o test.exe && test.exe
#include "reliable.h"

#include <cstdio>
#include <cstring>
#include <deque>
#include <string>

using namespace blk;

// ---------------------------------------------------------------- utilita'
static int g_checks = 0, g_fails = 0;
#define CHECK(cond, ...)                                              \
    do {                                                              \
        ++g_checks;                                                   \
        if (!(cond)) {                                                \
            ++g_fails;                                                \
            std::printf("    [X] riga %d: %s  ", __LINE__, #cond);    \
            std::printf(__VA_ARGS__);                                 \
            std::printf("\n");                                        \
        }                                                             \
    } while (0)

// Numeri casuali RIPETIBILI: stesso seme, stessa sequenza. Se un test fallisce
// lo rivedi fallire identico, invece di "a volte sì a volte no".
struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed) : s(seed ? seed : 1) {}
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    int pct() { return int(next() % 100); }          // 0..99
    int range(int lo, int hi) { return lo + int(next() % uint32_t(hi - lo + 1)); }
};

// Una direzione della linea: i pacchetti entrano, ESCONO dopo un ritardo.
struct Wire {
    struct InFlight { double due; std::vector<uint8_t> bytes; };
    std::deque<InFlight> q;
    int lossPct, dupPct, jitterTicks;
    double tick;
    Rng rng;
    size_t bytesSent = 0, packetsSent = 0, oversize = 0;

    Wire(int loss, int dup, int jitter, double tickSec, uint32_t seed)
        : lossPct(loss), dupPct(dup), jitterTicks(jitter), tick(tickSec), rng(seed) {}

    void send(const std::vector<uint8_t>& p, double now) {
        ++packetsSent; bytesSent += p.size();
        if (p.size() > kMaxPacket) ++oversize;
        if (rng.pct() < lossPct) return;                          // perso
        int copies = (rng.pct() < dupPct) ? 2 : 1;                // duplicato
        for (int c = 0; c < copies; ++c) {
            double delay = tick * (3 + rng.range(0, jitterTicks)); // latenza + jitter => RIORDINO
            q.push_back({now + delay, p});
        }
    }
    template <class F> void deliver(double now, F&& f) {
        // consegna tutto cio' che e' scaduto, NON in ordine di arrivo
        for (size_t i = 0; i < q.size();) {
            if (q[i].due <= now) { f(q[i].bytes); q.erase(q.begin() + long(i)); }
            else ++i;
        }
    }
};

static std::vector<uint8_t> makeMsg(uint32_t id, size_t len) {
    std::vector<uint8_t> m(len < 4 ? 4 : len, uint8_t(id));
    std::memcpy(m.data(), &id, 4);
    return m;
}
static uint32_t msgId(const std::vector<uint8_t>& m) {
    uint32_t id = 0; if (m.size() >= 4) std::memcpy(&id, m.data(), 4); return id;
}

// ---------------------------------------------------------------- scenario
struct Params {
    const char* name;
    int loss, dup, jitter;
    uint16_t startSeq;
    int nAtoB, nBtoA;       // quanti reliable per direzione
    bool unreliable;        // spedisce anche unreliable A->B
    double maxSeconds;
};

struct Result {
    bool allDelivered = false;
    int dupOrReorder = 0;       // reliable fuori ordine o doppi
    int unrelStale = 0;         // unreliable piu' vecchi di uno gia' consegnato
    int unrelDelivered = 0;
    size_t pendingA = 0, pendingB = 0;
    size_t bytesA = 0, payloadA = 0, oversize = 0;
    double doneAt = 0;
};

static Result run(const Params& P) {
    const double dt = 1.0 / 60.0;
    ReliableChannel A(P.startSeq), B(uint16_t(P.startSeq + 1000));
    Wire ab(P.loss, P.dup, P.jitter, dt, 12345), ba(P.loss, P.dup, P.jitter, dt, 54321);
    Result r;
    Rng sizes(777);

    int sentAB = 0, sentBA = 0;          // quanti ne ho gia' messi in coda
    uint32_t gotAB = 0, gotBA = 0;       // prossimo id atteso
    uint32_t lastUnrel = 0; bool haveUnrel = false; uint32_t unrelCounter = 0;

    double now = 0;
    for (int tick = 0; now < P.maxSeconds; ++tick, now = tick * dt) {
        // l'applicazione accoda qualche messaggio a ogni tick (burst irregolari)
        for (int k = 0; k < 3 && sentAB < P.nAtoB; ++k) {
            size_t len = size_t(sizes.range(4, 120));
            A.sendReliable(makeMsg(uint32_t(sentAB++), len));
            r.payloadA += len;
        }
        for (int k = 0; k < 2 && sentBA < P.nBtoA; ++k) B.sendReliable(makeMsg(uint32_t(sentBA++), size_t(sizes.range(4, 80))));
        if (P.unreliable && tick % 2 == 0) A.sendUnreliable(makeMsg(++unrelCounter, 16));

        ab.send(A.buildPacket(now), now);
        ba.send(B.buildPacket(now), now);

        ab.deliver(now, [&](const std::vector<uint8_t>& p) { B.onPacket(p.data(), p.size(), now); });
        ba.deliver(now, [&](const std::vector<uint8_t>& p) { A.onPacket(p.data(), p.size(), now); });

        std::vector<uint8_t> m;
        while (B.popReliable(m)) { if (msgId(m) == gotAB) ++gotAB; else ++r.dupOrReorder; }
        while (A.popReliable(m)) { if (msgId(m) == gotBA) ++gotBA; else ++r.dupOrReorder; }
        while (B.popUnreliable(m)) {
            uint32_t id = msgId(m);
            if (haveUnrel && id <= lastUnrel) ++r.unrelStale;     // vecchio o doppio
            lastUnrel = id; haveUnrel = true; ++r.unrelDelivered;
        }

        if (gotAB == uint32_t(P.nAtoB) && gotBA == uint32_t(P.nBtoA) &&
            A.pendingReliable() == 0 && B.pendingReliable() == 0 && r.doneAt == 0) {
            r.doneAt = now; r.allDelivered = true;
            break;
        }
    }
    r.allDelivered = (gotAB == uint32_t(P.nAtoB) && gotBA == uint32_t(P.nBtoA));
    r.pendingA = A.pendingReliable(); r.pendingB = B.pendingReliable();
    r.bytesA = ab.bytesSent; r.oversize = ab.oversize + ba.oversize;
    return r;
}

static void scenario(const Params& P, bool checkBandwidth = false) {
    std::printf("\n- %s  (perdita %d%%, duplicati %d%%, jitter 0..%d tick, seq iniziale %u)\n",
                P.name, P.loss, P.dup, P.jitter, unsigned(P.startSeq));
    Result r = run(P);
    CHECK(r.allDelivered, "non tutti i reliable sono arrivati entro %.0f s simulati", P.maxSeconds);
    CHECK(r.dupOrReorder == 0, "%d messaggi reliable doppi o fuori ordine", r.dupOrReorder);
    CHECK(r.pendingA == 0 && r.pendingB == 0, "pendingReliable non torna a 0 (A=%zu B=%zu): le conferme non arrivano?", r.pendingA, r.pendingB);
    CHECK(r.oversize == 0, "%zu pacchetti oltre kMaxPacket (%zu)", r.oversize, kMaxPacket);
    if (P.unreliable) {
        CHECK(r.unrelStale == 0, "%d unreliable vecchi/doppi consegnati", r.unrelStale);
        CHECK(r.unrelDelivered > 0, "nessun unreliable consegnato");
    }
    if (checkBandwidth && r.payloadA > 0) {
        double ratio = double(r.bytesA) / double(r.payloadA);
        CHECK(ratio < 6.0, "banda sprecata: %.1fx il payload, il limite e' 6x (rispedisci troppo spesso?)", ratio);
        std::printf("    banda A->B: %.1fx il payload utile\n", ratio);
    }
    if (r.allDelivered) std::printf("    completato a %.2f s simulati\n", r.doneAt);
}

int main() {
    std::printf("=== ESERCIZIO 01: affidabilita' sopra UDP ===\n");

    std::printf("\n- seqGreater (giro a 16 bit)\n");
    CHECK(seqGreater(2, 1),        "2 e' dopo 1");
    CHECK(!seqGreater(1, 2),       "1 non e' dopo 2");
    CHECK(!seqGreater(5, 5),       "uguali: nessuno e' avanti");
    CHECK(seqGreater(1, 65535),    "1 viene DOPO 65535 (giro)");
    CHECK(!seqGreater(65535, 1),   "65535 viene PRIMA di 1 (giro)");
    CHECK(seqGreater(0, 65535),    "0 viene dopo 65535");
    CHECK(seqGreater(100, 65500),  "100 dopo 65500 (giro, distanza 136)");
    CHECK(!seqGreater(40000, 5),   "distanza > 32768: 40000 e' DIETRO 5");

    scenario({"rete perfetta",                 0,  0, 0, 0,     100, 0,   false, 10});
    scenario({"perdita 30%",                  30,  0, 0, 0,     500, 0,   false, 30}, true);
    scenario({"perdita + duplicati + riordino", 30, 15, 6, 0,   500, 0,   true,  30});
    scenario({"traffico nei due versi",       25, 10, 4, 0,     400, 300, false, 30});
    scenario({"giro dei numeri (seq 65500)",  20, 10, 4, 65500, 800, 0,   true,  40});
    scenario({"linea pessima: perdita 60%",   60,  5, 5, 3,     200, 0,   false, 60});

    std::printf("\n=== %d controlli, %d falliti ===\n", g_checks, g_fails);
    if (g_fails == 0) std::printf("TUTTO OK. Ora: leggi la sezione 'Varianti' del README.\n");
    return g_fails ? 1 : 0;
}
