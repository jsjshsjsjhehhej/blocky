#pragma once

/**
 * Blocky Animator — editor di animazioni del personaggio a keyframe.
 *
 * Idea di fondo, la stessa dell'editor di Roblox: si sceglie un istante sulla
 * linea del tempo, si mette il corpo nella posa voluta, si preme "Metti
 * chiave". Il motore interpola da solo il passaggio fra una chiave e l'altra.
 * Niente Blender, niente file esterni: si anima il personaggio che si ha
 * davanti.
 *
 * Sulla PRECISIONE, che era la richiesta principale: ogni valore si scrive a
 * numero. I campi sono trascinabili per provare in fretta, ma un doppio clic
 * li apre alla digitazione e mostrano tre decimali, quindi "braccio a -90
 * gradi" e' davvero -90.000 e non "circa novanta".
 *
 * Il file vive in main_ui/ e non in ui/ perche' usa lo stato globale dello
 * Studio (g_player), come gli altri pannelli qui dentro.
 */

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "engine/RigAnim.h"

// Stato dell'editor. Vive qui e non dentro la funzione di disegno perche'
// sopravvive fra un frame e l'altro.
static bool  g_showRigAnimator = false;
static blk::riganim::Clip g_rigClip;
static int   g_rigSelectedPart = 0;
static float g_rigTime = 0.f;
static bool  g_rigPreviewing = false;
static char  g_rigClipName[64] = "Nuova animazione";
static char  g_rigClipId[64] = "";
static std::string g_rigStatus;

/** Nome della parte attualmente selezionata. */
inline const std::string& rigCurrentPart() {
    const auto& names = blk::riganim::partNames();
    const int i = std::clamp(g_rigSelectedPart, 0, (int)names.size() - 1);
    return names[(size_t)i];
}

/**
 * La posa che si sta modificando.
 *
 * Non e' una copia di lavoro: si legge e si scrive DIRETTAMENTE sul
 * personaggio, cosi' ogni numero digitato si vede subito addosso al modello.
 * E' la differenza fra "modifica e poi guarda com'e' venuto" e "muovi il
 * braccio e lo vedi muoversi".
 */
inline RigPose* rigLivePose() {
    for (auto& p : g_player.humanoid.parts)
        if (p.name == rigCurrentPart()) return &p.anim;
    return nullptr;
}

/** Riga di tre numeri: trascinabili, digitabili esatti, azzerabili. */
inline bool rigVec3Row(const char* label, glm::vec3& v, float step, float reset) {
    ImGui::PushID(label);
    ImGui::TextUnformatted(label);
    ImGui::SameLine(96);
    ImGui::SetNextItemWidth(230);
    // Tre decimali, non uno: chi scrive 12.5 gradi deve rivedere 12.5, non 12.
    bool changed = ImGui::DragFloat3("##v", &v.x, step, -10000.f, 10000.f, "%.3f");
    ImGui::SameLine();
    if (ImGui::SmallButton("azzera")) {
        v = glm::vec3(reset);
        changed = true;
    }
    ImGui::PopID();
    return changed;
}

/** Istante della chiave sotto al cursore del tempo, -1 se non ce n'e' nessuna. */
inline float rigKeyAtCurrentTime() {
    const blk::riganim::PartTrack* tr = g_rigClip.find(rigCurrentPart());
    if (!tr) return -1.f;
    for (const auto& k : tr->keys)
        if (std::fabs(k.t - g_rigTime) < 0.02f) return k.t;
    return -1.f;
}

