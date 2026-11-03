// Toolbox utente stile Roblox Studio: pubblica Part / Model / NPC e
// gestiscili (elenco + elimina). Persistenza in blockyDataDir()/toolbox/.
// Compila solo se incluso da main.cpp dopo PartPrefabs / WorldEditingTools.

#pragma once

#include <cctype>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

enum class ToolboxKind : int { Part = 0, Prefab = 1, Model = 2, Npc = 3 };

struct UserToolboxItem {
    std::string id;
    std::string name;
    ToolboxKind kind = ToolboxKind::Part;
    std::string file; // path relativo sotto toolbox/
};

static std::vector<UserToolboxItem> g_userToolbox;
static bool g_userToolboxLoaded = false;
static bool g_showPublishTool = false;
static bool g_showDeleteTool = false;
static char g_toolboxPublishName[96] = {};
static int g_toolboxPublishKind = 0; // 0 Part 1 Prefab(multi) 2 Model 3 NPC
static std::string g_toolboxStatus;

static const char* toolboxKindName(ToolboxKind k) {
    switch (k) {
    case ToolboxKind::Part: return "Part";
    case ToolboxKind::Prefab: return "Prefab";
    case ToolboxKind::Model: return "Model";
    case ToolboxKind::Npc: return "NPC";
    }
    return "Tool";
}

static ToolboxKind toolboxKindFromStr(const std::string& s) {
    if (s == "prefab") return ToolboxKind::Prefab;
    if (s == "model") return ToolboxKind::Model;
    if (s == "npc") return ToolboxKind::Npc;
    return ToolboxKind::Part;
}

static const char* toolboxKindToStr(ToolboxKind k) {
    switch (k) {
    case ToolboxKind::Prefab: return "prefab";
    case ToolboxKind::Model: return "model";
    case ToolboxKind::Npc: return "npc";
    default: return "part";
    }
}

static std::string toolboxRootDir() {
    std::string d = blockyDataDir() + "/toolbox";
    plat::makeDirs(d);
    plat::makeDirs(d + "/items");
    plat::makeDirs(d + "/meshes");
    return d;
}

static std::string toolboxManifestPath() {
    return toolboxRootDir() + "/manifest.json";
}

static std::string toolboxSlug(const std::string& name) {
    std::string s;
    for (unsigned char c : name) {
        if (std::isalnum(c)) s.push_back((char)std::tolower(c));
        else if (c == ' ' || c == '_' || c == '-') s.push_back('_');
    }
    if (s.empty()) s = "tool";
    if (s.size() > 40) s.resize(40);
    auto now = std::chrono::system_clock::now().time_since_epoch();
    long long t = std::chrono::duration_cast<std::chrono::seconds>(now).count();
    char buf[96];
    std::snprintf(buf, sizeof(buf), "%s_%lld", s.c_str(), t % 100000000LL);
    return buf;
}

static bool toolboxWriteFile(const std::string& path, const std::string& content) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f << content;
    return (bool)f;
}

static std::string toolboxEscape(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 8);
    for (unsigned char c : s) {
        if (c == '\\') o += "\\\\";
        else if (c == '"') o += "\\\"";
        else if (c == '\n') o += "\\n";
        else if (c == '\r') continue;
        else o.push_back((char)c);
    }
    return o;
}

static void saveUserToolboxManifest() {
    std::ostringstream j;
    j << "{\"format\":\"BlockyToolbox\",\"version\":1,\"items\":[\n";
    for (size_t i = 0; i < g_userToolbox.size(); ++i) {
        const auto& it = g_userToolbox[i];
        j << "  {\"id\":\"" << toolboxEscape(it.id)
          << "\",\"name\":\"" << toolboxEscape(it.name)
          << "\",\"kind\":\"" << toolboxKindToStr(it.kind)
          << "\",\"file\":\"" << toolboxEscape(it.file) << "\"}";
        if (i + 1 < g_userToolbox.size()) j << ",";
        j << "\n";
    }
    j << "]}\n";
    toolboxWriteFile(toolboxManifestPath(), j.str());
}

