#pragma once

/**
 * Traduzioni dell'interfaccia (Hub e Studio).
 *
 *   L("chiave")          testo nella lingua scelta
 *   LB("chiave", "id")   lo stesso, con "##id" per ImGui
 *
 * Italiano e inglese sono qui sotto, nel codice. Ogni altra lingua e' un
 * file assets/lang/<codice>.json:
 *
 *   { "_name": "Español", "ok": "Aceptar", "studio.home": "Inicio", ... }
 *
 * Aggiungere una lingua = aggiungere un file, senza ricompilare. Una frase
 * che manca nel file si mostra in inglese (poi in italiano, poi la chiave).
 * All'avvio si sceglie la lingua del sistema, se c'e' il suo file.
 *
 * Limite: ImGui non sa disegnare le scritture da destra a sinistra (arabo,
 * ebraico) ne' quelle che uniscono le lettere (hindi): quelle lingue non
 * sono supportate. Cinese, giapponese e coreano richiedono un font in piu'
 * (ui/UiFonts.h lo carica se la lingua scelta lo chiede).
 */

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "engine/MiniJson.h"
#include "platform/Platform.h"

#ifdef __ANDROID__
#include <sys/system_properties.h>   // lingua del telefono, senza passare da Java
#endif

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

struct LocEntry {
    const char* it;
    const char* en;
};

struct LangInfo {
    std::string code;   // "it", "en", "es", "zh"...
    std::string name;   // nel suo alfabeto: "Español", "中文"
};

inline std::string locSettingsPath() {
    // Prima fuori da Windows era il nome relativo "ui_lang.txt": su Android la
    // cartella di lavoro e' "/", in sola lettura, quindi la lingua scelta non
    // veniva mai salvata e a ogni avvio si ripartiva da capo.
    return plat::dataDir() + "/ui_lang.txt";
}

inline std::string locPacksDir() { return plat::selfDir() + "/assets/lang"; }

inline std::string locNormalizeCode(std::string s) {
    std::string out;
    for (char c : s) {
        if (c == '\r' || c == '\n' || c == ' ' || c == '\t') continue;
        if (c == '_' || c == '-' || c == '.') break;   // "es-ES", "pt_BR" -> "es", "pt"
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        out.push_back(c);
    }
    if (out == "english" || out == "1") return "en";
    if (out == "italiano" || out == "italian" || out == "0") return "it";
    return out;
}

/** Lingua del sistema: "it", "en", "es"... (vuoto se non si sa). */
inline std::string locDetectOsLang() {
#ifdef _WIN32
    wchar_t name[LOCALE_NAME_MAX_LENGTH] = {};
    if (GetUserDefaultLocaleName(name, LOCALE_NAME_MAX_LENGTH) > 0) {
        std::string s;
        for (const wchar_t* p = name; *p && *p != L'-'; ++p) s.push_back((char)*p);
        return locNormalizeCode(s);
    }
#elif defined(__ANDROID__)
    char value[PROP_VALUE_MAX] = {};
    if (__system_property_get("persist.sys.locale", value) <= 0)
        __system_property_get("ro.product.locale", value);
    return locNormalizeCode(value);
#else
    if (const char* l = std::getenv("LANG")) return locNormalizeCode(l);
#endif
    return {};
}

/** Lingue che ImGui disegna male (destra-sinistra, lettere unite): non offerte. */
inline bool locScriptUnsupported(const std::string& code) {
    static const char* bad[] = {"ar", "he", "fa", "ur", "hi", "bn", "th", "ta", "te", nullptr};
    for (int i = 0; bad[i]; ++i) if (code == bad[i]) return true;
    return false;
}

/** Lingue che vogliono il font CJK (ui/UiFonts.h). */
inline bool locNeedsCjkFont(const std::string& code) {
    return code == "zh" || code == "ja" || code == "ko";
}

struct Loc {
    std::string code = "it";
    bool loaded = false;
    std::vector<LangInfo> available;                    // it, en + file trovati
    std::unordered_map<std::string, std::string> pack;  // frasi della lingua scelta (file)

