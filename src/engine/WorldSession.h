#pragma once

// Prompt, beam, trail, attachment e tag salvati nel place.
// Il telefono e il server dedicato li accendono dallo stesso file.

#include "engine/DataModel.h"
#include "engine/LuaRuntime.h"
#include "engine/ScriptControls.h"
#include "systems/Prossimita.h"
#include "systems/Tag.h"

inline void capturePlaceSession(DataModel& dm, const Prossimita& prompts, const Tag& tags) {
    dm.savedPrompts.clear();
    for (const auto& p : prompts.all()) {
        DataModel::SavedPrompt s;
        s.name = p.name;
        s.text = p.text;
        s.objectText = p.objectText;
        s.x = p.x; s.y = p.y; s.z = p.z;
        s.radius = p.radius;
        s.partIndex = p.partIndex;
        s.key = p.key;
        s.hold = p.holdDuration;
        s.enabled = p.enabled;
        dm.savedPrompts.push_back(std::move(s));
    }
    auto& sc = scriptControls();
    dm.savedAttachments.clear();
    for (const auto& a : sc.attachments) {
        DataModel::SavedAttach s;
        s.name = a.name;
        s.partIndex = a.partIndex;
        s.ox = a.ox; s.oy = a.oy; s.oz = a.oz;
        dm.savedAttachments.push_back(std::move(s));
    }
    dm.savedBeams.clear();
    for (const auto& b : sc.beams) {
        DataModel::SavedBeam s;
        s.name = b.name;
        s.a0 = b.a0; s.a1 = b.a1;
        s.width0 = b.width0; s.width1 = b.width1;
        s.r = b.r; s.g = b.g; s.b = b.b;
        s.enabled = b.enabled;
        dm.savedBeams.push_back(std::move(s));
    }
    dm.savedTrails.clear();
    for (const auto& t : sc.trails) {
        DataModel::SavedTrail s;
        s.name = t.name;
        s.partIndex = t.partIndex;
        s.width = t.width;
        s.life = t.life;
        s.r = t.r; s.g = t.g; s.b = t.b;
        s.enabled = t.enabled;
        dm.savedTrails.push_back(std::move(s));
    }
    dm.savedTags.clear();
    for (const std::string& name : tags.allTagNames()) {
        for (int id : tags.get(name)) {
            DataModel::SavedTag s;
            s.tag = name;
            s.partIndex = id;
            dm.savedTags.push_back(std::move(s));
        }
    }
}

inline void applyPlaceSession(DataModel& dm, Prossimita& prompts, Tag& tags) {
    prompts.reset();
    for (const auto& s : dm.savedPrompts) {
        prompts.add(s.name, s.text, s.x, s.y, s.z, s.radius, s.partIndex);
        prompts.style(s.name, s.objectText, s.key, s.hold);
        if (!s.enabled) prompts.setEnabled(s.name, false);
    }
    auto& sc = scriptControls();
    sc.attachments.clear();
    for (const auto& a : dm.savedAttachments) {
        ScriptControls::ScriptAttachment s;
        s.name = a.name;
        s.partIndex = a.partIndex;
        s.ox = a.ox; s.oy = a.oy; s.oz = a.oz;
        sc.attachments.push_back(std::move(s));
    }
    sc.beams.clear();
    for (const auto& b : dm.savedBeams) {
        ScriptControls::ScriptBeam s;
        s.name = b.name;
        s.a0 = b.a0; s.a1 = b.a1;
        s.width0 = b.width0; s.width1 = b.width1;
        s.r = b.r; s.g = b.g; s.b = b.b;
        s.enabled = b.enabled;
        sc.beams.push_back(std::move(s));
    }
    sc.trails.clear();
    for (const auto& t : dm.savedTrails) {
        ScriptControls::ScriptTrail s;
        s.name = t.name;
        s.partIndex = t.partIndex;
        s.width = t.width;
        s.life = t.life;
        s.r = t.r; s.g = t.g; s.b = t.b;
        s.enabled = t.enabled;
        sc.trails.push_back(std::move(s));
    }
    tags.reset();
    for (const auto& t : dm.savedTags)
        tags.add(t.tag, t.partIndex);
}