static void loadUserToolbox() {
    g_userToolbox.clear();
    g_userToolboxLoaded = true;
    std::string json = readTextFile(toolboxManifestPath());
    if (json.empty()) return;
    size_t arr = json.find('[');
    if (arr == std::string::npos) return;
    size_t pos = arr;
    while ((pos = json.find('{', pos)) != std::string::npos) {
        size_t end = json.find('}', pos);
        if (end == std::string::npos) break;
        UserToolboxItem it;
        it.id = jsonFindStr(json, "id", pos);
        it.name = jsonFindStr(json, "name", pos);
        it.kind = toolboxKindFromStr(jsonFindStr(json, "kind", pos));
        it.file = jsonFindStr(json, "file", pos);
        if (!it.id.empty() && !it.name.empty())
            g_userToolbox.push_back(std::move(it));
        pos = end + 1;
    }
}

static void ensureUserToolbox() {
    if (!g_userToolboxLoaded) loadUserToolbox();
}

static std::string serializePartBlob(const WorkspacePart& p, const glm::vec3& origin) {
    std::ostringstream j;
    j << std::fixed << std::setprecision(3);
    glm::vec3 off = p.position - origin;
    j << "{\"name\":\"" << toolboxEscape(p.name)
      << "\",\"offset\":[" << off.x << "," << off.y << "," << off.z
      << "],\"size\":[" << p.size.x << "," << p.size.y << "," << p.size.z
      << "],\"rot\":[" << p.rotation.x << "," << p.rotation.y << "," << p.rotation.z
      << "],\"color\":[" << p.color.r << "," << p.color.g << "," << p.color.b << "]";
    partprops::writeAll(j, p);
    j << ",\"shape\":\"" << partShapeName(p.shape) << "\""
      << ",\"material\":\"" << partMaterialName(p.material) << "\""
      << ",\"emissive\":" << p.emissive
      << ",\"pointLight\":" << (p.pointLight ? "true" : "false")
      << ",\"lightRange\":" << p.lightRange
      << ",\"lightBrightness\":" << p.lightBrightness
      << ",\"script\":\"" << toolboxEscape(p.script) << "\""
      << ",\"scriptType\":" << (int)p.scriptType << "}";
    return j.str();
}

static bool parseToolboxPartAt(const std::string& json, size_t pos, size_t end,
                               WorkspacePart& out, glm::vec3& offset) {
    out = WorkspacePart{};
    out.name = jsonFindStr(json, "name", pos);
    if (out.name.empty()) out.name = "Part";
    if (!jsonFindVec3(json, "offset", pos, offset))
        offset = {0, 0, 0};
    jsonFindVec3(json, "size", pos, out.size);
    jsonFindVec3(json, "rot", pos, out.rotation);
    jsonFindVec3(json, "color", pos, out.color);
    partprops::readAll(json, pos, end, out);
    {
        std::string sh = jsonFindStr(json, "shape", pos);
        if (!sh.empty()) out.shape = partShapeFromName(sh);
    }
    {
        std::string mat = jsonFindStr(json, "material", pos);
        if (!mat.empty()) out.setMaterial(partMaterialFromName(mat));
    }
    jsonFindFloat(json, "emissive", pos, out.emissive);
    out.pointLight = jsonFindBool(json, "pointLight", pos, false);
    jsonFindFloat(json, "lightRange", pos, out.lightRange);
    jsonFindFloat(json, "lightBrightness", pos, out.lightBrightness);
    out.script = jsonFindEscapedStr(json, "script", pos);
    int st = jsonFindInt(json, "scriptType", pos);
    out.scriptType = (st == 1) ? ScriptType::Server
                    : (st == 2) ? ScriptType::Client
                    : (st == 3) ? ScriptType::ModuleScript
                    : ScriptType::Legacy;
    return true;
}

