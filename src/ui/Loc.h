#pragma once

/**
 * Localizzazione Blocky + Blocky Game Engine.
 * Lingua da stato (IT/EN), auto da Windows al primo avvio, persistente.
 *
 * Uso:  ImGui::Button(LB("btn.test", "test"));
 *       ImGui::TextUnformatted(L("hub.games"));
 */

#include <imgui.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <unordered_map>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

enum class Lang : int { It = 0, En = 1, Count = 2 };

struct LocEntry {
    const char* it;
    const char* en;
};

inline const char* locLangCode(Lang l) {
    return l == Lang::En ? "en" : "it";
}

inline const char* locLangLabel(Lang l) {
    switch (l) {
    case Lang::En: return "English";
    case Lang::It: return "Italiano";
    default: return "Italiano";
    }
}

inline Lang locParseLangCode(const std::string& s) {
    if (s.empty()) return Lang::It;
    std::string low;
    for (char c : s) {
        if (c == '\r' || c == '\n' || c == ' ' || c == '\t') continue;
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        low.push_back(c);
    }
    if (low == "en" || low == "english" || low == "1") return Lang::En;
    if (low == "it" || low == "italiano" || low == "italian" || low == "0") return Lang::It;
    // "unknown" o altri → italiano
    return Lang::It;
}

inline std::string locSettingsPath() {
#ifdef _WIN32
    char appdata[MAX_PATH] = {};
    DWORD n = GetEnvironmentVariableA("APPDATA", appdata, MAX_PATH);
    if (n > 0 && n < MAX_PATH)
        return std::string(appdata) + "\\Blocky\\ui_lang.txt";
#endif
    return "ui_lang.txt";
}

inline Lang locDetectOsLang() {
#ifdef _WIN32
    LANGID id = GetUserDefaultUILanguage();
    if (PRIMARYLANGID(id) == LANG_ENGLISH)
        return Lang::En;
#endif
    return Lang::It;
}

struct Loc {
    Lang lang = Lang::It;
    bool loaded = false;

