#pragma once

#include <functional>

#include "DataModel.h"

#include <cstdint>
#include <string>
#include <vector>

enum class UndoKind : int {
    InsertPart = 0,
    DeletePart,
    TransformPart,
    GroupChange,     // Ctrl+G / Ctrl+U / rinomina Model
    Custom,          // tutto il resto (GUI, Tool, servizi, Lighting...): sa ripristinarsi da solo
};

struct UndoEntry {
    UndoKind kind = UndoKind::TransformPart;
    int index = -1;
    WorkspacePart before{};
    WorkspacePart after{};
    // Voci con lo stesso batch (!= 0) si annullano/ripristinano insieme:
    // spostare un Model di 10 part e' UN Ctrl+Z, non dieci.
    uint32_t batch = 0;
    // GroupChange: Model e appartenenza di ogni part, prima e dopo.
    std::vector<PartGroup> groupsBefore, groupsAfter;
    std::vector<int> partGroupsBefore, partGroupsAfter;
    // Custom: chiamata con true per annullare, false per ripetere.
    std::function<void(bool undoing)> custom;
};

struct UndoStack {
    std::vector<UndoEntry> undo;
    std::vector<UndoEntry> redo;
    static constexpr int kMax = 256;
    uint32_t nextBatch = 1;
    uint32_t openBatch = 0;   // se != 0, le voci registrate finiscono in questo batch

    void clear() {
        undo.clear();
        redo.clear();
        openBatch = 0;
    }

    // Tutto quello che si registra fino a endBatch() e' un'unica operazione.
    uint32_t beginBatch() { openBatch = nextBatch++; return openBatch; }
    void endBatch() { openBatch = 0; }

    // Aggiunge al batch anche la voce gia' registrata in cima (es. il gizmo
    // ha appena salvato lo spostamento della part principale).
    void joinLastToBatch(uint32_t batch) {
        if (!undo.empty() && batch) undo.back().batch = batch;
    }

    void push(UndoEntry e) {
        if (!e.batch) e.batch = openBatch;
        undo.push_back(std::move(e));
        if ((int)undo.size() > kMax)
            undo.erase(undo.begin());
        redo.clear();
    }

    /** Una modifica qualsiasi che sa annullarsi e ripetersi da sola. */
    void recordCustom(std::function<void(bool undoing)> fn) {
        UndoEntry e;
        e.kind = UndoKind::Custom;
        e.custom = std::move(fn);
        push(std::move(e));
    }

    void recordInsert(int index, const WorkspacePart& part) {
        UndoEntry e;
        e.kind = UndoKind::InsertPart;
        e.index = index;
        e.after = part;
        push(std::move(e));
    }

    void recordDelete(int index, const WorkspacePart& part) {
        UndoEntry e;
        e.kind = UndoKind::DeletePart;
        e.index = index;
        e.before = part;
        push(std::move(e));
    }

    void recordTransform(int index, const WorkspacePart& before, const WorkspacePart& after) {
        if (index < 0) return;
        if (before.position == after.position && before.size == after.size
            && before.rotation == after.rotation)
            return;
        UndoEntry e;
        e.kind = UndoKind::TransformPart;
        e.index = index;
        e.before = before;
        e.after = after;
        push(std::move(e));
    }

    // Qualunque modifica a una part (colore, materiale, proprieta'...):
    // recordTransform scarta tutto cio' che non e' posizione/misura/rotazione.
    void recordEdit(int index, const WorkspacePart& before, const WorkspacePart& after) {
        if (index < 0) return;
        UndoEntry e;
        e.kind = UndoKind::TransformPart;   // si ripristina la part intera
        e.index = index;
        e.before = before;
        e.after = after;
        push(std::move(e));
    }

    static std::vector<int> partGroups(const DataModel& world) {
        std::vector<int> g;
        g.reserve(world.parts.size());
        for (const auto& p : world.parts) g.push_back(p.groupId);
        return g;
    }

    // Chiamare con lo stato di PRIMA (salvato dal chiamante) e il mondo gia'
    // cambiato.
    void recordGroupChange(const std::vector<PartGroup>& groupsBefore,
                           const std::vector<int>& partGroupsBefore, const DataModel& world) {
        UndoEntry e;
        e.kind = UndoKind::GroupChange;
        e.groupsBefore = groupsBefore;
        e.partGroupsBefore = partGroupsBefore;
        e.groupsAfter = world.groups;
        e.partGroupsAfter = partGroups(world);
        push(std::move(e));
    }

    bool canUndo() const { return !undo.empty(); }
    bool canRedo() const { return !redo.empty(); }

    bool applyUndo(DataModel& world, int& selectedPart) {
        if (undo.empty()) return false;
        const uint32_t batch = undo.back().batch;
        do {
            UndoEntry e = undo.back();
            undo.pop_back();
            applyOne(e, world, selectedPart, true);
            redo.push_back(std::move(e));
        } while (batch && !undo.empty() && undo.back().batch == batch);
        return true;
    }

    bool applyRedo(DataModel& world, int& selectedPart) {
        if (redo.empty()) return false;
        const uint32_t batch = redo.back().batch;
        do {
            UndoEntry e = redo.back();
            redo.pop_back();
            applyOne(e, world, selectedPart, false);
            undo.push_back(std::move(e));
        } while (batch && !redo.empty() && redo.back().batch == batch);
        return true;
    }

private:
    static void applyGroups(DataModel& world, const std::vector<PartGroup>& groups,
                            const std::vector<int>& partGroups) {
        world.groups = groups;
        for (const auto& g : groups)
            if (g.id >= world.nextGroupId) world.nextGroupId = g.id + 1;
        for (size_t i = 0; i < world.parts.size() && i < partGroups.size(); ++i)
            world.parts[i].groupId = partGroups[i];
    }

    static void applyOne(UndoEntry& e, DataModel& world, int& selectedPart, bool undoing) {
        const UndoKind k = e.kind;
        if (k == UndoKind::Custom) {
            if (e.custom) e.custom(undoing);
            return;
        }
        if (k == UndoKind::GroupChange) {
            if (undoing) applyGroups(world, e.groupsBefore, e.partGroupsBefore);
            else applyGroups(world, e.groupsAfter, e.partGroupsAfter);
            return;
        }
        if (k == UndoKind::TransformPart) {
            if (e.index >= 0 && e.index < (int)world.parts.size())
                world.parts[e.index] = undoing ? e.before : e.after;
            return;
        }
        // Inserire e cancellare sono l'uno l'inverso dell'altro.
        const bool removeNow = (k == UndoKind::InsertPart) == undoing;
        if (removeNow) {
            if (e.index >= 0 && e.index < (int)world.parts.size()) {
                world.parts.erase(world.parts.begin() + e.index);
                if (selectedPart == e.index) selectedPart = -1;
                else if (selectedPart > e.index) selectedPart--;
            }
        } else {
            if (e.index < 0 || e.index > (int)world.parts.size()) e.index = (int)world.parts.size();
            world.parts.insert(world.parts.begin() + e.index,
                               k == UndoKind::InsertPart ? e.after : e.before);
            selectedPart = e.index;
        }
    }
};
