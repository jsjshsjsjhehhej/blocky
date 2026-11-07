// Traduttore dell'interfaccia (ui/Loc.h) con i file assets/lang/*.json veri:
// le lingue si trovano da sole, l'UTF-8 arriva intatto, le frasi mancanti
// ripiegano sull'inglese, l'italiano resta quello del codice.

#include "ui/Loc.h"

#include <cstdio>
#include <string>

static int g_fail = 0;
static void check(bool ok, const std::string& what) {
    std::printf("[loc] %s  %s\n", ok ? "OK  " : "FAIL", what.c_str());
    if (!ok) ++g_fail;
}

int main() {
    Loc l;
    l.scan();
    std::string names;
    for (const auto& x : l.available) names += x.code + " ";
    std::printf("[loc] lingue: %s\n", names.c_str());
    for (const char* c : {"it", "en", "es", "fr", "de", "pt", "ru", "zh"})
        check(l.has(c), std::string("lingua presente: ") + c);
    check(!l.has("_ribbon_base") && !l.has(""), "il file base non e' una lingua");
    check(l.available.size() >= 8 && l.available[0].code == "it" && l.available[1].code == "en",
          "italiano e inglese sempre per primi");

    l.code = "es";
    check(l.loadPack("es"), "spagnolo caricato");
    check(std::string(l.tr("rb.paste")) == "Pegar", std::string("es rb.paste = ") + l.tr("rb.paste"));
    check(std::string(l.tr("avatar.copy_id")) == "Copy AnimationId",
          std::string("frase mancante in spagnolo -> inglese: ") + l.tr("avatar.copy_id"));
    check(std::string(l.tr("chiave.inesistente")) == "chiave.inesistente", "chiave sconosciuta -> la chiave stessa");

    l.code = "zh";
    check(l.loadPack("zh"), "cinese caricato");
    check(std::string(l.tr("rb.paste")) == "\xE7\xB2\x98\xE8\xB4\xB4", std::string("zh rb.paste UTF-8 intatto: ") + l.tr("rb.paste"));
    check(std::string(l.name("zh")).find("\xE4\xB8\xAD\xE6\x96\x87") == 0, "nome della lingua nel suo alfabeto");

    l.code = "ru";
    l.loadPack("ru");
    check(std::string(l.tr("rb.tab.home")) == "\xD0\x93\xD0\xBB\xD0\xB0\xD0\xB2\xD0\xBD\xD0\xB0\xD1\x8F", "russo (cirillico) intatto");

    l.code = "it";
    l.loadPack("it");
    check(std::string(l.tr("rb.tab.model")) == "Modello", "italiano dal codice");
    l.code = "en";
    l.loadPack("en");
    check(std::string(l.tr("rb.tab.model")) == "Model", "inglese dal codice");

    check(locNormalizeCode("pt-BR") == "pt" && locNormalizeCode("es_ES.UTF-8") == "es" && locNormalizeCode("English") == "en",
          "codici di sistema normalizzati (pt-BR, es_ES.UTF-8, English)");
    check(locScriptUnsupported("ar") && !locScriptUnsupported("ru"), "arabo escluso (ImGui non lo disegna), russo no");
    check(locNeedsCjkFont("ja") && !locNeedsCjkFont("de"), "giapponese vuole il font CJK");

    if (g_fail) {
        std::printf("[loc] %d controlli falliti\n", g_fail);
        return 1;
    }
    std::printf("ALL LOC TESTS PASSED\n");
    return 0;
}