    static const std::unordered_map<std::string, LocEntry>& table() {
        static const std::unordered_map<std::string, LocEntry> t = {
            // —— Lingua / comune ——
            {"lang", {"Lingua", "Language"}},
            {"lang.it", {"Italiano", "Italian"}},
            {"lang.en", {"English", "English"}},
            {"ok", {"OK", "OK"}},
            {"cancel", {"Annulla", "Cancel"}},
            {"save", {"Salva", "Save"}},
            {"open", {"Apri", "Open"}},
            {"close", {"Chiudi", "Close"}},
            {"delete", {"Elimina", "Delete"}},
            {"remove", {"Rimuovi", "Remove"}},
            {"duplicate", {"Duplica", "Duplicate"}},
            {"more", {"Altro", "More"}},
            {"back", {"Indietro", "Back"}},
            {"quit", {"Esci", "Quit"}},
            {"logout", {"Logout", "Log out"}},
            {"play", {"Gioca", "Play"}},
            {"stop", {"Stop", "Stop"}},
            {"test", {"Test", "Test"}},
            {"edit", {"Modifica", "Edit"}},
            {"insert", {"Inserisci", "Insert"}},
            {"view", {"Vista", "View"}},
            {"file", {"File", "File"}},
            {"player", {"Player", "Player"}},
            {"animation", {"Animazione", "Animation"}},

            // —— Hub Blocky ——
            {"hub.games", {"Games", "Games"}},
            {"hub.shop", {"Shop", "Shop"}},
            {"hub.chat", {"Chat", "Chat"}},
            {"hub.login", {"Accedi", "Sign in"}},
            {"hub.login_sub", {"Accedi per continuare", "Sign in to continue"}},
            {"hub.guest", {"Entra come Ospite", "Continue as Guest"}},
            {"hub.google", {"Continua con Google", "Continue with Google"}},
            {"hub.google_wait", {"Completa l'accesso su Google nel browser, poi conferma la tua email qui.",
                                "Finish signing in with Google in the browser, then confirm your email here."}},
            {"hub.google_confirm", {"Ho fatto accesso — entra", "I signed in — continue"}},
            {"hub.google_email", {"Email Google (Gmail)", "Google email (Gmail)"}},
            {"hub.passkey", {"Crea passkey", "Create passkey"}},
            {"hub.saved_accounts", {"Account salvati", "Saved accounts"}},
            {"hub.play", {"  PLAY  ", "  PLAY  "}},
            {"hub.clear_all", {"Togli tutto", "Clear all"}},
            {"hub.send", {"Invia", "Send"}},
            {"hub.buy", {"Compra", "Buy"}},
            {"hub.equip", {"Equipaggia", "Equip"}},
            {"hub.unequip", {"Disequipaggia", "Unequip"}},
            {"hub.owned", {"Posseduto", "Owned"}},
            {"hub.equipped", {"Equipaggiato", "Equipped"}},
            {"hub.report", {"Segnala giocatore", "Report player"}},
            {"hub.report_send", {"Invia segnalazione", "Send report"}},
            {"hub.hide_chat", {"Nascondi Chat", "Hide Chat"}},
            {"hub.show_chat", {"Chat", "Chat"}},

            // —— Studio toolbar / tools ——
            {"tool.select", {"Seleziona", "Select"}},
            {"tool.move", {"Muovi", "Move"}},
            {"tool.scale", {"Scala", "Scale"}},
            {"tool.rotate", {"Ruota", "Rotate"}},
            {"btn.part", {"+ Part", "+ Part"}},
            {"btn.model", {"+ Model", "+ Model"}},
            {"btn.anima", {"Anima", "Animate"}},
            {"btn.label", {"Label", "Label"}},
            {"btn.textlabel", {"TextLabel", "TextLabel"}},
            {"btn.textbutton", {"TextButton", "TextButton"}},
            {"btn.sound", {"Sound", "Sound"}},
            {"btn.script", {"Script", "Script"}},
            {"btn.avatar", {"Avatar Catalog", "Avatar Catalog"}},
            {"btn.anim_id", {"Anim ID", "Anim ID"}},
            {"btn.create", {"Create mappe/props", "Create maps/props"}},
            {"btn.publish_anim", {"Pubblica animazione...", "Publish animation..."}},
            {"btn.jump_block", {"Blocco che salta", "Jumping block"}},
            {"btn.explode_block", {"Blocco esplosivo", "Exploding block"}},
            {"btn.api_completa", {"API Lua completa (Test)", "Full Lua API (Test)"}},
            {"btn.add_script", {"Aggiungi Script", "Add Script"}},
            {"btn.save_script", {"Salva script", "Save script"}},
            {"btn.remove_script", {"Rimuovi Script", "Remove Script"}},
            {"btn.compile_bbc", {"Compila Safe Bytecode (.bbc)", "Compile Safe Bytecode (.bbc)"}},
            {"hint.scripts_test", {"Gli script partono solo con Test (F5)",
                                   "Scripts run only in Test (F5)"}},

            // —— Studio menu File ——
            {"menu.new_place", {"Nuovo place (vuoto)", "New place (empty)"}},
            {"menu.new_map", {"Nuova mappa...", "New map..."}},
            {"menu.open_place", {"Apri Place...", "Open Place..."}},
            {"menu.save_place", {"Salva Place", "Save Place"}},
            {"menu.save_place_as", {"Salva Place come...", "Save Place as..."}},
            {"menu.save_project", {"Salva Progetto (AppData)", "Save Project (AppData)"}},
            {"menu.export_place", {"Esporta Place...", "Export Place..."}},
            {"menu.publish", {"Pubblica su Blocky Games...", "Publish to Blocky Games..."}},
            {"menu.exit_engine", {"Esci da Blocky Game Engine", "Quit Blocky Game Engine"}},
            {"menu.back_blocky", {"Torna a Blocky", "Back to Blocky"}},
            {"menu.exit", {"Esci", "Quit"}},

            // —— Studio menu Inserisci / Modifica / Player ——
            {"menu.part", {"Part", "Part"}},
            {"menu.model", {"Model...", "Model..."}},
            {"menu.label", {"Label (UI)", "Label (UI)"}},
            {"menu.textlabel", {"TextLabel (UI)", "TextLabel (UI)"}},
            {"menu.textbutton", {"TextButton (UI)", "TextButton (UI)"}},
            {"menu.sound", {"Sound", "Sound"}},
            {"menu.props", {"Props", "Props"}},
            {"menu.tree", {"Albero", "Tree"}},
            {"menu.house", {"Casa", "House"}},
            {"menu.ramp", {"Rampa", "Ramp"}},
            {"menu.tower", {"Torre", "Tower"}},
            {"menu.dup", {"Duplica", "Duplicate"}},
            {"menu.script_part", {"Script sul blocco", "Script on part"}},
            {"menu.delete", {"Elimina", "Delete"}},
            {"menu.avatar_catalog", {"Avatar Catalog...", "Avatar Catalog..."}},
            {"menu.rig_player", {"Rig Player...", "Rig Player..."}},
            {"menu.publish_anim", {"Pubblica animazione (AnimationId)...",
                                   "Publish animation (AnimationId)..."}},
            {"menu.play_idle", {"Play Idle", "Play Idle"}},
            {"menu.play_walk", {"Play Walk", "Play Walk"}},
            {"menu.stop_anim", {"Stop", "Stop"}},
            {"menu.animate_model", {"Anima Model (keyframe)...", "Animate Model (keyframes)..."}},
            {"menu.quick_clips", {"Clip Player (Idle/Walk)", "Player clips (Idle/Walk)"}},

            // —— Vista ——
            {"menu.explorer", {"Explorer", "Explorer"}},
            {"menu.properties", {"Properties", "Properties"}},
            {"menu.create_panel", {"Create (mappe/props)", "Create (maps/props)"}},
            {"menu.global_script", {"Script globale", "Global script"}},
            {"menu.engine_settings", {"Impostazioni engine", "Engine settings"}},
            {"menu.chat", {"Chat", "Chat"}},

            // —— Maps ——
            {"map.baseplate", {"Baseplate", "Baseplate"}},
            {"map.garden", {"Garden", "Garden"}},
            {"map.obby", {"Obby", "Obby"}},
            {"map.arena", {"Arena", "Arena"}},
            {"map.park", {"Park", "Park"}},
            {"map.city", {"City", "City"}},

            // —— Test HUD ——
            {"test.playing", {"TEST PLAY", "TEST PLAY"}},
            {"test.stop", {"Stop Test", "Stop Test"}},
            {"test.hint", {"WASD · Space · Shift Lock · Esc = Stop",
                           "WASD · Space · Shift Lock · Esc = Stop"}},
            {"hud.health", {"Vita", "Health"}},
            {"hud.dead", {"Morti", "Dead"}},
            {"hud.inventory", {"Inventario", "Inventory"}},
            {"hud.empty", {"Vuoto", "Empty"}},
            {"hud.equipped", {"Equipaggiato", "Equipped"}},
            {"hud.unequipped", {"Non equipaggiato", "Unequipped"}},

            // —— Avatar / anim ——
            {"avatar.download_r6", {"1. Scarica modello Player R6 (default)",
                                    "1. Download default Player R6 model"}},
            {"avatar.import_anim", {"2. Importa animazione (FBX / GLB)",
                                    "2. Import animation (FBX / GLB)"}},
            {"avatar.publish_id", {"3. Pubblica → ottieni AnimationId",
                                   "3. Publish → get AnimationId"}},
            {"avatar.copy_id", {"Copia AnimationId", "Copy AnimationId"}},
            {"avatar.copy_play", {"Copia: Play(\"...\")", "Copy: Play(\"...\")"}},
            {"avatar.copy_walk", {"Copia: SetWalkAnim(\"...\")", "Copy: SetWalkAnim(\"...\")"}},
            {"avatar.try_play", {"Prova Play", "Try Play"}},
            {"avatar.web_guide", {"Apri guida web (HTML)", "Open web guide (HTML)"}},
            {"avatar.publish_shop", {"Pubblica nello Shop", "Publish to Shop"}},
            {"avatar.save_editor", {"Salva e torna all'editor", "Save and return to editor"}},
            {"avatar.back_editor", {"Torna all'editor", "Back to editor"}},
            {"avatar.back_menu", {"Torna al menu", "Back to menu"}},
        };
        return t;
    }

