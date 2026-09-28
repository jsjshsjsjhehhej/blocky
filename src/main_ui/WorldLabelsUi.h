// Etichette e pulsanti GUI nello spazio del mondo (world-space): proiezione
// schermo, billboard text, drag/resize delle etichette, pick e hover dei
// pulsanti. Non e' un header riusabile: usa direttamente le globali di
// main.cpp (g_world, g_selectedLabel, g_labelDrag*, ecc.). Compila solo se
// incluso da main.cpp esattamente in questo punto.

static bool worldToScreen(const glm::vec3& world, float& outX, float& outY) {
    glm::vec4 clip = g_lastProj * g_lastView * glm::vec4(world, 1.f);
    if (clip.w <= 0.001f) return false;
    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    if (ndc.z < -1.f || ndc.z > 1.f) return false;
    outX = (ndc.x * 0.5f + 0.5f) * (float)g_w;
    outY = (1.f - (ndc.y * 0.5f + 0.5f)) * (float)g_h;
    return true;
}

static void drawBillboardText(const glm::vec3& worldPos, const char* text,
                              glm::vec3 color, float fontScale, bool bg, bool selected) {
    float sx = 0.f, sy = 0.f;
    if (!worldToScreen(worldPos, sx, sy)) return;
    ImFont* font = ImGui::GetFont();
    float fs = ImGui::GetFontSize() * std::clamp(fontScale, 0.5f, 3.f);
    ImVec2 textSize = font->CalcTextSizeA(fs, FLT_MAX, 0.f, text);
    float padX = 12.f * fontScale, padY = 6.f * fontScale;
    float bob = selected ? std::sin(g_studioUiTime * 4.f) * 2.f : 0.f;
    ImVec2 boxMin{ sx - textSize.x * 0.5f - padX, sy - textSize.y - padY + bob };
    ImVec2 boxMax{ sx + textSize.x * 0.5f + padX, sy + padY + bob };
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    if (bg) {
        dl->AddRectFilled(boxMin, boxMax, IM_COL32(12, 18, 32, selected ? 230 : 190), 8.f);
        dl->AddRect(boxMin, boxMax,
            selected ? IM_COL32(80, 190, 255, 255) : IM_COL32(100, 150, 220, 140),
            8.f, 0, selected ? 2.f : 1.2f);
    }
    ImU32 col = IM_COL32((int)(color.x * 255), (int)(color.y * 255), (int)(color.z * 255), 255);
    ImVec2 tp{boxMin.x + padX, boxMin.y + padY};
    if (!bg) {
        tp = {sx - textSize.x * 0.5f, sy - textSize.y + bob};
        dl->AddText(font, fs, {tp.x + 1.f, tp.y + 1.f}, IM_COL32(0, 0, 0, 180), text);
    }
    dl->AddText(font, fs, tp, col, text);
}

static constexpr float kLabelHandleR = 7.f;
static constexpr float kLabelMinW = 24.f;
static constexpr float kLabelMinH = 16.f;

static void labelAutoTextSize(const WorkspaceLabel& l, float& tw, float& th) {
    ImFont* font = ImGui::GetFont();
    if (!font || ImGui::GetFontSize() <= 0.f) {
        tw = kLabelMinW;
        th = kLabelMinH;
        return;
    }
    float fs = ImGui::GetFontSize() * std::clamp(l.fontScale, 0.5f, 3.f);
    ImVec2 ts = font->CalcTextSizeA(fs, FLT_MAX, 0.f, l.text.c_str());
    float padX = 8.f * l.fontScale, padY = 4.f * l.fontScale;
    tw = ts.x + padX * 2.f;
    th = ts.y + padY * 2.f;
}

static void labelBoxSize(const WorkspaceLabel& l, float& w, float& h) {
    float aw = 0.f, ah = 0.f;
    labelAutoTextSize(l, aw, ah);
    w = (l.sizeW > 1.f) ? l.sizeW : aw;
    h = (l.sizeH > 1.f) ? l.sizeH : ah;
    w = std::max(w, kLabelMinW);
    h = std::max(h, kLabelMinH);
}

static void labelCenter(const WorkspaceLabel& l, float& cx, float& cy) {
    cx = l.anchorX * (float)g_w + l.offsetX;
    cy = l.anchorY * (float)g_h + l.offsetY;
}

static bool screenLabelRect(const WorkspaceLabel& l, ImVec2& outMin, ImVec2& outMax) {
    if (!l.visible) return false;
    float w = 0.f, h = 0.f, cx = 0.f, cy = 0.f;
    labelBoxSize(l, w, h);
    labelCenter(l, cx, cy);
    outMin = { cx - w * 0.5f, cy - h * 0.5f };
    outMax = { cx + w * 0.5f, cy + h * 0.5f };
    return true;
}