static bool publishToolboxParts(const std::string& name, const std::vector<int>& indices,
                                bool asPrefab) {
    if (indices.empty()) return false;
    ensureUserToolbox();
    std::string id = toolboxSlug(name);
    std::string rel = "items/" + id + ".json";
    std::string path = toolboxRootDir() + "/" + rel;

    glm::vec3 origin = g_world.parts[indices[0]].position;
    std::ostringstream j;
    j << std::fixed << std::setprecision(3);
    j << "{\"format\":\"BlockyToolboxItem\",\"version\":1"
      << ",\"kind\":\"" << (asPrefab ? "prefab" : "part") << "\""
      << ",\"name\":\"" << toolboxEscape(name) << "\""
      << ",\"parts\":[\n";
    for (size_t i = 0; i < indices.size(); ++i) {
        int idx = indices[i];
        if (idx < 0 || idx >= (int)g_world.parts.size()) continue;
        j << "  " << serializePartBlob(g_world.parts[idx], origin);
        if (i + 1 < indices.size()) j << ",";
        j << "\n";
    }
    j << "]}\n";
    if (!toolboxWriteFile(path, j.str())) return false;

    UserToolboxItem it;
    it.id = id;
    it.name = name;
    it.kind = asPrefab ? ToolboxKind::Prefab : ToolboxKind::Part;
    it.file = rel;
    g_userToolbox.push_back(std::move(it));
    saveUserToolboxManifest();
    return true;
}

static bool publishToolboxModel(const std::string& name, int modelIdx) {
    if (modelIdx < 0 || modelIdx >= (int)g_world.models.size()) return false;
    ensureUserToolbox();
    const auto& m = g_world.models[modelIdx];
    if (m.sourcePath.empty()) return false;

    std::string id = toolboxSlug(name);
    std::string ext = ".glb";
    auto dot = m.sourcePath.find_last_of('.');
    if (dot != std::string::npos)
        ext = m.sourcePath.substr(dot);

    std::string meshRel = "meshes/" + id + ext;
    std::string meshAbs = toolboxRootDir() + "/" + meshRel;
    std::error_code ec;
    fs::copy_file(m.sourcePath, meshAbs, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        // Se la copia fallisce, salva comunque il path originale.
        meshRel = m.sourcePath;
        meshAbs = m.sourcePath;
    }

    std::string itemRel = "items/" + id + ".json";
    std::ostringstream j;
    j << std::fixed << std::setprecision(3);
    j << "{\"format\":\"BlockyToolboxItem\",\"version\":1"
      << ",\"kind\":\"model\",\"name\":\"" << toolboxEscape(name) << "\""
      << ",\"sourcePath\":\"" << toolboxEscape(meshAbs) << "\""
      << ",\"scale\":[" << m.scale.x << "," << m.scale.y << "," << m.scale.z << "]"
      << ",\"rotation\":[" << m.rotation.x << "," << m.rotation.y << "," << m.rotation.z << "]"
      << ",\"tint\":[" << m.tint.x << "," << m.tint.y << "," << m.tint.z << "]"
      << ",\"canCollide\":" << (m.canCollide ? "true" : "false")
      << ",\"script\":\"" << toolboxEscape(m.script) << "\""
      << "}\n";
    if (!toolboxWriteFile(toolboxRootDir() + "/" + itemRel, j.str()))
        return false;

    UserToolboxItem it;
    it.id = id;
    it.name = name;
    it.kind = ToolboxKind::Model;
    it.file = itemRel;
    g_userToolbox.push_back(std::move(it));
    saveUserToolboxManifest();
    return true;
}

static bool publishToolboxNpc(const std::string& name) {
    ensureUserToolbox();
    std::string id = toolboxSlug(name);
    std::string rel = "items/" + id + ".json";
    std::ostringstream j;
    j << "{\"format\":\"BlockyToolboxItem\",\"version\":1"
      << ",\"kind\":\"npc\",\"name\":\"" << toolboxEscape(name) << "\"}\n";
    if (!toolboxWriteFile(toolboxRootDir() + "/" + rel, j.str()))
        return false;
    UserToolboxItem it;
    it.id = id;
    it.name = name;
    it.kind = ToolboxKind::Npc;
    it.file = rel;
    g_userToolbox.push_back(std::move(it));
    saveUserToolboxManifest();
    return true;
}

