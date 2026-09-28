
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

#include "engine/AssetAudit.h"

namespace {

void printReport(const AssetAudit::Report& r, const std::string& root) {
    std::printf("\n  Audit asset — %s\n", root.c_str());
    std::printf("  %zu file, %.2f MB totali\n\n",
                r.assets.size(), (double)r.totalBytes / (1024.0 * 1024.0));

    if (r.findings.empty()) {
        std::printf("  Nessun rilievo.\n\n");
        return;
    }

    for (int pass = 0; pass < 2; ++pass) {
        const auto want = pass == 0 ? AssetAudit::Severity::Error : AssetAudit::Severity::Warn;
        for (const auto& f : r.findings) {
            if (f.severity != want) continue;
            std::printf("  [%s] %-34s %s\n           %s\n",
                        AssetAudit::severityName(f.severity),
                        f.path.c_str(), f.rule.c_str(), f.detail.c_str());
        }
    }

    std::printf("\n  %d errori, %d avvisi\n\n", r.errors, r.warnings);
}

}

int main(int argc, char** argv) {
    std::string root = "assets";
    std::string manifest;
    bool strict = false;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--manifest") == 0 && i + 1 < argc) {
            manifest = argv[++i];
        } else if (std::strcmp(argv[i], "--strict") == 0) {
            strict = true;
        } else if (argv[i][0] != '-') {
            root = argv[i];
        }
    }

    const AssetAudit::Report report = AssetAudit::run(root);
    printReport(report, root);

    if (!manifest.empty()) {
        std::ofstream out(manifest, std::ios::binary);
        if (!out) {
            std::fprintf(stderr, "  Impossibile scrivere %s\n", manifest.c_str());
            return 1;
        }
        out << AssetAudit::toJson(report);
        std::printf("  Inventario scritto in %s\n\n", manifest.c_str());
    }

    if (report.errors > 0) {
        std::printf("  NON pronto per la release: correggi gli errori.\n\n");
        return 1;
    }
    if (strict && report.warnings > 0) {
        std::printf("  --strict: gli avvisi bloccano la release.\n\n");
        return 1;
    }

    std::printf("  Pronto per la release.\n\n");
    return 0;
}