static void labelCornerPos(const ImVec2& mn, const ImVec2& mx, int corner, float& x, float& y) {
    switch (corner) {
    case 0: x = mn.x; y = mn.y; break;
    case 1: x = mx.x; y = mn.y; break;
    case 2: x = mx.x; y = mx.y; break;
    default: x = mn.x; y = mx.y; break;
    }
}

static int pickLabelCornerAt(const WorkspaceLabel& l, float mx, float my) {
    ImVec2 mn, mxb;
    if (!screenLabelRect(l, mn, mxb)) return -1;
    for (int c = 0; c < 4; ++c) {
        float hx = 0.f, hy = 0.f;
        labelCornerPos(mn, mxb, c, hx, hy);
        float dx = mx - hx, dy = my - hy;
        if (dx * dx + dy * dy <= (kLabelHandleR + 4.f) * (kLabelHandleR + 4.f))
            return c;
    }
    return -1;
}

static void setLabelCenterFromScreen(WorkspaceLabel& l, float cx, float cy) {
    l.offsetX = cx - l.anchorX * (float)g_w;
    l.offsetY = cy - l.anchorY * (float)g_h;
}

static void beginLabelDrag(int idx, float mx, float my) {
    if (idx < 0 || idx >= (int)g_world.labels.size()) return;
    auto& l = g_world.labels[idx];
    ImVec2 mn, mxb;
    if (!screenLabelRect(l, mn, mxb)) return;
    int corner = pickLabelCornerAt(l, mx, my);
    g_labelDragging = true;
    g_labelDragCorner = corner;
    if (corner >= 0) {
        int opp = (corner + 2) % 4;
        labelCornerPos(mn, mxb, opp, g_labelDragFixedX, g_labelDragFixedY);
    } else {
        float cx = 0.f, cy = 0.f;
        labelCenter(l, cx, cy);
        g_labelGrabDX = mx - cx;
        g_labelGrabDY = my - cy;
    }
}

static void updateLabelDrag(float mx, float my) {
    if (!g_labelDragging || g_selectedLabel < 0
        || g_selectedLabel >= (int)g_world.labels.size())
        return;
    auto& l = g_world.labels[g_selectedLabel];
    if (g_labelDragCorner < 0) {
        setLabelCenterFromScreen(l, mx - g_labelGrabDX, my - g_labelGrabDY);
    } else {
        float fx = g_labelDragFixedX, fy = g_labelDragFixedY;
        float left = std::min(fx, mx), right = std::max(fx, mx);
        float top = std::min(fy, my), bottom = std::max(fy, my);
        if (right - left < kLabelMinW) {
            if (mx >= fx) right = left + kLabelMinW;
            else left = right - kLabelMinW;
        }
        if (bottom - top < kLabelMinH) {
            if (my >= fy) bottom = top + kLabelMinH;
            else top = bottom - kLabelMinH;
        }
        l.sizeW = right - left;
        l.sizeH = bottom - top;
        setLabelCenterFromScreen(l, (left + right) * 0.5f, (top + bottom) * 0.5f);
    }
    markPlaceDirty();
}

static void endLabelDrag() {
    g_labelDragging = false;
    g_labelDragCorner = -1;
}

static void drawOutlinedText(ImDrawList* dl, ImFont* font, float fs,
                             ImVec2 tp, ImU32 fill, const char* text) {
    const float o = std::max(1.f, fs * 0.06f);
    const ImU32 outline = IM_COL32(0, 0, 0, 255);
    static const float ox[8] = {-1,-1,-1, 0, 0, 1, 1, 1};
    static const float oy[8] = {-1, 0, 1,-1, 1,-1, 0, 1};
    for (int i = 0; i < 8; ++i)
        dl->AddText(font, fs, {tp.x + ox[i] * o, tp.y + oy[i] * o}, outline, text);
    dl->AddText(font, fs, tp, fill, text);
}