    const char* tr(const char* key) const {
        if (!key || !key[0]) return "";
        auto& t = table();
        auto it = t.find(key);
        if (it == t.end()) return key;
        return lang == Lang::En ? it->second.en : it->second.it;
    }

    void setLang(Lang l) {
        if (l < Lang::It || l >= Lang::Count) l = Lang::It;
        lang = l;
        save();
    }

    void cycle() {
        setLang(lang == Lang::It ? Lang::En : Lang::It);
    }

    void load() {
        if (loaded) return;
        loaded = true;
        lang = locDetectOsLang();
        std::ifstream f(locSettingsPath());
        if (!f) return;
        std::string s;
        if (std::getline(f, s))
            lang = locParseLangCode(s);
    }

    void save() const {
#ifdef _WIN32
        // assicura cartella Blocky
        char appdata[MAX_PATH] = {};
        if (GetEnvironmentVariableA("APPDATA", appdata, MAX_PATH) > 0) {
            std::string dir = std::string(appdata) + "\\Blocky";
            CreateDirectoryA(dir.c_str(), nullptr);
        }
#endif
        std::ofstream f(locSettingsPath(), std::ios::trunc);
        if (f) f << locLangCode(lang) << "\n";
    }
};

inline Loc& loc() {
    static Loc L;
    if (!L.loaded) L.load();
    return L;
}