    static const std::unordered_map<std::string, LocEntry>& table() {
        static const std::unordered_map<std::string, LocEntry> t = {
            {"lang", {"Lingua", "Language"}},
            {"lang.it", {"Italiano", "Italian"}},
            {"lang.en", {"English", "English"}},
            {"lang.restart", {"Riavvia Blocky per usare il font di questa lingua.", "Restart Blocky to load the font for this language."}},
            // RIBBON-BEGIN (generato da tools/gen_loc_ribbon.mjs: non modificare a mano)
            {"rb.tab.home", {"Home", "Home"}},
            {"rb.tab.model", {"Modello", "Model"}},
            {"rb.tab.avatar", {"Avatar", "Avatar"}},
            {"rb.tab.ui", {"Interfaccia", "UI"}},
            {"rb.tab.script", {"Script", "Script"}},
            {"rb.tab.test", {"Test", "Test"}},
            {"rb.tab.view", {"Visualizza", "View"}},
            {"rb.g.clipboard", {"Appunti", "Clipboard"}},
            {"rb.g.tools", {"Strumenti", "Tools"}},
            {"rb.g.insert", {"Inserisci", "Insert"}},
            {"rb.g.edit", {"Modifica", "Edit"}},
            {"rb.g.test", {"Test", "Test"}},
            {"rb.g.parts", {"Parti", "Parts"}},
            {"rb.g.arrange", {"Disponi", "Arrange"}},
            {"rb.g.physics", {"Fisica", "Physics"}},
            {"rb.g.world", {"Mondo", "World"}},
            {"rb.g.character", {"Personaggio", "Character"}},
            {"rb.g.anim", {"Animazione", "Animation"}},
            {"rb.g.gui", {"Elementi GUI", "GUI elements"}},
            {"rb.g.media", {"Media", "Media"}},
            {"rb.g.scripts", {"Script", "Scripts"}},
            {"rb.g.debug", {"Debug", "Debug"}},
            {"rb.g.help", {"Aiuto", "Help"}},
            {"rb.g.play", {"Gioca", "Play"}},
            {"rb.g.clients", {"Giocatori", "Clients"}},
            {"rb.g.windows", {"Finestre", "Windows"}},
            {"rb.g.show", {"Mostra", "Show"}},
            {"rb.g.camera", {"Camera", "Camera"}},
            {"rb.g.tools2", {"Strumenti", "Tools"}},
            {"rb.paste", {"Incolla", "Paste"}},
            {"rb.copy", {"Copia", "Copy"}},
            {"rb.cut", {"Taglia", "Cut"}},
            {"rb.duplicate", {"Duplica", "Duplicate"}},
            {"rb.select", {"Seleziona", "Select"}},
            {"rb.move", {"Sposta", "Move"}},
            {"rb.scale", {"Scala", "Scale"}},
            {"rb.rotate", {"Ruota", "Rotate"}},
            {"rb.part", {"Part", "Part"}},
            {"rb.block", {"Blocco", "Block"}},
            {"rb.ball", {"Sfera", "Sphere"}},
            {"rb.cylinder", {"Cilindro", "Cylinder"}},
            {"rb.wedge", {"Cuneo", "Wedge"}},
            {"rb.neon", {"Part al neon", "Neon part"}},
            {"rb.light", {"Part con luce", "Light part"}},
            {"rb.explode", {"Part che esplode", "Exploding part"}},
            {"rb.import", {"Importa", "Import"}},
            {"rb.toolbox", {"Toolbox", "Toolbox"}},
            {"rb.object", {"Oggetto...", "Object..."}},
            {"rb.newscript", {"Nuovo script", "New script"}},
            {"rb.group", {"Raggruppa", "Group"}},
            {"rb.ungroup", {"Separa", "Ungroup"}},
            {"rb.gm", {"Raggruppa come Model", "Group as Model"}},
            {"rb.lock", {"Blocca", "Lock"}},
            {"rb.anchor", {"Ancora", "Anchor"}},
            {"rb.unanchor", {"Sgancia", "Unanchor"}},
            {"rb.paint", {"Colora", "Paint"}},
            {"rb.play", {"Play", "Play"}},
            {"rb.stop", {"Stop", "Stop"}},
            {"rb.playhere", {"Play qui", "Play here"}},
            {"rb.multi", {"Più giocatori", "Multiplayer"}},
            {"rb.color", {"Colori", "Colors"}},
            {"rb.material", {"Materiali", "Materials"}},
            {"rb.align", {"Allinea", "Align"}},
            {"rb.al.min", {"minimo", "min"}},
            {"rb.al.center", {"centro", "center"}},
            {"rb.al.max", {"massimo", "max"}},
            {"rb.al.dist", {"Distribuisci", "Distribute"}},
            {"rb.transform", {"Trasforma", "Transform"}},
            {"rb.tr.rot90y", {"Ruota +90° Y", "Rotate +90° Y"}},
            {"rb.tr.rotm90y", {"Ruota -90° Y", "Rotate -90° Y"}},
            {"rb.tr.rot90x", {"Ruota +90° X", "Rotate +90° X"}},
            {"rb.tr.rot90z", {"Ruota +90° Z", "Rotate +90° Z"}},
            {"rb.tr.mirror", {"Specchia", "Mirror"}},
            {"rb.tr.scale2", {"Raddoppia la misura", "Double the size"}},
            {"rb.tr.scale05", {"Dimezza la misura", "Halve the size"}},
            {"rb.tr.matchsize", {"Stessa misura della prima", "Match size to first"}},
            {"rb.tr.matchcolor", {"Stesso colore della prima", "Match color to first"}},
            {"rb.tr.typein", {"Valori esatti...", "Exact values..."}},
            {"rb.collide", {"Collisione", "Collide"}},
            {"rb.nocollide", {"Senza collisione", "No collision"}},
            {"rb.colgroups", {"Gruppi collisione", "Collision groups"}},
            {"rb.constraints", {"Vincoli", "Constraints"}},
            {"rb.c.weld", {"Saldatura (Weld)", "Weld"}},
            {"rb.c.hinge", {"Cerniera (Hinge)", "Hinge"}},
            {"rb.c.rope", {"Corda (Rope)", "Rope"}},
            {"rb.c.spring", {"Molla (Spring)", "Spring"}},
            {"rb.c.rod", {"Asta (Rod)", "Rod"}},
            {"rb.c.join", {"Unisci superfici", "Join surfaces"}},
            {"rb.c.list", {"Elenco vincoli", "Constraint list"}},
            {"rb.terrain", {"Terreno", "Terrain"}},
            {"rb.avatar", {"Avatar", "Avatar"}},
            {"rb.rig", {"Rig", "Rig"}},
            {"rb.friend", {"NPC amico", "Friend NPC"}},
            {"rb.animstudio", {"Animation Studio", "Animation Studio"}},
            {"rb.animate", {"Anima", "Animate"}},
            {"rb.animator", {"Animatore", "Animator"}},
            {"rb.publishanim", {"Pubblica animazione", "Publish animation"}},
            {"rb.textlabel", {"Etichetta", "TextLabel"}},
            {"rb.textbutton", {"Pulsante", "TextButton"}},
            {"rb.frame", {"Riquadro", "Frame"}},
            {"rb.image", {"Immagine", "ImageLabel"}},
            {"rb.screengui", {"Schermo", "ScreenGui"}},
            {"rb.scroll", {"Scorrimento", "ScrollingFrame"}},
            {"rb.textbox", {"Casella", "TextBox"}},
            {"rb.imagebtn", {"Immagine clic", "ImageButton"}},
            {"rb.uilist", {"Lista", "UIListLayout"}},
            {"rb.uigrid", {"Griglia UI", "UIGridLayout"}},
            {"rb.uipad", {"Margine", "UIPadding"}},
            {"rb.uicorner", {"Angoli", "UICorner"}},
            {"rb.uistroke", {"Bordo", "UIStroke"}},
            {"rb.billboard", {"Cartello", "BillboardGui"}},
            {"rb.surface", {"Sulla faccia", "SurfaceGui"}},
            {"rb.uimore", {"Altri elementi", "More elements"}},
            {"rb.g.layout", {"Disponi GUI", "UI layout"}},
            {"rb.ui.left", {"Sinistra", "Left"}},
            {"rb.ui.cx", {"Centro X", "Center X"}},
            {"rb.ui.right", {"Destra", "Right"}},
            {"rb.ui.top", {"Alto", "Top"}},
            {"rb.ui.cy", {"Centro Y", "Center Y"}},
            {"rb.ui.bottom", {"Basso", "Bottom"}},
            {"rb.ui.front", {"Avanti", "Bring forward"}},
            {"rb.ui.back", {"Dietro", "Send back"}},
            {"rb.ui.dup", {"Duplica GUI", "Duplicate GUI"}},
            {"rb.label3d", {"Etichetta 3D", "3D label"}},
            {"rb.sound", {"Suono", "Sound"}},
            {"rb.placescript", {"Script del place", "Place script"}},
            {"rb.output", {"Output", "Output"}},
            {"rb.analysis", {"Analisi", "Analysis"}},
            {"rb.cmdbar", {"Comandi", "Command bar"}},
            {"rb.api", {"API", "API"}},
            {"rb.guide", {"Guida Luau", "Luau guide"}},
            {"rb.pause", {"Pausa", "Pause"}},
            {"rb.resume", {"Riprendi", "Resume"}},
            {"rb.closeplayers", {"Chiudi prove", "Close clients"}},
            {"rb.console", {"Console", "Console"}},
            {"rb.perf", {"Prestazioni", "Performance"}},
            {"rb.watch", {"Osserva", "Watch"}},
            {"rb.audioon", {"Audio sì", "Audio on"}},
            {"rb.audiooff", {"Audio no", "Audio off"}},
            {"rb.explorer", {"Esplora", "Explorer"}},
            {"rb.properties", {"Proprietà", "Properties"}},
            {"rb.grid", {"Griglia", "Grid"}},
            {"rb.wireframe", {"Wireframe", "Wireframe"}},
            {"rb.collisions", {"Collisioni", "Collisions"}},
            {"rb.focus", {"Inquadra", "Focus"}},
            {"rb.isolate", {"Isola", "Isolate"}},
            {"rb.search", {"Cerca", "Search"}},
            {"rb.morewin", {"Altre finestre", "More windows"}},
            {"rb.versions", {"Cronologia versioni", "Version history"}},
            {"rb.profiler", {"Profiler", "Profiler"}},
            {"rb.settings", {"Impostazioni del gioco", "Game settings"}},
            {"rb.attributes", {"Attributi", "Attributes"}},
            {"rb.quick", {"Azioni rapide", "Quick actions"}},
            {"rb.findreplace", {"Trova e sostituisci", "Find and replace"}},
            {"rb.spawn", {"Punto di spawn", "Spawn"}},
            {"rb.scriptmenu", {"Script", "Script"}},
            {"rb.uimenu", {"Interfaccia", "UI"}},
            {"rb.sc.server", {"Script (server)", "Script (server)"}},
            {"rb.sc.local", {"LocalScript (giocatore)", "LocalScript (client)"}},
            {"rb.sc.module", {"ModuleScript", "ModuleScript"}},
            {"rb.sc.needsel", {"Seleziona prima una Part", "Select a Part first"}},
            {"rb.templates", {"Modelli di script", "Script templates"}},
            {"rb.q.save", {"Salva (Ctrl+S)", "Save (Ctrl+S)"}},
            {"rb.q.undo", {"Annulla (Ctrl+Z)", "Undo (Ctrl+Z)"}},
            {"rb.q.redo", {"Ripeti (Ctrl+Y)", "Redo (Ctrl+Y)"}},
            {"rb.g.settings", {"Impostazioni", "Settings"}},
            {"rb.pivot", {"Strumenti pivot", "Pivot tools"}},
            {"rb.snap.on", {"Snap", "Snap"}},
            {"rb.snap.move", {"Sposta %.3f stud", "Move %.3f studs"}},
            {"rb.snap.rot", {"Ruota %.1f°", "Rotate %.1f°"}},
            {"rb.snap.fine", {"Passo di spostamento e scala", "Move and scale step"}},
            {"rb.g.snap", {"Griglia", "Snap"}},
            {"rb.g.advanced", {"Avanzate", "Advanced"}},
            {"rb.device", {"Dispositivo", "Device"}},
            {"rb.screenshot", {"Screenshot", "Screenshot"}},
            {"rb.fullscreen", {"Schermo intero", "Full screen"}},
            {"rb.g.screen", {"Schermo", "Screen"}},
            {"rb.tags", {"Tag", "Tags"}},
            {"rb.stats", {"Statistiche", "Stats"}},
            {"rb.hotkeys", {"Scorciatoie", "Shortcuts"}},
            {"rb.gamemenu", {"Gioco", "Game"}},
            {"rb.teams", {"Squadre", "Teams"}},
            {"rb.leaderstats", {"Classifica (leaderstats)", "Leaderstats"}},
            {"rb.spawns", {"Punti di spawn", "Spawn points"}},
            {"rb.badges", {"Medaglie", "Badges"}},
            {"rb.datastore", {"DataStore (salvataggi)", "DataStore"}},
            {"rb.remotes", {"Remote (eventi rete)", "Remotes"}},
            {"rb.http", {"HttpService", "HttpService"}},
            {"rb.placesettings", {"Impostazioni del posto", "Place settings"}},
            {"rb.humdefaults", {"Humanoid predefinito", "Humanoid defaults"}},
            {"rb.prompts", {"ProximityPrompt", "Proximity prompts"}},
            {"rb.ctxactions", {"Azioni contestuali", "Context actions"}},
            {"rb.selmenu", {"Seleziona", "Select"}},
            {"rb.sel.all", {"Tutte le parti", "All parts"}},
            {"rb.sel.invert", {"Inverti selezione", "Invert selection"}},
            {"rb.sel.color", {"Stesso colore", "Same color"}},
            {"rb.sel.material", {"Stesso materiale", "Same material"}},
            {"rb.sel.size", {"Stessa misura", "Same size"}},
            {"rb.sel.unanchored", {"Non ancorate", "Unanchored"}},
            {"rb.sel.unlocked", {"Non bloccate", "Unlocked"}},
            {"rb.findpart", {"Trova part", "Find part"}},
            {"rb.selsets", {"Gruppi di selezione", "Selection sets"}},
            {"rb.selhistory", {"Cronologia selezione", "Selection history"}},
            {"rb.measure", {"Metro", "Measure"}},
            {"rb.toolsmenu", {"Strumenti", "Tools"}},
            {"rb.batchrename", {"Rinomina in blocco", "Batch rename"}},
            {"rb.copyprops", {"Copia/incolla proprietà", "Copy/paste properties"}},
            {"rb.batchtools", {"Modifica in blocco", "Batch edit"}},
            {"rb.physbatch", {"Fisica in blocco", "Batch physics"}},
            {"rb.bulkscripts", {"Script in blocco", "Bulk scripts"}},
            {"rb.prefabs", {"Prefabbricati", "Prefabs"}},
            {"rb.generators", {"Generatori", "Generators"}},
            {"rb.randcolors", {"Colori casuali", "Random colors"}},
            {"rb.spawnhere", {"Spawn su questa part", "Spawn on this part"}},
            {"rb.g.selection", {"Selezione", "Selection"}},
            {"rb.lighting", {"Luce", "Lighting"}},
            {"rb.clock", {"Ora del giorno", "Time of day"}},
            {"rb.lightpresets", {"Preset di luce", "Lighting presets"}},
            {"rb.atmosphere", {"Atmosfera", "Atmosphere"}},
            {"rb.decal", {"Decal", "Decal"}},
            {"rb.highlight", {"Highlight", "Highlight"}},
            {"rb.selbox", {"SelectionBox", "SelectionBox"}},
            {"rb.dragdet", {"DragDetector", "DragDetector"}},
            {"rb.rigeditor", {"Editor del rig", "Rig editor"}},
            {"rb.animids", {"ID animazioni", "Animation IDs"}},
            {"rb.debugmenu", {"Diagnostica", "Diagnostics"}},
            {"rb.coldebug", {"Collisioni (debug)", "Collision debug"}},
            {"rb.pathdebug", {"Percorsi (pathfinding)", "Pathfinding debug"}},
            {"rb.physpanel", {"Pannello fisica", "Physics panel"}},
            {"rb.perfoverlay", {"FPS sullo schermo", "Performance overlay"}},
            {"rb.validator", {"Controlla il posto", "Validate place"}},
            {"rb.views", {"Viste", "Views"}},
            {"rb.v.top", {"Dall'alto", "Top"}},
            {"rb.v.front", {"Davanti", "Front"}},
            {"rb.v.back", {"Dietro", "Back"}},
            {"rb.v.left", {"Sinistra", "Left"}},
            {"rb.v.right", {"Destra", "Right"}},
            {"rb.v.iso", {"Isometrica", "Isometric"}},
            {"rb.zoomext", {"Inquadra tutto", "Zoom to extents"}},
            {"rb.cambookmarks", {"Segnalibri camera", "Camera bookmarks"}},
            {"rb.campanel", {"Impostazioni camera", "Camera settings"}},
            {"rb.fov", {"Campo visivo", "Field of view"}},
            {"rb.sounds", {"Suoni", "Sounds"}},
            {"rb.assets", {"Risorse", "Assets"}},
            {"rb.partcounter", {"Conta parti", "Part counter"}},
            {"rb.plugins", {"Barra plugin", "Plugin bar"}},
            {"rb.prefs", {"Preferenze dello Studio", "Studio preferences"}},
            {"rb.space.world", {"Mondo", "World"}},
            {"rb.space.local", {"Locale", "Local"}},
            {"rb.space.tip", {"Assi del gizmo: del mondo o della part (Ctrl+L)", "Gizmo axes: world or part (Ctrl+L)"}},
            {"rb.publish", {"Pubblica", "Publish"}},
            {"rb.collide.drag", {"Collisioni", "Collisions"}},
            {"rb.collide.tip", {"Spostando o scalando una part non entra dentro le altre", "Moved or scaled parts can't go inside other parts"}},
            {"rb.csg.union", {"Unione", "Union"}},
            {"rb.csg.negate", {"Nega", "Negate"}},
            {"rb.csg.intersect", {"Interseca", "Intersect"}},
            {"rb.csg.separate", {"Separa", "Separate"}},
            {"rb.g.solids", {"Solidi", "Solid Modeling"}},
            {"rb.c.tool", {"Clic su due punti", "Click two points"}},
            {"rb.c.fromsel", {"Tra le part selezionate", "Between selected parts"}},
            {"rb.theme.light", {"Tema chiaro", "Light theme"}},
            {"rb.theme.dark", {"Tema scuro", "Dark theme"}},
            // RIBBON-END
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

            {"menu.explorer", {"Esploratore", "Explorer"}},
            {"menu.properties", {"Proprietà", "Properties"}},
            {"menu.create_panel", {"Create (mappe/props)", "Create (maps/props)"}},
            {"menu.global_script", {"Script globale", "Global script"}},
            {"menu.engine_settings", {"Impostazioni engine", "Engine settings"}},
            {"menu.chat", {"Chat", "Chat"}},

            {"weak.title", {"Avviso prestazioni", "Performance notice"}},
            {"weak.body", {"Questo dispositivo potrebbe non essere abbastanza potente per far "
                           "girare i giochi in modo fluido. Potresti notare scatti o cali di FPS.",
                           "This device may not be powerful enough to run games smoothly. "
                           "You may notice stutter or low FPS."}},
            {"weak.tip", {"Consiglio: usa il preset grafico Low dal menu di gioco e chiudi le altre app.",
                          "Tip: use the Low graphics preset from the game menu and close other apps."}},
            {"weak.ok", {"Ho capito", "Got it"}},
            {"weak.low", {"Imposta grafica Low", "Set Low graphics"}},
            {"weak.dontshow", {"Non mostrare piu'", "Don't show again"}},

            {"map.baseplate", {"Baseplate", "Baseplate"}},
            {"map.garden", {"Garden", "Garden"}},
            {"map.obby", {"Obby", "Obby"}},
            {"map.arena", {"Arena", "Arena"}},
            {"map.park", {"Park", "Park"}},
            {"map.city", {"City", "City"}},

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
        if (!pack.empty()) {
            auto p = pack.find(key);
            if (p != pack.end()) return p->second.c_str();
        }
        auto& t = table();
        auto it = t.find(key);
        if (it == t.end()) return key;
        // Ogni lingua che non ha la frase la mostra in inglese, tranne l'italiano.
        return code == "it" ? it->second.it : it->second.en;
    }

    bool has(const std::string& c) const {
        for (const auto& l : available) if (l.code == c) return true;
        return false;
    }

    const char* name(const std::string& c) const {
        for (const auto& l : available) if (l.code == c) return l.name.c_str();
        return c.c_str();
    }

    void scan() {
        available.clear();
        available.push_back({"it", "Italiano"});
        available.push_back({"en", "English"});
        std::error_code ec;
        for (const auto& e : std::filesystem::directory_iterator(locPacksDir(), ec)) {
            if (e.path().extension() != ".json") continue;
            const std::string c = locNormalizeCode(e.path().stem().string());
            if (c.empty() || c == "it" || c == "en" || locScriptUnsupported(c) || has(c)) continue;
            std::string label = c;
            std::ifstream in(e.path(), std::ios::binary);
            std::stringstream ss;
            ss << in.rdbuf();
            blk::json::Value v;
            if (!blk::json::parse(ss.str(), v) || !v.isObject()) continue;
            if (const auto* n = v.get("_name"); n && n->type == blk::json::Value::Type::String) label = n->str;
            available.push_back({c, label});
        }
        std::sort(available.begin() + 2, available.end(),
                  [](const LangInfo& a, const LangInfo& b) { return a.name < b.name; });
    }

    bool loadPack(const std::string& c) {
        pack.clear();
        if (c == "it" || c == "en") return true;
        std::ifstream in(locPacksDir() + "/" + c + ".json", std::ios::binary);
        if (!in) return false;
        std::stringstream ss;
        ss << in.rdbuf();
        blk::json::Value v;
        if (!blk::json::parse(ss.str(), v) || !v.isObject()) return false;
        for (const auto& kv : v.obj)
            if (!kv.first.empty() && kv.first[0] != '_' && kv.second.type == blk::json::Value::Type::String)
                pack[kv.first] = kv.second.str;
        return true;
    }

    void setLang(const std::string& c) {
        const std::string n = locNormalizeCode(c);
        code = has(n) ? n : "it";
        if (!loadPack(code)) code = "it";
        save();
    }

    void cycle() {
        if (available.empty()) return;
        size_t i = 0;
        while (i < available.size() && available[i].code != code) ++i;
        setLang(available[(i + 1) % available.size()].code);
    }

    void load() {
        if (loaded) return;
        loaded = true;
        scan();
        std::string want;
        std::ifstream f(locSettingsPath());
        if (f) std::getline(f, want);
        want = locNormalizeCode(want);
        if (want.empty() || !has(want)) {
            const std::string os = locDetectOsLang();
            want = has(os) ? os : (os == "it" || os.empty() ? "it" : "en");
        }
        code = want;
        if (!loadPack(code)) code = "it";
    }

    void save() const {
        plat::makeDirs(plat::dataDir());
        std::ofstream f(locSettingsPath(), std::ios::trunc);
        if (f) f << code << "\n";
    }
};

inline Loc& loc() {
    static Loc L;
    if (!L.loaded) L.load();
    return L;
}

inline const char* L(const char* key) { return loc().tr(key); }

inline const char* LB(const char* key, const char* imguiId) {
    static thread_local char buf[192];
    std::snprintf(buf, sizeof(buf), "%s##%s", loc().tr(key), imguiId);
    return buf;
}

/** Nome della lingua scelta, nel suo alfabeto. */
inline const char* locCurrentName() { return loc().name(loc().code); }

/** Elenco di tutte le lingue. Vero se e' cambiata. */
inline bool drawLanguageSelector(const char* id = "lang") {
    loc().load();
    bool changed = false;
    ImGui::PushID(id);
    if (ImGui::BeginCombo("##langcombo", locCurrentName())) {
        for (const auto& l : loc().available) {
            if (ImGui::Selectable(l.name.c_str(), loc().code == l.code)) {
                const bool fontChange = locNeedsCjkFont(l.code) != locNeedsCjkFont(loc().code);
                loc().setLang(l.code);
                changed = true;
                if (fontChange) ImGui::OpenPopup("##riavvia");
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::BeginPopup("##riavvia")) {
        ImGui::TextUnformatted(L("lang.restart"));
        ImGui::EndPopup();
    }
    ImGui::PopID();
    return changed;
}

/** Pulsante compatto: mostra la lingua, apre l'elenco. */
inline bool drawLanguageButton(const char* id = "langbtn") {
    loc().load();
    char lab[96];
    std::snprintf(lab, sizeof(lab), "%s##%s", locCurrentName(), id);
    if (ImGui::Button(lab, {0, 0})) ImGui::OpenPopup(id);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", L("lang"));
    bool changed = false;
    if (ImGui::BeginPopup(id)) {
        for (const auto& l : loc().available) {
            if (ImGui::Selectable(l.name.c_str(), loc().code == l.code)) {
                loc().setLang(l.code);
                changed = true;
            }
        }
        ImGui::EndPopup();
    }
    return changed;
}