static bool deleteUserToolboxItem(int index) {
    ensureUserToolbox();
    if (index < 0 || index >= (int)g_userToolbox.size()) return false;
    UserToolboxItem it = g_userToolbox[index];
    std::error_code ec;
    fs::remove(toolboxRootDir() + "/" + it.file, ec);
    // Se e' un model, prova a togliere anche la mesh copiata.
    std::string itemJson = readTextFile(toolboxRootDir() + "/" + it.file);
    (void)itemJson;
    if (it.kind == ToolboxKind::Model) {
        std::string json = readTextFile(toolboxRootDir() + "/" + it.file);
        // file gia' rimosso: cerca mesh sotto meshes/id.*
        for (auto& e : fs::directory_iterator(toolboxRootDir() + "/meshes", ec)) {
            if (!e.is_regular_file()) continue;
            std::string fn = e.path().filename().string();
            if (fn.rfind(it.id, 0) == 0)
                fs::remove(e.path(), ec);
        }
    }
    g_userToolbox.erase(g_userToolbox.begin() + index);
    saveUserToolboxManifest();
    return true;
}

static void insertUserToolboxPartItem(const std::string& json) {
    size_t arr = json.find("\"parts\"");
    if (arr == std::string::npos) arr = json.find('[');
    else arr = json.find('[', arr);
    if (arr == std::string::npos) return;
    int depth = 0;
    size_t arrEnd = arr;
    for (size_t i = arr; i < json.size(); ++i) {
        if (json[i] == '[') ++depth;
        else if (json[i] == ']') {
            --depth;
            if (depth == 0) { arrEnd = i; break; }
        }
    }
    glm::vec3 base = insertPositionInFront();
    int firstIdx = -1;
    size_t pos = arr;
    while ((pos = json.find('{', pos)) != std::string::npos && pos < arrEnd) {
        size_t end = json.find('}', pos);
        if (end == std::string::npos || end > arrEnd) break;
        WorkspacePart p;
        glm::vec3 off{0};
        parseToolboxPartAt(json, pos, end, p, off);
        p.position = base + off;
        g_world.parts.push_back(p);
        int idx = (int)g_world.parts.size() - 1;
        g_undo.recordInsert(idx, g_world.parts[idx]);
        if (firstIdx < 0) firstIdx = idx;
        pos = end + 1;
    }
    if (firstIdx >= 0) selectPart(firstIdx);
    markPlaceDirty();
}

static glm::vec3 g_toolboxPendingModelScale{1, 1, 1};
static glm::vec3 g_toolboxPendingModelRot{0, 0, 0};
static glm::vec3 g_toolboxPendingModelTint{1, 1, 1};
static bool g_toolboxPendingModelMeta = false;
static std::string g_toolboxPendingModelScript;

static void insertUserToolboxModelItemFull(const std::string& json) {
    std::string path = jsonFindEscapedStr(json, "sourcePath");
    if (path.empty()) path = jsonFindStr(json, "sourcePath");
    if (path.empty()) {
        g_exportMsg = "Model Toolbox: path vuoto";
        return;
    }
    // Path relativo sotto toolbox/
    if (!fileExistsPath(path)) {
        std::string alt = toolboxRootDir() + "/" + path;
        if (fileExistsPath(alt)) path = alt;
    }
    if (!fileExistsPath(path)) {
        g_exportMsg = "Model Toolbox: file non trovato — " + path;
        return;
    }
    jsonFindVec3(json, "scale", 0, g_toolboxPendingModelScale);
    jsonFindVec3(json, "rotation", 0, g_toolboxPendingModelRot);
    jsonFindVec3(json, "tint", 0, g_toolboxPendingModelTint);
    g_toolboxPendingModelScript = jsonFindEscapedStr(json, "script");
    g_toolboxPendingModelMeta = true;
    g_pendingModelImportPath = path;
    g_pendingModelImport = true;
    g_exportMsg = "Importazione Model dalla Toolbox...";
}

