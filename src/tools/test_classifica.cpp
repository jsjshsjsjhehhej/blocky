// Test di Classifica (src/systems/Classifica.h), senza finestra ne' Lua.

#include "systems/Classifica.h"

#include <cstdio>

static int g_fail = 0;

static void check(bool ok, const char* what) {
    std::printf("  [%s] %s\n", ok ? " OK " : "FAIL", what);
    if (!ok) ++g_fail;
}

int main() {
    std::printf("=== Classifica ===\n");
    Classifica me;
    me.reset();
    me.setLocalPlayer("Walter");

    check(me.set("Monete", 10), "Set crea la colonna");
    double out = 0;
    check(me.add("Monete", 5, out) && out == 15 && me.get("Monete") == 15, "Add ritorna e salva il nuovo valore");
    check(me.get("Uccisioni") == 0, "colonna inesistente vale 0");
    check(!me.set("a=b", 1) && !me.set("a;b", 1) && !me.set("", 1)
          && !me.set("NomeTroppoLungoPerLaColonna", 1), "nomi che romperebbero la rete rifiutati");
    {
        Classifica full;
        full.reset();
        for (int i = 0; i < 10; ++i) full.set("C" + std::to_string(i), i);
        check(full.stats().size() == (size_t)Classifica::kMaxStats, "al massimo 6 colonne");
    }

    // Invio: subito al primo cambio, poi non piu' di 2 volte al secondo.
    Classifica a;
    a.reset();
    a.setLocalPlayer("Anna");
    std::string payload;
    a.set("Monete", 3);
    check(a.takeLocalRowToSend(payload) && payload == "Monete=3", "prima riga parte subito");
    a.set("Monete", 4);
    check(!a.takeLocalRowToSend(payload), "secondo cambio subito dopo: aspetta");
    a.update(0.6f);
    check(a.takeLocalRowToSend(payload) && payload == "Monete=4", "dopo mezzo secondo parte");
    a.update(5.1f);
    check(a.takeLocalRowToSend(payload), "rinfresco ogni 5 s anche senza cambi (per chi entra dopo)");

    // Rete: la riga di Anna arriva a Walter.
    me.applyRow("Anna", payload + ";Uccisioni=7");
    check(me.get("Monete", "Anna") == 4 && me.get("Uccisioni", "Anna") == 7, "riga di un altro applicata");
    me.applyRow("Walter", "Monete=999");
    check(me.get("Monete") == 15, "la propria riga dalla rete e' ignorata");
    me.applyRow("Hacker", "Monete=nan;Monete=1e999");
    check(me.get("Monete", "Hacker") == 0, "valori non finiti scartati");

    auto rows = me.sortedRows();
    check(!rows.empty() && rows[0].player == "Walter" && rows[0].local, "ordinata dal piu' alto, locale evidenziato");

    me.setPresentPlayers({"Anna"});
    rows = me.sortedRows();
    bool hackerGone = true;
    for (const auto& r : rows) if (r.player == "Hacker") hackerGone = false;
    check(hackerGone && rows.size() == 2, "chi esce dalla partita sparisce");

    me.reset();
    check(me.stats().empty() && me.sortedRows().empty(), "reset all'entrata in Play svuota tutto");

    std::printf("\n%s (%d falliti)\n", g_fail ? "FALLITO" : "TUTTO OK", g_fail);
    return g_fail ? 1 : 0;
}