/** Testo tradotto. */
inline const char* L(const char* key) { return loc().tr(key); }

/**
 * Label ImGui con id stabile (non cambia con la lingua).
 * Es: LB("btn.test", "test") → "Test##test" / "Test##test"
 */
inline const char* LB(const char* key, const char* imguiId) {
    static thread_local char buf[192];
    std::snprintf(buf, sizeof(buf), "%s##%s", loc().tr(key), imguiId);
    return buf;
}

/** Combo / bottoni lingua (hub + studio). */
inline bool drawLanguageSelector(const char* id = "lang") {
    loc().load();
    bool changed = false;
    ImGui::PushID(id);
    if (ImGui::BeginCombo("##langcombo", locLangLabel(loc().lang))) {
        if (ImGui::Selectable(L("lang.it"), loc().lang == Lang::It)) {
            loc().setLang(Lang::It);
            changed = true;
        }
        if (ImGui::Selectable(L("lang.en"), loc().lang == Lang::En)) {
            loc().setLang(Lang::En);
            changed = true;
        }
        ImGui::EndCombo();
    }
    ImGui::PopID();
    return changed;
}

inline bool drawLanguageButton(const char* id = "langbtn") {
    loc().load();
    char lab[80];
    // Mostra "Italiano" / "English" (non codice it/en)
    std::snprintf(lab, sizeof(lab), "%s##%s", locLangLabel(loc().lang), id);
    if (ImGui::Button(lab, {0, 0})) {
        loc().cycle();
        return true;
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s: %s", L("lang"), locLangLabel(loc().lang));
    return false;
}