static void applyPendingToolboxModelMeta() {
    if (!g_toolboxPendingModelMeta) return;
    if (g_selectedModel < 0 || g_selectedModel >= (int)g_world.models.size()) {
        g_toolboxPendingModelMeta = false;
        return;
    }
    auto& m = g_world.models[g_selectedModel];
    m.scale = g_toolboxPendingModelScale;
    m.rotation = g_toolboxPendingModelRot;
    m.tint = g_toolboxPendingModelTint;
    if (!g_toolboxPendingModelScript.empty())
        m.script = g_toolboxPendingModelScript;
    g_toolboxPendingModelMeta = false;
    markPlaceDirty();
}

static void insertUserToolboxNpcItem(const std::string& json) {
    std::string name = jsonFindStr(json, "name");
    if (name.empty()) name = "NPC";
    FriendPlayer f;
    f.name = name;
    f.pos = insertPositionInFront();
    f.pos.y = 0.f;
    glm::vec3 to = g_playerPos - f.pos;
    to.y = 0.f;
    if (glm::length(to) > 0.1f) {
        to = glm::normalize(to);
        f.yaw = glm::degrees(std::atan2(to.x, to.z));
    }
    g_friends.push_back(std::move(f));
    syncFriendsToWorld();
    markPlaceDirty();
    g_exportMsg = "NPC Toolbox inserito: " + name;
}

static void insertUserToolboxItem(const UserToolboxItem& it) {
    std::string path = toolboxRootDir() + "/" + it.file;
    std::string json = readTextFile(path);
    if (json.empty()) {
        g_exportMsg = "Tool non trovato su disco";
        return;
    }
    switch (it.kind) {
    case ToolboxKind::Part:
    case ToolboxKind::Prefab:
        insertUserToolboxPartItem(json);
        g_exportMsg = std::string("Inserito ") + toolboxKindName(it.kind) + ": " + it.name;
        break;
    case ToolboxKind::Model:
        insertUserToolboxModelItemFull(json);
        break;
    case ToolboxKind::Npc:
        insertUserToolboxNpcItem(json);
        break;
    }
}

static bool tryPublishCurrentSelection() {
    std::string name = g_toolboxPublishName;
    while (!name.empty() && (name.back() == ' ' || name.back() == '\t')) name.pop_back();
    if (name.empty()) {
        g_toolboxStatus = "Scrivi un nome per il tool";
        return false;
    }
    auto mod = Moderation::checkAvatarOrModelName(name);
    if (mod.verdict != Moderation::Verdict::Ok) {
        g_toolboxStatus = mod.reason;
        return false;
    }

    bool ok = false;
    switch (g_toolboxPublishKind) {
    case 0: { // Part selezionata
        if (g_selectedPart < 0 || g_selectedPart >= (int)g_world.parts.size()) {
            g_toolboxStatus = "Seleziona una Part da pubblicare";
            return false;
        }
        ok = publishToolboxParts(name, {g_selectedPart}, false);
        break;
    }
    case 1: { // Prefab: tutte le Part (esclusa Baseplate) — o Part selezionata + sorelle con stesso prefisso
        std::vector<int> idxs;
        if (g_selectedPart >= 0 && g_selectedPart < (int)g_world.parts.size()) {
            // Pubblica solo la Part selezionata come prefab a 1 pezzo, oppure
            // tutte le Part non-Baseplate se nessuna selezione multipla.
            idxs.push_back(g_selectedPart);
            // Se il nome della Part ha un suffisso _N, includi le sorelle.
            const std::string& n = g_world.parts[g_selectedPart].name;
            auto us = n.find_last_of('_');
            if (us != std::string::npos && us > 0) {
                std::string prefix = n.substr(0, us + 1);
                idxs.clear();
                for (int i = 0; i < (int)g_world.parts.size(); ++i) {
                    if (g_world.parts[i].name.rfind(prefix, 0) == 0)
                        idxs.push_back(i);
                }
                if (idxs.empty()) idxs.push_back(g_selectedPart);
            }
        } else {
            for (int i = 0; i < (int)g_world.parts.size(); ++i) {
                if (g_world.parts[i].name == "Baseplate") continue;
                idxs.push_back(i);
            }
        }
        if (idxs.empty()) {
            g_toolboxStatus = "Nessuna Part da mettere nel Prefab";
            return false;
        }
        ok = publishToolboxParts(name, idxs, true);
        break;
    }
    case 2: {
        if (g_selectedModel < 0 || g_selectedModel >= (int)g_world.models.size()) {
            g_toolboxStatus = "Seleziona un Model da pubblicare";
            return false;
        }
        ok = publishToolboxModel(name, g_selectedModel);
        break;
    }
    case 3:
        ok = publishToolboxNpc(name);
        break;
    default:
        g_toolboxStatus = "Tipo tool non valido";
        return false;
    }
    if (ok) {
        g_toolboxStatus = "Pubblicato in Toolbox: " + name;
        g_exportMsg = g_toolboxStatus;
        g_showToolbox = true;
        return true;
    }
    g_toolboxStatus = "Pubblicazione fallita (scrittura disco?)";
    return false;
}

