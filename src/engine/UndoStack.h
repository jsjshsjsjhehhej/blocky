#pragma once

#include "DataModel.h"

#include <string>
#include <vector>

/** Undo/Redo Studio — Part insert/delete/transform. */
enum class UndoKind : int {
    InsertPart = 0,
    DeletePart,
    TransformPart,
};

struct UndoEntry {
    UndoKind kind = UndoKind::TransformPart;
    int index = -1;
    WorkspacePart before{};
    WorkspacePart after{};
};

struct UndoStack {
    std::vector<UndoEntry> undo;
    std::vector<UndoEntry> redo;
    static constexpr int kMax = 64;

    void clear() {
        undo.clear();
        redo.clear();
    }

    void push(UndoEntry e) {
        undo.push_back(std::move(e));
        if ((int)undo.size() > kMax)
            undo.erase(undo.begin());
        redo.clear();
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
        // Ignora no-op
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

    bool canUndo() const { return !undo.empty(); }
    bool canRedo() const { return !redo.empty(); }

    bool applyUndo(DataModel& world, int& selectedPart) {
        if (undo.empty()) return false;
        UndoEntry e = undo.back();
        undo.pop_back();
        switch (e.kind) {
        case UndoKind::InsertPart:
            if (e.index >= 0 && e.index < (int)world.parts.size()) {
                world.parts.erase(world.parts.begin() + e.index);
                if (selectedPart == e.index) selectedPart = -1;
                else if (selectedPart > e.index) selectedPart--;
            }
            break;
        case UndoKind::DeletePart:
            if (e.index < 0) e.index = (int)world.parts.size();
            if (e.index > (int)world.parts.size()) e.index = (int)world.parts.size();
            world.parts.insert(world.parts.begin() + e.index, e.before);
            selectedPart = e.index;
            break;
        case UndoKind::TransformPart:
            if (e.index >= 0 && e.index < (int)world.parts.size())
                world.parts[e.index] = e.before;
            break;
        }
        redo.push_back(std::move(e));
        return true;
    }

    bool applyRedo(DataModel& world, int& selectedPart) {
        if (redo.empty()) return false;
        UndoEntry e = redo.back();
        redo.pop_back();
        switch (e.kind) {
        case UndoKind::InsertPart:
            if (e.index < 0) e.index = (int)world.parts.size();
            if (e.index > (int)world.parts.size()) e.index = (int)world.parts.size();
            world.parts.insert(world.parts.begin() + e.index, e.after);
            selectedPart = e.index;
            break;
        case UndoKind::DeletePart:
            if (e.index >= 0 && e.index < (int)world.parts.size()) {
                world.parts.erase(world.parts.begin() + e.index);
                if (selectedPart == e.index) selectedPart = -1;
                else if (selectedPart > e.index) selectedPart--;
            }
            break;
        case UndoKind::TransformPart:
            if (e.index >= 0 && e.index < (int)world.parts.size())
                world.parts[e.index] = e.after;
            break;
        }
        undo.push_back(std::move(e));
        return true;
    }
};