inline std::string emitterClassOf(const LuaRuntime::ScriptParticleEmitter& e) {
    if (!e.className.empty()) return e.className;
    if (e.preset == 2) return "Smoke"; // ParticlePreset::Smoke (Burst 0, Sparks 1, Smoke 2)
    if (e.customMotion) return "Fire";
    return "Sparkles";
}

inline void captureEmittersAndClicks(DataModel& dm,
                                    const std::vector<LuaRuntime::ScriptParticleEmitter>& emitters) {
    dm.savedEmitters.clear();
    for (const auto& e : emitters) {
        DataModel::SavedEmitter s;
        s.className = emitterClassOf(e);
        s.name = e.name.empty() ? s.className : e.name;
        s.partIndex = e.partIndex;
        s.preset = e.preset;
        s.rate = e.rate;
        s.speed = e.speed;
        s.life = e.life;
        s.r = e.color.r;
        s.g = e.color.g;
        s.b = e.color.b;
        s.enabled = e.enabled;
        s.customMotion = e.customMotion || s.className == "Fire";
        dm.savedEmitters.push_back(std::move(s));
    }
    dm.savedClicks.clear();
    for (const auto& kv : scriptControls().clickReach) {
        if (kv.first < 0) continue;
        DataModel::SavedClick c;
        c.partIndex = kv.first;
        c.reach = kv.second;
        dm.savedClicks.push_back(c);
    }
}

inline void restoreEmittersAndClicks(const DataModel& dm,
                                     std::vector<LuaRuntime::ScriptParticleEmitter>& emitters) {
    emitters.clear();
    for (const auto& s : dm.savedEmitters) {
        LuaRuntime::ScriptParticleEmitter e;
        e.className = s.className;
        e.name = s.name;
        e.partIndex = s.partIndex;
        e.preset = s.preset;
        e.rate = s.rate;
        e.speed = s.speed;
        e.life = s.life;
        e.color = {s.r, s.g, s.b};
        e.enabled = s.enabled;
        e.customMotion = s.customMotion || s.className == "Fire";
        emitters.push_back(std::move(e));
    }
    scriptControls().clickReach.clear();
    for (const auto& c : dm.savedClicks)
        if (c.partIndex >= 0) scriptControls().clickReach[c.partIndex] = c.reach;
}

// Copia di fuoco, fumo, attachment, scie e ClickDetector presa prima di Play.
// Stop la rimette: quello creato dagli script durante il Test non resta nel posto.
struct StudioFxSnap {
    std::vector<LuaRuntime::ScriptParticleEmitter> emitters;
    std::vector<ScriptControls::ScriptAttachment> attachments;
    std::vector<ScriptControls::ScriptBeam> beams;
    std::vector<ScriptControls::ScriptTrail> trails;
    std::map<int, float> clicks;
};
inline StudioFxSnap& studioFxSnap() {
    static StudioFxSnap s;
    return s;
}
inline void rememberStudioFx(const std::vector<LuaRuntime::ScriptParticleEmitter>& emitters) {
    StudioFxSnap& s = studioFxSnap();
    s.emitters = emitters;
    s.attachments = scriptControls().attachments;
    s.beams = scriptControls().beams;
    s.trails = scriptControls().trails;
    s.clicks = scriptControls().clickReach;
}
inline void recallStudioFx(std::vector<LuaRuntime::ScriptParticleEmitter>& emitters) {
    const StudioFxSnap& s = studioFxSnap();
    emitters = s.emitters;
    scriptControls().attachments = s.attachments;
    scriptControls().beams = s.beams;
    scriptControls().trails = s.trails;
    scriptControls().clickReach = s.clicks;
}