static void drawPublishToolWindow() {
    if (!g_showPublishTool) return;
    ImGui::SetNextWindowSize({420, 280}, ImGuiCond_Appearing);
    if (!ImGui::Begin("Pubblica tool##pubtool", &g_showPublishTool)) {
        ImGui::End();
        return;
    }
    ImGui::TextWrapped(
        "Pubblica nella Toolbox (come Roblox Studio): Part, Prefab multi-Part, "
        "Model 3D o NPC. Poi li ritrovi in Toolbox e li inserisci con un clic.");
    ImGui::Separator();

    if (g_toolboxPublishName[0] == 0) {
        if (g_selectedPart >= 0 && g_selectedPart < (int)g_world.parts.size())
            std::snprintf(g_toolboxPublishName, sizeof(g_toolboxPublishName), "%s",
                          g_world.parts[g_selectedPart].name.c_str());
        else if (g_selectedModel >= 0 && g_selectedModel < (int)g_world.models.size())
            std::snprintf(g_toolboxPublishName, sizeof(g_toolboxPublishName), "%s",
                          g_world.models[g_selectedModel].name.c_str());
        else
            std::snprintf(g_toolboxPublishName, sizeof(g_toolboxPublishName), "MioTool");
    }
    ImGui::InputText("Nome tool", g_toolboxPublishName, sizeof(g_toolboxPublishName));

    const char* kinds[] = {
        "Part (selezionata)",
        "Prefab (Part / gruppo _1 _2…)",
        "Model 3D (selezionato)",
        "NPC / Player amico"
    };
    ImGui::Combo("Tipo", &g_toolboxPublishKind, kinds, IM_ARRAYSIZE(kinds));

    if (g_toolboxPublishKind == 0 || g_toolboxPublishKind == 1) {
        if (g_selectedPart >= 0 && g_selectedPart < (int)g_world.parts.size())
            ImGui::TextDisabled("Part: %s", g_world.parts[g_selectedPart].name.c_str());
        else
            ImGui::TextColored({1.f, 0.55f, 0.3f, 1.f}, "Seleziona una Part nell'Explorer");
    } else if (g_toolboxPublishKind == 2) {
        if (g_selectedModel >= 0 && g_selectedModel < (int)g_world.models.size())
            ImGui::TextDisabled("Model: %s", g_world.models[g_selectedModel].name.c_str());
        else
            ImGui::TextColored({1.f, 0.55f, 0.3f, 1.f}, "Seleziona un Model nell'Explorer");
    } else {
        ImGui::TextDisabled("Crea un NPC riutilizzabile (amico che ti segue in Test)");
    }

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button, {0.12f, 0.62f, 0.38f, 1.f});
    if (ImGui::Button("Pubblica nella Toolbox", {-1, 36}))
        tryPublishCurrentSelection();
    ImGui::PopStyleColor();
    if (!g_toolboxStatus.empty())
        ImGui::TextWrapped("%s", g_toolboxStatus.c_str());
    ImGui::End();
}