inline void drawRigAnimatorUi() {
    using namespace blk::riganim;
    if (!g_showRigAnimator) return;

    ImGui::SetNextWindowSize({540, 580}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Blocky Animator##riganim", &g_showRigAnimator)) {
        ImGui::End();
        return;
    }

    // ---- Clip ----------------------------------------------------------
    ImGui::SetNextItemWidth(200);
    ImGui::InputText("Nome", g_rigClipName, sizeof(g_rigClipName));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    ImGui::InputText("Id file", g_rigClipId, sizeof(g_rigClipId));

    ImGui::SetNextItemWidth(120);
    ImGui::DragFloat("Durata (s)", &g_rigClip.duration, 0.05f, 0.1f, 60.f, "%.2f");
    ImGui::SameLine();
    ImGui::Checkbox("In ciclo", &g_rigClip.looped);

    if (ImGui::Button("Salva")) {
        g_rigClip.name = g_rigClipName;
        if (g_rigClipId[0] == 0)
            std::snprintf(g_rigClipId, sizeof(g_rigClipId), "%s",
                          sanitizeGameId(g_rigClipName).c_str());
        g_rigClip.id = g_rigClipId;
        g_rigStatus = save(g_rigClip) ? ("Salvata: " + g_rigClip.id)
                                      : "Salvataggio fallito";
    }
    ImGui::SameLine();
    if (ImGui::Button("Nuova")) {
        g_rigClip = Clip{};
        g_rigTime = 0.f;
        g_rigPreviewing = false;
        clearPose(g_player.humanoid);
        g_rigStatus = "Animazione vuota";
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150);
    if (ImGui::BeginCombo("##apri", "Apri salvata...")) {
        for (const auto& id : listSavedIds()) {
            if (!ImGui::Selectable(id.c_str())) continue;
            Clip loaded;
            if (load(id, loaded)) {
                g_rigClip = loaded;
                std::snprintf(g_rigClipName, sizeof(g_rigClipName), "%s", loaded.name.c_str());
                std::snprintf(g_rigClipId, sizeof(g_rigClipId), "%s", loaded.id.c_str());
                g_rigTime = 0.f;
                g_rigPreviewing = false;
                applyToHumanoid(g_rigClip, g_rigTime, g_player.humanoid);
                g_rigStatus = "Aperta: " + loaded.name;
            }
        }
        ImGui::EndCombo();
    }

    ImGui::Separator();

    // ---- Linea del tempo -----------------------------------------------
    ImGui::TextDisabled("Linea del tempo");
    ImGui::SetNextItemWidth(-110);
    if (ImGui::SliderFloat("##time", &g_rigTime, 0.f, g_rigClip.duration, "%.3f s")) {
        g_rigPreviewing = false;
        applyToHumanoid(g_rigClip, g_rigTime, g_player.humanoid);
    }
    ImGui::SameLine();
    if (ImGui::Button(g_rigPreviewing ? "Ferma" : "Prova", {90, 0})) {
        g_rigPreviewing = !g_rigPreviewing;
        if (!g_rigPreviewing) applyToHumanoid(g_rigClip, g_rigTime, g_player.humanoid);
    }

    // Tacche delle chiavi della parte selezionata: si vede a colpo d'occhio
    // dove si e' gia' messo qualcosa e dove no.
    {
        const ImVec2 p0 = ImGui::GetCursorScreenPos();
        const float width = std::max(80.f, ImGui::GetContentRegionAvail().x - 110.f);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(p0, {p0.x + width, p0.y + 12}, IM_COL32(35, 40, 50, 130), 3.f);
        if (const PartTrack* tr = g_rigClip.find(rigCurrentPart())) {
            for (const auto& k : tr->keys) {
                const float x = p0.x + width * (k.t / std::max(0.05f, g_rigClip.duration));
                const bool here = std::fabs(k.t - g_rigTime) < 0.02f;
                dl->AddRectFilled({x - 3, p0.y}, {x + 3, p0.y + 12},
                                  here ? IM_COL32(255, 210, 60, 255)
                                       : IM_COL32(90, 170, 255, 235), 2.f);
            }
        }
        ImGui::Dummy({width, 15});
    }

    ImGui::Separator();

    // ---- Parte del corpo -----------------------------------------------
    ImGui::TextDisabled("Parte da animare");
    const auto& names = partNames();
    for (int i = 0; i < (int)names.size(); ++i) {
        if (i % 3 != 0) ImGui::SameLine();
        const bool sel = (g_rigSelectedPart == i);
        if (sel) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.52f, 0.92f, 1.f));
        if (ImGui::Button(names[(size_t)i].c_str(), {155, 28})) g_rigSelectedPart = i;
        if (sel) ImGui::PopStyleColor();
    }

    ImGui::Separator();

    // ---- I numeri esatti ------------------------------------------------
    RigPose* pose = rigLivePose();
    if (!pose) {
        ImGui::TextColored({1.f, 0.5f, 0.4f, 1.f},
                           "Parte non trovata sul personaggio (rig non caricato?)");
        ImGui::End();
        return;
    }

    ImGui::TextDisabled("Doppio clic su un numero per digitarlo esatto");
    bool edited = false;
    if (rigVec3Row("Sposta", pose->pos,   0.02f, 0.f)) edited = true;
    if (rigVec3Row("Ruota",  pose->rot,   1.00f, 0.f)) edited = true;
    if (rigVec3Row("Scala",  pose->scale, 0.02f, 1.f)) edited = true;
    // Chi sta digitando dei numeri non vuole che l'anteprima glieli
    // sovrascriva al frame dopo.
    if (edited) g_rigPreviewing = false;

    ImGui::Spacing();
    if (ImGui::Button("Metti chiave qui", {175, 34})) {
        g_rigClip.setKey(rigCurrentPart(), g_rigTime, *pose);
        char buf[128];
        std::snprintf(buf, sizeof(buf), "Chiave su %s a %.3f s",
                      rigCurrentPart().c_str(), g_rigTime);
        g_rigStatus = buf;
    }
    ImGui::SameLine();
    const float existing = rigKeyAtCurrentTime();
    if (existing < 0.f) ImGui::BeginDisabled();
    if (ImGui::Button("Togli chiave", {140, 34})) {
        g_rigClip.removeKey(rigCurrentPart(), existing);
        g_rigStatus = "Chiave rimossa";
    }
    if (existing < 0.f) ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Posa a riposo", {140, 34})) *pose = RigPose{};

    ImGui::Separator();
    ImGui::Text("Chiavi in questa animazione: %d", g_rigClip.keyCount());
    if (!g_rigStatus.empty())
        ImGui::TextColored({0.45f, 0.95f, 0.55f, 1.f}, "%s", g_rigStatus.c_str());

    ImGui::End();
}

/**
 * Avanzamento dell'anteprima, da chiamare una volta per frame.
 *
 * Separato dal disegno perche' il tempo deve scorrere anche mentre si guarda
 * il personaggio invece della finestra dell'editor.
 */
inline void tickRigAnimatorPreview(float dt) {
    using namespace blk::riganim;
    if (!g_showRigAnimator || !g_rigPreviewing) return;
    if (g_rigClip.keyCount() == 0) return;

    g_rigTime += dt;
    if (g_rigTime > g_rigClip.duration) {
        if (g_rigClip.looped) g_rigTime = std::fmod(g_rigTime, g_rigClip.duration);
        else { g_rigTime = g_rigClip.duration; g_rigPreviewing = false; }
    }
    applyToHumanoid(g_rigClip, g_rigTime, g_player.humanoid);
}
