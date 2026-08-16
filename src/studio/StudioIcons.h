#pragma once

/**
 * Icone vettoriali disegnate a runtime con ImDrawList.
 *
 * Nessun file PNG, nessun atlante: le icone sono geometria, quindi restano
 * nitide a ogni scala e l'eseguibile non porta dietro asset. Ogni glifo è
 * costruito qui dentro con primitive elementari.
 */

#include <imgui.h>
#include <cmath>

namespace studio {

enum class Icon : int {
    Cursor,
    Move,
    Scale,
    Rotate,
    Cube,
    Sphere,
    Cylinder,
    Wedge,
    Duplicate,
    Trash,
    Undo,
    Redo,
    Play,
    Stop,
    Save,
    NewFile,
    Grid,
    Script,
    Light,
    Folder,
    Camera,
    Anchor,
};

/** `size` è il lato del riquadro logico dell'icona; il disegno lo riempie. */
inline void drawIcon(ImDrawList* dl, ImVec2 c, float size, Icon icon, ImU32 col) {
    const float r = size * 0.5f;
    const float t = (size * 0.085f > 1.4f) ? size * 0.085f : 1.4f;   // spessore tratto

    auto P = [&](float x, float y) { return ImVec2(c.x + x * r, c.y + y * r); };

    switch (icon) {

    case Icon::Cursor: {
        const ImVec2 pts[] = {P(-0.45f, -0.75f), P(-0.45f, 0.62f), P(-0.08f, 0.24f),
                              P(0.16f, 0.78f),   P(0.42f, 0.66f),  P(0.18f, 0.14f),
                              P(0.62f, 0.10f)};
        dl->AddConvexPolyFilled(pts, 7, col);
        break;
    }

    case Icon::Move: {
        dl->AddLine(P(0, -0.78f), P(0, 0.78f), col, t);
        dl->AddLine(P(-0.78f, 0), P(0.78f, 0), col, t);
        const float h = 0.26f;
        dl->AddTriangleFilled(P(0, -0.92f), P(-h, -0.56f), P(h, -0.56f), col);
        dl->AddTriangleFilled(P(0, 0.92f),  P(-h, 0.56f),  P(h, 0.56f),  col);
        dl->AddTriangleFilled(P(-0.92f, 0), P(-0.56f, -h), P(-0.56f, h), col);
        dl->AddTriangleFilled(P(0.92f, 0),  P(0.56f, -h),  P(0.56f, h),  col);
        break;
    }

    case Icon::Scale: {
        dl->AddRect(P(-0.85f, -0.10f), P(-0.10f, 0.85f), col, 1.f, 0, t);
        dl->AddRectFilled(P(0.22f, -0.85f), P(0.85f, -0.22f), col, 1.f);
        dl->AddLine(P(-0.10f, 0.10f), P(0.55f, -0.55f), col, t);
        break;
    }

    case Icon::Rotate: {
        dl->PathArcTo(c, r * 0.68f, -2.9f, 1.5f, 24);
        dl->PathStroke(col, 0, t);
        // Punta della freccia in coda all'arco.
        const float a = 1.5f;
        const ImVec2 tip(c.x + std::cos(a) * r * 0.68f, c.y + std::sin(a) * r * 0.68f);
        dl->AddTriangleFilled(ImVec2(tip.x + r * 0.26f, tip.y),
                              ImVec2(tip.x - r * 0.12f, tip.y - r * 0.24f),
                              ImVec2(tip.x - r * 0.12f, tip.y + r * 0.24f), col);
        break;
    }

    case Icon::Cube: {
        // Proiezione isometrica: tre facce, spigoli condivisi.
        const ImVec2 top = P(0, -0.86f), left = P(-0.80f, -0.42f), right = P(0.80f, -0.42f);
        const ImVec2 bottom = P(0, 0.86f), bl = P(-0.80f, 0.42f), br = P(0.80f, 0.42f);
        const ImVec2 mid = P(0, 0.0f);
        const ImVec2 faceTop[] = {top, right, mid, left};
        dl->AddConvexPolyFilled(faceTop, 4, col);
        const ImVec2 faceL[] = {left, mid, bottom, bl};
        dl->AddConvexPolyFilled(faceL, 4, (col & 0x00FFFFFF) | 0xA0000000);
        const ImVec2 faceR[] = {mid, right, br, bottom};
        dl->AddConvexPolyFilled(faceR, 4, (col & 0x00FFFFFF) | 0x70000000);
        break;
    }

    case Icon::Sphere: {
        dl->AddCircleFilled(c, r * 0.80f, (col & 0x00FFFFFF) | 0x60000000, 28);
        dl->AddCircle(c, r * 0.80f, col, 28, t);
        dl->PathArcTo(ImVec2(c.x, c.y), r * 0.80f, 0.f, 3.14159f, 20);
        dl->PathStroke(col, 0, t * 0.7f);
        break;
    }

    case Icon::Cylinder: {
        dl->AddLine(P(-0.62f, -0.52f), P(-0.62f, 0.52f), col, t);
        dl->AddLine(P(0.62f, -0.52f),  P(0.62f, 0.52f),  col, t);
        dl->AddEllipse(ImVec2(c.x, c.y - r * 0.52f), ImVec2(r * 0.62f, r * 0.26f), col, 0.f, 24, t);
        dl->PathArcTo(ImVec2(c.x, c.y + r * 0.52f), r * 0.62f, 0.f, 3.14159f, 18);
        dl->PathStroke(col, 0, t);
        break;
    }

    case Icon::Wedge: {
        const ImVec2 pts[] = {P(-0.82f, 0.66f), P(0.82f, 0.66f), P(0.82f, -0.72f)};
        dl->AddConvexPolyFilled(pts, 3, col);
        break;
    }

    case Icon::Duplicate: {
        dl->AddRect(P(-0.80f, -0.80f), P(0.28f, 0.28f), col, 2.f, 0, t);
        dl->AddRectFilled(P(-0.28f, -0.28f), P(0.80f, 0.80f), col, 2.f);
        break;
    }

    case Icon::Trash: {
        dl->AddRectFilled(P(-0.62f, -0.62f), P(0.62f, -0.40f), col, 1.f);
        dl->AddLine(P(-0.22f, -0.80f), P(0.22f, -0.80f), col, t);
        const ImVec2 body[] = {P(-0.50f, -0.30f), P(0.50f, -0.30f), P(0.36f, 0.82f), P(-0.36f, 0.82f)};
        dl->AddConvexPolyFilled(body, 4, (col & 0x00FFFFFF) | 0xC0000000);
        dl->AddLine(P(-0.16f, -0.10f), P(-0.12f, 0.60f), IM_COL32(20, 22, 26, 200), t * 0.8f);
        dl->AddLine(P(0.16f, -0.10f),  P(0.12f, 0.60f),  IM_COL32(20, 22, 26, 200), t * 0.8f);
        break;
    }

    case Icon::Undo:
    case Icon::Redo: {
        const bool flip = (icon == Icon::Redo);
        const float s = flip ? -1.f : 1.f;
        dl->PathArcTo(ImVec2(c.x, c.y + r * 0.16f), r * 0.60f, 3.14159f, 6.02f, 20);
        dl->PathStroke(col, 0, t);
        const ImVec2 tip = P(s * -0.60f, 0.16f);
        dl->AddTriangleFilled(ImVec2(tip.x - s * r * 0.30f, tip.y - r * 0.06f),
                              ImVec2(tip.x + s * r * 0.16f, tip.y - r * 0.38f),
                              ImVec2(tip.x + s * r * 0.16f, tip.y + r * 0.26f), col);
        break;
    }

    case Icon::Play:
        dl->AddTriangleFilled(P(-0.52f, -0.76f), P(0.76f, 0.f), P(-0.52f, 0.76f), col);
        break;

    case Icon::Stop:
        dl->AddRectFilled(P(-0.62f, -0.62f), P(0.62f, 0.62f), col, 2.f);
        break;

    case Icon::Save: {
        dl->AddRectFilled(P(-0.78f, -0.78f), P(0.78f, 0.78f), col, 3.f);
        dl->AddRectFilled(P(-0.42f, -0.78f), P(0.42f, -0.18f), IM_COL32(24, 27, 32, 255), 1.f);
        dl->AddRectFilled(P(-0.52f, 0.10f), P(0.52f, 0.78f), IM_COL32(24, 27, 32, 255), 1.f);
        break;
    }

    case Icon::NewFile: {
        const ImVec2 pts[] = {P(-0.60f, -0.85f), P(0.24f, -0.85f), P(0.62f, -0.44f),
                              P(0.62f, 0.85f),  P(-0.60f, 0.85f)};
        dl->AddConvexPolyFilled(pts, 5, col);
        dl->AddTriangleFilled(P(0.24f, -0.85f), P(0.62f, -0.44f), P(0.24f, -0.44f),
                              IM_COL32(24, 27, 32, 220));
        break;
    }

    case Icon::Grid: {
        for (int i = -1; i <= 1; ++i) {
            const float f = i * 0.44f;
            dl->AddLine(P(f, -0.82f), P(f, 0.82f), col, t * 0.8f);
            dl->AddLine(P(-0.82f, f), P(0.82f, f), col, t * 0.8f);
        }
        break;
    }

    case Icon::Script: {
        dl->AddRectFilled(P(-0.66f, -0.84f), P(0.66f, 0.84f), (col & 0x00FFFFFF) | 0x55000000, 2.f);
        dl->AddRect(P(-0.66f, -0.84f), P(0.66f, 0.84f), col, 2.f, 0, t * 0.8f);
        for (int i = 0; i < 4; ++i) {
            const float y = -0.48f + i * 0.34f;
            dl->AddLine(P(-0.40f, y), P(i == 3 ? 0.10f : 0.40f, y), col, t * 0.7f);
        }
        break;
    }

    case Icon::Light: {
        dl->AddCircleFilled(c, r * 0.40f, col, 20);
        for (int i = 0; i < 8; ++i) {
            const float a = i * 0.7853982f;
            dl->AddLine(ImVec2(c.x + std::cos(a) * r * 0.58f, c.y + std::sin(a) * r * 0.58f),
                        ImVec2(c.x + std::cos(a) * r * 0.88f, c.y + std::sin(a) * r * 0.88f),
                        col, t * 0.8f);
        }
        break;
    }

    case Icon::Folder: {
        const ImVec2 pts[] = {P(-0.82f, -0.56f), P(-0.16f, -0.56f), P(0.02f, -0.30f),
                              P(0.82f, -0.30f), P(0.82f, 0.70f),   P(-0.82f, 0.70f)};
        dl->AddConvexPolyFilled(pts, 6, col);
        break;
    }

    case Icon::Camera: {
        dl->AddRectFilled(P(-0.76f, -0.34f), P(0.30f, 0.56f), col, 2.f);
        dl->AddTriangleFilled(P(0.34f, -0.14f), P(0.80f, -0.42f), P(0.80f, 0.40f), col);
        break;
    }

    case Icon::Anchor: {
        dl->AddCircle(ImVec2(c.x, c.y - r * 0.56f), r * 0.22f, col, 14, t);
        dl->AddLine(P(0, -0.30f), P(0, 0.72f), col, t);
        dl->AddLine(P(-0.44f, 0.06f), P(0.44f, 0.06f), col, t);
        dl->PathArcTo(c, r * 0.62f, 0.35f, 2.79f, 18);
        dl->PathStroke(col, 0, t);
        break;
    }
    }
}

} // namespace studio