static void drawDeleteToolWindow() {
    if (!g_showDeleteTool) return;
    ensureUserToolbox();
    ImGui::SetNextWindowSize({400, 360}, ImGuiCond_Appearing);
    if (!ImGui::Begin("Elimina tool##deltool", &g_showDeleteTool)) {
        ImGui::End();
        return;
    }
    ImGui::TextWrapped("Tool che hai pubblicato. Eliminarli li toglie dalla Toolbox.");
    ImGui::Separator();
    if (g_userToolbox.empty()) {
        ImGui::TextDisabled("Nessun tool pubblicato.");
    } else {
        for (int i = 0; i < (int)g_userToolbox.size(); ++i) {
            auto& it = g_userToolbox[i];
            ImGui::PushID(i);
            ImGui::BulletText("%s  [%s]", it.name.c_str(), toolboxKindName(it.kind));
            ImGui::SameLine(ImGui::GetWindowWidth() - 90.f);
            ImGui::PushStyleColor(ImGuiCol_Button, {0.75f, 0.2f, 0.18f, 1.f});
            if (ImGui::SmallButton("Elimina")) {
                std::string n = it.name;
                if (deleteUserToolboxItem(i)) {
                    g_exportMsg = "Tool eliminato: " + n;
                    g_toolboxStatus = g_exportMsg;
                    ImGui::PopStyleColor();
                    ImGui::PopID();
                    break;
                }
            }
            ImGui::PopStyleColor();
            ImGui::PopID();
        }
    }
    ImGui::Separator();
    if (ImGui::Button("Apri Toolbox", {-1, 28}))
        g_showToolbox = true;
    ImGui::End();
}

static void drawUserToolboxSection() {
    ensureUserToolbox();
    if (g_userToolbox.empty()) {
        ImGui::TextDisabled("Nessun tool tuo. Usa «Pubblica tool» nella barra.");
        return;
    }
    ImGui::TextColored({0.45f, 0.85f, 1.f, 1.f}, "I tuoi tool");
    for (const UserToolboxItem& it : g_userToolbox) {
        char label[160];
        std::snprintf(label, sizeof(label), "%s [%s]", it.name.c_str(), toolboxKindName(it.kind));
        if (ImGui::Button(label, {-1, 32}))
            insertUserToolboxItem(it);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Inserisci %s", toolboxKindName(it.kind));
    }
}

static void drawToolboxSideButtons() {
    ImGui::PushStyleColor(ImGuiCol_Button, {0.14f, 0.55f, 0.36f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.18f, 0.68f, 0.44f, 1.f});
    if (ImGui::Button("Pubblica tool", {-1, 32})) {
        g_toolboxPublishName[0] = 0;
        g_toolboxStatus.clear();
        if (g_selectedModel >= 0) g_toolboxPublishKind = 2;
        else if (g_selectedPart >= 0) g_toolboxPublishKind = 0;
        else g_toolboxPublishKind = 3;
        g_showPublishTool = true;
    }
    ImGui::PopStyleColor(2);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Pubblica Part, Prefab, Model o NPC nella Toolbox");

    ImGui::PushStyleColor(ImGuiCol_Button, {0.65f, 0.22f, 0.2f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.8f, 0.3f, 0.25f, 1.f});
    if (ImGui::Button("Elimina tool", {-1, 32})) {
        ensureUserToolbox();
        g_showDeleteTool = true;
    }
    ImGui::PopStyleColor(2);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Vedi e rimuovi i tool che hai pubblicato");
}
