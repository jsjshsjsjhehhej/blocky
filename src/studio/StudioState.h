#pragma once

/**
 * Stato dell'editor: mondo, selezione, strumento attivo, log.
 *
 * Tenuto separato dai pannelli perché la viewport, la toolbar e l'Explorer
 * lavorano tutti sulla stessa selezione: se ognuno tenesse la propria copia
 * si disallineerebbero al primo undo.
 */

#include <cstdarg>
#include <cstdio>
#include <deque>
#include <string>

#include "engine/DataModel.h"
#include "engine/UndoStack.h"

namespace studio {

enum class Tool : int {
    Select = 0,
    Move,
    Scale,
    Rotate,
};

inline const char* toolName(Tool t) {
    switch (t) {
    case Tool::Move:   return "Move";
    case Tool::Scale:  return "Scale";
    case Tool::Rotate: return "Rotate";
    default:           return "Select";
    }
}

enum class LogLevel : int { Info = 0, Warn, Error, Script };

struct LogLine {
    LogLevel level = LogLevel::Info;
    std::string text;
};

/** Console dell'editor. Tenuta corta: serve l'ultimo errore, non l'archivio. */
struct OutputLog {
    std::deque<LogLine> lines;
    size_t maxLines = 500;
    bool scrollToBottom = false;

    void add(LogLevel level, const char* fmt, ...) {
        char buf[1024];
        va_list args;
        va_start(args, fmt);
        std::vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);

        lines.push_back({level, buf});
        while (lines.size() > maxLines) lines.pop_front();
        scrollToBottom = true;
    }

    void info(const char* fmt, ...) {
        char buf[1024];
        va_list args;
        va_start(args, fmt);
        std::vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        add(LogLevel::Info, "%s", buf);
    }

    void clear() { lines.clear(); }
};

struct StudioState {
    DataModel world;
    UndoStack undo;
    OutputLog output;

    int selected = -1;              // indice in world.parts, -1 = nessuna
    Tool tool = Tool::Select;

    bool snapEnabled = true;
    float snapStuds = 1.f;
    float snapDegrees = 15.f;

    std::string projectPath;        // vuoto = mai salvato
    bool dirty = false;

    bool showGrid = true;

    WorkspacePart* selectedPart() {
        if (selected < 0 || selected >= (int)world.parts.size()) return nullptr;
        return &world.parts[(size_t)selected];
    }

    const WorkspacePart* selectedPart() const {
        if (selected < 0 || selected >= (int)world.parts.size()) return nullptr;
        return &world.parts[(size_t)selected];
    }

    void selectNone() { selected = -1; }

    float snap(float value, float step) const {
        if (!snapEnabled || step <= 0.f) return value;
        return std::round(value / step) * step;
    }

    glm::vec3 snapVec(const glm::vec3& v, float step) const {
        if (!snapEnabled || step <= 0.f) return v;
        return {snap(v.x, step), snap(v.y, step), snap(v.z, step)};
    }

    /** Nome libero: due Part con lo stesso nome confondono l'Explorer. */
    std::string uniqueName(const std::string& base) const {
        std::string candidate = base;
        int suffix = 1;
        bool clash = true;
        while (clash) {
            clash = false;
            for (const WorkspacePart& p : world.parts) {
                if (p.name == candidate) { clash = true; break; }
            }
            if (clash) candidate = base + std::to_string(++suffix);
        }
        return candidate;
    }

    int addPart(PartShape shape, const glm::vec3& position) {
        WorkspacePart p;
        p.name = uniqueName(partShapeName(shape));
        p.position = position;
        p.size = (shape == PartShape::Ball) ? glm::vec3(4.f) : glm::vec3(4.f, 1.f, 2.f);
        p.shape = shape;
        p.color = {0.64f, 0.64f, 0.64f};

        world.parts.push_back(p);
        const int index = (int)world.parts.size() - 1;
        undo.recordInsert(index, p);
        selected = index;
        dirty = true;
        output.info("Inserita %s", p.name.c_str());
        return index;
    }

    void deleteSelected() {
        WorkspacePart* p = selectedPart();
        if (!p) return;
        const std::string name = p->name;
        undo.recordDelete(selected, *p);
        world.parts.erase(world.parts.begin() + selected);
        selected = -1;
        dirty = true;
        output.info("Eliminata %s", name.c_str());
    }

    void duplicateSelected() {
        const WorkspacePart* src = selectedPart();
        if (!src) return;
        WorkspacePart copy = *src;
        copy.name = uniqueName(src->name);
        copy.position.x += 4.f;

        world.parts.push_back(copy);
        const int index = (int)world.parts.size() - 1;
        undo.recordInsert(index, copy);
        selected = index;
        dirty = true;
        output.info("Duplicata in %s", copy.name.c_str());
    }
};

} // namespace studio
