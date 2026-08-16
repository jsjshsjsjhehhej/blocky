// CLI — esporta place di default in JSON
#include "engine/DataModel.h"
#include "engine/Humanoid.h"
#include "engine/PlaceExporter.h"
#include "engine/GameHub.h"

#include <cstdio>

int main() {
    DataModel dm;
    dm.loadBaseplatePlace();
    Humanoid h = Humanoid::createR6Bacon();
    std::string json = PlaceExporter::exportPlace(dm, h, {0, 0, 0}, -90.f);
    std::string path = PlaceExporter::makeExportPath("Baseplate");
    if (!PlaceExporter::saveToFile(path, json)) {
        std::fprintf(stderr, "Export fallito\n");
        return 1;
    }
    return 0;
}
