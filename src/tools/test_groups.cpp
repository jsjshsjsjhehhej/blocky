// Test dei Model (engine/PartGroups.h) e dell'Undo a lotti (engine/UndoStack.h),
// senza finestra: raggruppa, annida, separa, duplica, salva e ricarica un
// .blocky vero, muove un Model come un pezzo solo.

#include "engine/GameHub.h"
#include "engine/PartClipboard.h"
#include "engine/PartGroups.h"
#include "engine/PlaceExporter.h"
#include "engine/UndoStack.h"

#include <cmath>
#include <cstdio>

static int g_fail = 0;

static void check(bool ok, const char* what) {
    std::printf("  [%s] %s\n", ok ? " OK " : "FAIL", what);
    if (!ok) ++g_fail;
}

static DataModel world(int parts) {
    DataModel dm;
    for (int i = 0; i < parts; ++i)
        dm.addPart("P" + std::to_string(i), {(float)i * 4.f, 1.f, 0.f}, {2, 2, 2}, {0.5f, 0.5f, 0.5f});
    return dm;
}

static bool near3(const glm::vec3& a, const glm::vec3& b, float eps = 1e-3f) {
    return glm::length(a - b) < eps;
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("=== Model (Ctrl+G) ===\n");

    {
        DataModel dm = world(6);
        const int a = partgroups::group(dm, {0, 1});
        const int b = partgroups::group(dm, {2, 3});
        check(a && b && partgroups::memberParts(dm, a).size() == 2 && partgroups::topGroupOf(dm, 0) == a,
              "Ctrl+G su 2 part: un Model con 2 part");

        const int c = partgroups::group(dm, {0, 1, 2, 3});
        check(partgroups::find(dm, a)->parent == c && partgroups::find(dm, b)->parent == c
              && partgroups::memberParts(dm, c).size() == 4 && partgroups::topGroupOf(dm, 2) == c,
              "Model selezionati per intero finiscono annidati, non smontati");

        check(partgroups::ungroup(dm, c) && partgroups::find(dm, a)->parent == 0
              && partgroups::topGroupOf(dm, 0) == a && !partgroups::find(dm, c),
              "Ctrl+U: i Model figli tornano al primo livello");

        const int d = partgroups::group(dm, {0, 4});
        check(dm.parts[0].groupId == d && dm.parts[1].groupId == a && dm.parts[4].groupId == d,
              "selezione parziale: la part esce dal suo Model ed entra nel nuovo");

        dm.parts.erase(dm.parts.begin() + 2, dm.parts.begin() + 4);   // tutte le part di B
        partgroups::pruneEmpty(dm);
        check(!partgroups::find(dm, b) && partgroups::find(dm, a), "Model rimasti vuoti spariscono");
    }

    {
        DataModel dm = world(5);
        const int a = partgroups::group(dm, {0, 1});
        const int b = partgroups::group(dm, {2, 3});
        // Copia di tutto A e di una sola part di B (come farebbe Ctrl+D).
        std::vector<int> originals = {0, 1, 2}, copies;
        for (int i : originals) {
            dm.parts.push_back(dm.parts[(size_t)i]);
            copies.push_back((int)dm.parts.size() - 1);
        }
        partgroups::remapCopies(dm, originals, copies);
        const int copyA = dm.parts[(size_t)copies[0]].groupId;
        check(copyA != a && copyA > 0 && dm.parts[(size_t)copies[1]].groupId == copyA
              && partgroups::find(dm, copyA)->name == "Model",
              "duplicare un Model intero crea un Model nuovo");
        check(dm.parts[(size_t)copies[2]].groupId == b, "duplicare una part sola la lascia nel suo Model");
    }

    {
        // Salva e ricarica un .blocky vero (PlaceExporter -> loadPlaceFromJson).
        DataModel dm = world(4);
        const int a = partgroups::group(dm, {0, 1});
        partgroups::find(dm, a)->name = "Auto \"rossa\"";   // virgolette: non devono rompere il JSON
        const int outer = partgroups::group(dm, {0, 1, 2});
        Humanoid h;
        const std::string json = PlaceExporter::exportPlace(dm, h, {0, 0, 0}, 0.f);
        const std::string path = "test_groups_place.blocky";
        { std::ofstream f(path, std::ios::binary); f << json; }
        DataModel back;
        const bool ok = loadPlaceFromJson(path, back);
        std::remove(path.c_str());
        const PartGroup* ga = partgroups::find(back, a);
        check(ok && back.groups.size() == 2 && ga && ga->parent == outer
              && back.parts[1].groupId == a && back.parts[2].groupId == outer && back.parts[3].groupId == 0,
              "salva e ricarica: Model, annidamento e appartenenza");
        check(ga && ga->name == "Auto rossa", "nome con virgolette salvato senza rompere il file");
        check(back.nextGroupId > outer, "il prossimo Model non riusa un id gia' preso");
    }

    {
        // File rovinato: padre inesistente, ciclo, part che punta al vuoto.
        DataModel dm = world(3);
        dm.parts[0].groupId = 7;
        dm.parts[1].groupId = 99;
        const std::string json =
            "{\"groups\": [{\"id\":7,\"name\":\"A\",\"parent\":8},{\"id\":8,\"name\":\"B\",\"parent\":7},"
            "{\"id\":9,\"name\":\"C\",\"parent\":42}]}";
        partgroups::readJson(json, dm);
        bool noCycle = true;
        for (const auto& g : dm.groups) noCycle = noCycle && !partgroups::isWithin(dm, g.parent, g.id);
        check(noCycle && partgroups::find(dm, 9)->parent == 0 && dm.parts[1].groupId == 0
              && dm.parts[0].groupId == 7,
              "file rovinato riparato: niente cicli, padri e Model mancanti tolti");
    }

    {
        // Il gizmo muove la principale: le altre la seguono come un pezzo solo.
        DataModel dm = world(2);
        dm.parts[0].position = {10, 1, 0};
        dm.parts[1].position = {12, 1, 0};   // 2 stud a +X della principale
        const partgroups::Pose before{dm.parts[0].position, {0, 0, 0}};
        const partgroups::Pose after{{15, 1, 0}, {0, 90, 0}};
        dm.parts[0].position = after.position;
        dm.parts[0].rotation = after.rotation;
        partgroups::applyFollow(dm.parts, {1}, before, after);
        // Ruotare di 90 gradi attorno a Y porta +X su -Z.
        check(near3(dm.parts[1].position, {15, 1, -2}) && std::fabs(dm.parts[1].rotation.y - 90.f) < 1e-2f,
              "spostamento e rotazione di gruppo: la seconda part orbita attorno alla principale");

        const int g = partgroups::group(dm, {0, 1});
        partgroups::moveTo(dm, g, {0, 5, 0});
        check(near3(partgroups::pivot(dm, g), {0, 5, 0}), "Model:MoveTo porta il pivot dove chiesto");
    }

    {
        // Undo a lotti: un Ctrl+Z annulla tutto il Model.
        DataModel dm = world(3);
        UndoStack u;
        u.beginBatch();
        for (int i = 0; i < 3; ++i) {
            WorkspacePart b = dm.parts[(size_t)i];
            dm.parts[(size_t)i].position.y += 10.f;
            u.recordTransform(i, b, dm.parts[(size_t)i]);
        }
        u.endBatch();
        WorkspacePart b = dm.parts[0];
        dm.parts[0].position.z += 3.f;
        u.recordTransform(0, b, dm.parts[0]);   // fuori dal lotto: e' un'operazione a se'
        int sel = -1;
        u.applyUndo(dm, sel);
        check(dm.parts[0].position.z == 0.f && dm.parts[1].position.y == 11.f, "il primo Ctrl+Z annulla solo l'ultima operazione");
        u.applyUndo(dm, sel);
        check(dm.parts[0].position.y == 1.f && dm.parts[1].position.y == 1.f && dm.parts[2].position.y == 1.f,
              "il secondo Ctrl+Z annulla le 3 part insieme");
        u.applyRedo(dm, sel);
        check(dm.parts[2].position.y == 11.f && dm.parts[0].position.z == 0.f, "Ctrl+Y le rimette tutte e 3");

        const std::vector<PartGroup> gb = dm.groups;
        const std::vector<int> pb = UndoStack::partGroups(dm);
        const int g = partgroups::group(dm, {0, 1});
        u.recordGroupChange(gb, pb, dm);
        u.applyUndo(dm, sel);
        check(dm.groups.empty() && dm.parts[0].groupId == 0, "Ctrl+Z annulla Ctrl+G");
        u.applyRedo(dm, sel);
        check(partgroups::find(dm, g) && dm.parts[1].groupId == g, "Ctrl+Y rifa Ctrl+G");
    }

    {
        // Copia/incolla: un Model annidato con un vincolo interno, piu' una
        // part presa da un altro Model.
        DataModel dm = world(6);
        const int inner = partgroups::group(dm, {0, 1});
        const int outer = partgroups::group(dm, {0, 1, 2});
        const int other = partgroups::group(dm, {3, 4});
        dm.addWeld(0, 1);
        dm.addWeld(2, 5);   // verso una part non copiata: non va negli appunti
        const partclipboard::Clipboard cb = partclipboard::copy(dm, {0, 1, 2, 3});
        check(cb.parts.size() == 4 && cb.groups.size() == 2 && cb.welds.size() == 1,
              "copia: 4 part, i 2 Model interi (non quello preso a meta'), solo il vincolo interno");

        const glm::vec3 off = partclipboard::stackOffset(cb);
        const int base = partclipboard::paste(dm, cb, off);
        const int newOuter = partgroups::topGroupOf(dm, base);
        check(newOuter != outer && newOuter > 0 && partgroups::memberParts(dm, newOuter).size() == 3
              && partgroups::find(dm, dm.parts[(size_t)base].groupId)->parent == newOuter,
              "incolla: Model nuovi con la stessa struttura annidata");
        check(dm.parts[(size_t)base + 3].groupId == other,
              "una part presa da un Model torna in quel Model");
        check(dm.welds.size() == 3 && dm.welds.back().part0 == base && dm.welds.back().part1 == base + 1,
              "il vincolo interno segue le copie");
        check(std::fabs(off.y - 2.05f) < 1e-3f && std::fabs(dm.parts[(size_t)base].position.y - 3.05f) < 1e-3f,
              "la copia si appoggia sopra l'originale");
        (void)inner;
    }

    {
        DataModel dm = world(2);   // due cubi 2x2x2 a x=0 e x=4
        partgroups::scaleParts(dm, {0, 1}, 2.f);
        check(near3(dm.parts[0].position, {-2, 1, 0}) && near3(dm.parts[1].position, {6, 1, 0})
              && near3(dm.parts[1].size, {4, 4, 4}),
              "scala x2 attorno al centro: part e distanze raddoppiano");
    }

    std::printf("\n%s (%d falliti)\n", g_fail ? "FALLITO" : "TUTTO OK", g_fail);
    return g_fail ? 1 : 0;
}
