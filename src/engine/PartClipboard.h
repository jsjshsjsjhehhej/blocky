#pragma once

// Copia/incolla di part con i loro Model e vincoli (Ctrl+C / Ctrl+V di Studio).
// Solo dati: l'interfaccia e' in main_ui/StudioSelectionUi.h, i test in
// tools/test_groups.cpp.

#include "PartGroups.h"

#include <algorithm>
#include <map>
#include <set>
#include <vector>

namespace partclipboard {

struct Clipboard {
    std::vector<WorkspacePart> parts;
    std::vector<PartGroup> groups;        // solo i Model copiati per intero
    std::vector<WeldConstraint> welds;    // part0/part1 = indici in `parts`
    bool empty() const { return parts.empty(); }
};

inline Clipboard copy(const DataModel& dm, std::vector<int> sel) {
    Clipboard cb;
    std::sort(sel.begin(), sel.end());
    sel.erase(std::unique(sel.begin(), sel.end()), sel.end());
    std::map<int, int> toClip;
    for (int i : sel) {
        if (i < 0 || i >= (int)dm.parts.size()) continue;
        toClip[i] = (int)cb.parts.size();
        cb.parts.push_back(dm.parts[(size_t)i]);
        cb.parts.back().netId = 0;
    }
    // Model copiati per intero (anche annidati): tutte le loro part sono qui.
    std::set<int> whole;
    for (const auto& kv : toClip)
        for (int g = dm.parts[(size_t)kv.first].groupId; g > 0; g = partgroups::parentOf(dm, g)) {
            bool all = true;
            for (int m : partgroups::memberParts(dm, g)) all = all && toClip.count(m);
            if (all) whole.insert(g);
        }
    for (int g : whole) cb.groups.push_back(*partgroups::find(dm, g));
    for (const WeldConstraint& w : dm.welds) {
        if (!toClip.count(w.part0) || !toClip.count(w.part1)) continue;
        WeldConstraint c = w;
        c.part0 = toClip[w.part0];
        c.part1 = toClip[w.part1];
        cb.welds.push_back(c);
    }
    return cb;
}

// Incolla spostando di `offset`. I Model copiati diventano Model nuovi; una
// part presa da un Model torna in quel Model se esiste ancora. Ritorna
// l'indice della prima part incollata (le altre seguono, nello stesso ordine).
inline int paste(DataModel& dm, const Clipboard& cb, const glm::vec3& offset) {
    std::map<int, int> newGroup;
    std::vector<PartGroup> pending = cb.groups;
    auto inClip = [&](int id) {
        return std::any_of(cb.groups.begin(), cb.groups.end(), [&](const PartGroup& x) { return x.id == id; });
    };
    // Prima i genitori, cosi' i figli trovano il padre nuovo.
    for (size_t guard = 0; !pending.empty() && guard < 256; ++guard) {
        for (size_t k = 0; k < pending.size();) {
            const PartGroup g = pending[k];
            if (inClip(g.parent) && !newGroup.count(g.parent)) { ++k; continue; }
            const int parent = inClip(g.parent) ? newGroup[g.parent]
                             : (partgroups::find(dm, g.parent) ? g.parent : 0);
            newGroup[g.id] = partgroups::addGroup(dm, g.name, parent);
            if (PartGroup* ng = partgroups::find(dm, newGroup[g.id])) {
                ng->folder = g.folder;
                ng->dragDetector = g.dragDetector;
                ng->pivotOffset = g.pivotOffset;
            }
            pending.erase(pending.begin() + (long)k);
        }
    }
    const int base = (int)dm.parts.size();
    for (WorkspacePart p : cb.parts) {
        p.position += offset;
        if (newGroup.count(p.groupId)) p.groupId = newGroup[p.groupId];
        else if (!partgroups::find(dm, p.groupId)) p.groupId = 0;
        dm.parts.push_back(p);
    }
    for (WeldConstraint w : cb.welds) {
        w.part0 += base;
        w.part1 += base;
        dm.welds.push_back(w);
    }
    return base;
}

// Di quanto alzare la copia per appoggiarla sopra l'originale.
inline glm::vec3 stackOffset(const Clipboard& cb) {
    float top = -1e9f, bottom = 1e9f;
    for (const auto& p : cb.parts) {
        top = std::max(top, p.position.y + p.size.y * 0.5f);
        bottom = std::min(bottom, p.position.y - p.size.y * 0.5f);
    }
    return cb.parts.empty() ? glm::vec3(0.f) : glm::vec3(0.f, (top - bottom) + 0.05f, 0.f);
}

} // namespace partclipboard