static void drawScreenUiLabel(const WorkspaceLabel& l, bool selected) {
    if (!l.visible) return;
    ImVec2 boxMin, boxMax;
    if (!screenLabelRect(l, boxMin, boxMax)) return;
    ImFont* font = ImGui::GetFont();
    float fs = ImGui::GetFontSize() * std::clamp(l.fontScale, 0.5f, 3.f);
    const char* text = l.text.c_str();
    ImVec2 textSize = font->CalcTextSizeA(fs, FLT_MAX, 0.f, text);
    float cx = (boxMin.x + boxMax.x) * 0.5f;
    float cy = (boxMin.y + boxMax.y) * 0.5f;
    ImDrawList* dl = ImGui::GetForegroundDrawList();

    const bool isBtn = l.isButton();
    glm::vec3 bg = l.bgColor;
    float bgT = l.bgTransparency;
    int selfIdx = -1;
    for (int i = 0; i < (int)g_world.labels.size(); ++i) {
        if (&g_world.labels[i] == &l) { selfIdx = i; break; }
    }
    if (isBtn && selfIdx >= 0) {
        if (g_uiPressedLabel == selfIdx) {
            bg *= 0.72f;
            bgT = std::max(0.f, bgT - 0.05f);
        } else if (g_uiHoverLabel == selfIdx) {
            bg = glm::clamp(bg * 1.18f + glm::vec3(0.08f), glm::vec3(0.f), glm::vec3(1.f));
        }
    }

    float bgA = std::clamp(1.f - bgT, 0.f, 1.f);
    if (bgA > 0.001f) {
        ImU32 bgCol = IM_COL32((int)(bg.x * 255), (int)(bg.y * 255),
                               (int)(bg.z * 255), (int)(bgA * 255));
        float round = isBtn ? 4.f : 3.f;
        dl->AddRectFilled(boxMin, boxMax, bgCol, round);
        if (isBtn) {
            ImU32 border = (g_uiPressedLabel == selfIdx)
                ? IM_COL32(20, 20, 20, 220)
                : IM_COL32(255, 255, 255, 90);
            dl->AddRect(boxMin, boxMax, border, round, 0, 1.5f);
            if (g_uiPressedLabel != selfIdx)
                dl->AddRect({boxMin.x + 1.f, boxMin.y + 1.f},
                            {boxMax.x - 1.f, boxMax.y - 1.f},
                            IM_COL32(0, 0, 0, 100), round, 0, 1.f);
        }
    }
    if (selected && !g_studioTesting) {
        dl->AddRect(boxMin, boxMax, IM_COL32(80, 190, 255, 180), 2.f, 0, 1.2f);
    }

    ImU32 col = IM_COL32((int)(l.color.x * 255), (int)(l.color.y * 255),
                         (int)(l.color.z * 255), 255);
    ImVec2 tp{cx - textSize.x * 0.5f, cy - textSize.y * 0.5f};
    drawOutlinedText(dl, font, fs, tp, col, text);

    if (selected && !g_studioTesting) {
        for (int c = 0; c < 4; ++c) {
            float hx = 0.f, hy = 0.f;
            labelCornerPos(boxMin, boxMax, c, hx, hy);
            bool hot = (g_labelDragging && g_labelDragCorner == c);
            ImU32 fill = hot ? IM_COL32(255, 220, 80, 255) : IM_COL32(255, 255, 255, 255);
            ImU32 border = IM_COL32(40, 120, 220, 255);
            dl->AddRectFilled({hx - kLabelHandleR, hy - kLabelHandleR},
                              {hx + kLabelHandleR, hy + kLabelHandleR}, fill, 2.f);
            dl->AddRect({hx - kLabelHandleR, hy - kLabelHandleR},
                        {hx + kLabelHandleR, hy + kLabelHandleR}, border, 2.f, 0, 1.5f);
        }
    }
}

static int pickScreenLabelAt(float mx, float my) {
    for (int i = (int)g_world.labels.size() - 1; i >= 0; --i) {
        const auto& l = g_world.labels[i];
        if (i == g_selectedLabel && pickLabelCornerAt(l, mx, my) >= 0)
            return i;
        ImVec2 mn, mxb;
        if (!screenLabelRect(l, mn, mxb)) continue;
        if (mx >= mn.x && mx <= mxb.x && my >= mn.y && my <= mxb.y)
            return i;
    }
    return -1;
}

static void fireGuiButton(int idx) {
    if (idx < 0 || idx >= (int)g_world.labels.size()) return;
    const auto& l = g_world.labels[idx];
    if (!l.isButton() || !l.visible) return;
    scriptControls().lastGuiButton = l.name;
    if (g_lua.ok)
        g_lua.fireGuiButtonActivated(l.name);
    g_exportMsg = std::string("TextButton cliccato: ") + l.name;
}

static void updateGuiButtonHover(float mx, float my) {
    g_uiHoverLabel = -1;
    if (g_playCursorLocked) return;
    int hit = pickScreenLabelAt(mx, my);
    if (hit >= 0 && g_world.labels[hit].isButton())
        g_uiHoverLabel = hit;
}

static void drawWorldLabels() {
    for (int i = 0; i < (int)g_world.labels.size(); ++i)
        drawScreenUiLabel(g_world.labels[i], i == g_selectedLabel && !g_studioTesting);
}

static void drawSelectedPartLabel() {
    drawWorldLabels();
    if (g_selectedPart < 0 || g_selectedPart >= (int)g_world.parts.size()) return;
    if (g_studioTesting) return;
    const auto& p = g_world.parts[g_selectedPart];
    glm::vec3 top = p.position;
    glm::vec3 upLocal(0, p.size.y * 0.5f + 0.55f, 0);
    top += glm::vec3(partRotationMatrix(p.rotation) * glm::vec4(upLocal, 0.f));
    drawBillboardText(top, p.name.c_str(), {1.f, 1.f, 1.f}, 0.95f, false, false);
}

