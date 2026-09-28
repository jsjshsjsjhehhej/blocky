#pragma once


#include "AnimPack.h"

#include <imgui.h>

#include <string>
#include <vector>

struct AnimPackPublisherState {
    AnimPackDraft draft;
    char nameBuf[96] = {};
    char authorBuf[96] = {};
    std::string error;
    std::string lastPublishedId;
};

struct AnimPackClipEntry {
    std::string id;
    std::string label;
    R6AnimKind kind = R6AnimKind::Walk;
    bool looksLikeTool = false;
};

inline std::vector<AnimPackClipEntry> animPackAvailableClips() {
    std::vector<AnimPackClipEntry> out;
    std::map<std::string, bool> seen;
    for (const auto& kv : animLibrary().byId) {
        const R6WalkClip& c = kv.second;
        if (c.id.empty() || seen[c.id]) continue;
        seen[c.id] = true;
        AnimPackClipEntry e;
        e.id = c.id;
        e.kind = c.kind;
        e.looksLikeTool = Animator::looksLikeToolAnim(c);
        e.label = (c.displayName.empty() ? c.id : c.displayName)
                + std::string("  [") + (e.looksLikeTool ? "tool" : r6AnimKindName(c.kind)) + "]";
        out.push_back(e);
    }
    return out;
}

inline void drawAnimPackPublisher(AnimPackPublisherState& st) {
    std::vector<AnimPackClipEntry> clips = animPackAvailableClips();

    ImGui::TextWrapped(
        "Un pacchetto e' un set di animazioni che il giocatore equipaggia in blocco.\n"
        "I ruoli lasciati vuoti usano l'animazione di default.");
    ImGui::Spacing();

    if (clips.empty()) {
        ImGui::TextColored({1.f, 0.75f, 0.3f, 1.f},
            "Nessuna animazione in libreria: importa prima una clip FBX/GLB.");
        return;
    }

    ImGui::SeparatorText("1. Assegna le animazioni ai ruoli");
    for (int r = 0; r < (int)AnimRole::Count; ++r) {
        AnimRole role = (AnimRole)r;
        ImGui::PushID(r);
        const std::string& cur = st.draft.clipId[r];
        const char* preview = cur.empty() ? "(eredita dal default)" : cur.c_str();
        if (ImGui::BeginCombo(animRoleLabel(role), preview)) {
            if (ImGui::Selectable("(eredita dal default)", cur.empty()))
                st.draft.clipId[r].clear();
            for (const auto& c : clips) {
                if (animRoleIsLocomotion(role) && c.looksLikeTool) continue;
                if (ImGui::Selectable(c.label.c_str(), cur == c.id))
                    st.draft.clipId[r] = c.id;
            }
            ImGui::EndCombo();
        }
        ImGui::PopID();
    }
    ImGui::TextDisabled("Le clip [tool] non compaiono nei ruoli di movimento.");

    ImGui::Spacing();
    ImGui::SeparatorText("2. Nome del pacchetto");
    ImGui::InputText("Nome", st.nameBuf, sizeof(st.nameBuf));
    ImGui::InputText("Autore", st.authorBuf, sizeof(st.authorBuf));

    int filled = 0;
    for (int r = 0; r < (int)AnimRole::Count; ++r)
        if (!st.draft.clipId[r].empty()) ++filled;
    ImGui::TextDisabled("Ruoli assegnati: %d / %d", filled, (int)AnimRole::Count);

    ImGui::Spacing();
    ImGui::SeparatorText("3. Pubblica");
    ImGui::PushStyleColor(ImGuiCol_Button, {0.12f, 0.62f, 0.42f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.18f, 0.75f, 0.52f, 1.f});
    if (ImGui::Button("Pubblica pacchetto", {-1, 44})) {
        st.draft.displayName = st.nameBuf;
        st.draft.author = st.authorBuf;
        st.error.clear();
        std::string id;
        if (animPacks().publish(st.draft, st.error, &id)) {
            st.lastPublishedId = id;
            st.draft = AnimPackDraft();
            st.nameBuf[0] = '\0';
        }
    }
    ImGui::PopStyleColor(2);

    if (!st.error.empty())
        ImGui::TextColored({1.f, 0.45f, 0.4f, 1.f}, "%s", st.error.c_str());
    if (!st.lastPublishedId.empty()) {
        ImGui::TextColored({0.35f, 1.f, 0.55f, 1.f}, "Pubblicato: %s",
                           st.lastPublishedId.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Copia id"))
            ImGui::SetClipboardText(st.lastPublishedId.c_str());
    }

    ImGui::Spacing();
    ImGui::SeparatorText("Pacchetti nel catalogo");
    std::vector<const AnimPack*> packs = animPacks().list();
    for (size_t i = 0; i < packs.size(); ++i) {
        const AnimPack* p = packs[i];
        bool eq = (p->id == animPacks().equippedId);
        ImGui::PushID(3000 + (int)i);
        if (eq) ImGui::TextColored({0.3f, 0.9f, 0.5f, 1.f}, "* %s", p->displayName.c_str());
        else    ImGui::Text("%s", p->displayName.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("(%d ruoli%s)", p->filledRoles(), p->builtin ? ", sistema" : "");
        ImGui::SameLine();
        if (eq) {
            if (ImGui::SmallButton("Togli")) animPacks().unequip();
        } else if (ImGui::SmallButton("Equipaggia")) {
            animPacks().equip(p->id);
        }
        if (!p->builtin) {
            ImGui::SameLine();
            if (ImGui::SmallButton("Elimina")) animPacks().remove(p->id);
        }
        ImGui::PopID();
    }

    ImGui::Spacing();
    ImGui::SeparatorText("Attivo adesso");
    for (int r = 0; r < (int)AnimRole::Count; ++r) {
        const R6WalkClip* c = animPacks().clipFor((AnimRole)r);
        ImGui::Text("%-9s %s", animRoleLabel((AnimRole)r), c ? c->id.c_str() : "-");
    }
    if (!animPacks().toolClipId.empty())
        ImGui::TextColored({1.f, 0.8f, 0.35f, 1.f},
            "Tool in mano: %s sovrascrive %s",
            animPacks().toolClipId.c_str(), animRoleLabel(animPacks().toolRole));
}
