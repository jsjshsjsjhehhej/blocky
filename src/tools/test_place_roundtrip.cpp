// Salva e ricarica un place "cattivo": script e nomi con { } [ ] " \ a capo
// e le parole delle sezioni del file ("labels", "constraints", ...).
// Prima una tabella Luau in uno script creava part fantasma al caricamento, e
// una stringa "labels" faceva leggere le etichette dal punto sbagliato.

#include "engine/GameHub.h"
#include "engine/PartConstraints.h"
#include "engine/PlaceExporter.h"

#include <cstdio>
#include <fstream>
#include <string>

static int g_fail = 0;
static void check(bool ok, const std::string& what) {
    std::printf("[place] %s  %s\n", ok ? "OK  " : "FAIL", what.c_str());
    if (!ok) ++g_fail;
}

int main() {
    const std::string nasty =
        "local t = { a = 1, b = { c = \"}\" } }\n"
        "local arr = { [1] = \"]\", [2] = \"[\" }\n"
        "print(\"\\\"labels\\\": [ { } ]\", \"constraints\", \"parts\")\n"
        "local s = [[ testo lungo con } e { e \"virgolette\" ]]\n"
        "-- \"name\":\"Finta\",\"pos\":[1,2,3] }, {\"name\":\"Fantasma\"\n"
        "print('backslash \\\\ fine')\n";

    DataModel dm;
    dm.loadBaseplatePlace();
    for (int i = 0; i < 3; ++i) {
        WorkspacePart p;
        p.name = i == 1 ? "Nome \"strano\" {con} [parentesi]" : "Parte" + std::to_string(i);
        p.position = {(float)i * 4.f, 2.f, 0.f};
        p.script = nasty + "-- parte " + std::to_string(i) + "\n";
        p.scriptType = i == 2 ? ScriptType::Server : ScriptType::Client;
        p.anchored = i != 2;
        dm.parts.push_back(p);
    }
    const int parts = (int)dm.parts.size();
    WorkspaceLabel lab;
    lab.name = "Etichetta";
    lab.text = "Testo {con} \"virgolette\"";
    lab.className = "TextLabel";
    dm.labels.push_back(lab);
    SharedInstance sh;
    sh.name = "Cervello";
    sh.className = "Script";
    sh.service = SharedService::ServerScriptService;
    sh.scriptType = ScriptType::Server;
    sh.source = nasty;
    dm.instances.push_back(sh);
    partconstraints::create(dm, ConstraintKind::Rope, 1, 2);

    const std::string path = "test_place_roundtrip.json";
    {
        Humanoid hum;
        std::ofstream f(path, std::ios::binary);
        f << PlaceExporter::exportPlace(dm, hum, {0, 0, 0}, 0.f);
    }
    DataModel back;
    check(loadPlaceFromJson(path, back), "il place si ricarica");
    std::remove(path.c_str());

    check((int)back.parts.size() == parts,
          "niente part fantasma: " + std::to_string(back.parts.size()) + " su " + std::to_string(parts));
    bool scriptsOk = (int)back.parts.size() == parts;
    for (int i = 0; scriptsOk && i < parts; ++i)
        scriptsOk = back.parts[(size_t)i].script == dm.parts[(size_t)i].script;
    check(scriptsOk, "gli script tornano identici, graffe e virgolette comprese");
    const size_t strange = dm.parts.size() - 2;   // la seconda delle 3 part aggiunte
    check(back.parts.size() == dm.parts.size() && back.parts[strange].name == dm.parts[strange].name,
          "nome con virgolette e parentesi intatto");
    check(!back.parts.empty() && back.parts.back().scriptType == ScriptType::Server && !back.parts.back().anchored,
          "le proprieta' dopo lo script restano della part giusta");
    check(back.labels.size() == dm.labels.size() && back.labels.back().text == lab.text,
          "etichette: " + std::to_string(back.labels.size()) + " su " + std::to_string(dm.labels.size()) + ", testo intatto");
    check(back.instances.size() == 1 && back.instances[0].source == nasty
              && back.instances[0].scriptType == ScriptType::Server,
          "lo Script in ServerScriptService torna identico");
    check(back.welds.size() == 1 && back.welds[0].kind == ConstraintKind::Rope, "il vincolo c'e', non letto dal testo degli script");

    if (g_fail) {
        std::printf("[place] %d controlli falliti\n", g_fail);
        return 1;
    }
    std::printf("ALL PLACE ROUNDTRIP TESTS PASSED\n");
    return 0;
}
