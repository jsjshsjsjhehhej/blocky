// Blocky — client hub / play  ·  Blocky Game Engine — Studio (BLOCKY_GAME_ENGINE)

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <shellapi.h>
#endif
#include <csignal>
#include <cstdlib>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "BlockPlayer.h"
#include "SkinnedModel.h"
#include "avatar/AvatarCatalog.h"
#include "avatar/AnimPackUI.h"
#include "engine/DataModel.h"
#include "engine/GameHub.h"
#include "engine/Humanoid.h"
#include "engine/LuaRuntime.h"
#include "engine/PlaceExporter.h"
#include "engine/BlockyProject.h"
#include "engine/SoundService.h"
#include "engine/LightingService.h"
#include "engine/Animator.h"
#include "engine/AntiCheat.h"
#include "engine/LanNet.h"
#include "engine/NetGate.h"
#include "platform/Log.h"
#include "engine/Ragdoll.h"
#include "engine/R6PlayerExport.h"
#include "engine/UndoStack.h"
#include "platform/BlockyFileAssoc.h"
#include "platform/CrashHandler.h"
#include "platform/EngineConfig.h"
#include "platform/GamepadInput.h"
#include "render/BeveledCube.h"
#include "render/PartMeshes.h"
#include "render/Texture2D.h"
#include "render/GlShaders.h"
#include "render/WindowIcon.h"
#include "render/ShadowMap.h"
#include "render/SunSky.h"
#include "render/Particles.h"
#include "engine/Purchase.h"
#include "ui/ClassicMenu.h"
#include "ui/StudioChrome.h"
#include "ui/MenuPadCursor.h"
#include "ui/Loc.h"
#include "moderation/ContentFilter.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef BLOCKY_GAME_ENGINE
static constexpr bool kIsGameEngine = true;
static constexpr const char* kAppBrand = "Blocky Game Engine";
static constexpr const char* kAppShort = "Game Engine";
#else
static constexpr bool kIsGameEngine = false;
static constexpr const char* kAppBrand = "Blocky";
static constexpr const char* kAppShort = "Blocky";
#endif

static int g_w = 1280, g_h = 720;
static float g_fps = 0.f;

enum class AppScreen { MainMenu, Loading, Playing, Studio, AvatarPublisher };
enum class CamMode { Studio, Play };

struct Camera {
    glm::vec3 pos{ 0.f, 12.f, 28.f };
    float yaw = -90.f, pitch = -18.f, fov = 70.f;
    glm::vec3 front() const {
        glm::vec3 f;
        f.x = cosf(glm::radians(yaw)) * cosf(glm::radians(pitch));
        f.y = sinf(glm::radians(pitch));
        f.z = sinf(glm::radians(yaw)) * cosf(glm::radians(pitch));
        return glm::normalize(f);
    }
    glm::mat4 view() const { return glm::lookAt(pos, pos + front(), {0,1,0}); }
};

static AppScreen g_screen = AppScreen::MainMenu;
static CamMode g_mode = CamMode::Play;
static Camera g_cam;
static bool g_firstMouse = true;
static float g_lastMx = 0, g_lastMy = 0;
static bool g_rmbLook = false;

static BlockPlayer g_player;
static LanNet g_net;
static std::vector<BlockPlayer> g_remoteBodies;

/** NPC amico che segue il player in Test/Play. */
struct FriendPlayer {
    std::string name = "Amico";
    glm::vec3 pos{0};
    float yaw = 180.f;
    BlockPlayer body{};
    float health = 100.f;
    float maxHealth = 100.f;
};
static std::vector<FriendPlayer> g_friends;
static std::vector<FriendPlayer> g_friendsTestBackup;
static int g_friendSeq = 1;
static DataModel g_world;
static LuaRuntime g_lua;
static SoundService g_sounds;
static LightingService g_lighting;
static int g_selectedService = -1; // 0=Lighting 1=RunService 2=TweenService 3=UserInputService 4=Chat 5=Debris 6=ReplicatedStorage 7=StarterGui

static Animator g_animator;
static Ragdoll g_ragdoll;
static ParticleSystem g_particles;
static UndoStack g_undo;
static WorkspacePart g_xformBefore{}; // snapshot begin drag
static bool g_xformTrack = false;
static std::vector<uint8_t> g_touching;
static std::vector<StaticMesh> g_wsMeshes; // mesh per g_world.models
// Versione animata, popolata solo per i modelli con scheletro: StaticMesh
// appiattisce la geometria e butta le ossa, quindi un mostro con walk/chase
// va caricato anche qui per potersi muovere.
static std::vector<AnimatedModel> g_wsRigged;
static int g_selectedModel = -1;
static int g_selectedSound = -1;
static char g_soundPathBuf[512] = {};
static char g_animNameBuf[64] = "Wave";
static bool g_showAnimEditor = false;
static bool g_showRigPrompt = false;
static bool g_animEditorNeedsPlace = false;
static int g_animEditModel = -1;
static int g_animSelKey = 0;
static float g_animScrub = 0.f;
static bool g_animPlaying = false;
static int g_animStepFrames = 5; // avanzamento automatico dopo Set Frame
static bool g_animLiveCapture = false; // off di default: evita di sovrascrivere i key mentre navighi
static bool g_animHaveClipboard = false;
static AnimateFrame g_animClipboard{};

// Workspace → Players → LocalPlayer (rig)
static bool g_selectedPlayer = false;
static int g_selectedPlayerPart = -1; // indice in humanoid.parts, -1 = Player root
static bool g_selectedBaseplate = false;
static bool g_selectedSpawn = false;
static int g_selectedLabel = -1;
static int g_selectedBackpack = -1;
static bool g_labelDragging = false;
static int g_labelDragCorner = -1; // -1 = sposta box, 0..3 = vertice TL TR BR BL
static float g_labelGrabDX = 0.f, g_labelGrabDY = 0.f;
static float g_labelDragFixedX = 0.f, g_labelDragFixedY = 0.f; // angolo opposto fisso
static int g_uiHoverLabel = -1;
static int g_uiPressedLabel = -1;
static bool g_showCreatePanel = false; // off di default — meno confusione
static bool g_showPlayerRigPrompt = false;
static bool g_showBackpackOffer = false; // dopo import pistola/gun: chiede Backpack?
static std::string g_backpackOfferName;
static std::string g_backpackOfferPath;
static char g_labelNameBuf[64] = "Label";
static char g_labelTextBuf[128] = "Hello Blocky";
static float g_studioUiTime = 0.f;
static bool g_showPlayerRigEditor = false;
static bool g_playerRigEditorNeedsPlace = false;
static std::vector<GameEntry> g_games;
static ClassicMenuState g_menu;
static GameEntry g_activeGame;
static float g_loadTimer = 0.f;
static float g_loadDur = 1.8f;
static std::string g_exportMsg;
static std::string g_placeFilePath; // ultimo place aperto/salvato (Salva rapido)
static bool g_placeDirty = false;   // modifiche non salvate dall'utente

enum class PendingNav : int {
    None = 0,
    GoHome,
    QuitApp,
    NewPlace,
    OpenPlace,
};
static PendingNav g_pendingNav = PendingNav::None;
static bool g_showSavePrompt = false;
static bool g_showAutosavePrompt = false;
static bool g_startupHomeDone = false;

static void markPlaceDirty() { g_placeDirty = true; }
static void clearPlaceDirty() { g_placeDirty = false; }
static GLFWwindow* g_mainWin = nullptr;

static glm::vec3 g_playerPos{ 0, 0, 0 };
static float g_playerYaw = 180.f;   // facing: 0 = +Z (dove c'è la faccia smile)
static float g_orbitYaw = 0.f;      // camera orbit (gradi)
static float g_orbitPitch = -22.f;
static float g_orbitDist = 14.f;
static bool g_walking = false;
static bool g_shiftLock = false;     // Shift Lock stile Roblox
static bool g_firstPerson = false;   // Prima persona (occhi) — locale only
static bool g_playCursorLocked = false;
static constexpr float kFpEnterDist = 4.5f; // zoom sotto questa soglia → FP
static constexpr float kFpExitDist = 7.0f;  // uscita FP con zoom out
static glm::vec3 g_moveVel{0, 0, 0}; // velocità orizzontale (smooth)
static bool g_showFbx = false;
static bool g_useGlbAvatar = false; // sempre R6 Blocky (6 cubi) — facile e ottimizzato
static bool g_vsync = true;
static bool g_showExplorer = true;
static bool g_showProps = true;
static bool g_showScript = false; // apri da View quando serve
static bool g_showLuaGuide = true; // guida API Blocky in Studio
static bool g_showAnimDock = false;
static bool g_showPlayerAnimPublisher = false; // Studio: importa → pubblica → AnimationId
static char g_lastPublishedAnimId[96] = {};
static int g_selectedPart = -1; // indice in g_world.parts, -1 = nessuno
/** .blocky da doppio-clic: aperto al PRIMO frame del loop, non allo startup
    (a motore caldo), per evitare il crash sul primo render. */
static std::string g_pendingOpenPlace;
static char g_partNameBuf[64] = {};
static char g_partScriptBuf[16384] = {};

static void* publisherHwnd();
static void refreshGamesList();
static void doOpenPlace();
static void doSavePlace();
static void doSaveForPublish();
static void persistPublishDraftOnly();
static void markPlaceDirty();
static void clearPlaceDirty();
static void goGameEngineHome(GLFWwindow* win);
static void requestNav(GLFWwindow* win, PendingNav a);
static void drawStudioPlayerAnimPanel();
static void drawSelectedPartLabel();
static void drawWorldLabels();
static int pickScreenLabelAt(float mx, float my);
static void insertNewTextLabel();
static void insertNewTextButton();
static void fireGuiButton(int idx);
static void updateGuiButtonHover(float mx, float my);
static void beginLabelDrag(int idx, float mx, float my);
static void updateLabelDrag(float mx, float my);
static void endLabelDrag();
static void* publisherHwnd();
static void downloadDefaultR6PlayerModel();
static void importWalkingAnimationFromFile();
static bool releaseWalkingAnimation();
static void syncEquippedWalkToAnimator();
static std::string makeExplodeBlockScript(const WorkspacePart& p);
static std::string makeNeonBlockScript(const WorkspacePart& p);
static void insertExplodePart();
static void bootPlaceScript(const std::string& scriptRel);
static void bindLuaServices();
static void drawHealthBarHud();
static void drawPlayerHeadHealthBar();
static void insertFriendPlayer();
static void updateFriendPlayers(float dt);
static void drawFriendPlayers(GLuint vao, GLuint partP, const glm::mat4& view,
                              const glm::mat4& proj, const glm::vec3& sun);
static void syncFriendsFromWorld();
static void syncFriendsToWorld();
static void drawInventoryHud();
static void drawChatBubbleHud();
static void drawSpeechBubbles();
static bool sendPlayChat(const char* msg);
static void drawGizmoOverlayHud();
static void pollBackpackSwitch(GLFWwindow* win);
static void trySwordAttack();
static void drawEquippedSword(GLuint vao, GLuint partP, const glm::mat4& view,
                              const glm::mat4& proj, const glm::vec3& sun);
static bool playCameraActive();
static bool equipShopItem(int idx);
static bool unequipShopItem(int idx);
static void syncEquippedToMenu();

enum class StudioTool { Select, Move, Scale, Rotate };
static StudioTool g_tool = StudioTool::Select;
static bool g_editingPartScript = false;

// Scale gizmo (solo in tool Scale)
static bool g_scaleDragging = false;
static int g_scaleAxis = -1;
static int g_scaleSign = 0;
static float g_scaleStartFace = 0.f;
static glm::vec3 g_scaleStartPos{0}, g_scaleStartSize{1};
static constexpr float kHandleSize = 0.48f;

// Move gizmo
static bool g_moveDragging = false;
static int g_moveAxis = -1;
static glm::vec3 g_moveStartPos{0};
static glm::vec3 g_moveStartHit{0};

// Rotate gizmo — anelli RGB stile Studio
static bool g_rotateDragging = false;
static int g_rotateAxis = -1;
static float g_rotateStartAngle = 0.f;
static float g_rotateLiveDelta = 0.f; // gradi live (HUD)
static glm::vec3 g_rotateStartRot{0};
static int g_gizmoHoverAxis = -1; // 0..2 hover highlight
static float g_moveLiveDelta = 0.f;
static float g_scaleLiveSize = 0.f;
static char g_luaEditBuf[16384] =
    "-- Script Studio\n"
    "print('Ciao da Studio!')\n"
    "local p = Instance.new('Part')\n"
    "p.Name = 'StudioPart'\n"
    "p.Position = Vector3.new(0, 3, 6)\n"
    "p.Size = Vector3.new(4, 1, 4)\n"
    "p.BrickColor = Color3.new(1, 0.4, 0.1)\n"
    "workspace:Add(p)\n";
static bool g_loadApiCompleta = false; // Test carica lua_api_completo.lua
static bool g_loadStamina = false;     // Test carica stamina.lua (dopo "Carica")
static std::string g_luaRunMsg;
static glm::mat4 g_lastView{1.f}, g_lastProj{1.f};
static EngineConfig g_engine = EngineConfig::createDefault();
static bool g_showPlatformPanel = false;
static bool g_showChat = false;
static bool g_playMenuOpen = false; // menu Esc in Play: Leave / Chat

/** Preset grafico scelto dal giocatore (menu di gioco) — persistito su disco. */
static std::string gfxPresetPath() { return blockyDataDir() + "/graphics.txt"; }

static const char* gfxPresetName(QualityPreset p) {
    switch (p) {
    case QualityPreset::Low:    return "Low";
    case QualityPreset::Medium: return "Medium";
    case QualityPreset::High:   return "High";
    case QualityPreset::Ultra:  return "Ultra";
    default:                    return "Default";
    }
}

static void saveGfxPreset() {
    FILE* f = std::fopen(gfxPresetPath().c_str(), "wb");
    if (!f) return;
    std::fprintf(f, "%s\n", gfxPresetName(g_engine.preset));
    std::fclose(f);
}

/** Applica il preset a runtime (draw distance, parti, stud, physics, vsync) e salva. */
static void applyGfxPreset(QualityPreset p) {
    g_engine.setPreset(p);
    g_vsync = g_engine.quality.vsync;
    saveGfxPreset();
}

/** Rilegge il preset salvato — chiamare prima di creare la finestra (MSAA/render scale). */
static void loadGfxPreset() {
    FILE* f = std::fopen(gfxPresetPath().c_str(), "rb");
    if (!f) return;
    char line[32] = {};
    bool ok = std::fgets(line, sizeof(line), f) != nullptr;
    std::fclose(f);
    if (!ok) return;
    size_t n = std::strlen(line);
    while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r' || line[n - 1] == ' '))
        line[--n] = '\0';
    if (std::strcmp(line, "Low") == 0)          g_engine.setPreset(QualityPreset::Low);
    else if (std::strcmp(line, "Medium") == 0)  g_engine.setPreset(QualityPreset::Medium);
    else if (std::strcmp(line, "High") == 0)    g_engine.setPreset(QualityPreset::High);
    else if (std::strcmp(line, "Ultra") == 0)   g_engine.setPreset(QualityPreset::Ultra);
    else                                        g_engine.setPreset(QualityPreset::PlatformDefault);
    g_vsync = g_engine.quality.vsync;
}

/** Fumetto sopra la testa dopo un messaggio chat inviato. */
static std::string g_speechText;
static double g_speechUntil = 0.0;
static bool g_studioTesting = false; // Test Play da Studio
static bool g_bpKeyWas[9] = {};      // edge 1-9 inventario (GLFW, inizio frame)
static bool g_bpKpWas[9] = {};
static float g_swordSwing = 0.f;     // 1 → 0 animazione fendente
static float g_swordCooldown = 0.f;
static glm::vec3 g_studioCamBackup{0, 12, 28};
static float g_studioYawBackup = -90.f, g_studioPitchBackup = -18.f;
/** Snapshot place + player prima del Test → ripristino allo Stop. */
static DataModel g_worldTestBackup;
static bool g_hasTestBackup = false;
static glm::vec3 g_playerPosTestBackup{0, 0, 0};
static float g_playerYawTestBackup = 180.f;
static int g_selectedPartTestBackup = -1;
static int g_selectedModelTestBackup = -1;
static int g_selectedLabelTestBackup = -1;
static char g_studioChatBuf[160] = {};
static char g_importPathBuf[512] = {};
static char g_gameNameBuf[96] = "Il mio gioco";
static int g_maxPlayers = 8; // scelto in Game Engine → publish
static std::string g_publishMsg;
/** Dopo "Salva per pubblicare": autosave bozza verso publish futuro. */
static bool g_publishDraftActive = false;
static float g_publishDraftAcc = 0.f;
static std::string g_publishDraftPath;
static AvatarCatalogState g_catalog;
static AnimPackPublisherState g_animPackUi;
/** true = la prossima partita si connette all'IP scritto nel menu. */
static bool g_joinByIp = false;

// --- Gate online: non si gioca senza una sessione data da un server ---
/** Istante limite entro cui l'handshake deve concludersi. */
static double g_gateDeadline = 0.0;
/** true = il server ha concesso la sessione; smette di controllare. */
static bool g_gatePassed = false;
/** Messaggio da mostrare nel menu dopo un rifiuto. */
static char g_gateMsg[256] = {};
static char g_gateTitle[64] = {};
static BeveledCubeMesh g_bevelCube;
static PartMeshes g_partMeshes;
static int g_sharpCubeVerts = 36;

static constexpr float kStudTile = 4.f; // texture stud classica (cerchi + R)

static const char* kSkinVS = R"(#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in ivec4 aBoneIDs;
layout(location=3) in vec4 aWeights;
const int MAX_BONES=100;
uniform mat4 uFinalBones[MAX_BONES],uModel,uView,uProj;
out vec3 vN,vW;
void main(){
  mat4 skin=mat4(0.); float t=0.;
  for(int i=0;i<4;i++){ if(aBoneIDs[i]<0) continue;
    skin+=uFinalBones[aBoneIDs[i]]*aWeights[i]; t+=aWeights[i]; }
  if(t<0.001) skin=mat4(1.);
  vec4 w=uModel*skin*vec4(aPos,1.);
  vW=w.xyz; vN=mat3(uModel)*mat3(skin)*aNormal;
  gl_Position=uProj*uView*w;
})";

static GLuint compileShader(GLenum t, const char* s) {
    GLuint sh = glCreateShader(t);
    glShaderSource(sh, 1, &s, nullptr);
    glCompileShader(sh);
    GLint ok = GL_FALSE;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024] = {};
        glGetShaderInfoLog(sh, sizeof(log) - 1, nullptr, log);
        std::fprintf(stderr, "[GL] %s shader FAILED:\n%s\n",
                     t == GL_VERTEX_SHADER ? "vertex" : "fragment", log);
        CrashHandler::note("Shader compile fallita");
    }
    return sh;
}

static GLuint makeProg(const char* vs, const char* fs) {
    GLuint p = glCreateProgram();
    glAttachShader(p, compileShader(GL_VERTEX_SHADER, vs));
    glAttachShader(p, compileShader(GL_FRAGMENT_SHADER, fs));
    glLinkProgram(p);
    GLint ok = GL_FALSE;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024] = {};
        glGetProgramInfoLog(p, sizeof(log) - 1, nullptr, log);
        std::fprintf(stderr, "[GL] link FAILED:\n%s\n", log);
        CrashHandler::note("Shader link fallito");
    }
    return p;
}

static GLuint makeCubeVAO() {
    const float v[] = {
        -.5f,-.5f,.5f,0,0,1, .5f,-.5f,.5f,0,0,1, .5f,.5f,.5f,0,0,1,
        -.5f,-.5f,.5f,0,0,1, .5f,.5f,.5f,0,0,1, -.5f,.5f,.5f,0,0,1,
        -.5f,-.5f,-.5f,0,0,-1, -.5f,.5f,-.5f,0,0,-1, .5f,.5f,-.5f,0,0,-1,
        -.5f,-.5f,-.5f,0,0,-1, .5f,.5f,-.5f,0,0,-1, .5f,-.5f,-.5f,0,0,-1,
         .5f,-.5f,-.5f,1,0,0, .5f,.5f,-.5f,1,0,0, .5f,.5f,.5f,1,0,0,
         .5f,-.5f,-.5f,1,0,0, .5f,.5f,.5f,1,0,0, .5f,-.5f,.5f,1,0,0,
        -.5f,-.5f,-.5f,-1,0,0, -.5f,-.5f,.5f,-1,0,0, -.5f,.5f,.5f,-1,0,0,
        -.5f,-.5f,-.5f,-1,0,0, -.5f,.5f,.5f,-1,0,0, -.5f,.5f,-.5f,-1,0,0,
        -.5f,.5f,-.5f,0,1,0, -.5f,.5f,.5f,0,1,0, .5f,.5f,.5f,0,1,0,
        -.5f,.5f,-.5f,0,1,0, .5f,.5f,.5f,0,1,0, .5f,.5f,-.5f,0,1,0,
        -.5f,-.5f,-.5f,0,-1,0, .5f,-.5f,-.5f,0,-1,0, .5f,-.5f,.5f,0,-1,0,
        -.5f,-.5f,-.5f,0,-1,0, .5f,-.5f,.5f,0,-1,0, -.5f,-.5f,.5f,0,-1,0,
    };
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    return vao;
}

static constexpr int kMaxDynLights = 8;
struct DynLight {
    glm::vec3 pos{0};
    glm::vec3 color{1};
    float range = 20.f;
};
static DynLight g_dynLights[kMaxDynLights];
static int g_dynLightCount = 0;
static glm::vec3 g_renderCamPos{0, 8, 20};

/** Box che bloccano le luci dinamiche (ombre in shader): player + oggetti vicini. */
static constexpr int kMaxShadowBoxes = 8;
static glm::vec3 g_occCenter[kMaxShadowBoxes];
static glm::vec3 g_occHalf[kMaxShadowBoxes];
static glm::mat3 g_occRot[kMaxShadowBoxes]; // mondo → locale box (rotazione trasposta)
static int g_occCount = 0;
/** 0 = pieno giorno, 1 = piena notte (dal ciclo giorno/notte). */
static float g_night = 0.f;
/** Shadow map di sole/luna: ombre con la forma vera delle mesh. */
static ShadowMap g_shadowMap;
static bool g_sunShadowOn = false;

/** Raggio di luce di una Part Neon: dal blocco, senza bisogno di un PointLight. */
static float neonLightRange(const WorkspacePart& p) {
    float maxDim = std::max(p.size.x, std::max(p.size.y, p.size.z));
    return std::clamp(8.f + maxDim * 2.2f, 8.f, 42.f);
}

static void collectDynamicLights() {
    g_dynLightCount = 0;
    struct Cand { glm::vec3 pos, col; float range, d2; };
    static std::vector<Cand> cands;
    cands.clear();
    const float maxD = g_engine.quality.maxDrawDistance;
    for (const auto& p : g_world.parts) {
        const bool neon = p.isNeon();
        if (!p.pointLight && !neon) continue;
        Cand c;
        c.pos = p.position;
        if (p.pointLight) {
            c.col = p.color * std::max(0.05f, p.lightBrightness);
            c.range = std::max(1.f, p.lightRange);
        } else {
            // Neon senza PointLight: illumina comunque, intensità dall'emissive
            c.col = p.color * std::clamp(0.55f + p.effectiveEmissive() * 0.55f, 0.35f, 1.6f);
            c.range = neonLightRange(p);
        }
        glm::vec3 d = c.pos - g_renderCamPos;
        c.d2 = glm::dot(d, d);
        float reach = c.range + maxD;
        if (c.d2 > reach * reach) continue; // non può toccare nulla di visibile
        cands.push_back(c);
    }
    if ((int)cands.size() > kMaxDynLights) {
        std::nth_element(cands.begin(), cands.begin() + kMaxDynLights, cands.end(),
            [](const Cand& a, const Cand& b) { return a.d2 < b.d2; });
        cands.resize(kMaxDynLights);
    }
    for (const auto& c : cands) {
        DynLight& L = g_dynLights[g_dynLightCount++];
        L.pos = c.pos;
        L.color = c.col;
        L.range = c.range;
    }
}

/**
 * Occlusori: player (locale + remoti) e parti più vicine alla camera.
 * Servono sia alle luci dinamiche (Neon) sia all'ombra portata da sole/luna,
 * quindi vanno raccolti anche quando non c'è nessuna luce dinamica.
 */
static void collectShadowOccluders() {
    g_occCount = 0;
    const int budget = std::min(kMaxShadowBoxes, g_engine.quality.shadowOccluders);
    if (budget <= 0) return;
    if (g_dynLightCount == 0 && !g_lighting.globalShadows) return;

    auto add = [&](const glm::vec3& center, const glm::vec3& half, const glm::mat3& invRot) {
        if (g_occCount >= budget) return;
        g_occCenter[g_occCount] = center;
        g_occHalf[g_occCount] = glm::max(half, glm::vec3(0.05f));
        g_occRot[g_occCount] = invRot;
        ++g_occCount;
    };
    // Silhouette R6: piedi a y=0, testa ~5.3, braccia larghe ~4
    const glm::vec3 kBodyHalf{1.5f, 2.65f, 0.6f};
    auto addBody = [&](const glm::vec3& feet, float yawDeg) {
        add(feet + glm::vec3(0.f, kBodyHalf.y, 0.f), kBodyHalf,
            glm::mat3(glm::rotate(glm::mat4(1.f), glm::radians(-yawDeg), glm::vec3(0, 1, 0))));
    };

    const bool inPlay = (g_screen == AppScreen::Playing)
        || (g_screen == AppScreen::Studio && g_studioTesting);
    if (inPlay)
        addBody(g_playerPos, g_playerYaw);
    for (const auto& r : g_net.remotes)
        addBody(r.displayPos, r.displayYaw);

    // Poi le parti più vicine alla camera: sono quelle di cui vedi davvero l'ombra
    struct OccCand { int i; float d2; };
    static std::vector<OccCand> cands;
    cands.clear();
    for (int i = 0; i < (int)g_world.parts.size(); ++i) {
        const auto& p = g_world.parts[i];
        if (p.isNeon() || p.pointLight) continue; // la sorgente non fa ombra a sé stessa
        if (p.isTransparent()) continue;          // vetro / ForceField: la luce passa
        glm::vec3 d = p.position - g_renderCamPos;
        cands.push_back({i, glm::dot(d, d)});
    }
    int want = budget - g_occCount;
    if (want <= 0) return;
    if ((int)cands.size() > want) {
        std::nth_element(cands.begin(), cands.begin() + want, cands.end(),
            [](const OccCand& a, const OccCand& b) { return a.d2 < b.d2; });
        cands.resize(want);
    }
    for (const auto& c : cands) {
        const auto& p = g_world.parts[c.i];
        add(p.position, p.size * 0.5f,
            glm::transpose(glm::mat3(partRotationMatrix(p.rotation))));
    }
}

/** Schermate senza mondo 3D (menu / publisher): nessuna luce dinamica. */
static void clearDynamicLights() {
    g_dynLightCount = 0;
    g_occCount = 0;
}

static void uploadDynamicLights(GLuint prog) {
    glUniform1i(glGetUniformLocation(prog, "uLightCount"), g_dynLightCount);
    for (int i = 0; i < kMaxDynLights; ++i) {
        char name[32];
        std::snprintf(name, sizeof(name), "uLightPos[%d]", i);
        glm::vec3 pos = (i < g_dynLightCount) ? g_dynLights[i].pos : glm::vec3(0);
        glUniform3fv(glGetUniformLocation(prog, name), 1, glm::value_ptr(pos));
        std::snprintf(name, sizeof(name), "uLightCol[%d]", i);
        glm::vec3 col = (i < g_dynLightCount) ? g_dynLights[i].color : glm::vec3(0);
        glUniform3fv(glGetUniformLocation(prog, name), 1, glm::value_ptr(col));
        std::snprintf(name, sizeof(name), "uLightRange[%d]", i);
        float rng = (i < g_dynLightCount) ? g_dynLights[i].range : 1.f;
        glUniform1f(glGetUniformLocation(prog, name), rng);
    }
    glUniform1f(glGetUniformLocation(prog, "uNight"), g_night);
    glUniform1i(glGetUniformLocation(prog, "uOccCount"), g_occCount);
    // Ombre portate da sole/luna: shadow map (forma reale delle mesh)
    glUniform1i(glGetUniformLocation(prog, "uSunShadow"), g_sunShadowOn ? 1 : 0);
    if (g_sunShadowOn)
        g_shadowMap.bindForRead(prog, 3);
    if (g_occCount > 0) {
        glUniform3fv(glGetUniformLocation(prog, "uOccCenter"), g_occCount,
                     glm::value_ptr(g_occCenter[0]));
        glUniform3fv(glGetUniformLocation(prog, "uOccHalf"), g_occCount,
                     glm::value_ptr(g_occHalf[0]));
        glUniformMatrix3fv(glGetUniformLocation(prog, "uOccRot"), g_occCount, GL_FALSE,
                           glm::value_ptr(g_occRot[0]));
    }
}

static void drawCube(GLuint vao, GLuint prog, const glm::mat4& m, const glm::mat4& v,
                     const glm::mat4& p, const glm::vec3& col, const glm::vec3& sun,
                     bool studTex = false, GLuint studId = 0, int vertCount = 36,
                     float emissive = 0.f, PartMaterial mat = PartMaterial::Plastic) {
    if ((int)mat < 0 || mat >= PartMaterial::Count)
        mat = PartMaterial::Plastic;
    float specular = 0.12f, roughness = 0.75f, emBias = 0.f, alpha = 1.f;
    partMaterialShading(mat, specular, roughness, emBias, alpha);
    float em = std::max(emissive, emBias);

    bool transparent = alpha < 0.99f;
    if (transparent) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
    }

    glUseProgram(prog);
    glUniformMatrix4fv(glGetUniformLocation(prog,"uModel"),1,GL_FALSE,glm::value_ptr(m));
    glUniformMatrix4fv(glGetUniformLocation(prog,"uView"),1,GL_FALSE,glm::value_ptr(v));
    glUniformMatrix4fv(glGetUniformLocation(prog,"uProj"),1,GL_FALSE,glm::value_ptr(p));
    glUniform3fv(glGetUniformLocation(prog,"uColor"),1,glm::value_ptr(col));
    glUniform3fv(glGetUniformLocation(prog,"uSun"),1,glm::value_ptr(sun));
    glUniform3fv(glGetUniformLocation(prog,"uCamPos"),1,glm::value_ptr(g_renderCamPos));
    glUniform1f(glGetUniformLocation(prog, "uEmissive"), em);
    glUniform1f(glGetUniformLocation(prog, "uSpecular"), specular);
    glUniform1f(glGetUniformLocation(prog, "uRoughness"), roughness);
    glUniform1f(glGetUniformLocation(prog, "uAlpha"), alpha);
    glUniform1i(glGetUniformLocation(prog, "uMaterial"), (int)mat);
    uploadDynamicLights(prog);
    if (studTex && studId) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, studId);
        glUniform1i(glGetUniformLocation(prog, "uStud"), 0);
        glUniform1f(glGetUniformLocation(prog, "uStudScale"), kStudTile);
    }
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, vertCount);

    if (transparent) {
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
}

/** Boom visuale: solo particelle (i cubi Neon facevano lag). */
static void drawExplosionCubes(GLuint, GLuint, const glm::mat4&,
                               const glm::mat4&, const glm::vec3&) {
    // disabilitato — FX in ParticleSystem::draw (billboard leggeri)
}

static void drawCubePlayer(GLuint vao, GLuint prog, const glm::mat4& m, const glm::mat4& v,
                           const glm::mat4& p, const glm::vec3& col, const glm::vec3& sun) {
    // Limbs/corpo beveled se disponibile
    int vc = (g_bevelCube.vao && vao == g_bevelCube.vao) ? g_bevelCube.vertexCount : g_sharpCubeVerts;
    drawCube(vao, prog, m, v, p, col, sun, false, 0, vc);
}

/** Outline blu intorno alla selezione (Model / Spawn / Player — non Part). */
static void drawSelectedPartBlueOutline(GLuint vao, GLuint partP,
                                        const glm::mat4& view, const glm::mat4& proj) {
    const glm::vec3 outlineBlue{0.15f, 0.55f, 1.0f};
    auto drawOutlineBox = [&](const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& size) {
        glm::vec3 pad = glm::vec3(0.14f) + size * 0.05f;
        glm::mat4 m = glm::translate(glm::mat4(1.f), pos)
                    * partRotationMatrix(rot)
                    * glm::scale(glm::mat4(1.f), size + pad);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        drawCube(vao, partP, m, view, proj, outlineBlue, glm::vec3(0.f, -1.f, 0.2f), false, 0);
        glCullFace(GL_BACK);
        glDisable(GL_CULL_FACE);
    };

    // Part: niente outline blu → si vede solo il nome sopra
    if (g_selectedModel >= 0 && g_selectedModel < (int)g_world.models.size()) {
        const auto& m = g_world.models[g_selectedModel];
        drawOutlineBox(m.position, m.rotation, glm::max(m.scale * 2.f, glm::vec3(1.f)));
    }
    if (g_selectedSpawn) {
        drawOutlineBox(g_world.spawn.position + glm::vec3(0, 1.5f, 0), {0, g_world.spawn.yaw, 0}, {2.f, 3.f, 2.f});
    }
    if (g_selectedPlayer) {
        drawOutlineBox(g_playerPos + glm::vec3(0, 3.f, 0), {0, g_playerYaw, 0}, {2.4f, 6.8f, 2.4f});
    }
}

static std::string findFbx() {
    std::vector<std::string> paths = { assetRoot() + "/assets/character.fbx", "assets/character.fbx" };
    for (const auto& c : paths) {
        if (FILE* f = std::fopen(c.c_str(), "rb")) { std::fclose(f); return c; }
    }
    return {};
}

static std::string findNoobGlb() {
    std::vector<std::string> paths = {
        assetRoot() + "/assets/roblox-noob.glb",
        "assets/roblox-noob.glb",
        assetRoot() + "/roblox-noob.glb",
        "roblox-noob.glb",
    };
    for (const auto& c : paths) {
        if (FILE* f = std::fopen(c.c_str(), "rb")) { std::fclose(f); return c; }
    }
    return {};
}

/** Punto camera: torso ragdoll se morto, altrimenti player. */
static glm::vec3 playFocusPos() {
    if (g_ragdoll.active)
        return g_ragdoll.focusPos();
    return g_playerPos;
}

/** Facing orizzontale del player. Yaw 0 = +Z (faccia smile). */
static glm::vec3 playerFwd(float yaw) {
    float r = glm::radians(yaw);
    return glm::normalize(glm::vec3(sinf(r), 0.f, cosf(r)));
}

/** Yaw gradi da una direzione orizzontale (allineato a playerFwd). */
static float yawFromDir(const glm::vec3& dir) {
    return glm::degrees(atan2f(dir.x, dir.z));
}

/** Direzione orizzontale in cui guarda la camera (per WASD). */
static glm::vec3 camFlatForward() {
    float y = glm::radians(g_orbitYaw);
    return glm::normalize(glm::vec3(-sinf(y), 0.f, -cosf(y)));
}

static glm::vec3 camFlatRight() {
    return glm::normalize(glm::cross(camFlatForward(), glm::vec3(0, 1, 0)));
}

/** Terza persona. Senza Shift Lock = orbita libera. Con Shift Lock = spalla (non solo dietro). */
static glm::mat4 thirdPersonView() {
    float pitchR = glm::radians(g_orbitPitch);
    float cp = cosf(pitchR), sp = sinf(pitchR);
    glm::vec3 fwd = camFlatForward();
    glm::vec3 right = camFlatRight();

    if (g_shiftLock) {
        // Over-the-shoulder: a lato e un po' indietro rispetto alla visuale
        float dist = glm::clamp(g_orbitDist, 7.f, 12.f);
        glm::vec3 target = playFocusPos() + glm::vec3(0.f, 3.55f, 0.f) + right * 0.45f;
        glm::vec3 eye = target
            - fwd * (dist * cp)
            + glm::vec3(0.f, 1.f, 0.f) * (dist * (-sp))
            + right * 1.85f;
        return glm::lookAt(eye, target + fwd * 2.5f, glm::vec3(0, 1, 0));
    }

    // Orbita classica intorno al player
    float yawR = glm::radians(g_orbitYaw);
    glm::vec3 target = playFocusPos() + glm::vec3(0.f, 3.2f, 0.f);
    glm::vec3 eye = target + glm::vec3(
        g_orbitDist * cp * sinf(yawR),
        g_orbitDist * (-sp),
        g_orbitDist * cp * cosf(yawR));
    return glm::lookAt(eye, target, glm::vec3(0, 1, 0));
}

static void onResize(GLFWwindow*, int w, int h) {
    g_w = std::max(w, 1);
    g_h = std::max(h, 1);
    glViewport(0, 0, g_w, g_h);
}

/** Prima persona: camera all'altezza della testa. */
static glm::mat4 firstPersonView() {
    float pitchR = glm::radians(g_orbitPitch);
    float yawR = glm::radians(g_orbitYaw);
    glm::vec3 eye = (g_ragdoll.active ? playFocusPos() : g_playerPos)
        + glm::vec3(0.f, g_ragdoll.active ? 1.2f : 4.55f, 0.f);
    glm::vec3 front(
        -sinf(yawR) * cosf(pitchR),
         sinf(pitchR),
        -cosf(yawR) * cosf(pitchR));
    if (glm::dot(front, front) < 1e-8f)
        front = glm::vec3(0.f, 0.f, -1.f);
    else
        front = glm::normalize(front);
    return glm::lookAt(eye, eye + front, glm::vec3(0.f, 1.f, 0.f));
}

/** Camera Play/Test: prima persona oppure terza (orbita / shift lock). */
static glm::mat4 playCameraView() {
    if (g_firstPerson) return firstPersonView();
    return thirdPersonView();
}

static void syncPlayerYawToCamera() {
    g_playerYaw = yawFromDir(camFlatForward());
}

static void applyMouseLook(float dx, float dy) {
    // In Play: orbita / FPS. In Studio fly: g_cam.
    bool fp = g_firstPerson;
    float sens = (g_shiftLock || fp) ? 0.20f : 0.16f;
    float pitchSens = (g_shiftLock || fp) ? 0.15f : 0.13f;
    float pitchMin = fp ? -85.f : -75.f;
    float pitchMax = fp ? 80.f : 55.f;
    if (g_screen == AppScreen::Playing ||
        (g_screen == AppScreen::Studio && g_mode == CamMode::Play)) {
        // Mouse destra = guarda a destra
        g_orbitYaw   -= dx * sens;
        g_orbitPitch  = glm::clamp(g_orbitPitch + dy * pitchSens, pitchMin, pitchMax);
        // Corpo segue la visuale SOLO in Shift Lock / prima persona (non in orbita libera)
        if ((g_shiftLock || fp) && (g_screen == AppScreen::Playing || g_studioTesting))
            syncPlayerYawToCamera();
    } else if (g_screen == AppScreen::Studio && g_mode == CamMode::Studio) {
        g_cam.yaw -= dx * 0.12f;
        g_cam.pitch = glm::clamp(g_cam.pitch + dy * 0.12f, -89.f, 89.f);
    } else if (g_screen == AppScreen::AvatarPublisher) {
        g_catalog.camYaw -= dx * 0.25f;
        g_catalog.camPitch = glm::clamp(g_catalog.camPitch + dy * 0.2f, -80.f, 60.f);
    }
}

// Gizmo Catalog (stesso stile Studio: frecce RGB / anelli / scale)
static bool g_accGizmoDrag = false;
static int g_accGizmoAxis = -1; // -1 = rotazione libera (trackball)
static glm::vec3 g_accGizmoStartOff{0}, g_accGizmoStartRot{0}, g_accGizmoStartScl{1};
static glm::vec3 g_accGizmoStartHit{0};
static float g_accGizmoStartAngle = 0.f;
static float g_accGizmoStartMx = 0.f, g_accGizmoStartMy = 0.f;
static glm::mat4 g_accGizmoPartWorld{1.f};
static constexpr float kCatGizmoYaw = 180.f;

static bool pickCatalogMoveAxis(float mx, float my, int& outAxis);
static bool pickCatalogRotateAxis(float mx, float my, int& outAxis);
static bool pickCatalogScaleAxis(float mx, float my, int& outAxis);
static void beginCatalogGizmo(int axis, float mx, float my);
static void updateCatalogGizmo(float mx, float my);
static void endCatalogGizmo();
static void drawCatalogGizmos(GLuint vao, GLuint partP, const glm::mat4& view,
                              const glm::mat4& proj, const glm::vec3& sun);

static bool playCameraActive() {
    return ((g_screen == AppScreen::Playing) && !g_playMenuOpen)
        || (g_screen == AppScreen::Studio && g_studioTesting && !g_playMenuOpen);
}

static void syncPlayCursor(GLFWwindow* win);

static void onScroll(GLFWwindow* win, double /*x*/, double yoff) {
    const bool inPlay = playCameraActive()
        || (g_screen == AppScreen::Studio && g_studioTesting);
    // In Play/Test non lasciare che ImGui blocchi zoom / inventario
    if (!playCameraActive() && ImGui::GetIO().WantCaptureMouse) return;
    // Shift + rotella = switch inventario (PC)
    if (inPlay && win
        && (glfwGetKey(win, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS
            || glfwGetKey(win, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS)) {
        if (yoff > 0.0) g_world.cycleBackpack(-1);
        else if (yoff < 0.0) g_world.cycleBackpack(+1);
        return;
    }
    if (g_screen == AppScreen::AvatarPublisher) {
        g_catalog.camDist = glm::clamp(g_catalog.camDist - (float)yoff * 0.9f, 4.f, 40.f);
    } else if (g_screen == AppScreen::Playing ||
               (g_screen == AppScreen::Studio && g_mode == CamMode::Play)) {
        // Zoom: avvicina fino a entrare in prima persona; allontana per uscire
        if (g_firstPerson) {
            if (yoff < 0.0) {
                g_firstPerson = false;
                g_orbitDist = glm::clamp(kFpExitDist - (float)yoff * 0.8f, kFpEnterDist, 40.f);
                syncPlayCursor(win);
            }
        } else {
            float next = g_orbitDist - (float)yoff * 0.8f;
            if (yoff > 0.0 && next <= kFpEnterDist) {
                g_firstPerson = true;
                g_shiftLock = false;
                g_playerYaw = yawFromDir(camFlatForward());
                g_orbitDist = kFpEnterDist;
                syncPlayCursor(win);
            } else {
                g_orbitDist = glm::clamp(next, kFpEnterDist, 40.f);
            }
        }
    }
}

static void onMouse(GLFWwindow*, double x, double y) {
    // Terza persona: look solo con RMB. Prima persona: look sempre (cursore locked).
    const bool fpLook = playCameraActive() && g_firstPerson && !g_playMenuOpen && !g_showChat;
    const bool looking = fpLook || g_rmbLook;
    if (!looking && ImGui::GetIO().WantCaptureMouse) return;
    if (!looking) return;
    if (g_firstMouse) { g_lastMx = (float)x; g_lastMy = (float)y; g_firstMouse = false; return; }
    float dx = (float)x - g_lastMx;
    float dy = g_lastMy - (float)y;
    g_lastMx = (float)x;
    g_lastMy = (float)y;
    applyMouseLook(dx, dy);
}

/** Cursore: libero in 3ª persona; locked in 1ª (o RMB look). */
static void syncPlayCursor(GLFWwindow* win) {
    if (!win) return;
    const bool inPlay = (g_screen == AppScreen::Playing)
        || (g_screen == AppScreen::Studio && g_studioTesting);
    bool lock = false;
    if (inPlay && !g_playMenuOpen && !g_showChat) {
        if (g_firstPerson) lock = true;
        else if (g_rmbLook) lock = true;
    }
    if (lock == g_playCursorLocked) return;
    g_playCursorLocked = lock;
    g_firstMouse = true;
    glfwSetInputMode(win, GLFW_CURSOR,
        lock ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    if (lock && ImGui::GetCurrentContext()) {
        ImGuiIO& io = ImGui::GetIO();
        io.ClearInputKeys();
        io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
    }
}

static void setPlayCursor(GLFWwindow* win, bool playing) {
    // Compat: entering/leaving play — stato reale da syncPlayCursor
    if (!playing) {
        g_playCursorLocked = true; // forza refresh
        g_rmbLook = false;
        g_playCursorLocked = false;
        g_firstMouse = true;
        glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        return;
    }
    g_playCursorLocked = !g_playCursorLocked; // force edge
    syncPlayCursor(win);
}

/** Puntino mirino al centro (solo prima persona). */
static void drawFpCrosshair() {
    if (!g_firstPerson) return;
    if (!(g_screen == AppScreen::Playing
          || (g_screen == AppScreen::Studio && g_studioTesting)))
        return;
    if (g_playMenuOpen) return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const float cx = (float)g_w * 0.5f;
    const float cy = (float)g_h * 0.5f;
    dl->AddCircleFilled({cx, cy}, 3.2f, IM_COL32(255, 255, 255, 230), 14);
    dl->AddCircle({cx, cy}, 3.2f, IM_COL32(0, 0, 0, 180), 14, 1.4f);
    dl->AddCircleFilled({cx, cy}, 1.2f, IM_COL32(0, 0, 0, 200), 10);
}

/** Ray vs AABB (centro + size stile Roblox). Restituisce distanza t se colpito. */
static bool rayHitAabb(const glm::vec3& o, const glm::vec3& d,
                       const glm::vec3& center, const glm::vec3& size, float& tOut) {
    glm::vec3 half = size * 0.5f;
    glm::vec3 bmin = center - half;
    glm::vec3 bmax = center + half;
    float tmin = 0.f, tmax = 1e6f;
    for (int i = 0; i < 3; ++i) {
        if (std::fabs(d[i]) < 1e-8f) {
            if (o[i] < bmin[i] || o[i] > bmax[i]) return false;
            continue;
        }
        float inv = 1.f / d[i];
        float t0 = (bmin[i] - o[i]) * inv;
        float t1 = (bmax[i] - o[i]) * inv;
        if (t0 > t1) std::swap(t0, t1);
        tmin = std::max(tmin, t0);
        tmax = std::min(tmax, t1);
        if (tmax < tmin) return false;
    }
    if (tmax < 0.f) return false;
    tOut = tmin >= 0.f ? tmin : tmax;
    return true;
}

static void screenToRay(float mx, float my, const glm::mat4& view, const glm::mat4& proj,
                        glm::vec3& origin, glm::vec3& dir) {
    float nx = (2.f * mx) / (float)g_w - 1.f;
    float ny = 1.f - (2.f * my) / (float)g_h;
    glm::mat4 inv = glm::inverse(proj * view);
    glm::vec4 n4 = inv * glm::vec4(nx, ny, -1.f, 1.f);
    glm::vec4 f4 = inv * glm::vec4(nx, ny,  1.f, 1.f);
    n4 /= n4.w;
    f4 /= f4.w;
    origin = glm::vec3(n4);
    dir = glm::normalize(glm::vec3(f4 - n4));
}

static void syncPartEditors() {
    if (g_selectedPart < 0 || g_selectedPart >= (int)g_world.parts.size()) {
        g_partNameBuf[0] = 0;
        g_partScriptBuf[0] = 0;
        return;
    }
    const auto& p = g_world.parts[g_selectedPart];
    std::snprintf(g_partNameBuf, sizeof(g_partNameBuf), "%s", p.name.c_str());
    std::snprintf(g_partScriptBuf, sizeof(g_partScriptBuf), "%s", p.script.c_str());
}

static void clearPlayerSelection() {
    g_selectedPlayer = false;
    g_selectedPlayerPart = -1;
}

static void clearSpecialSelection() {
    g_selectedBaseplate = false;
    g_selectedSpawn = false;
}

static void clearAllSelection() {
    g_selectedPart = -1;
    g_selectedModel = -1;
    g_selectedSound = -1;
    g_selectedLabel = -1;
    g_selectedService = -1;
    clearPlayerSelection();
    clearSpecialSelection();
    g_editingPartScript = false;
    g_scaleDragging = g_moveDragging = g_rotateDragging = false;
}

static void selectService(int id) {
    clearAllSelection();
    g_selectedService = id;
    g_showProps = true;
}

static void selectPart(int idx) {
    g_selectedPart = idx;
    if (idx >= 0) {
        g_selectedModel = -1;
        g_selectedSound = -1;
        g_selectedLabel = -1;
        clearPlayerSelection();
        clearSpecialSelection();
        g_showProps = true;
    }
    g_scaleDragging = false;
    g_moveDragging = false;
    g_rotateDragging = false;
    syncPartEditors();
}

static void selectBaseplate() {
    clearAllSelection();
    g_selectedBaseplate = true;
    g_showProps = true;
}

static void selectSpawn() {
    clearAllSelection();
    g_selectedSpawn = true;
    g_showProps = true;
}

/** Palette colori rapida stile BrickColor. */
static void drawPartColorPicker(glm::vec3& color) {
    ImGui::TextColored({0.15f, 0.55f, 0.95f, 1.f}, "Colore");
    ImGui::ColorEdit3("##partcolor", &color.x,
        ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel
        | ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_DisplayRGB);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);
    ImGui::ColorEdit3("RGB", &color.x, ImGuiColorEditFlags_Float);

    ImGui::TextDisabled("Preset:");
    struct Swatch { const char* name; glm::vec3 c; };
    static const Swatch kSwatches[] = {
        {"Bianco",  {0.95f, 0.95f, 0.97f}},
        {"Grigio",  {0.64f, 0.64f, 0.64f}},
        {"Nero",    {0.12f, 0.12f, 0.14f}},
        {"Rosso",   {0.90f, 0.18f, 0.18f}},
        {"Arancio", {0.95f, 0.50f, 0.12f}},
        {"Giallo",  {0.95f, 0.85f, 0.15f}},
        {"Verde",   {0.22f, 0.72f, 0.32f}},
        {"Cyan",    {0.15f, 0.78f, 0.85f}},
        {"Blu",     {0.18f, 0.45f, 0.92f}},
        {"Viola",   {0.55f, 0.28f, 0.85f}},
        {"Rosa",    {0.95f, 0.45f, 0.70f}},
        {"Marrone", {0.55f, 0.32f, 0.18f}},
    };
    for (int i = 0; i < (int)(sizeof(kSwatches) / sizeof(kSwatches[0])); ++i) {
        if (i % 6 != 0) ImGui::SameLine();
        ImGui::PushID(i);
        ImVec4 col{kSwatches[i].c.x, kSwatches[i].c.y, kSwatches[i].c.z, 1.f};
        if (ImGui::ColorButton(kSwatches[i].name, col, ImGuiColorEditFlags_NoTooltip, {28, 22}))
            color = kSwatches[i].c;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", kSwatches[i].name);
        ImGui::PopID();
    }
}

static void selectModel(int idx) {
    g_selectedModel = idx;
    g_selectedPart = -1;
    g_selectedSound = -1;
    g_selectedLabel = -1;
    clearPlayerSelection();
    clearSpecialSelection();
    g_editingPartScript = false;
    g_scaleDragging = g_moveDragging = g_rotateDragging = false;
    if (idx >= 0) g_showProps = true;
}

static void selectSound(int idx) {
    g_selectedSound = idx;
    g_selectedPart = -1;
    g_selectedModel = -1;
    g_selectedLabel = -1;
    clearPlayerSelection();
    clearSpecialSelection();
    g_editingPartScript = false;
    if (idx >= 0 && idx < (int)g_sounds.sounds.size())
        std::snprintf(g_soundPathBuf, sizeof(g_soundPathBuf), "%s",
            g_sounds.sounds[idx].soundId.c_str());
    if (idx >= 0) g_showProps = true;
}

static void selectLabel(int idx) {
    clearAllSelection();
    g_selectedLabel = idx;
    if (idx >= 0 && idx < (int)g_world.labels.size()) {
        const auto& l = g_world.labels[idx];
        std::snprintf(g_labelNameBuf, sizeof(g_labelNameBuf), "%s", l.name.c_str());
        std::snprintf(g_labelTextBuf, sizeof(g_labelTextBuf), "%s", l.text.c_str());
        g_showProps = true;
    }
}

static void selectPlayer(int partIdx = -1) {
    g_selectedPart = -1;
    g_selectedModel = -1;
    g_selectedSound = -1;
    g_selectedLabel = -1;
    clearSpecialSelection();
    g_editingPartScript = false;
    g_selectedPlayer = true;
    g_selectedPlayerPart = partIdx;
    g_showProps = true;
    g_scaleDragging = g_moveDragging = g_rotateDragging = false;
}

static void openPlayerRigEditor() {
    g_player.humanoid.userRigged = true;
    g_player.humanoid.rigEnabled = true;
    g_showPlayerRigPrompt = false;
    g_showPlayerRigEditor = true;
    g_playerRigEditorNeedsPlace = true;
    if (!g_selectedPlayer) selectPlayer(-1);
    g_exportMsg = "Rig Player aperto — modifica arti R6";
}

static void beginRigPlayer() {
    selectPlayer(-1);
    if (g_player.humanoid.userRigged) {
        openPlayerRigEditor();
        return;
    }
    g_showPlayerRigPrompt = true;
    g_exportMsg = "Player non riggato — conferma per creare il Rig R6";
}

static void confirmPlayerRig(bool yes) {
    g_showPlayerRigPrompt = false;
    if (!yes) {
        g_exportMsg = "Rig Player annullato";
        return;
    }
    openPlayerRigEditor();
}

/** Nome file/modello sembra un'arma da mettere in Backpack. */
static bool nameLooksLikeGunItem(std::string s) {
    for (char& c : s)
        c = (char)std::tolower((unsigned char)c);
    return s.find("pistola") != std::string::npos
        || s.find("pistol") != std::string::npos
        || s.find("gun") != std::string::npos
        || s.find("fucile") != std::string::npos
        || s.find("shotgun") != std::string::npos
        || s.find("pompa") != std::string::npos
        || s.find("revolver") != std::string::npos;
}

static std::string stripFileExt(std::string s) {
    auto slash = s.find_last_of("\\/");
    if (slash != std::string::npos) s = s.substr(slash + 1);
    auto dot = s.find_last_of('.');
    if (dot != std::string::npos) s = s.substr(0, dot);
    return s;
}

/** Dopo import da Blender: se e' una pistola, chiedi se metterla nel Backpack. */
static void offerBackpackForGunImport(const std::string& path) {
    if (!nameLooksLikeGunItem(path)) return;
    g_backpackOfferName = stripFileExt(path);
    if (g_backpackOfferName.empty()) g_backpackOfferName = "Pistola";
    g_backpackOfferPath = path;
    g_showBackpackOffer = true;
}

static void confirmBackpackOffer(bool yes) {
    g_showBackpackOffer = false;
    if (!yes) {
        g_exportMsg = "Ok — " + g_backpackOfferName + " resta solo come modello (non in Backpack)";
        g_backpackOfferName.clear();
        g_backpackOfferPath.clear();
        return;
    }
    auto& it = g_world.addBackpackItem(g_backpackOfferName, 4, BackpackTool::Pistol);
    if (!g_backpackOfferPath.empty())
        it.meshPath = g_backpackOfferPath;
    g_selectedBackpack = (int)g_world.backpack.size() - 1;
    g_selectedPart = -1;
    g_selectedModel = -1;
    g_showExplorer = true;
    g_showProps = true;
    g_exportMsg = "\"" + it.name + "\" aggiunta al Backpack (tool Pistol) — slot 1-9 in Test";
    g_backpackOfferName.clear();
    g_backpackOfferPath.clear();
}

static void bindLuaServices() {
    g_lua.soundService = &g_sounds;
    g_lua.lighting = &g_lighting;
    g_lua.animator = &g_animator;
    g_lua.particles = &g_particles;
    g_lua.net = &g_net;
    g_lua.playerHumanoid = &g_player.humanoid;
    g_lua.playerPos = &g_playerPos;
    g_lua.playerYaw = &g_playerYaw;
}

static void applyModelAnimSample(WorkspaceModel& m, float t) {
    auto k = m.anim.sample(t);
    m.position = k.position;
    m.rotation = k.rotation;
    m.scale = k.scale;
}

static void openModelAnimEditor(int modelIdx) {
    if (modelIdx < 0 || modelIdx >= (int)g_world.models.size()) return;
    auto& m = g_world.models[modelIdx];
    // Per animare TRS serve sempre un Rig Root (anche se il file aveva ossa)
    if (!m.rig.simpleRig) {
        m.rig.simpleRig = true;
        m.rig.rootName = "Root";
    }
    m.anim.ensureSeed(m.position, m.rotation, m.scale);
    m.anim.duration = std::max(m.anim.duration, 0.5f);
    if (m.anim.name.empty()) m.anim.name = "Anim";
    g_animEditModel = modelIdx;
    g_animSelKey = 0;
    g_animScrub = 0.f;
    g_animPlaying = false;
    g_showAnimEditor = true;
    g_showRigPrompt = false;
    g_animEditorNeedsPlace = true;
    applyModelAnimSample(m, 0.f);
    selectModel(modelIdx);
    g_tool = StudioTool::Move; // Move pronto per posare i keyframe
    g_exportMsg = "Anima: " + m.name + " — muovi il Model, poi Set Frame (K)";
}

static void askRigOrOpenAnimator(int modelIdx) {
    if (modelIdx < 0 || modelIdx >= (int)g_world.models.size()) return;
    selectModel(modelIdx);
    auto& m = g_world.models[modelIdx];
    // Già pronto (Root o scheletro file) → editor subito
    if (m.rig.hasAnimRig()) {
        openModelAnimEditor(modelIdx);
        return;
    }
    g_animEditModel = modelIdx;
    g_showRigPrompt = true;
    g_showAnimEditor = false;
    g_exportMsg = "Serve un Rig Root per animare questo Model";
}

static void beginAnimateSelectedModel() {
    if (g_selectedModel < 0 || g_selectedModel >= (int)g_world.models.size()) {
        if (!g_world.models.empty()) {
            selectModel(0);
        } else {
            g_exportMsg = "Inserisci un Model (toolbar Model), poi Anima";
            g_showExplorer = true;
            g_showProps = true;
            return;
        }
    }
    askRigOrOpenAnimator(g_selectedModel);
}

static void confirmModelRig(bool yes) {
    int idx = g_animEditModel;
    g_showRigPrompt = false;
    if (idx < 0 || idx >= (int)g_world.models.size()) {
        g_animEditModel = -1;
        return;
    }
    if (!yes) {
        // Non bloccare: apri comunque con Rig Root (altrimenti l'utente resta bloccato)
        g_exportMsg = "Apro comunque con Rig Root";
        openModelAnimEditor(idx);
        return;
    }
    auto& m = g_world.models[idx];
    m.rig.simpleRig = true;
    m.rig.rootName = "Root";
    g_exportMsg = "Rig Root creato — puoi animare " + m.name;
    openModelAnimEditor(idx);
}

static float g_modelAnimClock = 0.f;

static void updateModelAnimPlayback(float dt) {
    // Editor: scrub sul model selezionato
    if (g_animPlaying && g_animEditModel >= 0 && g_animEditModel < (int)g_world.models.size()) {
        auto& m = g_world.models[g_animEditModel];
        g_animScrub += dt;
        if (m.anim.looped) {
            float dur = std::max(0.001f, m.anim.duration);
            if (g_animScrub > dur) g_animScrub = std::fmod(g_animScrub, dur);
        } else if (g_animScrub >= m.anim.duration) {
            g_animScrub = m.anim.duration;
            g_animPlaying = false;
        }
        applyModelAnimSample(m, g_animScrub);
    }

    // Play / Test: riproduci TUTTI i model che hanno keyframe (anche senza editor aperto)
    const bool inPlay = (g_screen == AppScreen::Playing)
        || (g_screen == AppScreen::Studio && g_studioTesting);
    if (!inPlay) return;
    g_modelAnimClock += dt;
    for (int i = 0; i < (int)g_world.models.size(); ++i) {
        if (g_showAnimEditor && i == g_animEditModel && g_animPlaying)
            continue; // già gestito sopra
        auto& m = g_world.models[i];
        // Solo clip con movimento vero: le due key identiche del seed non
        // devono riportare il Model dov'era prima di spostarlo/scalarlo
        if (!m.anim.hasMotion()) continue;
        float dur = std::max(0.001f, m.anim.duration);
        float t = m.anim.looped ? std::fmod(g_modelAnimClock, dur)
                                : std::min(g_modelAnimClock, dur);
        applyModelAnimSample(m, t);
    }
}

static bool rayPlane(const glm::vec3& o, const glm::vec3& d,
                     const glm::vec3& planeP, const glm::vec3& planeN, float& tOut) {
    float denom = glm::dot(planeN, d);
    if (std::fabs(denom) < 1e-6f) return false;
    float t = glm::dot(planeN, planeP - o) / denom;
    if (t < 0.f) return false;
    tOut = t;
    return true;
}

static float angleAroundAxis(const glm::vec3& center, const glm::vec3& axisN,
                             const glm::vec3& point) {
    glm::vec3 a = glm::normalize(axisN);
    glm::vec3 v = point - center;
    v -= a * glm::dot(v, a);
    if (glm::length(v) < 1e-5f) return 0.f;
    glm::vec3 ref = glm::abs(a.y) < 0.9f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
    glm::vec3 t = glm::normalize(glm::cross(a, ref));
    glm::vec3 b = glm::cross(a, t);
    return std::atan2(glm::dot(v, b), glm::dot(v, t));
}

static float rayDistToPoint(const glm::vec3& o, const glm::vec3& d, const glm::vec3& p) {
    glm::vec3 w = p - o;
    float tt = glm::dot(w, d);
    if (tt < 0.f) return 1e9f;
    return glm::length(w - d * tt);
}

static glm::vec3 partAxisDir(const WorkspacePart& p, int axis) {
    glm::vec3 local(0); local[axis] = 1.f;
    return glm::normalize(glm::vec3(partRotationMatrix(p.rotation) * glm::vec4(local, 0.f)));
}

static float gizmoRingRadius(const WorkspacePart& p) {
    float m = std::max(p.size.x, std::max(p.size.y, p.size.z)) * 0.5f;
    return std::clamp(m + 2.4f, 3.2f, 14.f);
}

static float gizmoArrowLen(const WorkspacePart& p) {
    float m = std::max(p.size.x, std::max(p.size.y, p.size.z)) * 0.5f;
    return std::clamp(m + 2.8f, 3.5f, 12.f);
}

/** Matrice: asse Z lungo `dir`, origine `origin`, scale (x,y,z). */
static glm::mat4 orientAlongZ(glm::vec3 origin, glm::vec3 dir, glm::vec3 scl) {
    dir = glm::normalize(dir);
    glm::vec3 up = (std::fabs(dir.y) < 0.92f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
    glm::vec3 right = glm::normalize(glm::cross(up, dir));
    up = glm::normalize(glm::cross(dir, right));
    glm::mat4 m(1.f);
    m[0] = glm::vec4(right * scl.x, 0.f);
    m[1] = glm::vec4(up * scl.y, 0.f);
    m[2] = glm::vec4(dir * scl.z, 0.f);
    m[3] = glm::vec4(origin, 1.f);
    return m;
}

static glm::vec3 handleWorldPos(const WorkspacePart& p, int axis, int sign) {
    glm::vec3 local(0);
    local[axis] = sign * (p.size[axis] * 0.5f + kHandleSize * 0.65f);
    return p.position + glm::vec3(partRotationMatrix(p.rotation) * glm::vec4(local, 0.f));
}

static glm::vec3 moveHandleTip(const WorkspacePart& p, int axis) {
    return p.position + partAxisDir(p, axis) * gizmoArrowLen(p);
}

// ————— Gizmo su Model importati —————
// I gizmo sono scritti per WorkspacePart. Per muovere/ruotare/scalare anche i
// Model si usa una Part "proxy" che ne rispecchia position/rotation/dimensione;
// a fine drag i valori tornano sul Model (la size diventa scale).

static WorkspacePart g_modelGizmoProxy;

static bool gizmoOnModel() {
    return g_selectedPart < 0 && g_selectedModel >= 0
        && g_selectedModel < (int)g_world.models.size();
}

/** Dimensione locale della mesh (bounds), 2 unità se non caricata. */
static glm::vec3 modelBaseExtent(int idx) {
    glm::vec3 ext{2.f};
    if (idx >= 0 && idx < (int)g_wsMeshes.size() && g_wsMeshes[idx].loaded)
        ext = glm::abs(g_wsMeshes[idx].boundsMax - g_wsMeshes[idx].boundsMin);
    return glm::max(ext, glm::vec3(0.05f));
}

/** Part su cui agiscono i gizmo: quella selezionata, oppure la proxy del Model. */
static WorkspacePart* gizmoPart() {
    if (g_selectedPart >= 0 && g_selectedPart < (int)g_world.parts.size())
        return &g_world.parts[g_selectedPart];
    if (!gizmoOnModel()) return nullptr;
    const auto& m = g_world.models[g_selectedModel];
    g_modelGizmoProxy.position = m.position;
    g_modelGizmoProxy.rotation = m.rotation;
    g_modelGizmoProxy.size = modelBaseExtent(g_selectedModel) * m.scale;
    return &g_modelGizmoProxy;
}

/**
 * I Model si scalano in modo continuo: lo snap a 0.25 e il minimo di 0.5 unità
 * delle Part schiaccerebbero un modello sottile (es. una pedana alta 0.1).
 */
static float gizmoMinSize() { return gizmoOnModel() ? 0.02f : 0.5f; }
static bool gizmoSnapSize() { return !gizmoOnModel(); }

/** Riporta sul Model quello che il drag ha scritto sulla proxy. */
static void gizmoCommitToModel() {
    if (!gizmoOnModel()) return;
    auto& m = g_world.models[g_selectedModel];
    m.position = g_modelGizmoProxy.position;
    m.rotation = g_modelGizmoProxy.rotation;
    m.scale = glm::max(g_modelGizmoProxy.size / modelBaseExtent(g_selectedModel),
                       glm::vec3(0.01f));
    // Se il Model ha solo le key "seed" (ancora ferme), portale sul nuovo
    // transform: altrimenti in Test l'animazione lo rimetterebbe com'era
    m.anim.reseedIfStatic(m.position, m.rotation, m.scale);
    markPlaceDirty();
}

static bool gizmoShift() {
    return glfwGetKey(glfwGetCurrentContext(), GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS
        || glfwGetKey(glfwGetCurrentContext(), GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
}
static bool gizmoCtrl() {
    return glfwGetKey(glfwGetCurrentContext(), GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS
        || glfwGetKey(glfwGetCurrentContext(), GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
}

static float snapValue(float v, float step) {
    if (step <= 1e-6f) return v;
    return std::round(v / step) * step;
}

static void catalogAnimParams(float& bob, float& armL, float& armR, float& legL, float& legR) {
    const auto& hum = g_player.humanoid;
    bob = humanoidBob(hum);
    float armBase = humanoidArmSwing(hum);
    float jL = 0.f, jR = 0.f, jl = 0.f, jr = 0.f;
    humanoidJumpPose(hum, jL, jR, jl, jr);
    const float jb = hum.jumpArmBlend;
    armL = armBase * (1.f - jb) + jL;
    armR = -armBase * (1.f - jb) + jR;
    legL = humanoidLegSwing(hum) * (1.f - jb) + jl;
    legR = -humanoidLegSwing(hum) * (1.f - jb) + jr;
}

static glm::mat4 catalogRootMat() {
    return glm::rotate(glm::translate(glm::mat4(1.f), glm::vec3(0)),
                       glm::radians(kCatGizmoYaw), glm::vec3(0, 1, 0));
}

static bool catalogSelectedAcc(AvatarAccessory*& out) {
    if (g_catalog.selected < 0 || g_catalog.selected >= (int)g_catalog.items.size())
        return false;
    out = &g_catalog.items[g_catalog.selected];
    return true;
}

static glm::vec3 catalogAccWorldOrigin(const AvatarAccessory& a) {
    float bob, armL, armR, legL, legR;
    catalogAnimParams(bob, armL, armR, legL, legR);
    glm::mat4 partW = catalogRootMat()
        * accessoryPartMatrix(a, g_player.humanoid, bob, armL, armR, legL, legR);
    return glm::vec3(partW * glm::vec4(a.offset, 1.f));
}

static float catalogGizmoSize() { return 2.8f; }

static bool pickCatalogMoveAxis(float mx, float my, int& outAxis) {
    AvatarAccessory* ap = nullptr;
    if (!catalogSelectedAcc(ap)) return false;
    glm::vec3 o, d;
    screenToRay(mx, my, g_lastView, g_lastProj, o, d);
    glm::vec3 origin = catalogAccWorldOrigin(*ap);
    float len = catalogGizmoSize();
    float bestT = 1e9f;
    bool hit = false;
    for (int axis = 0; axis < 3; ++axis) {
        glm::vec3 axisV(0); axisV[axis] = 1.f;
        glm::vec3 tip = origin + axisV * len;
        float t = 0.f;
        if (rayHitAabb(o, d, tip, glm::vec3(0.45f), t) && t < bestT) {
            bestT = t; outAxis = axis; hit = true;
        }
        for (int k = 1; k <= 5; ++k) {
            glm::vec3 pt = origin + axisV * (len * (float)k / 6.f);
            if (rayHitAabb(o, d, pt, glm::vec3(0.22f), t) && t < bestT) {
                bestT = t; outAxis = axis; hit = true;
            }
        }
    }
    return hit;
}

static bool pickCatalogRotateAxis(float mx, float my, int& outAxis) {
    AvatarAccessory* ap = nullptr;
    if (!catalogSelectedAcc(ap)) return false;
    glm::vec3 o, d;
    screenToRay(mx, my, g_lastView, g_lastProj, o, d);
    glm::vec3 origin = catalogAccWorldOrigin(*ap);
    float R = catalogGizmoSize() * 0.95f;
    float best = 1e9f;
    bool hit = false;
    for (int axis = 0; axis < 3; ++axis) {
        glm::vec3 axisV(0); axisV[axis] = 1.f;
        // Solo distanza raggio↔anello (affidabile anche se l'anello è di taglio)
        glm::vec3 ref = (std::fabs(axisV.y) < 0.9f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
        glm::vec3 u = glm::normalize(glm::cross(axisV, ref));
        glm::vec3 v = glm::normalize(glm::cross(axisV, u));
        float err = 1e9f;
        for (int i = 0; i < 64; ++i) {
            float a = (float)i / 64.f * 6.2831853f;
            glm::vec3 p = origin + (u * std::cos(a) + v * std::sin(a)) * R;
            err = std::min(err, rayDistToPoint(o, d, p));
        }
        if (err < 0.45f && err < best) { best = err; outAxis = axis; hit = true; }
    }
    return hit;
}

// Area gizmo → rotazione libera (mouse X/Y su pitch+yaw)
static bool pickCatalogRotateFree(float mx, float my) {
    AvatarAccessory* ap = nullptr;
    if (!catalogSelectedAcc(ap)) return false;
    glm::vec3 o, d;
    screenToRay(mx, my, g_lastView, g_lastProj, o, d);
    glm::vec3 origin = catalogAccWorldOrigin(*ap);
    float R = catalogGizmoSize() * 1.05f;
    return rayDistToPoint(o, d, origin) < R;
}

static bool pickCatalogScaleAxis(float mx, float my, int& outAxis) {
    AvatarAccessory* ap = nullptr;
    if (!catalogSelectedAcc(ap)) return false;
    glm::vec3 o, d;
    screenToRay(mx, my, g_lastView, g_lastProj, o, d);
    glm::vec3 origin = catalogAccWorldOrigin(*ap);
    float len = catalogGizmoSize() * 0.75f;
    float bestT = 1e9f;
    bool hit = false;
    for (int axis = 0; axis < 3; ++axis) {
        glm::vec3 axisV(0); axisV[axis] = 1.f;
        glm::vec3 hp = origin + axisV * len;
        float t = 0.f;
        if (rayHitAabb(o, d, hp, glm::vec3(0.4f), t) && t < bestT) {
            bestT = t; outAxis = axis; hit = true;
        }
    }
    return hit;
}

static void beginCatalogGizmo(int axis, float mx, float my) {
    AvatarAccessory* ap = nullptr;
    if (!catalogSelectedAcc(ap)) return;
    float bob, armL, armR, legL, legR;
    catalogAnimParams(bob, armL, armR, legL, legR);
    g_accGizmoPartWorld = catalogRootMat()
        * accessoryPartMatrix(*ap, g_player.humanoid, bob, armL, armR, legL, legR);
    g_accGizmoDrag = true;
    g_accGizmoAxis = axis;
    g_accGizmoStartOff = ap->offset;
    g_accGizmoStartRot = ap->rotation;
    g_accGizmoStartScl = ap->scale;
    g_accGizmoStartMx = mx;
    g_accGizmoStartMy = my;
    glm::vec3 origin = catalogAccWorldOrigin(*ap);
    glm::vec3 o, d;
    screenToRay(mx, my, g_lastView, g_lastProj, o, d);
    float t = 0.f;
    if (g_catalog.tool == AccelTool::Rotate) {
        // Rotazione via delta mouse (libera o asse bloccato) — niente piano 3D fragile
        g_accGizmoStartAngle = 0.f;
    } else {
        glm::vec3 axisV(0); axisV[axis] = 1.f;
        glm::vec3 camF = glm::normalize(glm::vec3(glm::inverse(g_lastView)[2]));
        glm::vec3 side = glm::cross(axisV, camF);
        if (glm::length(side) < 1e-4f) side = glm::cross(axisV, glm::vec3(0, 1, 0));
        glm::vec3 planeN = glm::normalize(glm::cross(axisV, glm::normalize(side)));
        if (rayPlane(o, d, origin, planeN, t))
            g_accGizmoStartHit = o + d * t;
        else
            g_accGizmoStartHit = origin;
    }
}

static void updateCatalogGizmo(float mx, float my) {
    if (!g_accGizmoDrag) return;
    AvatarAccessory* ap = nullptr;
    if (!catalogSelectedAcc(ap)) return;
    glm::vec3 origin0 = glm::vec3(g_accGizmoPartWorld * glm::vec4(g_accGizmoStartOff, 1.f));
    glm::vec3 o, d;
    screenToRay(mx, my, g_lastView, g_lastProj, o, d);
    glm::vec3 axisV(0);
    if (g_accGizmoAxis >= 0 && g_accGizmoAxis < 3)
        axisV[g_accGizmoAxis] = 1.f;
    float t = 0.f;

    if (g_catalog.tool == AccelTool::Move) {
        glm::vec3 camF = glm::normalize(glm::vec3(glm::inverse(g_lastView)[2]));
        glm::vec3 side = glm::cross(axisV, camF);
        if (glm::length(side) < 1e-4f) side = glm::cross(axisV, glm::vec3(0, 1, 0));
        glm::vec3 planeN = glm::normalize(glm::cross(axisV, glm::normalize(side)));
        if (!rayPlane(o, d, origin0, planeN, t)) return;
        float delta = glm::dot((o + d * t) - g_accGizmoStartHit, axisV);
        if (!gizmoCtrl()) delta = snapValue(delta, gizmoShift() ? 0.05f : 0.1f);
        glm::vec3 newWorld = origin0 + axisV * delta;
        glm::vec3 local = glm::vec3(glm::inverse(g_accGizmoPartWorld) * glm::vec4(newWorld, 1.f));
        ap->offset = local;
    } else if (g_catalog.tool == AccelTool::Rotate) {
        float dx = mx - g_accGizmoStartMx;
        float dy = my - g_accGizmoStartMy;
        float sens = gizmoShift() ? 0.18f : 0.45f;
        float snap = gizmoShift() ? 1.f : 5.f;
        ap->rotation = g_accGizmoStartRot;
        if (g_accGizmoAxis < 0) {
            // Libera: orizzontale = yaw, verticale = pitch (come orientare un oggetto a mano)
            float dYaw = dx * sens;
            float dPitch = dy * sens;
            if (!gizmoCtrl()) {
                dYaw = snapValue(dYaw, snap);
                dPitch = snapValue(dPitch, snap);
            }
            ap->rotation.y = g_accGizmoStartRot.y + dYaw;
            ap->rotation.x = g_accGizmoStartRot.x + dPitch;
        } else {
            // Anello RGB: un solo asse. X→verticale, Y/Z→orizzontale
            float deltaDeg = (g_accGizmoAxis == 0 ? dy : dx) * sens;
            if (!gizmoCtrl()) deltaDeg = snapValue(deltaDeg, snap);
            ap->rotation[g_accGizmoAxis] = g_accGizmoStartRot[g_accGizmoAxis] + deltaDeg;
        }
    } else { // Scale
        glm::vec3 camF = glm::normalize(glm::vec3(glm::inverse(g_lastView)[2]));
        glm::vec3 side = glm::cross(axisV, camF);
        if (glm::length(side) < 1e-4f) side = glm::cross(axisV, glm::vec3(0, 1, 0));
        glm::vec3 planeN = glm::normalize(glm::cross(axisV, glm::normalize(side)));
        if (!rayPlane(o, d, origin0, planeN, t)) return;
        float delta = glm::dot((o + d * t) - g_accGizmoStartHit, axisV);
        float mul = 1.f + delta * 0.35f;
        mul = std::clamp(mul, 0.15f, 6.f);
        if (!gizmoCtrl()) mul = snapValue(mul, gizmoShift() ? 0.05f : 0.1f);
        ap->scale = g_accGizmoStartScl;
        ap->scale[g_accGizmoAxis] = std::clamp(g_accGizmoStartScl[g_accGizmoAxis] * mul, 0.05f, 8.f);
    }
}

static void endCatalogGizmo() {
    if (g_accGizmoDrag)
        saveEquippedAvatar(g_catalog);
    g_accGizmoDrag = false;
}

static void drawCatalogGizmos(GLuint vao, GLuint partP, const glm::mat4& view,
                              const glm::mat4& proj, const glm::vec3& sun) {
    AvatarAccessory* ap = nullptr;
    if (!catalogSelectedAcc(ap)) return;
    glm::vec3 origin = catalogAccWorldOrigin(*ap);
    float len = catalogGizmoSize();
    const glm::vec3 cols[3] = {
        {0.95f, 0.22f, 0.20f}, {0.22f, 0.88f, 0.32f}, {0.28f, 0.52f, 1.00f}
    };

    drawCube(vao, partP, glm::scale(glm::translate(glm::mat4(1.f), origin), glm::vec3(0.28f)),
             view, proj, glm::vec3(0.95f), sun, false, 0);

    if (g_catalog.tool == AccelTool::Move) {
        for (int axis = 0; axis < 3; ++axis) {
            bool hot = g_accGizmoDrag && g_accGizmoAxis == axis;
            glm::vec3 col = hot ? glm::mix(cols[axis], glm::vec3(1.f), 0.4f) : cols[axis];
            glm::vec3 dir(0); dir[axis] = 1.f;
            float stemLen = len * 0.78f;
            glm::vec3 mid = origin + dir * (stemLen * 0.5f);
            float thick = hot ? 0.14f : 0.10f;
            drawCube(vao, partP, orientAlongZ(mid, dir, {thick, thick, stemLen}),
                     view, proj, col * 0.92f, sun, false, 0);
            glm::vec3 tip = origin + dir * len;
            float tipS = hot ? 0.48f : 0.38f;
            drawCube(vao, partP, orientAlongZ(tip - dir * (tipS * 0.3f), dir, {tipS, tipS, tipS * 1.3f}),
                     view, proj, col, sun, false, 0);
        }
    } else if (g_catalog.tool == AccelTool::Rotate) {
        float R = len * 0.95f;
        const int segs = 48;
        for (int axis = 0; axis < 3; ++axis) {
            bool hot = g_accGizmoDrag && (g_accGizmoAxis == axis || g_accGizmoAxis < 0);
            glm::vec3 col = hot ? glm::mix(cols[axis], glm::vec3(1.f), g_accGizmoAxis < 0 ? 0.25f : 0.45f)
                                : cols[axis];
            float thick = (hot && g_accGizmoAxis == axis) ? 0.18f : 0.12f;
            glm::vec3 axisV(0); axisV[axis] = 1.f;
            glm::vec3 ref = (std::fabs(axisV.y) < 0.9f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
            glm::vec3 u = glm::normalize(glm::cross(axisV, ref));
            glm::vec3 v = glm::normalize(glm::cross(axisV, u));
            for (int i = 0; i < segs; ++i) {
                float a = (float)i / (float)segs * 6.2831853f;
                glm::vec3 pos = origin + (u * std::cos(a) + v * std::sin(a)) * R;
                drawCube(vao, partP, glm::scale(glm::translate(glm::mat4(1.f), pos), glm::vec3(thick)),
                         view, proj, col, sun, false, 0);
            }
            for (int tick = 0; tick < 24; ++tick) {
                float a = (float)tick / 24.f * 6.2831853f;
                glm::vec3 pos = origin + (u * std::cos(a) + v * std::sin(a)) * (R + 0.22f);
                float ts = (tick % 6 == 0) ? 0.16f : 0.09f;
                drawCube(vao, partP, glm::scale(glm::translate(glm::mat4(1.f), pos), glm::vec3(ts)),
                         view, proj, col * 0.75f, sun, false, 0);
            }
        }
    } else {
        for (int axis = 0; axis < 3; ++axis) {
            bool hot = g_accGizmoDrag && g_accGizmoAxis == axis;
            glm::vec3 col = hot ? glm::mix(cols[axis], glm::vec3(1.f), 0.4f) : cols[axis];
            glm::vec3 dir(0); dir[axis] = 1.f;
            glm::vec3 hp = origin + dir * (len * 0.75f);
            float hs = hot ? 0.48f : 0.36f;
            drawCube(vao, partP, glm::scale(glm::translate(glm::mat4(1.f), hp), glm::vec3(hs)),
                     view, proj, col, sun, false, 0);
            glm::vec3 mid = origin + dir * (len * 0.38f);
            drawCube(vao, partP, orientAlongZ(mid, dir, {0.08f, 0.08f, len * 0.7f}),
                     view, proj, col * 0.7f, sun, false, 0);
        }
    }
}

static bool pickScaleHandle(float mx, float my, int& outAxis, int& outSign) {
    if (g_tool != StudioTool::Scale) return false;
    const WorkspacePart* pt = gizmoPart();
    if (!pt) return false;
    const auto& p = *pt;
    glm::vec3 o, d;
    screenToRay(mx, my, g_lastView, g_lastProj, o, d);
    float bestT = 1e9f;
    bool hit = false;
    for (int axis = 0; axis < 3; ++axis) {
        for (int s = -1; s <= 1; s += 2) {
            float t = 0.f;
            glm::vec3 hp = handleWorldPos(p, axis, s);
            if (rayHitAabb(o, d, hp, glm::vec3(kHandleSize * 1.15f), t) && t < bestT) {
                bestT = t;
                outAxis = axis;
                outSign = s;
                hit = true;
            }
        }
    }
    return hit;
}

static bool pickMoveHandle(float mx, float my, int& outAxis) {
    if (g_tool != StudioTool::Move) return false;
    const WorkspacePart* pt = gizmoPart();
    if (!pt) return false;
    const auto& p = *pt;
    glm::vec3 o, d;
    screenToRay(mx, my, g_lastView, g_lastProj, o, d);
    float bestT = 1e9f;
    bool hit = false;
    float len = gizmoArrowLen(p);
    for (int axis = 0; axis < 3; ++axis) {
        glm::vec3 tip = moveHandleTip(p, axis);
        float t = 0.f;
        // punta
        if (rayHitAabb(o, d, tip, glm::vec3(0.55f), t) && t < bestT) {
            bestT = t; outAxis = axis; hit = true;
        }
        // stelo (AABB allungato approssimato con segmenti)
        glm::vec3 dir = partAxisDir(p, axis);
        for (int k = 1; k <= 6; ++k) {
            glm::vec3 pt = p.position + dir * (len * (float)k / 7.f);
            if (rayHitAabb(o, d, pt, glm::vec3(0.28f), t) && t < bestT) {
                bestT = t; outAxis = axis; hit = true;
            }
        }
    }
    return hit;
}

/** Pick anello di rotazione: intersezione piano + distanza ≈ raggio. */
static bool pickRotateHandle(float mx, float my, int& outAxis) {
    if (g_tool != StudioTool::Rotate) return false;
    const WorkspacePart* pt = gizmoPart();
    if (!pt) return false;
    const auto& p = *pt;
    glm::vec3 o, d;
    screenToRay(mx, my, g_lastView, g_lastProj, o, d);
    float bestScore = 1e9f;
    bool hit = false;
    float R = gizmoRingRadius(p);
    const float band = 0.55f;
    for (int axis = 0; axis < 3; ++axis) {
        glm::vec3 axisV = partAxisDir(p, axis);
        float t = 0.f;
        if (!rayPlane(o, d, p.position, axisV, t)) continue;
        glm::vec3 hitP = o + d * t;
        glm::vec3 radial = hitP - p.position;
        radial -= axisV * glm::dot(radial, axisV);
        float dist = glm::length(radial);
        float err = std::fabs(dist - R);
        if (err < band && err < bestScore) {
            bestScore = err;
            outAxis = axis;
            hit = true;
        }
    }
    g_gizmoHoverAxis = hit ? outAxis : -1;
    return hit;
}

static void beginScaleDrag(int axis, int sign) {
    const WorkspacePart* pt = gizmoPart();
    if (!pt) return;
    const auto& p = *pt;
    g_xformBefore = p;
    g_xformTrack = true;
    g_scaleDragging = true;
    g_moveDragging = false;
    g_rotateDragging = false;
    g_scaleAxis = axis;
    g_scaleSign = sign;
    g_scaleStartPos = p.position;
    g_scaleStartSize = p.size;
    g_scaleStartFace = p.position[axis] + sign * p.size[axis] * 0.5f;
    g_scaleLiveSize = p.size[axis];
}

static void updateScaleDrag(float mx, float my) {
    WorkspacePart* pt = g_scaleDragging ? gizmoPart() : nullptr;
    if (!pt) return;
    auto& p = *pt;
    glm::vec3 o, d;
    screenToRay(mx, my, g_lastView, g_lastProj, o, d);

    glm::vec3 axisV = partAxisDir(p, g_scaleAxis);
    glm::vec3 camF = g_cam.front();
    glm::vec3 side = glm::cross(axisV, camF);
    if (glm::length(side) < 1e-4f) side = glm::cross(axisV, glm::vec3(0, 1, 0));
    side = glm::normalize(side);
    glm::vec3 planeN = glm::normalize(glm::cross(axisV, side));

    glm::vec3 faceLocal(0);
    faceLocal[g_scaleAxis] = (float)g_scaleSign * g_scaleStartSize[g_scaleAxis] * 0.5f;
    glm::vec3 planeP = g_scaleStartPos + glm::vec3(partRotationMatrix(p.rotation) * glm::vec4(faceLocal, 0.f));
    float t = 0.f;
    if (!rayPlane(o, d, planeP, planeN, t)) return;
    glm::vec3 hit = o + d * t;
    float newFace = glm::dot(hit - g_scaleStartPos, axisV);
    float startFace = (float)g_scaleSign * g_scaleStartSize[g_scaleAxis] * 0.5f;
    float expand = (newFace - startFace) * (float)g_scaleSign;
    const float minSize = gizmoMinSize();
    float newSize = std::clamp(g_scaleStartSize[g_scaleAxis] + expand, minSize, 80.f);
    // Snap live: Shift = 0.1, default 0.25, Ctrl = libero
    if (!gizmoCtrl() && gizmoSnapSize()) {
        float step = gizmoShift() ? 0.1f : 0.25f;
        newSize = std::clamp(snapValue(newSize, step), minSize, 80.f);
    }
    float applied = newSize - g_scaleStartSize[g_scaleAxis];
    p.size = g_scaleStartSize;
    p.size[g_scaleAxis] = newSize;
    glm::vec3 offsetLocal(0);
    offsetLocal[g_scaleAxis] = (float)g_scaleSign * applied * 0.5f;
    p.position = g_scaleStartPos + glm::vec3(partRotationMatrix(p.rotation) * glm::vec4(offsetLocal, 0.f));
    g_scaleLiveSize = newSize;
    gizmoCommitToModel(); // la proxy si rigenera dal Model: va scritta subito
}

static void endScaleDrag() {
    WorkspacePart* pt = g_scaleDragging ? gizmoPart() : nullptr;
    if (pt) {
        auto& p = *pt;
        if (!gizmoCtrl()) {
            float step = gizmoShift() ? 0.1f : 0.25f;
            if (gizmoSnapSize())
                p.size = glm::vec3(snapValue(p.size.x, step), snapValue(p.size.y, step),
                                   snapValue(p.size.z, step));
            p.position = glm::vec3(snapValue(p.position.x, step), snapValue(p.position.y, step),
                                   snapValue(p.position.z, step));
        }
        p.size = glm::clamp(p.size, glm::vec3(gizmoMinSize()), glm::vec3(80.f));
        if (g_xformTrack && g_selectedPart >= 0)
            g_undo.recordTransform(g_selectedPart, g_xformBefore, p);
        gizmoCommitToModel();
    }
    g_scaleDragging = false;
    g_xformTrack = false;
}

static void beginMoveDrag(int axis, float mx, float my) {
    const WorkspacePart* pt = gizmoPart();
    if (!pt) return;
    const auto& p = *pt;
    g_xformBefore = p;
    g_xformTrack = true;
    g_moveDragging = true;
    g_scaleDragging = false;
    g_rotateDragging = false;
    g_moveAxis = axis;
    g_moveStartPos = p.position;
    g_moveLiveDelta = 0.f;
    glm::vec3 o, d;
    screenToRay(mx, my, g_lastView, g_lastProj, o, d);
    float t = 0.f;
    glm::vec3 planeN = (axis == 1) ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
    if (axis >= 0) {
        glm::vec3 axisV = partAxisDir(p, axis);
        glm::vec3 camF = g_cam.front();
        glm::vec3 side = glm::cross(axisV, camF);
        if (glm::length(side) < 1e-4f) side = glm::cross(axisV, glm::vec3(0, 1, 0));
        planeN = glm::normalize(glm::cross(axisV, glm::normalize(side)));
    }
    if (rayPlane(o, d, p.position, planeN, t))
        g_moveStartHit = o + d * t;
    else
        g_moveStartHit = p.position;
}

static void updateMoveDrag(float mx, float my) {
    WorkspacePart* pt = g_moveDragging ? gizmoPart() : nullptr;
    if (!pt) return;
    auto& p = *pt;
    glm::vec3 o, d;
    screenToRay(mx, my, g_lastView, g_lastProj, o, d);
    float t = 0.f;

    if (g_moveAxis >= 0 && g_moveAxis <= 2) {
        glm::vec3 axisV = partAxisDir(p, g_moveAxis);
        glm::vec3 camF = g_cam.front();
        glm::vec3 side = glm::cross(axisV, camF);
        if (glm::length(side) < 1e-4f) side = glm::cross(axisV, glm::vec3(0, 1, 0));
        glm::vec3 planeN = glm::normalize(glm::cross(axisV, glm::normalize(side)));
        if (!rayPlane(o, d, g_moveStartPos, planeN, t)) return;
        glm::vec3 hit = o + d * t;
        float delta = glm::dot(hit - g_moveStartHit, axisV);
        if (!gizmoCtrl()) {
            float step = gizmoShift() ? 0.05f : 0.25f;
            delta = snapValue(delta, step);
        }
        p.position = g_moveStartPos + axisV * delta;
        g_moveLiveDelta = delta;
        gizmoCommitToModel();
    }
}

static void endMoveDrag() {
    WorkspacePart* pt = g_moveDragging ? gizmoPart() : nullptr;
    if (pt) {
        auto& p = *pt;
        if (!gizmoCtrl()) {
            float step = gizmoShift() ? 0.05f : 0.25f;
            p.position = glm::vec3(snapValue(p.position.x, step),
                                   snapValue(p.position.y, step),
                                   snapValue(p.position.z, step));
        }
        if (g_xformTrack && g_selectedPart >= 0)
            g_undo.recordTransform(g_selectedPart, g_xformBefore, p);
        gizmoCommitToModel();
    }
    g_moveDragging = false;
    g_moveLiveDelta = 0.f;
    g_xformTrack = false;
}

static void beginRotateDrag(int axis, float mx, float my) {
    const WorkspacePart* pt = gizmoPart();
    if (!pt) return;
    const auto& p = *pt;
    g_xformBefore = p;
    g_xformTrack = true;
    g_rotateDragging = true;
    g_scaleDragging = false;
    g_moveDragging = false;
    g_rotateAxis = axis;
    g_rotateStartRot = p.rotation;
    g_rotateLiveDelta = 0.f;
    glm::vec3 o, d;
    screenToRay(mx, my, g_lastView, g_lastProj, o, d);
    glm::vec3 axisV = partAxisDir(p, axis);
    float t = 0.f;
    if (rayPlane(o, d, p.position, axisV, t))
        g_rotateStartAngle = angleAroundAxis(p.position, axisV, o + d * t);
    else
        g_rotateStartAngle = 0.f;
}

static void updateRotateDrag(float mx, float my) {
    WorkspacePart* pt = g_rotateDragging ? gizmoPart() : nullptr;
    if (!pt) return;
    auto& p = *pt;
    glm::vec3 o, d;
    screenToRay(mx, my, g_lastView, g_lastProj, o, d);
    glm::vec3 axisV = partAxisDir(p, g_rotateAxis);
    float t = 0.f;
    if (!rayPlane(o, d, p.position, axisV, t)) return;
    float ang = angleAroundAxis(p.position, axisV, o + d * t);
    float deltaDeg = glm::degrees(ang - g_rotateStartAngle);
    // Snap live: default 5°, Shift = 1°, Ctrl = libero (perfetto)
    if (!gizmoCtrl()) {
        float step = gizmoShift() ? 1.f : 5.f;
        deltaDeg = snapValue(deltaDeg, step);
    }
    p.rotation = g_rotateStartRot;
    p.rotation[g_rotateAxis] = g_rotateStartRot[g_rotateAxis] + deltaDeg;
    g_rotateLiveDelta = deltaDeg;
    gizmoCommitToModel();
}

static void endRotateDrag() {
    WorkspacePart* pt = g_rotateDragging ? gizmoPart() : nullptr;
    if (pt) {
        auto& p = *pt;
        // Normalizza angoli in -180..180 per chiarezza
        for (int i = 0; i < 3; ++i) {
            while (p.rotation[i] > 180.f) p.rotation[i] -= 360.f;
            while (p.rotation[i] < -180.f) p.rotation[i] += 360.f;
            if (!gizmoCtrl()) {
                float step = gizmoShift() ? 1.f : 5.f;
                p.rotation[i] = snapValue(p.rotation[i], step);
            }
        }
        if (g_xformTrack && g_selectedPart >= 0)
            g_undo.recordTransform(g_selectedPart, g_xformBefore, p);
        gizmoCommitToModel();
    }
    g_rotateDragging = false;
    g_rotateLiveDelta = 0.f;
    g_xformTrack = false;
}

static bool rayHitPart(const glm::vec3& o, const glm::vec3& d, const WorkspacePart& p, float& tOut) {
    glm::mat3 R = glm::mat3(partRotationMatrix(p.rotation));
    glm::mat3 Rt = glm::transpose(R);
    glm::vec3 lo = Rt * (o - p.position);
    glm::vec3 ld = Rt * d;
    return rayHitAabb(lo, ld, glm::vec3(0), p.size, tOut);
}

static int pickPartAtMouse(float mx, float my) {
    glm::vec3 o, d;
    screenToRay(mx, my, g_lastView, g_lastProj, o, d);
    int best = -1;
    float bestT = 1e9f;
    for (int i = 0; i < (int)g_world.parts.size(); ++i) {
        float t = 0.f;
        if (rayHitPart(o, d, g_world.parts[i], t) && t < bestT) {
            bestT = t;
            best = i;
        }
    }
    return best;
}

enum class PickKind { None, Part, Model, Label, Baseplate, Spawn, PlayerRoot, PlayerPart };

struct ScenePick {
    PickKind kind = PickKind::None;
    int index = -1;
    float t = 1e9f;
};

static bool rayHitAabbWorld(const glm::vec3& o, const glm::vec3& d,
                            const glm::vec3& center, const glm::vec3& size, float& tOut) {
    return rayHitAabb(o, d, center, size, tOut);
}

/** Seleziona Part, Model, Baseplate, Spawn o Player (click viewport). */
static ScenePick pickSceneAtMouse(float mx, float my) {
    glm::vec3 o, d;
    screenToRay(mx, my, g_lastView, g_lastProj, o, d);
    ScenePick best;

    for (int i = 0; i < (int)g_world.parts.size(); ++i) {
        float t = 0.f;
        if (rayHitPart(o, d, g_world.parts[i], t) && t < best.t) {
            best = {PickKind::Part, i, t};
        }
    }

    for (int i = 0; i < (int)g_world.models.size(); ++i) {
        const auto& m = g_world.models[i];
        glm::vec3 sz = glm::max(m.scale * 2.f, glm::vec3(1.f));
        float t = 0.f;
        if (rayHitAabbWorld(o, d, m.position, sz, t) && t < best.t)
            best = {PickKind::Model, i, t};
    }

    // Label/TextLabel = UI schermo: pick separato (pickScreenLabelAt)

    if (g_world.hasBaseplate) {
        // baseplate centrata in (0,-1,0) con size baseplateSize
        float t = 0.f;
        if (rayHitAabbWorld(o, d, {0.f, -1.f, 0.f}, g_world.baseplateSize, t) && t < best.t)
            best = {PickKind::Baseplate, -1, t};
    }

    {
        float t = 0.f;
        glm::vec3 spawnPos = g_world.spawn.position + glm::vec3(0, 1.5f, 0);
        if (rayHitAabbWorld(o, d, spawnPos, {2.f, 3.f, 2.f}, t) && t < best.t)
            best = {PickKind::Spawn, -1, t};
    }

    // Player / arti R6 (AABB approssimata in world)
    {
        glm::mat4 root = glm::rotate(glm::translate(glm::mat4(1.f), g_playerPos),
                                     glm::radians(g_playerYaw), {0, 1, 0});
        float tRoot = 0.f;
        if (rayHitAabbWorld(o, d, g_playerPos + glm::vec3(0, 3.f, 0), {2.2f, 6.5f, 2.2f}, tRoot)
            && tRoot < best.t) {
            best = {PickKind::PlayerRoot, -1, tRoot};
            // prova a colpire un arto specifico
            for (int i = 0; i < (int)g_player.humanoid.parts.size(); ++i) {
                const auto& bp = g_player.humanoid.parts[i];
                if (bp.name == "HumanoidRootPart") continue;
                glm::vec3 world = glm::vec3(root * glm::vec4(bp.offset, 1.f));
                float t = 0.f;
                if (rayHitAabbWorld(o, d, world, bp.size * 1.15f, t) && t < best.t)
                    best = {PickKind::PlayerPart, i, t};
            }
        }
    }

    return best;
}

static void applyScenePick(const ScenePick& hit) {
    switch (hit.kind) {
    case PickKind::Part: selectPart(hit.index); break;
    case PickKind::Model: selectModel(hit.index); break;
    case PickKind::Label: selectLabel(hit.index); break;
    case PickKind::Baseplate: selectBaseplate(); break;
    case PickKind::Spawn: selectSpawn(); break;
    case PickKind::PlayerRoot: selectPlayer(-1); break;
    case PickKind::PlayerPart: selectPlayer(hit.index); break;
    default: clearAllSelection(); break;
    }
}

static glm::vec3 insertPositionInFront() {
    if (g_mode == CamMode::Studio) {
        glm::vec3 p = g_cam.pos + g_cam.front() * 12.f;
        p.y = std::max(1.f, p.y);
        // snap grezzo a stud
        p.x = std::round(p.x);
        p.z = std::round(p.z);
        p.y = std::round(p.y * 2.f) * 0.5f;
        return p;
    }
    return g_playerPos + glm::vec3(0, 2, 6);
}

static void insertNewPart() {
    int n = g_world.nextPartIndex();
    std::string name = "Part" + std::to_string(n);
    auto& p = g_world.addPart(name, insertPositionInFront(), {4, 1, 4}, {0.64f, 0.64f, 0.64f});
    (void)p;
    g_selectedModel = -1;
    g_selectedSound = -1;
    g_selectedLabel = -1;
    selectPart((int)g_world.parts.size() - 1);
    g_undo.recordInsert(g_selectedPart, g_world.parts[g_selectedPart]);
    markPlaceDirty();
}

static void insertShapedPart(PartShape shape) {
    int n = g_world.nextPartIndex();
    glm::vec3 size{4, 4, 4};
    glm::vec3 col{0.64f, 0.64f, 0.64f};
    const char* base = "Part";
    switch (shape) {
    case PartShape::Ball:
        base = "Ball";
        size = {4, 4, 4};
        col = {0.95f, 0.45f, 0.35f};
        break;
    case PartShape::Cylinder:
        base = "Cylinder";
        size = {2, 5, 2};
        col = {0.35f, 0.65f, 0.95f};
        break;
    case PartShape::Wedge:
        base = "Wedge";
        size = {4, 2, 6};
        col = {0.45f, 0.85f, 0.4f};
        break;
    default: break;
    }
    auto& p = g_world.addPart(std::string(base) + std::to_string(n),
                              insertPositionInFront(), size, col);
    p.shape = shape;
    p.useStud = false;
    g_selectedModel = -1;
    g_selectedSound = -1;
    g_selectedLabel = -1;
    selectPart((int)g_world.parts.size() - 1);
    syncPartEditors();
    g_undo.recordInsert(g_selectedPart, g_world.parts[g_selectedPart]);
    markPlaceDirty();
    g_exportMsg = std::string("Inserito ") + partShapeName(shape);
}

/** Parte Neon lucente + PointLight dinamico (illumina intorno). */
static void insertNeonPart() {
    int n = g_world.nextPartIndex();
    auto& p = g_world.addPart("Neon" + std::to_string(n), insertPositionInFront(),
                              {2, 2, 2}, {0.25f, 1.f, 0.85f});
    p.setMaterial(PartMaterial::Neon);
    p.pointLight = true;
    p.lightRange = 22.f;
    p.lightBrightness = 1.4f;
    p.script = makeNeonBlockScript(p);
    g_selectedModel = -1;
    g_selectedSound = -1;
    g_selectedLabel = -1;
    selectPart((int)g_world.parts.size() - 1);
    syncPartEditors();
    g_undo.recordInsert(g_selectedPart, g_world.parts[g_selectedPart]);
    g_exportMsg = "Neon Part + script Lua — Test (F5) per glow/pulse";
}

/** Solo PointLight (cubo piccolo che illumina, quasi invisibile). */
static void insertPointLightPart() {
    int n = g_world.nextPartIndex();
    auto& p = g_world.addPart("Light" + std::to_string(n), insertPositionInFront(),
                              {1, 1, 1}, {1.f, 0.95f, 0.75f});
    p.setMaterial(PartMaterial::Neon);
    p.emissive = 1.2f;
    p.pointLight = true;
    p.lightRange = 28.f;
    p.lightBrightness = 1.8f;
    p.canCollide = false;
    g_selectedModel = -1;
    g_selectedSound = -1;
    g_selectedLabel = -1;
    selectPart((int)g_world.parts.size() - 1);
    g_undo.recordInsert(g_selectedPart, g_world.parts[g_selectedPart]);
    g_exportMsg = "PointLight — regola Range/Brightness in Properties";
}

/** Blocco esplosivo: script Lua (monete + Explode). */
static void insertExplodePart() {
    int n = g_world.nextPartIndex();
    auto& p = g_world.addPart("Explode" + std::to_string(n), insertPositionInFront(),
                              {3, 3, 3}, {0.85f, 0.18f, 0.12f});
    p.setMaterial(PartMaterial::Metal);
    p.canDestroy = true;
    p.boomOnTouch = false;  // boom gestito dallo script Lua
    p.boomAutoSecs = 0.f;
    p.canCollide = false;   // trigger senza rimbalzo
    p.health = 100.f;
    p.maxHealth = 100.f;
    p.anchored = true;
    p.script = makeExplodeBlockScript(p);
    g_selectedModel = -1;
    g_selectedSound = -1;
    g_selectedLabel = -1;
    selectPart((int)g_world.parts.size() - 1);
    syncPartEditors();
    g_undo.recordInsert(g_selectedPart, g_world.parts[g_selectedPart]);
    g_exportMsg = "Explode Part — script Lua: tocco = monete + boom";
    markPlaceDirty();
}

static void insertNewLabel() {
    int n = g_world.nextLabelIndex();
    auto& l = g_world.addLabel("Label" + std::to_string(n), "Label",
                               0.5f, 0.18f, {1.f, 1.f, 1.f}, "Label");
    l.bgColor = {0.f, 0.f, 0.f};
    l.bgTransparency = 0.35f;
    l.fontScale = 1.25f;
    selectLabel((int)g_world.labels.size() - 1);
    g_exportMsg = "Label creata — testo + outline + bg nero";
}

static void insertNewTextLabel() {
    int n = g_world.nextLabelIndex();
    auto& l = g_world.addLabel("TextLabel" + std::to_string(n), "TextLabel",
                               0.5f, 0.12f, {1.f, 1.f, 1.f}, "TextLabel");
    l.bgColor = {0.f, 0.f, 0.f};
    l.bgTransparency = 0.35f;
    l.fontScale = 1.15f;
    selectLabel((int)g_world.labels.size() - 1);
    g_exportMsg = "TextLabel creato — testo + outline + bg nero";
}

static void insertNewTextButton() {
    int n = g_world.nextLabelIndex();
    auto& l = g_world.addLabel("TextButton" + std::to_string(n), "Button",
                               0.5f, 0.55f, {1.f, 1.f, 1.f}, "TextButton");
    l.bgColor = {0.55f, 0.55f, 0.58f}; // grigio classico Roblox
    l.bgTransparency = 0.f;
    l.fontScale = 1.1f;
    l.sizeW = 140.f;
    l.sizeH = 40.f;
    selectLabel((int)g_world.labels.size() - 1);
    g_exportMsg = "TextButton creato — cliccabile in Test/Play";
}

/** Props costruiti con Parts (map creating senza file esterni). */
static void insertPropPreset(int kind) {
    glm::vec3 base = insertPositionInFront();
    base.y = 0.f;
    switch (kind) {
    case 0: { // Albero
        g_world.addPart("Trunk", base + glm::vec3(0, 2, 0), {1.2f, 4, 1.2f}, {0.42f, 0.26f, 0.12f});
        g_world.addPart("Leaves", base + glm::vec3(0, 5.5f, 0), {4, 3.5f, 4}, {0.18f, 0.55f, 0.22f});
        g_exportMsg = "Prop: Albero";
        break;
    }
    case 1: { // Casa
        g_world.addPart("HouseBody", base + glm::vec3(0, 3, 0), {10, 6, 8}, {0.85f, 0.78f, 0.65f});
        g_world.addPart("Roof", base + glm::vec3(0, 7.2f, 0), {12, 2, 10}, {0.75f, 0.28f, 0.22f});
        g_world.addPart("Door", base + glm::vec3(0, 1.5f, 4.1f), {2, 3, 0.3f}, {0.35f, 0.22f, 0.12f});
        g_exportMsg = "Prop: Casa";
        break;
    }
    case 2: { // Rampa
        for (int i = 0; i < 6; ++i)
            g_world.addPart("Ramp" + std::to_string(i),
                base + glm::vec3(i * 3.f, 0.5f + i * 0.8f, 0), {3, 1, 4},
                {0.3f + i * 0.08f, 0.55f, 0.9f});
        g_exportMsg = "Prop: Rampa";
        break;
    }
    case 3: { // Torre
        g_world.addPart("Tower", base + glm::vec3(0, 8, 0), {5, 16, 5}, {0.55f, 0.58f, 0.65f});
        g_world.addPart("Top", base + glm::vec3(0, 17, 0), {7, 2, 7}, {0.9f, 0.4f, 0.25f});
        g_exportMsg = "Prop: Torre";
        break;
    }
    default: break;
    }
    if (!g_world.parts.empty())
        selectPart((int)g_world.parts.size() - 1);
}

static void applyMapTemplate(int kind) {
    g_sounds.stopAll();
    g_sounds.sounds.clear();
    g_wsMeshes.clear();
    g_showAnimEditor = false;
    g_animEditModel = -1;
    switch (kind) {
    case 0: g_world.loadBaseplatePlace(); break;
    case 1: g_world.loadGardenPlace(); break;
    case 2: g_world.loadObbyPlace(); break;
    case 3: g_world.loadArenaPlace(); break;
    case 4: g_world.loadParkPlace(); break;
    case 5: g_world.loadCityPlace(); break;
    default: g_world.loadBaseplatePlace(); break;
    }
    std::snprintf(g_gameNameBuf, sizeof(g_gameNameBuf), "%s", g_world.placeName.c_str());
    clearAllSelection();
    g_playerPos = g_world.spawn.position;
    if (g_playerPos.y < 0.01f) g_playerPos = {0, 0, 0};
    acAllowTeleport();
    g_friends.clear();
    g_friendSeq = 1;
    g_exportMsg = std::string("Mappa: ") + g_world.placeName;
}

static void reloadWorkspaceModelMeshes() {
    g_wsMeshes.clear();
    g_wsRigged.clear();
    g_wsMeshes.reserve(g_world.models.size());
    g_wsRigged.resize(g_world.models.size());

    for (size_t i = 0; i < g_world.models.size(); ++i) {
        auto& m = g_world.models[i];

        StaticMesh mesh;
        mesh.exactImport = true; // modelli del mondo: submesh e materiali intatti
        if (!m.sourcePath.empty())
            mesh.load(m.sourcePath);

        // StaticMesh rileva già lo scheletro ma non lo usa: se c'è, carichiamo
        // il file una seconda volta come modello animato.
        m.rigged = false;
        if (mesh.hasBones && !m.sourcePath.empty()) {
            if (g_wsRigged[i].load(m.sourcePath)) {
                m.rigged = g_wsRigged[i].clipCount() > 0;
                if (m.rigged) {
                    std::printf("[Model] '%s': %d clip animate disponibili\n",
                                m.name.c_str(), g_wsRigged[i].clipCount());

                    // Un modello animato deve animarsi appena importato: chi lo
                    // mette nella scena si aspetta di vederlo muoversi, non di
                    // dover prima scrivere uno script. Gli script poi possono
                    // fermare questa traccia e comandarne altre.
                    if (m.animator.tracks.empty()) {
                        const int id = m.animator.load(g_wsRigged[i].activeClipName());
                        if (AnimTrack* t = m.animator.get(id)) {
                            t->looped = true;
                            t->playing = true;
                        }
                    }
                }
            }
        }

        g_wsMeshes.push_back(std::move(mesh));
    }
}

static void drawWorkspaceModels(GLuint meshP, GLuint skinP, const glm::mat4& view,
                                const glm::mat4& proj, const glm::vec3& sun) {
    if (g_wsMeshes.size() != g_world.models.size())
        reloadWorkspaceModelMeshes();

    // Delta locale: la funzione non riceve dt e non vale la pena cambiare
    // la firma di mezzo renderer per farlo arrivare fin qui.
    static double lastAnimTime = glfwGetTime();
    const double nowAnim = glfwGetTime();
    const float animDt = std::min((float)(nowAnim - lastAnimTime), 0.05f);
    lastAnimTime = nowAnim;

    for (int i = 0; i < (int)g_world.models.size(); ++i) {
        if (i >= (int)g_wsMeshes.size() || !g_wsMeshes[i].loaded) continue;
        auto& m = g_world.models[i];
        glm::mat4 model = glm::translate(glm::mat4(1.f), m.position)
                        * partRotationMatrix(m.rotation)
                        * glm::scale(glm::mat4(1.f), m.scale);
        glm::vec3 tint = m.tint;
        if (i == g_selectedModel)
            tint = glm::mix(tint, glm::vec3(1.f, 0.85f, 0.2f), 0.3f);

        // --- Modello animato: la traccia in riproduzione comanda la posa ---
        const int track = m.animator.dominant();
        if (m.rigged && track >= 0 && i < (int)g_wsRigged.size() && g_wsRigged[i].loaded) {
            AnimTrack* t = m.animator.get(track);
            AnimatedModel& rig = g_wsRigged[i];

            if (t && rig.activeClipName() != t->clipName)
                rig.setClip(t->clipName);

            m.animator.advance(animDt);
            rig.update(animDt * (t ? std::max(0.f, t->speed) : 1.f));

            // correction porta gli export Z-up nel nostro mondo Y-up.
            const glm::mat4 skinModel = model * rig.correction;

            glUseProgram(skinP);
            glUniformMatrix4fv(glGetUniformLocation(skinP,"uModel"),1,GL_FALSE,glm::value_ptr(skinModel));
            glUniformMatrix4fv(glGetUniformLocation(skinP,"uView"),1,GL_FALSE,glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(skinP,"uProj"),1,GL_FALSE,glm::value_ptr(proj));
            glUniform3fv(glGetUniformLocation(skinP,"uSun"),1,glm::value_ptr(sun));
            glUniform3fv(glGetUniformLocation(skinP,"uCamPos"),1,glm::value_ptr(g_renderCamPos));
            glUniform1f(glGetUniformLocation(skinP,"uEmissive"), 0.f);
            glUniform1f(glGetUniformLocation(skinP,"uSpecular"), 0.12f);
            glUniform1f(glGetUniformLocation(skinP,"uRoughness"), 0.75f);
            glUniform1f(glGetUniformLocation(skinP,"uAlpha"), 1.f);
            glUniform1i(glGetUniformLocation(skinP,"uMaterial"), (int)PartMaterial::Plastic);
            uploadDynamicLights(skinP);
            rig.draw(skinP);
            continue;   // disegnato animato: salta la versione statica
        }

        glUseProgram(meshP);
        glUniformMatrix4fv(glGetUniformLocation(meshP,"uModel"),1,GL_FALSE,glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(meshP,"uView"),1,GL_FALSE,glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(meshP,"uProj"),1,GL_FALSE,glm::value_ptr(proj));
        glUniform3fv(glGetUniformLocation(meshP,"uSun"),1,glm::value_ptr(sun));
        glUniform3fv(glGetUniformLocation(meshP,"uCamPos"),1,glm::value_ptr(g_renderCamPos));
        uploadDynamicLights(meshP); // Neon / PointLight illuminano anche i modelli
        g_wsMeshes[i].drawTextured(meshP, tint, 1.f);
    }
}

static void ensureDefaultBeepWav() {
#ifdef _WIN32
    std::string dir = blockyDataDir() + "/sounds";
    _mkdir(dir.c_str());
    std::string path = dir + "/beep.wav";
    if (FILE* exists = std::fopen(path.c_str(), "rb")) { std::fclose(exists); return; }
    const int sampleRate = 22050;
    const int nSamples = sampleRate / 5;
    const int dataBytes = nSamples * 2;
    std::vector<char> buf(44 + dataBytes);
    auto* hdr = reinterpret_cast<unsigned char*>(buf.data());
    std::memcpy(hdr, "RIFF", 4);
    unsigned chunkSize = 36 + (unsigned)dataBytes;
    std::memcpy(hdr + 4, &chunkSize, 4);
    std::memcpy(hdr + 8, "WAVEfmt ", 8);
    unsigned sub1 = 16; short fmt = 1, ch = 1; unsigned rate = (unsigned)sampleRate;
    unsigned byteRate = (unsigned)sampleRate * 2; short align = 2, bits = 16;
    std::memcpy(hdr + 16, &sub1, 4);
    std::memcpy(hdr + 20, &fmt, 2);
    std::memcpy(hdr + 22, &ch, 2);
    std::memcpy(hdr + 24, &rate, 4);
    std::memcpy(hdr + 28, &byteRate, 4);
    std::memcpy(hdr + 32, &align, 2);
    std::memcpy(hdr + 34, &bits, 2);
    std::memcpy(hdr + 36, "data", 4);
    std::memcpy(hdr + 40, &dataBytes, 4);
    short* samples = reinterpret_cast<short*>(buf.data() + 44);
    for (int i = 0; i < nSamples; ++i) {
        float t = (float)i / (float)sampleRate;
        samples[i] = (short)(std::sin(2.f * 3.14159265f * 880.f * t) * 12000.f);
    }
    FILE* f = std::fopen(path.c_str(), "wb");
    if (f) { std::fwrite(buf.data(), 1, buf.size(), f); std::fclose(f); }
    if (!g_soundPathBuf[0])
        std::snprintf(g_soundPathBuf, sizeof(g_soundPathBuf), "%s", path.c_str());
#endif
}

static void insertImportedModel() {
    std::string path;
    if (!openModelFileDialog(path, publisherHwnd())) {
        g_exportMsg = "Import modello annullato";
        return;
    }
    auto mod = Moderation::checkAvatarOrModelName(path);
    if (mod.verdict != Moderation::Verdict::Ok) {
        g_exportMsg = mod.reason;
        return;
    }

    StaticMesh mesh;
    mesh.exactImport = true; // import nel mondo: niente scarti, materiali come nel file
    bool ok = false;
    try {
        ok = mesh.load(path);
    } catch (...) {
        ok = false;
    }
    if (!ok || !mesh.loaded) {
        g_exportMsg = "Import fallito (file non supportato o troppo pesante)";
        std::fprintf(stderr, "[Import] FAIL %s\n", path.c_str());
        return;
    }

    auto slash = path.find_last_of("\\/");
    std::string base = (slash == std::string::npos) ? path : path.substr(slash + 1);
    std::string name = "Model" + std::to_string(g_world.nextModelIndex());
    auto& wm = g_world.addModel(name, path, insertPositionInFront());
    wm.scale = {2, 2, 2};
    wm.rig.fileHasBones = mesh.hasBones; // dal primo load — niente secondo Assimp
    wm.rig.simpleRig = false;
    g_wsMeshes.push_back(std::move(mesh));
    g_selectedPart = -1;
    g_selectedSound = -1;
    int idx = (int)g_world.models.size() - 1;
    g_selectedModel = idx;
    g_showExplorer = true;
    g_showProps = true;
    g_exportMsg = "Importato: " + base + " — conferma Rig";
    // Chiedi subito il Rig (finestra al centro)
    askRigOrOpenAnimator(idx);
    // Se il file si chiama pistola/gun → chiedi anche Backpack
    offerBackpackForGunImport(path);
}

static void insertSound() {
    ensureDefaultBeepWav();
    auto& s = g_sounds.createSound();
    s.soundId = g_soundPathBuf[0] ? g_soundPathBuf : (blockyDataDir() + "/sounds/beep.wav");
    g_selectedPart = -1;
    g_selectedModel = -1;
    g_selectedSound = (int)g_sounds.sounds.size() - 1;
    g_exportMsg = "Sound creato — Play dalla Properties";
}

static void duplicateSelected() {
    if (g_selectedPart < 0 || g_selectedPart >= (int)g_world.parts.size()) return;
    WorkspacePart copy = g_world.parts[g_selectedPart];
    copy.name += "_Copy";
    copy.position.x += copy.size.x + 1.f;
    g_world.parts.push_back(std::move(copy));
    selectPart((int)g_world.parts.size() - 1);
    g_undo.recordInsert(g_selectedPart, g_world.parts[g_selectedPart]);
}

static void deleteSelected() {
    if (g_selectedModel >= 0 && g_selectedModel < (int)g_world.models.size()) {
        if (g_animEditModel == g_selectedModel) {
            g_showAnimEditor = false;
            g_showRigPrompt = false;
            g_animPlaying = false;
            g_animEditModel = -1;
        }
        g_world.models.erase(g_world.models.begin() + g_selectedModel);
        if (g_selectedModel < (int)g_wsMeshes.size())
            g_wsMeshes.erase(g_wsMeshes.begin() + g_selectedModel);
        selectModel(-1);
        return;
    }
    if (g_selectedLabel >= 0 && g_selectedLabel < (int)g_world.labels.size()) {
        g_world.labels.erase(g_world.labels.begin() + g_selectedLabel);
        selectLabel(-1);
        return;
    }
    if (g_selectedSound >= 0 && g_selectedSound < (int)g_sounds.sounds.size()) {
        g_sounds.stopAll();
        g_sounds.sounds.erase(g_sounds.sounds.begin() + g_selectedSound);
        selectSound(-1);
        return;
    }
    if (g_selectedPart < 0 || g_selectedPart >= (int)g_world.parts.size()) return;
    g_undo.recordDelete(g_selectedPart, g_world.parts[g_selectedPart]);
    g_world.parts.erase(g_world.parts.begin() + g_selectedPart);
    g_editingPartScript = false;
    selectPart(-1);
    markPlaceDirty();
}

static std::string loadStaminaScriptSource() {
    std::string path = resolveScriptPath("scripts/stamina.lua");
    std::string src = readTextFile(path);
    return src;
}

/** Attacca stamina.lua alla Part selezionata (script completo, senza troncamento editor). */
static bool applyStaminaToSelectedPart() {
    if (g_selectedPart < 0 || g_selectedPart >= (int)g_world.parts.size())
        return false;
    std::string src = loadStaminaScriptSource();
    if (src.empty()) {
        g_luaRunMsg = "File stamina.lua non trovato in assets/scripts";
        return false;
    }
    auto& p = g_world.parts[g_selectedPart];
    p.script = std::move(src);
    syncPartEditors();
    g_editingPartScript = true;
    g_luaRunMsg = "Stamina su \"" + p.name + "\" — Test (F5), poi CTRL+WASD";
    return true;
}

static std::string makeJumpBlockScript(const WorkspacePart& p) {
    (void)p;
    // API nuova (Lua 5.4): salta SOLO script.Parent — nessun altro oggetto
    return
        "-- Blocco che salta (solo questa Part) · Lua 5.4\n"
        "local part = script.Parent\n"
        "local base = part.Position\n"
        "local altezza = 5\n"
        "local velocita = 2.5\n"
        "local t = 0\n"
        "\n"
        "print(\"Salto: \" .. part.Name)\n"
        "\n"
        "game.RunService.Heartbeat:Connect(function(dt)\n"
        "    t = t + dt\n"
        "    local y = base.y + math.abs(math.sin(t * velocita)) * altezza\n"
        "    part.Position = Vector3.new(base.x, y, base.z)\n"
        "end)\n";
}

static std::string makeExplodeBlockScript(const WorkspacePart& p) {
    (void)p;
    return
        "-- Esplosione leggera (anti-freeze)\n"
        "local part = script.Parent\n"
        "part.CanDestroy = true\n"
        "part.CanCollide = false\n"
        "local esploso = false\n"
        "part.Touched:Connect(function()\n"
        "    if esploso then return end\n"
        "    esploso = true\n"
        "    print(\"BOOM!\")\n"
        "    part:Explode(16)\n"
        "end)\n";
}

/** Script Neon: Material + PointLight + pulse emissive. */
static std::string makeNeonBlockScript(const WorkspacePart& p) {
    (void)p;
    return
        "-- Parte Neon (Material + luce dinamica)\n"
        "local part = script.Parent\n"
        "\n"
        "part.Material = \"Neon\"           -- oppure Enum.Material.Neon\n"
        "part.Emissive = 1\n"
        "part.PointLight = true\n"
        "part.LightRange = 22\n"
        "part.LightBrightness = 1.4\n"
        "\n"
        "print(\"Neon attivo:\", part.Name, part.Material)\n"
        "\n"
        "-- Pulse leggero (opzionale)\n"
        "local t = 0\n"
        "game.RunService.Heartbeat:Connect(function(dt)\n"
        "    t = t + dt\n"
        "    part.Emissive = 0.85 + math.sin(t * 3) * 0.35\n"
        "    part.LightBrightness = 1.1 + math.sin(t * 3) * 0.4\n"
        "end)\n"
        "\n"
        "-- Tocca → flash piu' forte\n"
        "part.Touched:Connect(function()\n"
        "    part.Emissive = 2\n"
        "    part.LightBrightness = 2.5\n"
        "    print(\"Neon flash!\")\n"
        "end)\n";
}

static void addScriptToSelected() {
    if (g_selectedPart < 0 || g_selectedPart >= (int)g_world.parts.size()) return;
    auto& p = g_world.parts[g_selectedPart];
    if (p.script.empty())
        p.script = makeJumpBlockScript(p);
    syncPartEditors();
    g_editingPartScript = true;
    g_showProps = true;
    g_luaRunMsg = "Script salto su " + p.name + " — premi Esegui / Test";
}

static void removeScriptFromSelected() {
    if (g_selectedPart < 0 || g_selectedPart >= (int)g_world.parts.size()) return;
    auto& p = g_world.parts[g_selectedPart];
    p.script.clear();
    g_partScriptBuf[0] = 0;
    g_editingPartScript = false;
    g_luaRunMsg = "Script rimosso da " + p.name;
}

/** Avvia tutti gli script Part (solo da Test). clearFirst=false se il place script è già partito. */
static void runAllPartScripts(bool clearFirst = true) {
    if (!g_lua.ok) g_lua.init(&g_world);
    bindLuaServices();
    if (clearFirst)
        g_lua.clearHeartbeats();
    int n = 0;
    for (int i = 0; i < (int)g_world.parts.size(); ++i) {
        auto& p = g_world.parts[i];
        if (p.script.empty()) continue;
        if (g_lua.runPartScript(p.script, i))
            ++n;
        else
            std::fprintf(stderr, "[Lua] Part '%s': %s\n", p.name.c_str(), g_lua.lastError.c_str());
    }
    if (n > 0)
        g_luaRunMsg = "Test: script Part avviati (" + std::to_string(n) + ")";
}

/** Solo in Test: place script + script dei blocchi. In editor non gira nulla. */
static void startTestScripts() {
    if (!g_lua.ok) g_lua.init(&g_world);
    bindLuaServices();
    g_lua.clearHeartbeats();
    // Reset controlli player (gli script possono disattivare jump/walk)
    scriptControls().jumpEnabled = true;
    scriptControls().walkEnabled = true;
    scriptControls().walkSpeedOverride = -1.f;
    scriptControls().jumpPowerOverride = -1.f;
    // Place script (se presente)
    if (g_activeGame.script[0])
        bootPlaceScript(g_activeGame.script);
    if (g_loadApiCompleta) {
        bootPlaceScript("scripts/lua_api_completo.lua");
        g_luaRunMsg = "API completa caricata — tocca i pad a z=12";
    }
    if (g_loadStamina) {
        bootPlaceScript("scripts/stamina.lua");
        g_luaRunMsg = "Stamina attiva — CTRL+WASD per correre";
    }
    runAllPartScripts(false);
}

static void scaleSelected(float factor) {
    if (g_selectedPart < 0 || g_selectedPart >= (int)g_world.parts.size()) return;
    auto& s = g_world.parts[g_selectedPart].size;
    s *= factor;
    s = glm::max(s, glm::vec3(0.5f));
    s = glm::min(s, glm::vec3(80.f));
}

/** AABB world di una Part (tiene conto di size + rotation). */
static void partWorldAabb(const WorkspacePart& p, glm::vec3& outMin, glm::vec3& outMax) {
    glm::mat4 m = partModelMatrix(p);
    outMin = glm::vec3(1e9f);
    outMax = glm::vec3(-1e9f);
    for (int ix = 0; ix < 2; ++ix)
        for (int iy = 0; iy < 2; ++iy)
            for (int iz = 0; iz < 2; ++iz) {
                glm::vec4 c = m * glm::vec4(ix ? 0.5f : -0.5f, iy ? 0.5f : -0.5f, iz ? 0.5f : -0.5f, 1.f);
                outMin = glm::min(outMin, glm::vec3(c));
                outMax = glm::max(outMax, glm::vec3(c));
            }
}

static bool aabbOverlap(const glm::vec3& amin, const glm::vec3& amax,
                        const glm::vec3& bmin, const glm::vec3& bmax) {
    return amin.x < bmax.x && amax.x > bmin.x
        && amin.y < bmax.y && amax.y > bmin.y
        && amin.z < bmax.z && amax.z > bmin.z;
}

/** Hitbox player R6 (piedi a g_playerPos.y). */
static void playerAabb(glm::vec3& outMin, glm::vec3& outMax) {
    const float hx = 1.05f, hy = 2.55f, hz = 1.05f;
    glm::vec3 c = g_playerPos + glm::vec3(0.f, hy, 0.f);
    outMin = c - glm::vec3(hx, hy, hz);
    outMax = c + glm::vec3(hx, hy, hz);
}

/** Boom FX leggero + danno player (falloff distanza). */
static void doBoomAt(glm::vec3 boomPos, float radius = 14.f, float maxDmg = 55.f, int fx = 24) {
    g_particles.spawnExplosion(boomPos, radius, fx);
    if (g_lua.playerHumanoid && g_lua.playerPos)
        g_lua.applyExplosionPlayerDamage(boomPos, radius, maxDmg);
    else if (!g_player.humanoid.dead) {
        glm::vec3 center = g_playerPos + glm::vec3(0.f, 2.5f, 0.f);
        float d = glm::length(center - boomPos);
        if (d < radius) {
            float t = 1.f - d / radius;
            g_player.humanoid.takeDamage(std::max(1.f, maxDmg * t * t));
            glm::vec3 push = center - boomPos;
            if (glm::length(push) > 1e-3f) {
                g_playerPos += glm::normalize(push) * (2.5f * t);
                g_player.humanoid.velocityY += 6.f * t;
                g_player.humanoid.grounded = false;
            }
        }
    }
}

/** Touched / boom: 1 avvicinamento basta; boomOnTouch puo' anche auto-esplodere. */
static void pollPartTouched() {
    if (g_screen != AppScreen::Playing
        && !(g_screen == AppScreen::Studio && g_studioTesting))
        return;
    glm::vec3 pmin, pmax;
    playerAabb(pmin, pmax);
    glm::vec3 playerCenter = (pmin + pmax) * 0.5f;
    if ((int)g_touching.size() != (int)g_world.parts.size())
        g_touching.assign(g_world.parts.size(), 0);

    std::vector<int> boomIdx;
    for (int i = 0; i < (int)g_world.parts.size(); ++i) {
        auto& part = g_world.parts[i];
        glm::vec3 bmin, bmax;
        partWorldAabb(part, bmin, bmax);
        bool aabbHit = aabbOverlap(pmin, pmax, bmin, bmax);

        // Distanza orizzontale generosa: il bounce CanCollide non fa perdere il 1° tocco
        glm::vec3 d = playerCenter - part.position;
        d.y = 0.f;
        float xzDist = glm::length(d);
        float reach = 0.5f * std::max(part.size.x, part.size.z) + 4.5f;
        bool nearHit = xzDist <= reach
            && std::abs(playerCenter.y - part.position.y) < (part.size.y * 0.5f + 4.f);

        if (part.boomOnTouch) {
            // PRIMO frame in range → boom (niente edge leave/re-enter)
            if (aabbHit || nearHit)
                boomIdx.push_back(i);
        } else {
            bool now = aabbHit;
            if (now && i < (int)g_touching.size() && !g_touching[i] && g_lua.ok)
                g_lua.firePartTouched(i);
            if (i < (int)g_touching.size())
                g_touching[i] = now ? 1 : 0;
        }
    }

    // Boom nativi (dal fondo: indici stabili)
    std::sort(boomIdx.begin(), boomIdx.end(), std::greater<int>());
    boomIdx.erase(std::unique(boomIdx.begin(), boomIdx.end()), boomIdx.end());
    for (int idx : boomIdx) {
        if (idx < 0 || idx >= (int)g_world.parts.size()) continue;
        if (!g_world.parts[idx].boomOnTouch) continue;
        glm::vec3 boomPos = g_world.parts[idx].position;
        doBoomAt(boomPos, 14.f, 55.f, 24);
        g_luaRunMsg = "BOOM! " + g_world.parts[idx].name;
        g_exportMsg = g_luaRunMsg;
        std::fprintf(stdout, "[Boom] touch/near '%s'\n", g_world.parts[idx].name.c_str());
        if (g_selectedPart == idx) selectPart(-1);
        else if (g_selectedPart > idx) --g_selectedPart;
        g_world.parts.erase(g_world.parts.begin() + idx);
        if (idx < (int)g_touching.size())
            g_touching.erase(g_touching.begin() + idx);
    }

    if (g_lua.ok)
        g_lua.flushPendingDestroy();
    if ((int)g_touching.size() != (int)g_world.parts.size())
        g_touching.assign(g_world.parts.size(), 0);
    if (g_selectedPart >= (int)g_world.parts.size())
        selectPart(-1);
}

/** Countdown auto-boom per parti boomOnTouch con boomAutoSecs > 0. */
static void tickBoomAutoFuse(float dt) {
    if (dt <= 0.f) return;
    if (g_screen != AppScreen::Playing
        && !(g_screen == AppScreen::Studio && g_studioTesting))
        return;
    for (int i = (int)g_world.parts.size() - 1; i >= 0; --i) {
        auto& p = g_world.parts[i];
        if (!p.boomOnTouch || p.boomAutoSecs <= 0.f) continue;
        p.boomAutoSecs -= dt;
        if (p.boomAutoSecs > 0.f) continue;
        p.boomAutoSecs = 0.f;
        glm::vec3 boomPos = p.position;
        doBoomAt(boomPos, 14.f, 55.f, 24);
        g_luaRunMsg = "BOOM! (auto) " + p.name;
        g_exportMsg = g_luaRunMsg;
        std::fprintf(stdout, "[Boom] auto fuse '%s'\n", p.name.c_str());
        if (g_selectedPart == i) selectPart(-1);
        else if (g_selectedPart > i) --g_selectedPart;
        g_world.parts.erase(g_world.parts.begin() + i);
        if (i < (int)g_touching.size())
            g_touching.erase(g_touching.begin() + i);
    }
}

/**
 * Collisione player SOLO vs Part con CanCollide.
 * Se CanCollide è off (No Collision) → attraversa.
 * axis: 0=X, 1=Y, 2=Z.
 *
 * Importante: sui pavimenti NON risolvere X/Z (altrimenti l'MTD laterale
 * ti scaraventa verso il bordo della piattaforma → void → respawn a casa).
 */
static void resolvePlayerAxis(int axis) {
    glm::vec3 pmin, pmax;
    playerAabb(pmin, pmax);

    // Solo il player viene spostato: le Part (anche non ancorate) NON si muovono al contatto.
    auto pushOut = [&](const glm::vec3& bmin, const glm::vec3& bmax) {
        if (!aabbOverlap(pmin, pmax, bmin, bmax)) return;

        float penX = std::min(bmax.x - pmin.x, pmax.x - bmin.x);
        float penY = std::min(bmax.y - pmin.y, pmax.y - bmin.y);
        float penZ = std::min(bmax.z - pmin.z, pmax.z - bmin.z);
        if (penX <= 0.f || penY <= 0.f || penZ <= 0.f) return;

        float pushPos = bmax[axis] - pmin[axis];
        float pushNeg = pmax[axis] - bmin[axis];
        if (pushPos <= 0.f && pushNeg <= 0.f) return;

        // Skin piccolo: evita vibrazione (parte che "balla" quando ci cammini sopra)
        const float skin = 0.002f;

        if (axis == 1) {
            const float maxStep = 3.2f;
            float up = pushPos;
            float down = pushNeg;
            // Atterraggio / cammino: preferisci sempre salire (non spingere sotto il pavimento)
            bool landing = g_player.humanoid.velocityY <= 0.5f && up <= maxStep;
            if (landing || up <= down) {
                if (up > maxStep) return; // troppo alto → gestito da X/Z (muro)
                g_playerPos.y += up + skin;
                if (g_player.humanoid.velocityY <= 0.f) {
                    g_player.humanoid.velocityY = 0.f;
                    g_player.humanoid.grounded = true;
                }
            } else {
                g_playerPos.y -= down + skin;
                if (g_player.humanoid.velocityY > 0.f)
                    g_player.humanoid.velocityY = 0.f;
            }
            playerAabb(pmin, pmax);
            return;
        }

        // Contatto pavimento/soffitto: la penetrazione Y è la più piccola → solo asse Y.
        // Senza questo, camminare su una piattaforma grande ti spinge al bordo più vicino.
        if (penY <= penX + 0.02f && penY <= penZ + 0.02f)
            return;
        // Piedi sulla faccia superiore (anche se penY non è la minima per float error)
        if (pmin.y >= bmax.y - 0.65f && g_player.humanoid.velocityY <= 4.f)
            return;

        float amount = (pushPos < pushNeg) ? pushPos : -pushNeg;
        // Cap: evita "teleport" laterali enormi da overlap ambigui
        if (std::fabs(amount) > 1.5f) {
            if (penY < 0.9f) return; // ancora sospetto pavimento
            amount = (amount > 0.f) ? 0.75f : -0.75f;
        }

        if (axis == 0) {
            g_playerPos.x += amount + (amount > 0.f ? skin : -skin);
            if (amount > 0.f && g_moveVel.x < 0.f) g_moveVel.x = 0.f;
            if (amount < 0.f && g_moveVel.x > 0.f) g_moveVel.x = 0.f;
        } else {
            g_playerPos.z += amount + (amount > 0.f ? skin : -skin);
            if (amount > 0.f && g_moveVel.z < 0.f) g_moveVel.z = 0.f;
            if (amount < 0.f && g_moveVel.z > 0.f) g_moveVel.z = 0.f;
        }
        playerAabb(pmin, pmax);
    };

    // Solo le Part con CanCollide (mai spostare la Part — solo il player)
    for (const auto& part : g_world.parts) {
        if (!part.canCollide) continue;
        glm::vec3 bmin, bmax;
        partWorldAabb(part, bmin, bmax);
        pushOut(bmin, bmax);
    }
}

static void respawnPlayerAtSpawn() {
    // Piedi sullo spawn (Y >= 0). Sblocca anche soft-kick AC (altrimenti resti fermo).
    g_playerPos = g_world.spawn.position;
    if (!std::isfinite(g_playerPos.x) || !std::isfinite(g_playerPos.y) || !std::isfinite(g_playerPos.z)
        || g_playerPos.y < 0.01f) {
        g_playerPos = {0.f, 0.f, 0.f};
    } else {
        g_playerPos.y = std::max(0.f, g_playerPos.y);
    }
    // Leggero offset sopra il suolo: evita di restare incastrato nella baseplate
    if (g_playerPos.y < 0.05f)
        g_playerPos.y = 0.05f;
    g_moveVel = {0, 0, 0};
    g_ragdoll.clear();
    g_player.humanoid.respawnFull();
    g_player.humanoid.velocityY = 0.f;
    g_player.humanoid.grounded = true;
    // Ripristina walk se l'AC l'aveva clampato per errore
    if (g_player.humanoid.walkSpeed < 4.f)
        g_player.humanoid.walkSpeed = 16.f;
    g_shiftLock = false;
    g_playMenuOpen = false;
    acOnLegitimateRespawn();
}

static void stepPlayerPhysicsWithCollision(float dt) {
    auto& h = g_player.humanoid;
    if (h.dead) {
        if (!g_ragdoll.active) {
            g_ragdoll.start(h, g_playerPos, g_playerYaw, g_moveVel, 0.f);
        }
        g_ragdoll.update(dt);
        if (g_ragdoll.torsoIdx >= 0)
            g_playerPos = g_ragdoll.focusPos() - glm::vec3(0.f, 3.f, 0.f);
        h.tickDeath(dt);
        g_moveVel = {0, 0, 0};
        h.velocityY = 0.f;
        if (h.shouldRespawn()) {
            g_ragdoll.clear();
            h.respawnFull();
            respawnPlayerAtSpawn();
        }
        return;
    }
    if (g_ragdoll.active)
        g_ragdoll.clear();

    // 1) Muri (X/Z) — i pavimenti vengono ignorati in resolvePlayerAxis
    resolvePlayerAxis(0);
    resolvePlayerAxis(2);

    const float gravity = 80.f;
    float prevVy = h.velocityY;
    h.grounded = false;
    h.velocityY -= gravity * dt;
    g_playerPos.y += h.velocityY * dt;

    // 2) Atterraggio / soffitto
    resolvePlayerAxis(1);
    // 3) Di nuovo X/Z dopo Y (ora i piedi sono sul pavimento → niente shove)
    resolvePlayerAxis(0);
    resolvePlayerAxis(2);

    // Pavimento solo DENTRO la baseplate (fuori = vuoto / skybox → cadi)
    bool onBaseplate = false;
    float hx = 64.f, hz = 64.f;
    if (g_world.hasBaseplate) {
        hx = g_world.baseplateSize.x * 0.5f;
        hz = g_world.baseplateSize.z * 0.5f;
        onBaseplate = std::fabs(g_playerPos.x) <= hx + 0.5f
                   && std::fabs(g_playerPos.z) <= hz + 0.5f;
    }
    if (onBaseplate && g_playerPos.y <= 0.f) {
        g_playerPos.y = 0.f;
        h.velocityY = 0.f;
        h.grounded = true;
    }

    // Danno da caduta (atterraggio duro)
    if (h.grounded && prevVy < -45.f) {
        float dmg = (-prevVy - 45.f) * 1.8f;
        h.takeDamage(dmg);
    }

    // Vuoto / fuori mappa → morte (health bar grigia) poi respawn
    const float killY = -12.f;
    const float mapMargin = 80.f;
    bool fellOffMap = g_playerPos.y < killY
        || std::fabs(g_playerPos.x) > hx + mapMargin
        || std::fabs(g_playerPos.z) > hz + mapMargin
        || !std::isfinite(g_playerPos.x) || !std::isfinite(g_playerPos.y)
        || !std::isfinite(g_playerPos.z);
    if (fellOffMap)
        h.takeDamage(h.maxHealth + 1.f);
}

static void onMouseBtn(GLFWwindow* win, int btn, int action, int) {
    if (btn == GLFW_MOUSE_BUTTON_RIGHT) {
        g_rmbLook = (action == GLFW_PRESS);
        g_firstMouse = true;
        if (g_rmbLook) endCatalogGizmo();
        if (g_screen == AppScreen::Playing
            || (g_screen == AppScreen::Studio && g_studioTesting)) {
            syncPlayCursor(win);
        } else {
            glfwSetInputMode(win, GLFW_CURSOR,
                g_rmbLook ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        }
    }
    if (btn == GLFW_MOUSE_BUTTON_LEFT && g_screen == AppScreen::AvatarPublisher
        && !g_rmbLook && !ImGui::GetIO().WantCaptureMouse) {
        if (action == GLFW_RELEASE) {
            endCatalogGizmo();
        } else if (action == GLFW_PRESS && g_catalog.selected >= 0
                   && g_catalog.selected < (int)g_catalog.items.size()) {
            double mx = 0, my = 0;
            glfwGetCursorPos(win, &mx, &my);
            int axis = 0;
            bool hit = false;
            if (g_catalog.tool == AccelTool::Move)
                hit = pickCatalogMoveAxis((float)mx, (float)my, axis);
            else if (g_catalog.tool == AccelTool::Rotate) {
                // Rotazione libera (mouse X/Y). Anello RGB solo se Shift (asse bloccato).
                if (gizmoShift() && pickCatalogRotateAxis((float)mx, (float)my, axis))
                    hit = true;
                else if (pickCatalogRotateFree((float)mx, (float)my)
                         || pickCatalogRotateAxis((float)mx, (float)my, axis)) {
                    hit = true;
                    axis = -1;
                }
            } else
                hit = pickCatalogScaleAxis((float)mx, (float)my, axis);
            if (hit) beginCatalogGizmo(axis, (float)mx, (float)my);
        }
    }
    if (btn == GLFW_MOUSE_BUTTON_LEFT && g_screen == AppScreen::Studio
        && g_mode == CamMode::Studio && !g_rmbLook && !ImGui::GetIO().WantCaptureMouse) {
        if (action == GLFW_RELEASE) {
            endScaleDrag();
            endMoveDrag();
            endRotateDrag();
            endLabelDrag();
            return;
        }
        if (action == GLFW_PRESS) {
            double mx = 0, my = 0;
            glfwGetCursorPos(win, &mx, &my);
            int axis = 0, sign = 0;
            if (g_tool == StudioTool::Scale && pickScaleHandle((float)mx, (float)my, axis, sign)) {
                beginScaleDrag(axis, sign);
            } else if (g_tool == StudioTool::Move && pickMoveHandle((float)mx, (float)my, axis)) {
                beginMoveDrag(axis, (float)mx, (float)my);
            } else if (g_tool == StudioTool::Rotate && pickRotateHandle((float)mx, (float)my, axis)) {
                beginRotateDrag(axis, (float)mx, (float)my);
            } else {
                int labHit = pickScreenLabelAt((float)mx, (float)my);
                if (labHit >= 0) {
                    selectLabel(labHit);
                    beginLabelDrag(labHit, (float)mx, (float)my);
                } else {
                    ScenePick hit = pickSceneAtMouse((float)mx, (float)my);
                    applyScenePick(hit);
                }
            }
        }
    }
    // Play/Test: TextButton ha priorità sull'attacco spada
    if (btn == GLFW_MOUSE_BUTTON_LEFT && playCameraActive()
        && !ImGui::GetIO().WantCaptureMouse) {
        double mx = 0, my = 0;
        glfwGetCursorPos(win, &mx, &my);
        if (action == GLFW_PRESS) {
            if (!g_playCursorLocked) {
                int hit = pickScreenLabelAt((float)mx, (float)my);
                if (hit >= 0 && g_world.labels[hit].isButton()) {
                    g_uiPressedLabel = hit;
                    return;
                }
            }
            trySwordAttack();
        } else if (action == GLFW_RELEASE) {
            if (g_uiPressedLabel >= 0) {
                int hit = g_playCursorLocked ? g_uiPressedLabel
                    : pickScreenLabelAt((float)mx, (float)my);
                if (hit == g_uiPressedLabel)
                    fireGuiButton(g_uiPressedLabel);
                g_uiPressedLabel = -1;
            }
        }
    }
}

static const glm::vec3 kGizmoRGB[3] = {
    {0.95f, 0.22f, 0.20f},
    {0.22f, 0.88f, 0.32f},
    {0.28f, 0.52f, 1.00f},
};

static void drawGizmoOverlayHud() {
    if (g_studioTesting || g_screen != AppScreen::Studio) return;
    if (!g_moveDragging && !g_rotateDragging && !g_scaleDragging) return;
    ImGui::SetNextWindowPos({(float)g_w * 0.5f - 140.f, 64.f}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.72f);
    ImGui::Begin("##GizmoHud", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs);
    if (g_rotateDragging) {
        const char* ax = (g_rotateAxis == 0) ? "X" : (g_rotateAxis == 1) ? "Y" : "Z";
        ImGui::TextColored({1.f, 0.85f, 0.3f, 1.f}, "Rotate %s  %+0.1f°", ax, g_rotateLiveDelta);
        ImGui::TextDisabled("Snap 5° · Shift=1° · Ctrl=libero");
    } else if (g_moveDragging) {
        const char* ax = (g_moveAxis == 0) ? "X" : (g_moveAxis == 1) ? "Y" : "Z";
        ImGui::TextColored({0.5f, 0.9f, 1.f, 1.f}, "Move %s  %+0.2f", ax, g_moveLiveDelta);
        ImGui::TextDisabled("Snap 0.25 · Shift=0.05 · Ctrl=libero");
    } else if (g_scaleDragging) {
        const char* ax = (g_scaleAxis == 0) ? "X" : (g_scaleAxis == 1) ? "Y" : "Z";
        ImGui::TextColored({0.6f, 1.f, 0.55f, 1.f}, "Scale %s  %.2f", ax, g_scaleLiveSize);
        ImGui::TextDisabled("Snap 0.25 · Shift=0.1 · Ctrl=libero");
    }
    ImGui::End();
}

static void drawScaleHandles(GLuint vao, GLuint partP, const glm::mat4& view,
                             const glm::mat4& proj, const glm::vec3& sun) {
    if (g_tool != StudioTool::Scale || g_studioTesting) return;
    if (g_screen != AppScreen::Studio) return;
    const WorkspacePart* pt = gizmoPart();
    if (!pt) return;
    const auto& p = *pt;
    for (int axis = 0; axis < 3; ++axis) {
        for (int s = -1; s <= 1; s += 2) {
            glm::vec3 hp = handleWorldPos(p, axis, s);
            bool hot = (g_scaleDragging && g_scaleAxis == axis && g_scaleSign == s);
            float hs = hot ? kHandleSize * 1.45f : kHandleSize;
            glm::vec3 col = hot ? glm::mix(kGizmoRGB[axis], glm::vec3(1.f), 0.35f) : kGizmoRGB[axis];
            // cubo faccia + piccolo bordo
            glm::mat4 m = glm::scale(glm::translate(glm::mat4(1.f), hp), glm::vec3(hs));
            drawCube(vao, partP, m, view, proj, col, sun, false, 0);
            glm::mat4 m2 = glm::scale(glm::translate(glm::mat4(1.f), hp), glm::vec3(hs * 1.2f));
            drawCube(vao, partP, m2, view, proj, col * 0.45f, sun, false, 0);
        }
    }
}

static void drawMoveHandles(GLuint vao, GLuint partP, const glm::mat4& view,
                            const glm::mat4& proj, const glm::vec3& sun) {
    if (g_tool != StudioTool::Move || g_studioTesting) return;
    if (g_screen != AppScreen::Studio) return;
    const WorkspacePart* pt = gizmoPart();
    if (!pt) return;
    const auto& p = *pt;
    float len = gizmoArrowLen(p);
    if (!g_moveDragging) {
        double mx = 0, my = 0;
        glfwGetCursorPos(glfwGetCurrentContext(), &mx, &my);
        int ax = -1;
        if (!pickMoveHandle((float)mx, (float)my, ax))
            g_gizmoHoverAxis = -1;
        else
            g_gizmoHoverAxis = ax;
    }
    // sfera al centro
    {
        glm::mat4 m = glm::scale(glm::translate(glm::mat4(1.f), p.position), glm::vec3(0.35f));
        drawCube(vao, partP, m, view, proj, glm::vec3(0.95f, 0.95f, 0.98f), sun, false, 0);
    }
    for (int axis = 0; axis < 3; ++axis) {
        bool hot = (g_moveDragging && g_moveAxis == axis) || (g_gizmoHoverAxis == axis && !g_moveDragging);
        glm::vec3 col = hot ? glm::mix(kGizmoRGB[axis], glm::vec3(1.f), 0.4f) : kGizmoRGB[axis];
        glm::vec3 dir = partAxisDir(p, axis);
        // stelo
        float stemLen = len * 0.78f;
        glm::vec3 stemMid = p.position + dir * (stemLen * 0.5f);
        float thick = hot ? 0.16f : 0.12f;
        glm::mat4 mStem = orientAlongZ(stemMid, dir, {thick, thick, stemLen});
        drawCube(vao, partP, mStem, view, proj, col * 0.92f, sun, false, 0);
        // punta a piramide (cubo rastremato)
        glm::vec3 tip = p.position + dir * len;
        float tipS = hot ? 0.55f : 0.42f;
        glm::mat4 mTip = orientAlongZ(tip - dir * (tipS * 0.35f), dir, {tipS, tipS, tipS * 1.35f});
        drawCube(vao, partP, mTip, view, proj, col, sun, false, 0);
    }
}

static void drawRotateHandles(GLuint vao, GLuint partP, const glm::mat4& view,
                              const glm::mat4& proj, const glm::vec3& sun) {
    if (g_tool != StudioTool::Rotate || g_studioTesting) return;
    if (g_screen != AppScreen::Studio) return;
    const WorkspacePart* pt = gizmoPart();
    if (!pt) return;
    const auto& p = *pt;
    float R = gizmoRingRadius(p);
    const int segs = 64;

    // hover update se non stiamo trascinando
    if (!g_rotateDragging) {
        double mx = 0, my = 0;
        glfwGetCursorPos(glfwGetCurrentContext(), &mx, &my);
        int ax = -1;
        if (!pickRotateHandle((float)mx, (float)my, ax))
            g_gizmoHoverAxis = -1;
    }

    for (int axis = 0; axis < 3; ++axis) {
        bool hot = (g_rotateDragging && g_rotateAxis == axis)
                || (g_gizmoHoverAxis == axis);
        glm::vec3 col = hot ? glm::mix(kGizmoRGB[axis], glm::vec3(1.f), 0.45f) : kGizmoRGB[axis];
        float thick = hot ? 0.22f : 0.14f;
        glm::vec3 axisV = partAxisDir(p, axis);
        glm::vec3 ref = (std::fabs(axisV.y) < 0.9f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
        glm::vec3 u = glm::normalize(glm::cross(axisV, ref));
        glm::vec3 v = glm::normalize(glm::cross(axisV, u));

        // arco pieno dettagliato
        for (int i = 0; i < segs; ++i) {
            float a = (float)i / (float)segs * 6.2831853f;
            glm::vec3 pos = p.position + (u * std::cos(a) + v * std::sin(a)) * R;
            glm::mat4 m = glm::scale(glm::translate(glm::mat4(1.f), pos), glm::vec3(thick));
            drawCube(vao, partP, m, view, proj, col, sun, false, 0);
        }

        // tick ogni 15° per rotazione precisa
        for (int tick = 0; tick < 24; ++tick) {
            float a = (float)tick / 24.f * 6.2831853f;
            glm::vec3 radial = u * std::cos(a) + v * std::sin(a);
            glm::vec3 pos = p.position + radial * (R + 0.28f);
            float ts = (tick % 6 == 0) ? 0.20f : 0.11f; // ogni 90° più grande
            glm::mat4 m = glm::scale(glm::translate(glm::mat4(1.f), pos), glm::vec3(ts));
            drawCube(vao, partP, m, view, proj, col * 0.75f, sun, false, 0);
        }

        // settore angolo durante drag
        if (g_rotateDragging && g_rotateAxis == axis && std::fabs(g_rotateLiveDelta) > 0.5f) {
            int nFill = std::clamp((int)std::fabs(g_rotateLiveDelta) / 3, 2, 40);
            float sign = (g_rotateLiveDelta >= 0.f) ? 1.f : -1.f;
            float startA = g_rotateStartAngle;
            for (int i = 0; i <= nFill; ++i) {
                float a = startA + sign * glm::radians(std::fabs(g_rotateLiveDelta)) * ((float)i / nFill);
                glm::vec3 pos = p.position + (u * std::cos(a) + v * std::sin(a)) * (R * 0.92f);
                glm::mat4 m = glm::scale(glm::translate(glm::mat4(1.f), pos), glm::vec3(0.18f));
                drawCube(vao, partP, m, view, proj, glm::vec3(1.f, 0.92f, 0.35f), sun, false, 0);
            }
        }
    }
    // nucleo
    glm::mat4 core = glm::scale(glm::translate(glm::mat4(1.f), p.position), glm::vec3(0.28f));
    drawCube(vao, partP, core, view, proj, glm::vec3(0.9f, 0.9f, 0.95f), sun, false, 0);
}

static void drawAvatarAccessories(GLuint meshP, const glm::mat4& view, const glm::mat4& proj,
                                  const glm::vec3& sun, glm::vec3 worldPos, float yawDeg,
                                  const Humanoid& hum, GLuint markerVao = 0, GLuint partP = 0,
                                  bool editorGhostMode = false) {
    if (g_catalog.items.empty()) return;
    float bob = humanoidBob(hum);
    float legSwing = humanoidLegSwing(hum);
    float armBase = humanoidArmSwing(hum);
    float jL = 0.f, jR = 0.f, jl = 0.f, jr = 0.f;
    humanoidJumpPose(hum, jL, jR, jl, jr);
    const float jb = hum.jumpArmBlend;
    float armL = armBase * (1.f - jb) + jL;
    float armR = -armBase * (1.f - jb) + jR;
    float legL = legSwing * (1.f - jb) + jl;
    float legR = -legSwing * (1.f - jb) + jr;

    glm::mat4 root = glm::rotate(glm::translate(glm::mat4(1.f), worldPos),
                                 glm::radians(yawDeg), glm::vec3(0, 1, 0));

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    auto drawOne = [&](int i, float alpha, bool depthWrite) {
        const auto& a = g_catalog.items[i];
        if (!a.mesh.loaded || a.mesh.vertexCount <= 0) return;
        glm::mat4 local = accessoryWorldMatrix(a, hum, bob, armL, armR, legL, legR);
        glm::mat4 model = root * local;

        glm::vec3 tint = a.tint;
        if (editorGhostMode && i == g_catalog.selected)
            tint = glm::mix(tint, glm::vec3(1.f, 0.88f, 0.35f), 0.22f);
        else if (!editorGhostMode && i == g_catalog.selected)
            tint = glm::mix(tint, glm::vec3(1.f, 0.85f, 0.2f), 0.25f);

        glDepthMask(depthWrite ? GL_TRUE : GL_FALSE);
        if (alpha < 0.99f) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        } else {
            glDisable(GL_BLEND);
        }

        glUseProgram(meshP);
        glUniformMatrix4fv(glGetUniformLocation(meshP,"uModel"),1,GL_FALSE,glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(meshP,"uView"),1,GL_FALSE,glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(meshP,"uProj"),1,GL_FALSE,glm::value_ptr(proj));
        glUniform3fv(glGetUniformLocation(meshP,"uSun"),1,glm::value_ptr(sun));
        glUniform3fv(glGetUniformLocation(meshP,"uCamPos"),1,glm::value_ptr(g_renderCamPos));
        uploadDynamicLights(meshP); // accessori illuminati come il resto del mondo
        a.mesh.drawTextured(meshP, tint, alpha);
        (void)markerVao;
        (void)partP;
    };

    if (editorGhostMode) {
        // Accessori già sull'avatar → trasparenti
        for (int i = 0; i < (int)g_catalog.items.size(); ++i) {
            if (i == g_catalog.selected) continue;
            drawOne(i, 0.38f, false);
        }
        // Oggetto in modifica → pieno
        if (g_catalog.selected >= 0 && g_catalog.selected < (int)g_catalog.items.size())
            drawOne(g_catalog.selected, 1.f, true);
        else {
            for (int i = 0; i < (int)g_catalog.items.size(); ++i)
                drawOne(i, 0.72f, true);
        }
    } else {
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        for (int i = 0; i < (int)g_catalog.items.size(); ++i)
            drawOne(i, 1.f, true);
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
}

/** Draw dei cubi nel depth pass: solo uModel + geometria (ignora colore/luci). */
static void drawCubeShadow(GLuint vao, GLuint prog, const glm::mat4& m, const glm::mat4&,
                           const glm::mat4&, const glm::vec3&, const glm::vec3&) {
    (void)prog;
    int vc = (g_bevelCube.vao && vao == g_bevelCube.vao) ? g_bevelCube.vertexCount : g_sharpCubeVerts;
    g_shadowMap.setModel(m);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, vc);
}

/**
 * Depth pass di sole/luna: ridisegna la geometria reale (sfere, cilindri, wedge,
 * modelli importati, avatar) così l'ombra ha la forma della mesh, non del box.
 */
static void renderShadowDepth(GLuint vao, GLuint meshP) {
    g_sunShadowOn = false;
    if (!g_lighting.globalShadows || g_engine.quality.shadowOccluders <= 0) return;
    if (!g_shadowMap.ready) return;

    // Centra la mappa poco davanti alla camera: più risoluzione dove guardi
    glm::vec3 focus = g_renderCamPos;
    if (!g_shadowMap.begin(g_lighting.toSkyLight(), focus)) return;
    g_sunShadowOn = true;

    const float reach = g_shadowMap.extent + 24.f;
    const float reach2 = reach * reach;

    // —— Parti del mondo (con la loro mesh: block / ball / cylinder / wedge) ——
    for (const auto& p : g_world.parts) {
        if (p.isTransparent()) continue; // vetro/ForceField: la luce passa
        glm::vec3 d = p.position - focus;
        if (glm::dot(d, d) > reach2) continue;
        GLuint dv = vao;
        int vc = 36;
        if (p.shape == PartShape::Ball && g_partMeshes.ball.vao) {
            dv = g_partMeshes.ball.vao;
            vc = g_partMeshes.ball.vertexCount;
        } else if (p.shape == PartShape::Cylinder && g_partMeshes.cylinder.vao) {
            dv = g_partMeshes.cylinder.vao;
            vc = g_partMeshes.cylinder.vertexCount;
        } else if (p.shape == PartShape::Wedge && g_partMeshes.wedge.vao) {
            dv = g_partMeshes.wedge.vao;
            vc = g_partMeshes.wedge.vertexCount;
        }
        g_shadowMap.setModel(partModelMatrix(p));
        glBindVertexArray(dv);
        glDrawArrays(GL_TRIANGLES, 0, vc);
    }

    // —— Modelli importati ——
    for (int i = 0; i < (int)g_world.models.size(); ++i) {
        if (i >= (int)g_wsMeshes.size() || !g_wsMeshes[i].loaded) continue;
        const auto& m = g_world.models[i];
        glm::vec3 d = m.position - focus;
        if (glm::dot(d, d) > reach2) continue;
        g_shadowMap.setModel(glm::translate(glm::mat4(1.f), m.position)
                             * partRotationMatrix(m.rotation)
                             * glm::scale(glm::mat4(1.f), m.scale));
        g_wsMeshes[i].drawTextured(g_shadowMap.depthProg, {1.f, 1.f, 1.f}, 1.f);
    }

    // —— Avatar: player locale, remoti, amici ——
    const glm::mat4 idm(1.f);
    const glm::vec3 zero(0.f);
    GLuint playerVao = g_bevelCube.vao ? g_bevelCube.vao : vao;
    BlockDrawContext sctx{ playerVao, g_shadowMap.depthProg, idm, idm, zero };
    const bool inWorld = (g_screen == AppScreen::Playing)
        || (g_screen == AppScreen::Studio);
    if (inWorld && !g_ragdoll.active) {
        bool hideBody[(int)AvatarSlot::Count] = {};
        fillBodyHideMask(g_catalog, hideBody);
        // In prima persona il corpo non si disegna, ma l'ombra deve restare
        g_player.draw(sctx, drawCubeShadow, g_playerPos, g_playerYaw, hideBody);
        drawAvatarAccessories(g_shadowMap.depthProg, idm, idm, zero,
                              g_playerPos, g_playerYaw, g_player.humanoid);
        // drawAvatarAccessories riaccende il culling: nel depth pass non lo vogliamo
        glDisable(GL_CULL_FACE);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
        glUseProgram(g_shadowMap.depthProg);
    }
    for (size_t i = 0; i < g_net.remotes.size() && i < g_remoteBodies.size(); ++i)
        g_remoteBodies[i].draw(sctx, drawCubeShadow,
                               g_net.remotes[i].displayPos, g_net.remotes[i].displayYaw);

    g_shadowMap.end(g_w, g_h);
}

static void renderWorld3D(GLuint vao, GLuint studP, GLuint partP, GLuint studId,
                          GLuint skinP, GLuint meshP, AnimatedModel* avatarGlb, bool useGlb,
                          const glm::mat4& view, const glm::mat4& proj, const glm::vec3& sun) {
    g_lastView = view;
    g_lastProj = proj;
    g_renderCamPos = (g_screen == AppScreen::Studio && g_mode == CamMode::Studio && !g_studioTesting)
        ? g_cam.pos : g_playerPos + glm::vec3(0, 5, 0);
    collectDynamicLights();
    collectShadowOccluders();
    renderShadowDepth(vao, meshP);
    if (g_world.hasBaseplate) {
        glm::vec3 bpCol = g_world.baseplateColor;
        if (g_selectedBaseplate)
            bpCol = glm::mix(bpCol, glm::vec3(0.2f, 0.65f, 1.f), 0.35f);
        glm::mat4 bm = glm::scale(glm::translate(glm::mat4(1.f), {0,-1,0}), g_world.baseplateSize);
        drawCube(vao, studP, bm, view, proj, bpCol, sun, true, studId, 36, 0.f, PartMaterial::Concrete);
    }
    // Culling per distanza + priorità ai più vicini (engine power)
    {
        glm::vec3 origin = g_renderCamPos;
        const float maxD = g_engine.quality.maxDrawDistance;
        const int maxN = g_engine.quality.maxVisibleParts;
        struct Cand { int i; float d2; bool transparent; };
        std::vector<Cand> cands;
        cands.reserve(g_world.parts.size());
        for (int i = 0; i < (int)g_world.parts.size(); ++i) {
            glm::vec3 d = g_world.parts[i].position - origin;
            float d2 = glm::dot(d, d);
            if (d2 > maxD * maxD) continue;
            cands.push_back({i, d2, g_world.parts[i].isTransparent()});
        }
        if ((int)cands.size() > maxN) {
            std::nth_element(cands.begin(), cands.begin() + maxN, cands.end(),
                [](const Cand& a, const Cand& b) { return a.d2 < b.d2; });
            cands.resize(maxN);
        }
        // opachi far→near, poi trasparenti near→far
        std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) {
            if (a.transparent != b.transparent) return !a.transparent && b.transparent;
            if (a.transparent) return a.d2 < b.d2;
            return a.d2 > b.d2;
        });
        for (const auto& c : cands) {
            auto& p = g_world.parts[c.i];
            glm::mat4 m = partModelMatrix(p);
            bool useStud = p.useStud && g_engine.quality.studTextures
                && !p.isNeon() && p.material == PartMaterial::Plastic
                && p.shape == PartShape::Block;
            GLuint drawVao = vao;
            int vertCount = 36;
            if (p.shape == PartShape::Ball && g_partMeshes.ball.vao) {
                drawVao = g_partMeshes.ball.vao;
                vertCount = g_partMeshes.ball.vertexCount;
            } else if (p.shape == PartShape::Cylinder && g_partMeshes.cylinder.vao) {
                drawVao = g_partMeshes.cylinder.vao;
                vertCount = g_partMeshes.cylinder.vertexCount;
            } else if (p.shape == PartShape::Wedge && g_partMeshes.wedge.vao) {
                drawVao = g_partMeshes.wedge.vao;
                vertCount = g_partMeshes.wedge.vertexCount;
            }
            drawCube(drawVao, useStud ? studP : partP, m, view, proj, p.color, sun, useStud, studId,
                     vertCount, p.effectiveEmissive(), p.material);
        }
    }
    // Outline blu solo su Model/Spawn/Player — NON sulle Part (solo nome sopra)
    // drawSelectedPartBlueOutline: parti senza cubo blu
    drawSelectedPartBlueOutline(vao, partP, view, proj);
    drawScaleHandles(vao, partP, view, proj, sun);
    drawMoveHandles(vao, partP, view, proj, sun);
    drawRotateHandles(vao, partP, view, proj, sun);
    drawWorkspaceModels(meshP, skinP, view, proj, sun);

    // Avatar animato R6 (walk + jump arms). GLB opzionale solo se richiesto e a terra.
    bool drawGlb = useGlb && avatarGlb && avatarGlb->loaded
                && g_player.humanoid.jumpArmBlend < 0.15f
                && !g_walking
                && !(g_firstPerson && (g_screen == AppScreen::Playing
                    || (g_screen == AppScreen::Studio && g_studioTesting)));
    if (drawGlb) {
        avatarGlb->update(0.f);
        glm::mat4 fm = avatarGlb->placement(5.5f, g_playerPos.y,
            { g_playerPos.x, 0.f, g_playerPos.z });
        fm = glm::translate(glm::mat4(1.f), g_playerPos)
           * glm::rotate(glm::mat4(1.f), glm::radians(g_playerYaw + 90.f), {0,1,0})
           * glm::translate(glm::mat4(1.f), -g_playerPos)
           * fm;
        glUseProgram(skinP);
        glUniformMatrix4fv(glGetUniformLocation(skinP,"uModel"),1,GL_FALSE,glm::value_ptr(fm));
        glUniformMatrix4fv(glGetUniformLocation(skinP,"uView"),1,GL_FALSE,glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(skinP,"uProj"),1,GL_FALSE,glm::value_ptr(proj));
        glUniform3fv(glGetUniformLocation(skinP,"uSun"),1,glm::value_ptr(sun));
        avatarGlb->draw(skinP);
    } else {
        GLuint playerVao = g_bevelCube.vao ? g_bevelCube.vao : vao;
        BlockDrawContext bctx{ playerVao, partP, view, proj, sun };
        // Nascosto SOLO in locale in prima persona: gli altri client
        // disegnano comunque questo player come remote avatar.
        const bool hideLocalFp = g_firstPerson
            && (g_screen == AppScreen::Playing
                || (g_screen == AppScreen::Studio && g_studioTesting))
            && !g_ragdoll.active;
        if (g_ragdoll.active) {
            g_ragdoll.draw(drawCubePlayer, playerVao, partP, view, proj, sun);
        } else if (!hideLocalFp) {
            bool hideBody[(int)AvatarSlot::Count] = {};
            fillBodyHideMask(g_catalog, hideBody);
            // Se il player tiene un oggetto (item equipaggiato), alza il braccio DX.
            float armHold = (g_world.equippedBackpack >= 0) ? -55.f : 0.f;
            g_player.draw(bctx, drawCubePlayer, g_playerPos, g_playerYaw, hideBody, armHold);
            drawAvatarAccessories(meshP, view, proj, sun,
                g_playerPos, g_playerYaw, g_player.humanoid, vao, partP);
            drawEquippedSword(vao, partP, view, proj, sun);
        }
        // Sempre: avatar degli altri giocatori (anche se tu sei in FP)
        if (g_remoteBodies.size() < g_net.remotes.size())
            g_remoteBodies.resize(g_net.remotes.size());
        for (size_t i = 0; i < g_net.remotes.size(); ++i) {
            const auto& r = g_net.remotes[i];
            if (i < g_remoteBodies.size())
                g_remoteBodies[i].draw(bctx, drawCubePlayer, r.displayPos, r.displayYaw);
        }
    }
    drawFriendPlayers(vao, partP, view, proj, sun);
}

static void startLoadingGame(int idx) {
    if (idx < 0 || idx >= (int)g_games.size()) return;
    g_activeGame = g_games[idx];
    g_loadTimer = 0.f;
    g_screen = AppScreen::Loading;
}

static void bootPlaceScript(const std::string& scriptRel) {
    if (!g_lua.ok) {
        if (!g_lua.init(&g_world)) {
            std::fprintf(stderr, "Lua init fallita: %s\n", g_lua.lastError.c_str());
            return;
        }
        bindLuaServices();
    }
    if (scriptRel.empty()) return;
    // Se sembra sorgente Lua inline (package vecchio), eseguilo direttamente
    if (scriptRel.find('\n') != std::string::npos ||
        (scriptRel.size() > 8 && scriptRel.find(".lua") == std::string::npos
         && scriptRel.find(".bbc") == std::string::npos
         && scriptRel.find('\\') == std::string::npos && scriptRel.find('/') == std::string::npos)) {
        std::fprintf(stdout, "Eseguo script inline (%d char)\n", (int)scriptRel.size());
        if (!g_lua.runString(scriptRel))
            g_lua.runString("print('Avviso: script non caricato')");
        return;
    }
    // Percorso assoluto già risolto (package) oppure relativo assets/
    std::string scriptPath = fileExistsPath(scriptRel) ? scriptRel : resolveScriptPath(scriptRel);
    std::fprintf(stdout, "Carico script (SafeBytecode preferito): %s\n", scriptPath.c_str());
    // Play: preferisci .bbc sigillato; builtin .lua restano ok
    if (!g_lua.runScriptPathSmart(scriptPath, true)) {
        g_lua.runString("print('Avviso: script non caricato')");
    }
}

static void finishLoading() {
    applyGamePlace(g_activeGame, g_world, &g_lighting);
    syncFriendsFromWorld();
    g_lua.requireSealedScripts = true; // Play: .bbc o assets trusted
    bootPlaceScript(g_activeGame.script);
    runAllPartScripts(false); // stessi script Part anche in Play
    g_lua.requireSealedScripts = false;
    acBoot();
    respawnPlayerAtSpawn();
    g_playerYaw = 180.f; // guarda verso -Z (dove punta la camera a orbitYaw=0)
    g_orbitYaw = 0.f;
    g_orbitPitch = -22.f;
    g_scaleDragging = g_moveDragging = g_rotateDragging = false;
    g_selectedPart = -1;
    g_mode = CamMode::Play;
    g_screen = AppScreen::Playing;
    g_playMenuOpen = false;
    g_showChat = false;
    g_shiftLock = false;
    g_firstPerson = false;
    g_moveVel = {0, 0, 0};
    g_menu.wantPlay = false;
    g_menu.launchGame = -1;
    g_useGlbAvatar = false; // animazioni walk/jump sul BlockPlayer
    g_remoteBodies.clear();

    const char* uname = g_menu.username[0] ? g_menu.username : "Player";
    int gi = 0;
    for (int i = 0; i < (int)g_games.size(); ++i)
        if (g_games[i].id == g_activeGame.id) { gi = i; break; }
    int maxP = g_activeGame.maxPlayers > 0 ? g_activeGame.maxPlayers : 8;
    maxP = std::clamp(maxP, 1, 32);
    // Il gate riparte a ogni partita: 8 secondi per concludere l'handshake.
    g_gatePassed = false;
    g_gateDeadline = glfwGetTime() + 8.0;
    g_gateTitle[0] = '\0';
    g_gateMsg[0] = '\0';

    if (g_joinByIp) {
        // Indirizzo scritto a mano: niente broadcast, si va dritti al server.
        // E' l'unico modo di raggiungere un server fuori dalla propria rete.
        g_joinByIp = false;
        std::snprintf(g_net.localName, sizeof(g_net.localName), "%.23s", uname);
        if (!g_net.startClient(g_menu.joinIp, g_menu.joinPort, gi, uname)) {
            std::snprintf(g_menu.netStatus, sizeof(g_menu.netStatus),
                          "Impossibile contattare %s:%d", g_menu.joinIp, g_menu.joinPort);
        }
    } else {
        // PLAY = auto multiplayer: join a chi è già in partita, altrimenti sei tu l'host
        g_net.startAutoMatch(27015, gi, uname, maxP);
    }
    if (g_net.role == NetRole::Client)
        g_playerPos.z = 4.f;
    else
        g_playerPos.z = 0.f;
    std::snprintf(g_menu.netStatus, sizeof(g_menu.netStatus), "%s", g_net.status);

    g_world.equippedBackpack = -1;
    for (bool& b : g_bpKeyWas) b = false;
    for (bool& b : g_bpKpWas) b = false;

    glfwSetWindowTitle(glfwGetCurrentContext(), ("Blocky — " + g_activeGame.title).c_str());
    setPlayCursor(glfwGetCurrentContext(), true);
}

static void enterStudio() {
    g_world.loadBaseplatePlace();
    g_placeFilePath.clear();
    g_wsMeshes.clear();
    g_sounds.stopAll();
    g_sounds.sounds.clear();
    g_selectedModel = -1;
    g_selectedSound = -1;
    g_animator.stop();
    g_showAnimEditor = false;
    g_showRigPrompt = false;
    g_animPlaying = false;
    g_animEditModel = -1;
    g_showPlayerRigPrompt = false;
    g_showPlayerRigEditor = false;
    clearPlayerSelection();
    acBoot();
    ensureDefaultBeepWav();
    if (g_gameNameBuf[0])
        g_world.placeName = g_gameNameBuf;
    else
        std::snprintf(g_gameNameBuf, sizeof(g_gameNameBuf), "%s", g_world.placeName.c_str());
    g_publishMsg.clear();
    g_studioTesting = false;
    g_activeGame = {"studio", "Studio", "Build", "scripts/baseplate.lua", 0, {0.2f, 0.62f, 0.35f}};
    // Editor: nessuno script in esecuzione (partono solo con Test)
    if (g_lua.ok) g_lua.clearHeartbeats();
    g_mode = CamMode::Studio;
    g_cam.pos = {0, 12, 28};
    g_screen = AppScreen::Studio;
    g_useGlbAvatar = false;
    selectPart(g_world.parts.empty() ? -1 : 0);
    g_tool = StudioTool::Select;
    applyStudioTheme();
    glfwSetWindowTitle(glfwGetCurrentContext(), kAppBrand);
    setPlayCursor(glfwGetCurrentContext(), false);
    clearPlaceDirty();
}

static void startStudioTest(GLFWwindow* win) {
    if (g_screen != AppScreen::Studio) return;
    CrashHandler::note("Start Studio Test");
    // Salva stato editor (parti / player / selezione) prima che gli script modifichino tutto
    g_worldTestBackup = g_world;
    g_hasTestBackup = true;
    g_friendsTestBackup = g_friends;
    g_playerPosTestBackup = g_playerPos;
    g_playerYawTestBackup = g_playerYaw;
    g_selectedPartTestBackup = g_selectedPart;
    g_selectedModelTestBackup = g_selectedModel;
    g_selectedLabelTestBackup = g_selectedLabel;

    g_studioCamBackup = g_cam.pos;
    g_studioYawBackup = g_cam.yaw;
    g_studioPitchBackup = g_cam.pitch;
    g_scaleDragging = g_moveDragging = g_rotateDragging = false;
    g_studioTesting = true;
    g_mode = CamMode::Play;
    g_modelAnimClock = 0.f;
    respawnPlayerAtSpawn();
    g_playerYaw = 180.f;
    g_orbitYaw = 0.f;
    g_orbitPitch = -22.f;
    g_orbitDist = 14.f;
    g_moveVel = {0, 0, 0};
    g_shiftLock = false;
    g_firstPerson = false;
    g_playMenuOpen = false;
    g_showChat = false;
    g_scaleDragging = g_moveDragging = g_rotateDragging = false;
    g_player = BlockPlayer{};
    g_ragdoll.clear();
    g_particles.clear();
    g_touching.clear();
    g_world.equippedBackpack = -1; // nessuno equip all'avvio Test
    for (bool& b : g_bpKeyWas) b = false;
    for (bool& b : g_bpKpWas) b = false;
    // Parti "Explode*" SENZA script → boom nativo; CON script → solo Lua
    for (auto& p : g_world.parts) {
        if (p.name.size() >= 7 && p.name.compare(0, 7, "Explode") == 0
            && p.script.empty()) {
            p.boomOnTouch = true;
            if (p.boomAutoSecs <= 0.f) p.boomAutoSecs = 3.f;
            p.canCollide = false;
        }
    }
    startTestScripts(); // script SOLO in Test
    setPlayCursor(win, true);
    char t[192];
    std::snprintf(t, sizeof(t), "%s — Test: %s", kAppBrand, g_world.placeName.c_str());
    glfwSetWindowTitle(win, t);
}

static void stopStudioTest(GLFWwindow* win) {
    if (!g_studioTesting) return;
    g_studioTesting = false;
    g_mode = CamMode::Studio;

    // Ferma script e ripristina place + player come prima del Test
    g_lua.clearHeartbeats();
    scriptControls().jumpEnabled = true;
    scriptControls().walkEnabled = true;
    scriptControls().walkSpeedOverride = -1.f;
    scriptControls().jumpPowerOverride = -1.f;
    g_particles.clear();
    g_touching.clear();
    g_ragdoll.clear();
    g_moveVel = {0, 0, 0};
    g_walking = false;
    g_shiftLock = false;
    g_firstPerson = false;
    g_playMenuOpen = false;
    g_swordSwing = 0.f;
    g_swordCooldown = 0.f;

    if (g_hasTestBackup) {
        g_world = std::move(g_worldTestBackup);
        g_hasTestBackup = false;
        g_friends = std::move(g_friendsTestBackup);
        g_playerPos = g_playerPosTestBackup;
        g_playerYaw = g_playerYawTestBackup;
        g_player = BlockPlayer{};
        g_animator.stop();
        // Ripristina selezione se ancora valida
        g_selectedPart = -1;
        g_selectedModel = -1;
        g_selectedLabel = -1;
        g_selectedSound = -1;
        g_selectedBaseplate = false;
        g_selectedSpawn = false;
        g_selectedPlayer = false;
        if (g_selectedPartTestBackup >= 0
            && g_selectedPartTestBackup < (int)g_world.parts.size())
            selectPart(g_selectedPartTestBackup);
        else if (g_selectedModelTestBackup >= 0
                 && g_selectedModelTestBackup < (int)g_world.models.size())
            selectModel(g_selectedModelTestBackup);
        else if (g_selectedLabelTestBackup >= 0
                 && g_selectedLabelTestBackup < (int)g_world.labels.size())
            selectLabel(g_selectedLabelTestBackup);
        syncPartEditors();
    }

    g_cam.pos = g_studioCamBackup;
    g_cam.yaw = g_studioYawBackup;
    g_cam.pitch = g_studioPitchBackup;
    g_luaRunMsg = "Test fermato — place e player ripristinati";
    setPlayCursor(win, false);
    applyStudioTheme();
    glfwSetWindowTitle(win, kAppBrand);
}

static void enterAvatarPublisher() {
    g_screen = AppScreen::AvatarPublisher;
    g_catalog.open = true;
    g_catalog.wizardStep = 0;
    g_catalog.pendingSlot = AvatarSlot::Head;
    g_catalog.camYaw = 35.f;
    g_catalog.camPitch = -15.f;
    g_catalog.camDist = 12.f;
    g_catalog.status = "Scegli dove mettere il modello (Head / Torso / Arms / Legs)";
    g_playerPos = {0, 0, 0};
    g_playerYaw = 180.f;
    applyStudioTheme();
    if (kIsGameEngine)
        glfwSetWindowTitle(glfwGetCurrentContext(), "Blocky Game Engine — Avatar Catalog");
    else
        glfwSetWindowTitle(glfwGetCurrentContext(), "Blocky — Avatar Catalog");
    setPlayCursor(glfwGetCurrentContext(), false);
}

/** Torna all'editor senza toccare il place (e senza hub player). */
static void returnToStudioEditor(GLFWwindow* win) {
    stopStudioTest(win);
    g_catalog.open = false;
    g_screen = AppScreen::Studio;
    g_mode = CamMode::Studio;
    g_studioTesting = false;
    applyStudioTheme();
    glfwSetWindowTitle(win, kAppBrand);
    setPlayCursor(win, false);
}

/** Home Game Engine (senza hub player / login). */
static void goGameEngineHome(GLFWwindow* win) {
    stopStudioTest(win);
    g_catalog.open = false;
    g_studioTesting = false;
    g_screen = AppScreen::MainMenu;
    g_mode = CamMode::Studio;
    applyStudioTheme();
    glfwSetWindowTitle(win, "Blocky Game Engine — Home");
    setPlayCursor(win, false);
}

static void returnToMenu(GLFWwindow* win) {
    if (kIsGameEngine) {
        goGameEngineHome(win);
        return;
    }
    g_screen = AppScreen::MainMenu;
    g_lua.shutdown();
    // Mantieni login + valuta + shop (B$ da economy sealed)
    bool wasIn = g_menu.loggedIn;
    char user[64];
    std::snprintf(user, sizeof(user), "%s", g_menu.username);
    int coins = ecoBalance(user);
    auto chat = g_menu.chatLines;
    auto mod = std::move(g_menu.mod);
    auto shopItems = std::move(g_menu.shopItems);
    bool shopSeeded = g_menu.shopSeeded;
    g_menu = ClassicMenuState{};
    g_menu.loggedIn = wasIn;
    g_menu.showLogin = !wasIn;
    std::snprintf(g_menu.username, sizeof(g_menu.username), "%s", user);
    ecoBoot(g_menu.username, coins);
    ecoSyncMenu(g_menu);
    g_menu.chatLines = std::move(chat);
    g_menu.mod = std::move(mod);
    g_menu.shopItems = std::move(shopItems);
    g_menu.shopSeeded = shopSeeded;
    g_net.shutdown();
    g_remoteBodies.clear();
    g_selectedPart = -1;
    g_studioTesting = false;
    saveUserProfile(g_menu);
    refreshGamesList(); // mostra giochi appena pubblicati da Studio
    applyClassicRobloxTheme();
    setPlayCursor(win, false);
    glfwSetWindowTitle(win, "Blocky");
}

/** Avvia BlockyGameEngine.exe se presente accanto a Blocky.exe. */
static bool launchGameEngineApp() {
#ifdef _WIN32
    char exePath[MAX_PATH] = {};
    if (!GetModuleFileNameA(nullptr, exePath, MAX_PATH)) return false;
    std::string dir(exePath);
    auto slash = dir.find_last_of("\\/");
    if (slash == std::string::npos) return false;
    dir = dir.substr(0, slash + 1);
    std::string engine = dir + "BlockyGameEngine.exe";
    DWORD attr = GetFileAttributesA(engine.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY))
        return false;
    HINSTANCE h = ShellExecuteA(nullptr, "open", engine.c_str(), nullptr, dir.c_str(), SW_SHOWNORMAL);
    return (INT_PTR)h > 32;
#else
    return false;
#endif
}

/** Avvia BlockyServer.exe dedicato (max giocatori dal Game Engine). */
static bool launchDedicatedServer(int maxPlayers, const char* title) {
#ifdef _WIN32
    char exePath[MAX_PATH] = {};
    if (!GetModuleFileNameA(nullptr, exePath, MAX_PATH)) return false;
    std::string dir(exePath);
    auto slash = dir.find_last_of("\\/");
    if (slash == std::string::npos) return false;
    dir = dir.substr(0, slash + 1);
    std::string server = dir + "BlockyServer.exe";
    DWORD attr = GetFileAttributesA(server.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        g_publishMsg = "BlockyServer.exe non trovato — ricompila il target BlockyServer";
        return false;
    }
    char args[256];
    std::snprintf(args, sizeof(args), "--port 27015 --max %d --name \"%s\"",
                  std::clamp(maxPlayers, 1, 32), title && title[0] ? title : "Blocky");
    HINSTANCE h = ShellExecuteA(nullptr, "open", server.c_str(), args, dir.c_str(), SW_SHOWNORMAL);
    if ((INT_PTR)h > 32) {
        g_publishMsg = std::string("Server dedicato avviato (max ")
            + std::to_string(std::clamp(maxPlayers, 1, 32)) + ") — Join da Blocky con IP di questo PC";
        return true;
    }
    g_publishMsg = "Impossibile avviare BlockyServer.exe";
    return false;
#else
    (void)maxPlayers; (void)title;
    return false;
#endif
}

static void syncEquippedWalkToAnimator() {
    if (g_catalog.equippedWalkId.empty()) {
        g_animator.clearWalkOverride();
        return;
    }
    for (const auto& c : g_catalog.walkClips) {
        if (c.id == g_catalog.equippedWalkId && c.valid()) {
            // pistola/shoot non e' una walk → altrimenti braccio SX resta alzato
            if (Animator::looksLikeToolAnim(c)) {
                g_catalog.equippedWalkId.clear();
                animLibrary().defaultWalkId.clear();
                g_animator.clearWalkOverride();
                return;
            }
            g_animator.setWalkOverride(c);
            return;
        }
    }
    g_animator.clearWalkOverride();
}

static void loadWalkLibraryIntoCatalog() {
    animLibrary().loadIndex(); // reseed pack R6 walk/jump
    animPacks().load();        // pacchetti animazione (dopo la libreria: valida gli id)
    std::string eq;
    r6LoadLibrary(g_catalog.walkClips, eq);
    for (const auto& c : g_catalog.walkClips)
        animLibrary().byId[c.id] = c;
    // Assicura che i default pack siano in catalog + libreria
    if (const R6WalkClip* w = animLibrary().get("classic_walk")) {
        bool found = false;
        for (const auto& c : g_catalog.walkClips)
            if (c.id == "classic_walk") { found = true; break; }
        if (!found) g_catalog.walkClips.push_back(*w);
    }
    if (!eq.empty()) {
        g_catalog.equippedWalkId = eq;
        animLibrary().defaultWalkId = eq;
    } else {
        g_catalog.equippedWalkId = "classic_walk";
        animLibrary().defaultWalkId = "classic_walk";
    }
    if (animLibrary().defaultJumpId.empty())
        animLibrary().defaultJumpId = "classic_jump";
    syncEquippedWalkToAnimator();
    g_animator.setWalkAnimId(animLibrary().defaultWalkId);
    g_animator.setJumpAnimId(animLibrary().defaultJumpId);
}

static void openWalkingAnimWebGuide() {
#ifdef _WIN32
    char exePath[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    std::string dir(exePath);
    auto slash = dir.find_last_of("\\/");
    if (slash != std::string::npos) dir = dir.substr(0, slash + 1);
    std::string html = dir + "assets\\web\\walking_anim.html";
    DWORD attr = GetFileAttributesA(html.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) {
        // fallback workspace-relative
        html = "assets\\web\\walking_anim.html";
    }
    HINSTANCE h = ShellExecuteA(nullptr, "open", html.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if ((INT_PTR)h <= 32)
        g_catalog.status = "Impossibile aprire la guida web (assets/web/walking_anim.html)";
    else
        g_catalog.status = "Guida web Walking Animation aperta nel browser";
#else
    g_catalog.status = "Guida web: apri assets/web/walking_anim.html";
#endif
}

static void downloadDefaultR6PlayerModel() {
    std::string path;
    if (!saveR6PlayerObjDialog(path, publisherHwnd())) {
        // Fallback: libreria Blocky
        std::string err;
        if (!exportDefaultR6ToLibrary(path, err)) {
            g_catalog.status = "Download R6 annullato / fallito: " + err;
            return;
        }
        g_catalog.status = "Player R6 salvato in: " + path;
    } else {
        std::string err;
        if (!exportDefaultR6PlayerObj(path, err)) {
            g_catalog.status = "Export R6 fallito: " + err;
            return;
        }
        g_catalog.status = "Player R6 scaricato: " + path + " — aprilo in Blender/Maya";
    }
    g_menu.chatLines.push_back("[Avatar] Modello R6 default scaricato (OBJ beveled → Blender/Maya)");
}

static void importWalkingAnimationFromFile() {
    std::string path;
    if (!openModelFileDialog(path, publisherHwnd())) {
        g_catalog.status = "Import animazione annullato";
        return;
    }
    R6WalkClip clip;
    std::string err;
    std::vector<std::string> names;
    std::string prefer = g_catalog.walkClipPrefBuf[0]
        ? g_catalog.walkClipPrefBuf : "WalkingAnimation";
    if (!r6ImportWalkFromFile(path, clip, err, prefer, &names)) {
        g_catalog.status = err;
        return;
    }
    g_catalog.pendingClipNames = std::move(names);
    g_catalog.pendingWalk = std::move(clip);
    g_catalog.hasPendingWalk = true;
    g_catalog.previewWalk = true;
    // pistola/shoot: non spuntare "walk di default" (causa braccio alzato)
    g_catalog.pendingAsWalkDefault = !Animator::looksLikeToolAnim(g_catalog.pendingWalk)
        && g_catalog.pendingWalk.looped;
    std::snprintf(g_catalog.walkNameBuf, sizeof(g_catalog.walkNameBuf), "%s",
                  g_catalog.pendingWalk.clipName.c_str());
    g_catalog.status = "Keyframe da \"" + g_catalog.pendingWalk.clipName
        + "\" — imposta AnimationId e rilascia";
}

static bool releaseWalkingAnimation() {
    if (!g_catalog.hasPendingWalk || !g_catalog.pendingWalk.valid()) {
        g_catalog.status = "Prima importa FBX/GLB da Blender/Maya con animazioni";
        return false;
    }
    std::string name = g_catalog.walkNameBuf[0]
        ? g_catalog.walkNameBuf
        : g_catalog.pendingWalk.displayName;
    auto mod = Moderation::checkAvatarOrModelName(name);
    if (mod.verdict != Moderation::Verdict::Ok) {
        g_catalog.status = mod.reason;
        return false;
    }
    g_catalog.pendingWalk.displayName = name;
    g_catalog.pendingWalk.id = sanitizeGameId(name);
    std::string low = name;
    for (char& c : low) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    if (low.find("walk") == std::string::npos && low.find("run") == std::string::npos)
        g_catalog.pendingWalk.looped = false;

    if (!animLibrary().registerClip(name, g_catalog.pendingWalk)) {
        g_catalog.status = "Registrazione AnimationId fallita";
        return false;
    }
    std::string path = r6AnimsDir() + "/" + g_catalog.pendingWalk.id + ".walk.json";

    bool replaced = false;
    for (auto& c : g_catalog.walkClips) {
        if (c.id == g_catalog.pendingWalk.id) {
            c = g_catalog.pendingWalk;
            replaced = true;
            break;
        }
    }
    if (!replaced)
        g_catalog.walkClips.push_back(g_catalog.pendingWalk);

    if (g_catalog.pendingAsWalkDefault) {
        if (Animator::looksLikeToolAnim(g_catalog.pendingWalk)) {
            g_catalog.pendingAsWalkDefault = false;
            g_menu.chatLines.push_back(
                "[Studio] Anim tool/pistola: NON impostata come walk (usa Animator:Play)");
        } else {
            g_catalog.equippedWalkId = g_catalog.pendingWalk.id;
            animLibrary().setDefaultWalk(name);
            syncEquippedWalkToAnimator();
        }
    }
    r6SaveLibrary(g_catalog.walkClips, g_catalog.equippedWalkId);

    // Pubblica anche nello Shop (kind = walk)
    seedDefaultShop(g_menu);
    std::string seller = g_menu.username[0] ? g_menu.username : "Builder";
    bool exists = false;
    for (auto& s : g_menu.shopItems) {
        if (s.kind == 1 && (s.sourcePath == path || s.name == name)) {
            s.name = name;
            s.sourcePath = path;
            s.owned = true;
            s.fromPublisher = true;
            s.seller = seller;
            exists = true;
            break;
        }
    }
    if (!exists) {
        ClassicMenuState::ShopItem it;
        it.name = name;
        it.seller = seller;
        it.sourcePath = path;
        it.kind = 1;
        it.price = 80;
        it.owned = true;
        it.fromPublisher = true;
        g_menu.shopItems.push_back(std::move(it));
    }
    saveShopItems(g_menu);
    ecoGrant(g_menu.username, 30, "publish walk anim");
    ecoSyncMenu(g_menu);
    saveUserProfile(g_menu);

    g_catalog.hasPendingWalk = false;
    std::snprintf(g_lastPublishedAnimId, sizeof(g_lastPublishedAnimId), "%s", name.c_str());
    g_catalog.status = "Pubblicato! AnimationId = \"" + name
        + "\"  →  game.Animator:Play(\"" + name + "\")";
    g_menu.chatLines.push_back("[Studio] AnimationId pubblicato: " + name);
    g_exportMsg = "AnimationId: " + name;
    g_showPlayerAnimPublisher = true;
    return true;
}

static bool equipWalkClipById(const std::string& id) {
    for (const auto& c : g_catalog.walkClips) {
        if (c.id == id && c.valid()) {
            g_catalog.equippedWalkId = id;
            syncEquippedWalkToAnimator();
            r6SaveLibrary(g_catalog.walkClips, g_catalog.equippedWalkId);
            return true;
        }
    }
    // Prova load da path shop
    return false;
}

/** Aggiorna/crea voce inventorio personale per un accessorio (senza dover pubblicare). */
static void upsertOwnedAvatarInventory(const AvatarAccessory& a) {
    if (a.sourcePath.empty() || a.name.empty()) return;
    seedDefaultShop(g_menu);
    std::string seller = g_menu.username[0] ? g_menu.username : "Player";
    ClassicMenuState::ShopItem* found = nullptr;
    for (auto& s : g_menu.shopItems) {
        if (s.kind != 0) continue;
        if ((!a.sourcePath.empty() && s.sourcePath == a.sourcePath && s.slot == (int)a.slot)
            || (s.name == a.name && s.slot == (int)a.slot && s.fromPublisher && s.seller == seller)) {
            found = &s;
            break;
        }
    }
    if (!found) {
        g_menu.shopItems.push_back({});
        found = &g_menu.shopItems.back();
        found->price = 0; // creato da te — non in vendita finché non Pubblici
        found->fromPublisher = true;
    }
    found->name = a.name;
    found->seller = seller;
    found->sourcePath = a.sourcePath;
    found->slot = (int)a.slot;
    found->offset[0] = a.offset.x; found->offset[1] = a.offset.y; found->offset[2] = a.offset.z;
    found->rotation[0] = a.rotation.x; found->rotation[1] = a.rotation.y; found->rotation[2] = a.rotation.z;
    found->scale[0] = a.scale.x; found->scale[1] = a.scale.y; found->scale[2] = a.scale.z;
    found->tint[0] = a.tint.x; found->tint[1] = a.tint.y; found->tint[2] = a.tint.z;
    found->replacesBody = a.replacesBody;
    found->owned = true;
    found->kind = 0;
    found->equipped = true;
}

static void syncCatalogItemsToInventory() {
    for (const auto& a : g_catalog.items)
        upsertOwnedAvatarInventory(a);
    saveShopItems(g_menu);
}

/** Pubblica gli accessori correnti nello Shop e dà una ricompensa in B$. */
static int publishAccessoriesToShop() {
    if (g_catalog.items.empty()) return 0;
    seedDefaultShop(g_menu);
    std::string seller = g_menu.username[0] ? g_menu.username : "Player";
    int published = 0;
    int reward = 0;
    int blocked = 0;
    for (auto& a : g_catalog.items) {
        if (a.sourcePath.empty() || !a.mesh.loaded) continue;

        auto modName = Moderation::checkAvatarOrModelName(a.name);
        if (modName.verdict != Moderation::Verdict::Ok) {
            g_menu.chatLines.push_back(modName.reason + " (" + a.name + ")");
            blocked++;
            continue;
        }
        auto modPath = Moderation::checkAvatarOrModelName(a.sourcePath);
        if (modPath.verdict != Moderation::Verdict::Ok) {
            g_menu.chatLines.push_back(modPath.reason + " (" + a.name + ")");
            blocked++;
            continue;
        }

        // Copia in libreria permanente e aggiorna path sull'avatar
        std::string lib = copyModelToLibrary(a.sourcePath, a.name);
        if (!lib.empty()) a.sourcePath = lib;

        // Evita duplicati stesso path/nome+slot già in shop dal medesimo seller
        ClassicMenuState::ShopItem* found = nullptr;
        for (auto& s : g_menu.shopItems) {
            if (!s.fromPublisher || s.seller != seller || s.slot != (int)a.slot || s.kind != 0) continue;
            if (s.sourcePath == a.sourcePath || s.name == a.name) {
                found = &s;
                break;
            }
        }
        if (found) {
            // Aggiorna trasformazioni / prezzo (già creato → ora in vendita)
            found->sourcePath = a.sourcePath;
            found->offset[0] = a.offset.x; found->offset[1] = a.offset.y; found->offset[2] = a.offset.z;
            found->rotation[0] = a.rotation.x; found->rotation[1] = a.rotation.y; found->rotation[2] = a.rotation.z;
            found->scale[0] = a.scale.x; found->scale[1] = a.scale.y; found->scale[2] = a.scale.z;
            found->tint[0] = a.tint.x; found->tint[1] = a.tint.y; found->tint[2] = a.tint.z;
            found->replacesBody = a.replacesBody;
            found->owned = true;
            found->equipped = true;
            int base = 40;
            if (a.slot == AvatarSlot::Head) base = 75;
            else if (a.slot == AvatarSlot::Torso) base = 60;
            int newPrice = base + (int)(glm::length(a.scale) * 10.f);
            if (found->price <= 0) {
                found->price = newPrice;
                published++;
                reward += 50;
            } else {
                found->price = newPrice;
            }
            continue;
        }

        ClassicMenuState::ShopItem it;
        it.name = a.name;
        it.seller = seller;
        it.sourcePath = a.sourcePath;
        it.slot = (int)a.slot;
        it.offset[0] = a.offset.x; it.offset[1] = a.offset.y; it.offset[2] = a.offset.z;
        it.rotation[0] = a.rotation.x; it.rotation[1] = a.rotation.y; it.rotation[2] = a.rotation.z;
        it.scale[0] = a.scale.x; it.scale[1] = a.scale.y; it.scale[2] = a.scale.z;
        it.tint[0] = a.tint.x; it.tint[1] = a.tint.y; it.tint[2] = a.tint.z;
        it.replacesBody = a.replacesBody;
        int base = 40;
        if (a.slot == AvatarSlot::Head) base = 75;
        else if (a.slot == AvatarSlot::Torso) base = 60;
        it.price = base + (int)(glm::length(a.scale) * 10.f);
        it.owned = true;
        it.fromPublisher = true;
        it.equipped = true;
        g_menu.shopItems.push_back(std::move(it));
        published++;
        reward += 50;
    }
    if (blocked > 0 && published == 0) {
        g_catalog.status = "Pubblicazione bloccata dalla moderazione";
    }
    if (published > 0) {
        int give = std::min(reward, EconomyState::kMaxGrant);
        ecoGrant(g_menu.username, give, "publish avatar");
        ecoSyncMenu(g_menu);
        g_menu.chatLines.push_back(
            "[Shop] Pubblicati " + std::to_string(published) + " avatar item (+"
            + std::to_string(give) + " B$). Salvati su disco!");
        g_menu.tab = HubTab::Shop;
        saveUserProfile(g_menu);
        saveShopItems(g_menu);
        saveEquippedAvatar(g_catalog);
    } else if (blocked == 0) {
        // Path/trasformazioni aggiornati ma già tutti pubblicati
        saveShopItems(g_menu);
        saveEquippedAvatar(g_catalog);
    }
    return published;
}

static bool equipShopItem(int idx) {
    if (idx < 0 || idx >= (int)g_menu.shopItems.size()) return false;
    const auto& it = g_menu.shopItems[idx];
    if (!it.owned || it.sourcePath.empty()) return false;

    // Walking Animation
    if (it.kind == 1) {
        R6WalkClip clip;
        if (!r6LoadClipFile(it.sourcePath, clip)) {
            g_menu.chatLines.push_back("[Shop] Walk non trovata: " + it.sourcePath);
            return false;
        }
        clip.displayName = it.name;
        bool found = false;
        for (auto& c : g_catalog.walkClips) {
            if (c.id == clip.id) { c = clip; found = true; break; }
        }
        if (!found) g_catalog.walkClips.push_back(clip);
        g_catalog.equippedWalkId = clip.id;
        syncEquippedWalkToAnimator();
        r6SaveLibrary(g_catalog.walkClips, g_catalog.equippedWalkId);
        g_menu.chatLines.push_back("[Shop] Walking Animation equipaggiata: " + it.name);
        return true;
    }

    AvatarAccessory acc;
    acc.slot = (AvatarSlot)it.slot;
    acc.name = it.name;
    acc.sourcePath = it.sourcePath;
    acc.offset = {it.offset[0], it.offset[1], it.offset[2]};
    acc.rotation = {it.rotation[0], it.rotation[1], it.rotation[2]};
    acc.scale = {it.scale[0], it.scale[1], it.scale[2]};
    acc.tint = {it.tint[0], it.tint[1], it.tint[2]};
    acc.replacesBody = it.replacesBody;
    if (!acc.mesh.load(it.sourcePath)) {
        g_menu.chatLines.push_back("[Shop] Equip fallito: file non trovato");
        return false;
    }
    // Rimuovi altro accessorio sullo stesso slot (sostituisci)
    for (auto ait = g_catalog.items.begin(); ait != g_catalog.items.end();) {
        if (ait->slot == acc.slot)
            ait = g_catalog.items.erase(ait);
        else
            ++ait;
    }
    if (acc.replacesBody)
        g_catalog.hideDefaultPart[(int)acc.slot] = true;
    g_catalog.items.push_back(std::move(acc));
    g_catalog.selected = (int)g_catalog.items.size() - 1;
    g_menu.chatLines.push_back(std::string("[Shop] Equipaggiato ") + it.name);
    saveEquippedAvatar(g_catalog);
    return true;
}

static bool unequipShopItem(int idx) {
    if (idx < 0 || idx >= (int)g_menu.shopItems.size()) return false;
    const auto& it = g_menu.shopItems[idx];
    bool removed = false;
    for (auto ait = g_catalog.items.begin(); ait != g_catalog.items.end();) {
        if ((!it.sourcePath.empty() && ait->sourcePath == it.sourcePath && (int)ait->slot == it.slot)
            || (ait->name == it.name && (int)ait->slot == it.slot)) {
            ait = g_catalog.items.erase(ait);
            removed = true;
        } else {
            ++ait;
        }
    }
    if (g_catalog.selected >= (int)g_catalog.items.size())
        g_catalog.selected = (int)g_catalog.items.size() - 1;
    if (removed)
        g_menu.chatLines.push_back(std::string("[Shop] Disequipaggiato ") + it.name);
    else
        g_menu.chatLines.push_back(std::string("[Shop] ") + it.name + " non era equipaggiato");
    saveEquippedAvatar(g_catalog);
    return removed;
}

static bool unequipEquippedIndex(int idx) {
    if (idx == -2) {
        int n = (int)g_catalog.items.size();
        g_catalog.items.clear();
        g_catalog.selected = -1;
        if (n > 0)
            g_menu.chatLines.push_back("[Avatar] Rimossi tutti gli accessori (" + std::to_string(n) + ")");
        saveEquippedAvatar(g_catalog);
        return n > 0;
    }
    if (idx < 0 || idx >= (int)g_catalog.items.size()) return false;
    std::string name = g_catalog.items[idx].name;
    g_catalog.items.erase(g_catalog.items.begin() + idx);
    if (g_catalog.selected == idx) g_catalog.selected = -1;
    else if (g_catalog.selected > idx) g_catalog.selected--;
    g_menu.chatLines.push_back("[Avatar] Disequipaggiato " + name);
    saveEquippedAvatar(g_catalog);
    return true;
}

static void syncEquippedToMenu() {
    g_menu.equippedList.clear();
    for (const auto& a : g_catalog.items) {
        ClassicMenuState::EquippedInfo e;
        e.name = a.name;
        e.slot = (int)a.slot;
        e.sourcePath = a.sourcePath;
        g_menu.equippedList.push_back(std::move(e));
    }
    for (auto& it : g_menu.shopItems) {
        it.equipped = false;
        if (it.sourcePath.empty() && it.kind == 0) continue;
        for (const auto& a : g_catalog.items) {
            bool pathMatch = !it.sourcePath.empty() && a.sourcePath == it.sourcePath
                && (it.kind == 1 || (int)a.slot == it.slot);
            bool nameMatch = it.kind == 0 && a.name == it.name && (int)a.slot == it.slot;
            if (pathMatch || nameMatch) {
                it.equipped = true;
                break;
            }
        }
    }
}

static void doExport() {
    syncFriendsToWorld();
    std::string json = PlaceExporter::exportPlace(g_world, g_player.humanoid, g_playerPos, g_playerYaw, &g_lighting);
    std::string path = PlaceExporter::makeExportPath(g_world.placeName);
    if (PlaceExporter::saveToFile(path, json))
        g_exportMsg = "Esportato: " + path;
    else
        g_exportMsg = "Export fallito!";
}

static void refreshGamesList() {
    g_games = loadGameManifest();
    auto packaged = BlockyProject::listInstalledGames();
    std::vector<GameEntry> extras;
    for (const auto& g : packaged)
        extras.push_back(BlockyProject::toGameEntry(g));
    mergeGamePackages(g_games, extras);
}

static bool writeCurrentPlaceTo(const std::string& path) {
    if (path.empty()) return false;
    syncFriendsToWorld();
    std::string json = PlaceExporter::exportPlace(g_world, g_player.humanoid, g_playerPos, g_playerYaw, &g_lighting);
    return PlaceExporter::saveToFile(path, json);
}

static std::string placeAutosavePath() {
    return blockyDataDir() + "/autosave_place.json";
}

/** Salva profilo / avatar / shop / place — anche su chiusura improvvisa. */
static bool g_persistBusy = false;
static bool g_didPersistExit = false;
static float g_autosaveAcc = 0.f;

/** Solo place autosave (leggero) — niente profilo/fflush (evita hitch ogni pochi secondi). */
static void persistAutosavePlaceOnly() {
    if (g_persistBusy || !kIsGameEngine) return;
    if (!(g_screen == AppScreen::Studio || g_screen == AppScreen::AvatarPublisher
          || !g_world.parts.empty() || g_world.hasBaseplate))
        return;
    g_persistBusy = true;
    writeCurrentPlaceTo(placeAutosavePath());
    g_persistBusy = false;
}

/** Chiusura veloce: 1 write place + profili piccoli. Niente doppio JSON / fflush / anim dump. */
static void persistFastExit() {
    if (g_didPersistExit || g_persistBusy) return;
    g_didPersistExit = true;
    g_persistBusy = true;
    if (g_menu.username[0])
        saveUserProfile(g_menu);
    saveShopItems(g_menu);
    saveEquippedAvatar(g_catalog);
    if (kIsGameEngine) {
        // UNA sola scrittura place (doppio path faceva aspettare secondi)
        if (!g_placeFilePath.empty())
            writeCurrentPlaceTo(g_placeFilePath);
        else if (g_screen == AppScreen::Studio || !g_world.parts.empty() || g_world.hasBaseplate)
            writeCurrentPlaceTo(placeAutosavePath());
    }
    g_persistBusy = false;
    std::fprintf(stderr, "[Blocky] Salvato (fast-exit)\n");
}

static void persistAllUserData(const char* reason) {
    if (g_persistBusy) return;
    bool isExit = reason && (std::strcmp(reason, "shutdown") == 0
                          || std::strcmp(reason, "exit") == 0
                          || std::strcmp(reason, "window-close") == 0
                          || std::strcmp(reason, "console-ctrl") == 0
                          || std::strcmp(reason, "signal") == 0);
    if (isExit) {
        persistFastExit();
        return;
    }
    g_persistBusy = true;

    for (const auto& a : g_catalog.items)
        upsertOwnedAvatarInventory(a);
    saveEquippedAvatar(g_catalog);
    saveShopItems(g_menu);
    if (g_menu.username[0])
        saveUserProfile(g_menu);

    r6SaveLibrary(g_catalog.walkClips, g_catalog.equippedWalkId);
    animLibrary().persistIndex();

    if (kIsGameEngine) {
        if (!g_placeFilePath.empty())
            writeCurrentPlaceTo(g_placeFilePath);
        if (g_screen == AppScreen::Studio || g_screen == AppScreen::AvatarPublisher
            || !g_world.parts.empty() || g_world.hasBaseplate) {
            writeCurrentPlaceTo(placeAutosavePath());
        }
    }
    g_persistBusy = false;
    if (reason && reason[0])
        std::fprintf(stderr, "[Blocky] Salvato (%s)\n", reason);
}

static void persistAtExit() {
    // no-op: già fatto in persistFastExit allo shutdown (atexit ridondante = ritardo)
}

static void onWindowCloseRequest(GLFWwindow* win) {
    // Se place dirty → non chiudere subito, chiedi salva
    if (kIsGameEngine && g_placeDirty) {
        glfwSetWindowShouldClose(win, 0);
        g_pendingNav = PendingNav::QuitApp;
        g_showSavePrompt = true;
    }
}

static void onFatalSignal(int) {
    persistAllUserData("signal");
    std::_Exit(128);
}

#ifdef _WIN32
static BOOL WINAPI onConsoleCtrl(DWORD type) {
    switch (type) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
        persistAllUserData("console-ctrl");
        if (g_mainWin) glfwSetWindowShouldClose(g_mainWin, 1);
        return TRUE;
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        persistAllUserData("console-ctrl");
        return TRUE;
    default:
        return FALSE;
    }
}
#endif

/** Salva place sul percorso corrente, altrimenti Salva come… */
static std::string suggestPlaceFileName() {
    std::string suggest = g_gameNameBuf[0] ? g_gameNameBuf : g_world.placeName;
    if (suggest.empty()) suggest = "place";
    for (char& c : suggest) if (c == ' ') c = '_';
    // togli estensione precedente se presente
    auto dot = suggest.find_last_of('.');
    if (dot != std::string::npos) {
        std::string ext = suggest.substr(dot);
        for (char& c : ext) c = (char)std::tolower((unsigned char)c);
        if (ext == ".json" || ext == ".blocky")
            suggest = suggest.substr(0, dot);
    }
    return suggest + ".blocky";
}

static void doSavePlace() {
    if (g_placeFilePath.empty()) {
        std::string path;
        if (!savePlaceFileDialog(path, publisherHwnd(), suggestPlaceFileName().c_str())) {
            g_exportMsg = "Salvataggio annullato";
            return;
        }
        ensureBlockyPlaceExt(path);
        g_placeFilePath = path;
    }
    if (writeCurrentPlaceTo(g_placeFilePath)) {
        g_exportMsg = "Salvato: " + g_placeFilePath;
        clearPlaceDirty();
    } else
        g_exportMsg = "Salvataggio fallito";
}

static void doSavePlaceAs() {
    std::string path;
    if (!savePlaceFileDialog(path, publisherHwnd(), suggestPlaceFileName().c_str())) {
        g_exportMsg = "Salvataggio annullato";
        return;
    }
    ensureBlockyPlaceExt(path);
    g_placeFilePath = path;
    if (writeCurrentPlaceTo(g_placeFilePath)) {
        g_exportMsg = "Salvato come: " + g_placeFilePath;
        clearPlaceDirty();
    } else
        g_exportMsg = "Salvataggio fallito";
}

static std::string publishDraftDirForUser() {
    std::string creator = g_menu.username[0] ? g_menu.username : "Builder";
    std::string dir = blockyDataDir() + "/publish_draft/" + normalizeCreatorKey(creator);
#ifdef _WIN32
    _mkdir((blockyDataDir() + "/publish_draft").c_str());
    _mkdir(dir.c_str());
#else
    _mkdir((blockyDataDir() + "/publish_draft").c_str());
    _mkdir(dir.c_str());
#endif
    return dir;
}

static std::string currentPublishDraftPath() {
    std::string title = g_gameNameBuf[0] ? g_gameNameBuf : g_world.placeName;
    if (title.empty()) title = "Il_mio_gioco";
    std::string id = sanitizeGameId(title);
    return publishDraftDirForUser() + "/" + id + ".json";
}

/** Salva bozza pronta per Pubblica (non va live). Autosave continua ogni 30s. */
static void doSaveForPublish() {
    std::string path = currentPublishDraftPath();
    if (!writeCurrentPlaceTo(path)) {
        g_exportMsg = "Salva per pubblicare fallito";
        g_publishMsg = g_exportMsg;
        return;
    }
    g_publishDraftPath = path;
    g_publishDraftActive = true;
    g_publishDraftAcc = 0.f;
    // Meta bozza (max giocatori + nome)
    {
        std::string meta = publishDraftDirForUser() + "/draft_meta.txt";
        FILE* f = std::fopen(meta.c_str(), "wb");
        if (f) {
            std::fprintf(f, "%s\n%d\n%s\n",
                g_gameNameBuf[0] ? g_gameNameBuf : g_world.placeName.c_str(),
                g_maxPlayers, path.c_str());
            std::fclose(f);
        }
    }
    int used = 0;
    long long now = (long long)std::time(nullptr);
    std::string creator = g_menu.username[0] ? g_menu.username : "Builder";
    std::string id = sanitizeGameId(g_gameNameBuf[0] ? g_gameNameBuf : g_world.placeName);
    canPublishGame(g_games, id, creator, now, &used);
    clearPlaceDirty();
    g_exportMsg = "Salvato per pubblicare (bozza) — autosave attivo";
    g_publishMsg = "Bozza OK · slot " + std::to_string(used) + "/"
        + std::to_string(kMaxPublishedGamesPerUser)
        + " in 2 mesi · Pubblica quando vuoi";
}

static void persistPublishDraftOnly() {
    if (!g_publishDraftActive || g_persistBusy) return;
    if (!(g_screen == AppScreen::Studio || g_screen == AppScreen::AvatarPublisher))
        return;
    if (g_publishDraftPath.empty())
        g_publishDraftPath = currentPublishDraftPath();
    g_persistBusy = true;
    writeCurrentPlaceTo(g_publishDraftPath);
    g_persistBusy = false;
}

static bool autosaveFileExists() {
    std::string p = placeAutosavePath();
    FILE* f = std::fopen(p.c_str(), "rb");
    if (!f) return false;
    std::fclose(f);
    return true;
}

static void loadAutosaveIntoEditor() {
    std::string p = placeAutosavePath();
    if (!loadPlaceFromJson(p, g_world, &g_lighting)) {
        g_exportMsg = "Autosave non leggibile";
        return;
    }
    g_placeFilePath.clear(); // non e' un salvataggio utente
    g_wsMeshes.clear();
    syncFriendsFromWorld();
    selectPart(g_world.parts.empty() ? -1 : 0);
    markPlaceDirty(); // va ancora salvato con Salva Place
    g_exportMsg = "Ripristinato place non salvato (autosave)";
}

static void executePendingNav(GLFWwindow* win) {
    PendingNav a = g_pendingNav;
    g_pendingNav = PendingNav::None;
    g_showSavePrompt = false;
    switch (a) {
    case PendingNav::GoHome:
        goGameEngineHome(win);
        break;
    case PendingNav::QuitApp:
        glfwSetWindowShouldClose(win, true);
        break;
    case PendingNav::NewPlace:
        applyMapTemplate(0);
        clearPlaceDirty();
        g_placeFilePath.clear();
        returnToStudioEditor(win);
        g_exportMsg = "Nuovo place Baseplate";
        break;
    case PendingNav::OpenPlace:
        doOpenPlace();
        returnToStudioEditor(win);
        break;
    default:
        break;
    }
}

/** Se dirty → chiede salva; altrimenti esegue subito. */
static void requestNav(GLFWwindow* win, PendingNav a) {
    if (!kIsGameEngine) {
        g_pendingNav = a;
        executePendingNav(win);
        return;
    }
    if (g_placeDirty) {
        g_pendingNav = a;
        g_showSavePrompt = true;
        return;
    }
    g_pendingNav = a;
    executePendingNav(win);
}

static void drawSavePlacePrompt(GLFWwindow* win) {
    if (!g_showSavePrompt) return;
    ImGui::OpenPopup("Salvare il place?##saveprompt");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, {0.5f, 0.5f});
    if (ImGui::BeginPopupModal("Salvare il place?##saveprompt", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Il place ha modifiche non salvate.\nVuoi salvarlo prima di continuare?");
        ImGui::Spacing();
        if (ImGui::Button("Salva", {120, 36})) {
            doSavePlace();
            if (!g_placeDirty) // salvato ok (o path scelto)
                executePendingNav(win);
            else
                g_showSavePrompt = false; // annullato dialogo file
        }
        ImGui::SameLine();
        if (ImGui::Button("Non salvare", {120, 36}))
            executePendingNav(win);
        ImGui::SameLine();
        if (ImGui::Button("Annulla", {120, 36})) {
            g_showSavePrompt = false;
            g_pendingNav = PendingNav::None;
        }
        ImGui::EndPopup();
    }
}

static void drawAutosavePrompt(GLFWwindow* win) {
    if (!g_showAutosavePrompt) return;
    ImGui::OpenPopup("Place non salvato##autosprompt");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, {0.5f, 0.5f});
    if (ImGui::BeginPopupModal("Place non salvato##autosprompt", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped(
            "C'e' un place non salvato dall'ultima sessione.\n"
            "Vuoi aprirlo nell'editor?");
        ImGui::Spacing();
        if (ImGui::Button("Si, apri", {140, 36})) {
            loadAutosaveIntoEditor();
            g_showAutosavePrompt = false;
            returnToStudioEditor(win);
        }
        ImGui::SameLine();
        if (ImGui::Button("No, Home", {140, 36}))
            g_showAutosavePrompt = false;
        ImGui::EndPopup();
    }
}

/** Schermata Home di Blocky Game Engine. */
static void drawGameEngineHome(GLFWwindow* win, float /*dt*/) {
    const float W = (float)g_w, H = (float)g_h;
    const float cardW = std::min(720.f, W * 0.72f);
    const float cardH = std::min(640.f, H * 0.78f);
    const float cardX = (W - cardW) * 0.5f;
    const float cardY = (H - cardH) * 0.5f;

    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize({W, H});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.05f, 0.08f, 0.14f, 1.f});
    ImGui::Begin("##GEHome", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus);

    // sfondo pieno + accento
    ImDrawList* bg = ImGui::GetWindowDrawList();
    bg->AddRectFilledMultiColor({0, 0}, {W, H},
        IM_COL32(12, 22, 42, 255), IM_COL32(18, 36, 64, 255),
        IM_COL32(10, 18, 34, 255), IM_COL32(8, 14, 28, 255));

    ImGui::SetCursorPos({cardX, cardY});
    ImGui::PushStyleColor(ImGuiCol_ChildBg, {0.09f, 0.13f, 0.22f, 1.f});
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 18.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {36.f, 32.f});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {14.f, 14.f});
    ImGui::BeginChild("##gehomebox", {cardW, cardH}, true,
        ImGuiWindowFlags_NoScrollbar);

    ImGui::SetWindowFontScale(1.55f);
    ImGui::TextColored({0.40f, 0.88f, 1.f, 1.f}, "BLOCKY");
    ImGui::SetWindowFontScale(1.25f);
    ImGui::TextColored({1.f, 0.82f, 0.30f, 1.f}, "Game Engine");
    ImGui::SetWindowFontScale(1.f);
    ImGui::Spacing();
    ImGui::TextDisabled("Home — crea e modifica place");
    if (g_placeDirty)
        ImGui::TextColored({1.f, 0.55f, 0.25f, 1.f}, "Place non salvato");
    else if (!g_placeFilePath.empty())
        ImGui::TextWrapped("%s", g_placeFilePath.c_str());
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.f);
    ImGui::PushStyleColor(ImGuiCol_Button, {0.14f, 0.52f, 0.92f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.22f, 0.62f, 1.f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_Text, {1.f, 1.f, 1.f, 1.f});
    if (ImGui::Button("Continua editor", {-1, 64}))
        returnToStudioEditor(win);
    ImGui::PopStyleColor(3);

    ImGui::PushStyleColor(ImGuiCol_Button, {0.16f, 0.28f, 0.46f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.22f, 0.38f, 0.62f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_Text, {1.f, 1.f, 1.f, 1.f});
    if (ImGui::Button("Nuovo place", {-1, 56}))
        requestNav(win, PendingNav::NewPlace);
    if (ImGui::Button("Apri place...", {-1, 56}))
        requestNav(win, PendingNav::OpenPlace);
    ImGui::PopStyleColor(3);

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button, {0.42f, 0.18f, 0.18f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.58f, 0.24f, 0.24f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_Text, {1.f, 1.f, 1.f, 1.f});
    if (ImGui::Button("Esci", {-1, 52}))
        requestNav(win, PendingNav::QuitApp);
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();

    ImGui::EndChild();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor();
    ImGui::End();
    ImGui::PopStyleColor();

    drawAutosavePrompt(win);
    drawSavePlacePrompt(win);
}

/** Apri place JSON in Studio. */
/** Carica un place da un path noto. Da chiamare a motore avviato (nel loop). */
static bool openPlacePath(const std::string& path) {
    if (!loadPlaceFromJson(path, g_world, &g_lighting)) {
        g_exportMsg = "Impossibile aprire: " + path;
        return false;
    }
    g_placeFilePath = path;
    g_wsMeshes.clear();
    syncFriendsFromWorld();
    g_selectedPart = -1;
    g_selectedModel = -1;
    g_selectedSound = -1;
    g_selectedLabel = -1;
    g_scaleDragging = g_moveDragging = g_rotateDragging = false;
    clearPlaceDirty();
    std::snprintf(g_gameNameBuf, sizeof(g_gameNameBuf), "%s", g_world.placeName.c_str());
    if (!g_world.parts.empty())
        selectPart(0);
    g_exportMsg = "Aperto: " + path + " (" + std::to_string(g_world.parts.size()) + " parti"
        + (g_friends.empty() ? "" : (", " + std::to_string(g_friends.size()) + " amici")) + ")";
    glfwSetWindowTitle(glfwGetCurrentContext(),
        (std::string(kAppBrand) + " - " + g_world.placeName).c_str());
    return true;
}

static void doOpenPlace() {
    std::string path;
    if (!openPlaceFileDialog(path, publisherHwnd())) {
        g_exportMsg = "Apertura annullata";
        return;
    }
    openPlacePath(path);
}

/** Salva come progetto in %APPDATA%/Blocky/projects/<id>/ */
static void doSaveProject() {
    std::string title = g_gameNameBuf[0] ? g_gameNameBuf : g_world.placeName;
    if (title.empty()) title = "Il mio gioco";
    g_world.placeName = title;
    std::string id = sanitizeGameId(title);
    std::string root = BlockyProject::joinPath(BlockyProject::projectsDir(), id);
    syncFriendsToWorld();
    std::string json = PlaceExporter::exportPlace(g_world, g_player.humanoid, g_playerPos, g_playerYaw, &g_lighting);

    BlockyProject::ProjectManifest m;
    m.name = title;
    m.id = id;
    m.creator = g_menu.username[0] ? g_menu.username : "Builder";
    m.maxPlayers = std::clamp(g_maxPlayers, 1, 32);

    std::string placePath = BlockyProject::joinPath(root, "places\\main.place.json");
    std::string manifestPath = BlockyProject::joinPath(root, "project.blocky.json");
    if (!fileExistsPath(manifestPath)) {
        if (!BlockyProject::createNewProject(root, m, json)) {
            g_exportMsg = "Salvataggio progetto fallito";
            return;
        }
    } else {
        BlockyProject::mkDir(root);
        BlockyProject::mkDir(BlockyProject::joinPath(root, "places"));
        if (!BlockyProject::writeTextFile(placePath, json)) {
            g_exportMsg = "Salvataggio place progetto fallito";
            return;
        }
        BlockyProject::writeTextFile(manifestPath, BlockyProject::writeProjectJson(m));
    }
    g_placeFilePath = placePath;
    g_exportMsg = "Progetto salvato: " + root;
}

/** Pubblica il place corrente nei Games dell'hub (con nome scelto). */
static bool publishCurrentGame() {
    std::string title = g_gameNameBuf[0] ? g_gameNameBuf : g_world.placeName;
    if (title.empty()) title = "Il mio gioco";

    auto mod = Moderation::checkGameTitle(title);
    if (mod.verdict != Moderation::Verdict::Ok) {
        g_publishMsg = mod.reason;
        g_exportMsg = g_publishMsg;
        g_menu.chatLines.push_back(mod.reason);
        return false;
    }

    g_world.placeName = title;

    std::string id = sanitizeGameId(title);
    std::string creator = g_menu.username[0] ? g_menu.username : "Builder";
    long long now = (long long)std::time(nullptr);
    int used = 0;
    if (!canPublishGame(g_games, id, creator, now, &used)) {
        g_publishMsg = "Limite raggiunto: max "
            + std::to_string(kMaxPublishedGamesPerUser)
            + " giochi ogni 2 mesi (" + std::to_string(used) + "/"
            + std::to_string(kMaxPublishedGamesPerUser)
            + "). Aggiorna un gioco esistente o aspetta.";
        g_exportMsg = g_publishMsg;
        g_menu.chatLines.push_back("[Games] " + g_publishMsg);
        return false;
    }

    // Se c'è bozza "Salva per pubblicare", usala come source (auto-aggiornata)
    if (g_publishDraftActive) {
        if (g_publishDraftPath.empty())
            g_publishDraftPath = currentPublishDraftPath();
        writeCurrentPlaceTo(g_publishDraftPath);
    }

    std::string placePath = blockyDataDir() + "/published/" + id + ".json";

    // Escape backslash not needed — usiamo slash
    for (char& c : placePath)
        if (c == '\\') c = '/';

    syncFriendsToWorld();
    std::string json = PlaceExporter::exportPlace(g_world, g_player.humanoid, g_playerPos, g_playerYaw, &g_lighting);
    if (!PlaceExporter::saveToFile(placePath, json)) {
        g_publishMsg = "Pubblicazione fallita (salvataggio file)";
        g_exportMsg = g_publishMsg;
        return false;
    }

    GameEntry e;
    e.id = id;
    e.title = title;
    e.subtitle = "di " + creator;
    e.script = "scripts/baseplate.lua";
    e.playersOnline = 1;
    e.maxPlayers = std::clamp(g_maxPlayers, 1, 32);
    e.accent = {0.15f, 0.48f, 0.92f};
    e.isPublished = true;
    e.placePath = placePath;
    e.creator = creator;
    e.publishedAt = now;

    bool updated = false;
    for (auto& g : g_games) {
        if (g.id == id) {
            // aggiornamento: mantieni publishedAt originale se già in finestra
            if (g.publishedAt > 0) e.publishedAt = g.publishedAt;
            g = e;
            updated = true;
            break;
        }
    }
    if (!updated) g_games.push_back(e);
    savePublishedGamesList(g_games);

    // Package professionale (solo runtime Blocky)
    BlockyProject::GamePackage pkg;
    pkg.id = id;
    pkg.title = title;
    pkg.creator = creator;
    pkg.description = "Pubblicato con Blocky Game Engine";
    pkg.maxPlayers = e.maxPlayers;
    std::string lua =
        "-- " + title + " (pubblicato da Blocky Game Engine)\n"
        "print(\"[Blocky] Gioco: " + title + "\")\n"
        "print(\"[Blocky] Safe Bytecode — logica sigillata, OpenGL solo rendering\")\n";
    std::string pkgDir = BlockyProject::publishGamePackage(pkg, json, lua);
    if (!pkgDir.empty()) {
        // Preferisci package: place + Safe Bytecode (.bbc)
        e.placePath = BlockyProject::joinPath(pkgDir, "place.json");
        std::string bbc = BlockyProject::joinPath(pkgDir, "scripts\\main.bbc");
        std::string luaPath = BlockyProject::joinPath(pkgDir, "scripts\\main.lua");
        e.script = fileExistsPath(bbc) ? bbc : luaPath;
        for (auto& g : g_games) {
            if (g.id == id) { g = e; break; }
        }
        savePublishedGamesList(g_games);
        refreshGamesList();
        if (fileExistsPath(bbc))
            g_publishMsg += " · Safe Bytecode (.bbc) sigillato";
    }

    int usedAfter = countUserPublishedInWindow(g_games, creator, now);
    g_publishMsg = "Pubblicato \"" + title + "\" — max " + std::to_string(e.maxPlayers)
        + " giocatori · slot " + std::to_string(usedAfter) + "/"
        + std::to_string(kMaxPublishedGamesPerUser) + " (2 mesi)";
    g_exportMsg = g_publishMsg;
    g_menu.chatLines.push_back("[Games] Pubblicato: " + title
        + " (max " + std::to_string(e.maxPlayers) + " player) · "
        + std::to_string(usedAfter) + "/" + std::to_string(kMaxPublishedGamesPerUser));
    g_menu.tab = HubTab::Games;
    ecoGrant(g_menu.username, 25, "publish game");
    ecoSyncMenu(g_menu);
    saveUserProfile(g_menu);
    CrashHandler::note(("Pubblicato gioco: " + title).c_str());
    return true;
}

/** Proietta un punto world in coordinate schermo ImGui. Restituisce false se dietro la camera. */
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
    // Il font e' NULL finche' non e' partito il primo ImGui::NewFrame. Se una
    // label viene misurata prima (es. label-picking al primo frame dopo aver
    // aperto un .blocky), CalcTextSizeA fa deref di NULL -> crash. Guardia:
    if (!font || ImGui::GetFontSize() <= 0.f) {
        tw = kLabelMinW;
        th = kLabelMinH;
        return;
    }
    float fs = ImGui::GetFontSize() * std::clamp(l.fontScale, 0.5f, 3.f);
    ImVec2 ts = font->CalcTextSizeA(fs, FLT_MAX, 0.f, l.text.c_str());
    // Padding stretto intorno al testo (background “piccolo”)
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
    case 0: x = mn.x; y = mn.y; break; // TL
    case 1: x = mx.x; y = mn.y; break; // TR
    case 2: x = mx.x; y = mx.y; break; // BR
    default: x = mn.x; y = mx.y; break; // BL
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
        // angolo opposto resta fisso
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

/** Outline nero 8-dir — stile TextLabel Roblox classico. */
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
    // Hover / press solo per TextButton in Play/Test (o anteprima se selezionato)
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
            // bordo classico bottone
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

    // Vertici trascinabili (solo selezione editor)
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

/** Hit-test UI Label/TextLabel/TextButton sullo schermo (sopra il pick 3D). */
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
    if (g_studioTesting) return; // in Test niente etichetta editor
    const auto& p = g_world.parts[g_selectedPart];
    glm::vec3 top = p.position;
    glm::vec3 upLocal(0, p.size.y * 0.5f + 0.55f, 0);
    top += glm::vec3(partRotationMatrix(p.rotation) * glm::vec4(upLocal, 0.f));
    // Solo il nome (niente box / bordo blu)
    drawBillboardText(top, p.name.c_str(), {1.f, 1.f, 1.f}, 0.95f, false, false);
}

static void drawPlayerRigWindows() {
    if (g_showBackpackOffer) {
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, {0.5f, 0.5f});
        ImGui::SetNextWindowSize({460, 240}, ImGuiCond_Always);
        ImGui::SetNextWindowFocus();
        bool open = true;
        if (ImGui::Begin("Mettere nel Backpack?##bpoffer", &open, ImGuiWindowFlags_NoCollapse)) {
            ImGui::TextColored({1.f, 0.75f, 0.3f, 1.f}, "Import da Blender · arma rilevata");
            ImGui::Spacing();
            ImGui::TextWrapped(
                "Il file \"%s\" sembra una pistola/arma.\n"
                "Vuoi aggiungerla al Backpack come item (hotbar 1-9 in Test)?",
                g_backpackOfferName.c_str());
            ImGui::Spacing();
            ImGui::TextDisabled("Si = item Pistol nel Backpack  ·  No = solo modello nel mondo");
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Button, {0.15f, 0.65f, 0.40f, 1.f});
            if (ImGui::Button("Si, mettila nel Backpack", {-1, 42}))
                confirmBackpackOffer(true);
            ImGui::PopStyleColor();
            if (ImGui::Button("No, solo modello", {-1, 0}))
                confirmBackpackOffer(false);
        }
        ImGui::End();
        if (!open) confirmBackpackOffer(false);
    }

    if (g_showPlayerRigPrompt) {
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, {0.5f, 0.5f});
        ImGui::SetNextWindowSize({440, 220}, ImGuiCond_Always);
        ImGui::SetNextWindowFocus();
        bool open = true;
        if (ImGui::Begin("Rig Player##playerrigask", &open, ImGuiWindowFlags_NoCollapse)) {
            ImGui::TextColored({1.f, 0.7f, 0.3f, 1.f}, "Workspace → Players → LocalPlayer");
            ImGui::Spacing();
            ImGui::TextWrapped("Il Player non e' ancora riggato.\nVuoi creare il Rig R6 (Head, Torso, Arm, Leg)?");
            ImGui::Spacing();
            ImGui::TextDisabled("Potrai spostare, ruotare e ridimensionare ogni parte.");
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Button, {0.15f, 0.65f, 0.40f, 1.f});
            if (ImGui::Button("Si, rigga il Player", {-1, 42}))
                confirmPlayerRig(true);
            ImGui::PopStyleColor();
            if (ImGui::Button("No, annulla", {-1, 0}))
                confirmPlayerRig(false);
        }
        ImGui::End();
        if (!open) confirmPlayerRig(false);
    }

    if (!g_showPlayerRigEditor) return;

    if (g_playerRigEditorNeedsPlace) {
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos({center.x, center.y + 40.f}, ImGuiCond_Always, {0.5f, 0.5f});
        ImGui::SetNextWindowSize({520, 380}, ImGuiCond_Always);
        g_playerRigEditorNeedsPlace = false;
    }
    if (ImGui::Begin("Rigging Player R6##playerrigedit", &g_showPlayerRigEditor)) {
        auto& h = g_player.humanoid;
        ImGui::TextColored({0.45f, 0.85f, 1.f, 1.f}, "LocalPlayer · Humanoid");
        ImGui::SameLine();
        ImGui::TextDisabled(h.userRigged ? "| Rig attivo" : "| non riggato");
        ImGui::Checkbox("RigEnabled", &h.rigEnabled);
        ImGui::SliderFloat("WalkSpeed", &h.walkSpeed, 4.f, 40.f);
        ImGui::SliderFloat("JumpPower", &h.jumpPower, 20.f, 90.f);
        ImGui::SliderFloat("Health", &h.health, 0.f, h.maxHealth);

        ImGui::Separator();
        ImGui::Text("Parti del Rig (seleziona e modifica)");
        ImGui::BeginChild("##rigparts", {160, 220}, true);
        if (ImGui::Selectable("LocalPlayer##root", g_selectedPlayer && g_selectedPlayerPart < 0))
            selectPlayer(-1);
        for (int i = 0; i < (int)h.parts.size(); ++i) {
            bool sel = g_selectedPlayer && g_selectedPlayerPart == i;
            if (ImGui::Selectable(h.parts[i].name.c_str(), sel))
                selectPlayer(i);
        }
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("##rigprops", {0, 220}, true);
        if (g_selectedPlayerPart >= 0 && g_selectedPlayerPart < (int)h.parts.size()) {
            auto& p = h.parts[g_selectedPlayerPart];
            ImGui::TextColored({0.95f, 0.75f, 0.3f, 1.f}, "%s", p.name.c_str());
            ImGui::DragFloat3("Offset (spostamento)", &p.offset.x, 0.05f);
            ImGui::DragFloat3("Size (ingrandimento)", &p.size.x, 0.05f, 0.2f, 8.f);
            ImGui::DragFloat3("Pivot", &p.pivot.x, 0.05f);
            ImGui::DragFloat("Rot X °", &p.rotX, 1.f, -180.f, 180.f);
            ImGui::DragFloat("Rot Z °", &p.rotZ, 1.f, -180.f, 180.f);
            ImGui::ColorEdit3("Colore", &p.color.x);
            ImGui::Checkbox("IsLimb", &p.isLimb);
        } else {
            ImGui::TextWrapped("Seleziona una parte a sinistra (es. Left Arm) per fare il rigging: spostamento, rotazione, scala.");
            ImGui::Spacing();
            if (ImGui::Button("Reset Rig R6 default", {-1, 32})) {
                bool was = h.userRigged;
                float ws = h.walkSpeed, jp = h.jumpPower, hp = h.health;
                h = Humanoid::createR6Blocky();
                h.userRigged = was;
                h.walkSpeed = ws;
                h.jumpPower = jp;
                h.health = hp;
                g_exportMsg = "Rig Player ripristinato (R6 default)";
            }
        }
        ImGui::EndChild();

        ImGui::Separator();
        if (ImGui::Button("Play Wave (prova)")) g_animator.play("Wave");
        ImGui::SameLine();
        if (ImGui::Button("Play Dance")) g_animator.play("Dance");
        ImGui::SameLine();
        if (ImGui::Button("Idle")) g_animator.stop();
        ImGui::TextDisabled("In Studio vedi il Player aggiornato subito nel viewport.");
    }
    ImGui::End();
    if (!g_showPlayerRigEditor) {
        /* resta userRigged */
    }
}

static void drawModelAnimWindows() {
    if (g_showRigPrompt) {
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, {0.5f, 0.5f});
        ImGui::SetNextWindowSize({420, 210}, ImGuiCond_Always);
        ImGui::SetNextWindowFocus();
        bool open = true;
        if (ImGui::Begin("Rig richiesto##rigask", &open, ImGuiWindowFlags_NoCollapse)) {
            const char* mname = "?";
            if (g_animEditModel >= 0 && g_animEditModel < (int)g_world.models.size())
                mname = g_world.models[g_animEditModel].name.c_str();
            ImGui::TextColored({1.f, 0.7f, 0.3f, 1.f}, "Crea Rig Root");
            ImGui::Spacing();
            ImGui::TextWrapped(
                "\"%s\" non ha ancora un Rig per l'editor.\n"
                "Crea un Rig Root: poi sposti, ruoti e scali con i keyframe.",
                mname);
            ImGui::Spacing();
            ImGui::TextDisabled("Workflow: Move → Set Frame (K) → Play");
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Button, {0.15f, 0.65f, 0.40f, 1.f});
            if (ImGui::Button("Crea Rig e anima", {-1, 40}))
                confirmModelRig(true);
            ImGui::PopStyleColor();
            if (ImGui::Button("Apri comunque", {-1, 0}))
                confirmModelRig(false);
        }
        ImGui::End();
        if (!open) confirmModelRig(false);
    }

    if (!g_showAnimEditor || g_animEditModel < 0 || g_animEditModel >= (int)g_world.models.size())
        return;

    auto& m = g_world.models[g_animEditModel];
    if (m.anim.fps < 1) m.anim.fps = 30;
    if (g_animEditorNeedsPlace) {
        // Timeline in basso — non copre il viewport al centro
        ImGui::SetNextWindowPos({(float)g_w * 0.5f, (float)g_h - 8.f}, ImGuiCond_Always, {0.5f, 1.f});
        ImGui::SetNextWindowSize({std::min(720.f, (float)g_w - 24.f), 280.f}, ImGuiCond_Always);
        g_animEditorNeedsPlace = false;
    }

    // Sync live: se muovi il modello nel mondo, aggiorna la key selezionata
    if (g_animLiveCapture && !g_animPlaying && g_animSelKey >= 0 && g_animSelKey < (int)m.anim.keys.size()) {
        auto& k = m.anim.keys[g_animSelKey];
        float d = glm::length(k.position - m.position)
                + glm::length(k.rotation - m.rotation) * 0.01f
                + glm::length(k.scale - m.scale);
        if (d > 0.001f) {
            k.position = m.position;
            k.rotation = m.rotation;
            k.scale = m.scale;
        }
    }

    auto gotoFrame = [&](int fr) {
        fr = std::max(0, fr);
        g_animPlaying = false;
        g_animScrub = m.anim.frameToTime(fr);
        m.anim.duration = std::max(m.anim.duration, g_animScrub);
        applyModelAnimSample(m, g_animScrub);
        // seleziona key sullo stesso frame se esiste
        g_animSelKey = -1;
        for (int i = 0; i < (int)m.anim.keys.size(); ++i) {
            int kf = m.anim.keys[i].frame > 0 ? m.anim.keys[i].frame : m.anim.timeToFrame(m.anim.keys[i].time);
            if (kf == fr) { g_animSelKey = i; break; }
        }
    };

    auto setAnimateFrameHere = [&](bool advance) {
        int fr = m.anim.timeToFrame(g_animScrub);
        g_animSelKey = m.anim.animateFrame(fr, m.position, m.rotation, m.scale);
        m.anim.duration = std::max(m.anim.duration, m.anim.frameToTime(fr));
        g_exportMsg = "AnimateFrame(" + std::to_string(fr) + ", "
            + std::to_string((int)m.position.x) + "," + std::to_string((int)m.position.y)
            + "," + std::to_string((int)m.position.z) + ")";
        if (advance && g_animStepFrames > 0)
            gotoFrame(fr + g_animStepFrames);
    };

    if (ImGui::Begin("AnimateFrame Studio##animedit", &g_showAnimEditor)) {
        ImGui::TextColored({0.25f, 0.75f, 1.f, 1.f}, "AnimateFrame");
        ImGui::SameLine();
        ImGui::TextDisabled("· %s · Rig %s", m.name.c_str(), m.rig.rootName.c_str());

        char clipName[64];
        std::snprintf(clipName, sizeof(clipName), "%s", m.anim.name.c_str());
        ImGui::SetNextItemWidth(160);
        if (ImGui::InputText("Clip", clipName, sizeof(clipName)))
            m.anim.name = clipName;
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70);
        ImGui::DragInt("FPS", &m.anim.fps, 1, 1, 120);
        ImGui::SameLine();
        ImGui::Checkbox("Loop", &m.anim.looped);
        ImGui::SameLine();
        ImGui::Checkbox("Live", &g_animLiveCapture);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Sposta il modello nel viewport: aggiorna il frame selezionato");

        int scrubFrame = m.anim.timeToFrame(g_animScrub);
        int maxFrame = std::max(30, m.anim.lastFrame() + m.anim.fps);
        for (const auto& k : m.anim.keys)
            maxFrame = std::max(maxFrame, (k.frame > 0 ? k.frame : m.anim.timeToFrame(k.time)) + 10);
        m.anim.duration = std::max(m.anim.duration, m.anim.frameToTime(maxFrame));

        // Transport
        if (ImGui::Button("|<")) gotoFrame(0);
        ImGui::SameLine();
        if (ImGui::Button("<<")) gotoFrame(scrubFrame - g_animStepFrames);
        ImGui::SameLine();
        if (ImGui::Button("<")) gotoFrame(scrubFrame - 1);
        ImGui::SameLine();
        if (ImGui::Button(g_animPlaying ? "Pause" : "Play", {70, 0}))
            g_animPlaying = !g_animPlaying;
        ImGui::SameLine();
        if (ImGui::Button(">")) gotoFrame(scrubFrame + 1);
        ImGui::SameLine();
        if (ImGui::Button(">>")) gotoFrame(scrubFrame + g_animStepFrames);
        ImGui::SameLine();
        if (ImGui::Button(">|")) gotoFrame(m.anim.lastFrame());
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70);
        ImGui::DragInt("Step", &g_animStepFrames, 1, 1, 60);

        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderInt("##timeline", &scrubFrame, 0, maxFrame, "Frame %d"))
            gotoFrame(scrubFrame);

        // Timeline grafica (click = vai al frame)
        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 p0 = ImGui::GetCursorScreenPos();
            float w = ImGui::GetContentRegionAvail().x;
            float h = 36.f;
            ImGui::InvisibleButton("##dope", {w, h});
            ImVec2 p1 = {p0.x + w, p0.y + h};
            dl->AddRectFilled(p0, p1, IM_COL32(18, 32, 52, 255), 6.f);
            // ticks ogni 5 frame
            for (int f = 0; f <= maxFrame; f += 5) {
                float x = p0.x + (w - 4.f) * ((float)f / (float)maxFrame) + 2.f;
                dl->AddLine({x, p0.y + 4}, {x, p1.y - 4},
                    f % 10 == 0 ? IM_COL32(120, 160, 210, 180) : IM_COL32(70, 100, 140, 120));
            }
            // key markers
            for (const auto& k : m.anim.keys) {
                int f = k.frame > 0 ? k.frame : m.anim.timeToFrame(k.time);
                float x = p0.x + (w - 4.f) * ((float)f / (float)maxFrame) + 2.f;
                dl->AddCircleFilled({x, p0.y + h * 0.5f}, 5.f, IM_COL32(255, 190, 60, 255));
            }
            // playhead
            float px = p0.x + (w - 4.f) * ((float)scrubFrame / (float)maxFrame) + 2.f;
            dl->AddLine({px, p0.y + 2}, {px, p1.y - 2}, IM_COL32(80, 220, 120, 255), 2.f);
            if (ImGui::IsItemActive() || (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0))) {
                float t = std::clamp((ImGui::GetIO().MousePos.x - p0.x) / std::max(1.f, w), 0.f, 1.f);
                gotoFrame((int)std::lround(t * maxFrame));
            }
        }

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, {0.12f, 0.62f, 0.38f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.18f, 0.75f, 0.48f, 1.f});
        if (ImGui::Button("Set AnimateFrame (K)", {-1, 40}))
            setAnimateFrameHere(true);
        ImGui::PopStyleColor(2);

        ImGui::TextDisabled("Scorciatoie: ← → frame  |  K set+avanza  |  Space play  |  C/V copia/incolla");

        // Shortcuts
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
            if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) gotoFrame(scrubFrame - 1);
            if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) gotoFrame(scrubFrame + 1);
            if (ImGui::IsKeyPressed(ImGuiKey_K)) setAnimateFrameHere(true);
            if (ImGui::IsKeyPressed(ImGuiKey_Space)) g_animPlaying = !g_animPlaying;
            if (ImGui::IsKeyPressed(ImGuiKey_C) && g_animSelKey >= 0 && g_animSelKey < (int)m.anim.keys.size()) {
                const auto& k = m.anim.keys[g_animSelKey];
                g_animClipboard = {k.frame > 0 ? k.frame : m.anim.timeToFrame(k.time), k.position, k.rotation, k.scale};
                g_animHaveClipboard = true;
                g_exportMsg = "Frame copiato";
            }
            if (ImGui::IsKeyPressed(ImGuiKey_V) && g_animHaveClipboard) {
                g_animSelKey = m.anim.animateFrame(scrubFrame, g_animClipboard.position,
                    g_animClipboard.rotation, g_animClipboard.scale);
                applyModelAnimSample(m, g_animScrub);
                g_exportMsg = "Frame incollato su F" + std::to_string(scrubFrame);
            }
        }

        ImGui::Separator();
        ImGui::BeginChild("##keys", {0, 120}, true);
        for (int i = 0; i < (int)m.anim.keys.size(); ++i) {
            auto& k = m.anim.keys[i];
            if (k.frame <= 0) k.frame = m.anim.timeToFrame(k.time);
            char lab[128];
            std::snprintf(lab, sizeof(lab), "F%-4d  pos (%.1f, %.1f, %.1f)##k%d",
                k.frame, k.position.x, k.position.y, k.position.z, i);
            if (ImGui::Selectable(lab, i == g_animSelKey))
                gotoFrame(k.frame);
        }
        ImGui::EndChild();

        if (g_animSelKey >= 0 && g_animSelKey < (int)m.anim.keys.size()) {
            auto& k = m.anim.keys[g_animSelKey];
            int keyFr = k.frame > 0 ? k.frame : m.anim.timeToFrame(k.time);
            ImGui::Text("Key F%d", keyFr);
            if (ImGui::DragInt("Frame##kf", &keyFr, 1, 0, 9999)) {
                k.frame = keyFr;
                k.time = m.anim.frameToTime(keyFr);
                m.anim.duration = std::max(m.anim.duration, k.time);
                m.anim.sortKeys();
                gotoFrame(keyFr);
            }
            if (ImGui::DragFloat3("Position", &k.position.x, 0.1f))
                m.position = k.position;
            if (ImGui::DragFloat3("Rotation", &k.rotation.x, 1.f))
                m.rotation = k.rotation;
            if (ImGui::DragFloat3("Scale", &k.scale.x, 0.05f, 0.05f, 80.f))
                m.scale = k.scale;

            if (ImGui::Button("Copia")) {
                g_animClipboard = {keyFr, k.position, k.rotation, k.scale};
                g_animHaveClipboard = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Incolla qui") && g_animHaveClipboard) {
                g_animSelKey = m.anim.animateFrame(scrubFrame, g_animClipboard.position,
                    g_animClipboard.rotation, g_animClipboard.scale);
            }
            ImGui::SameLine();
            if (ImGui::Button("Elimina") && m.anim.keys.size() > 1) {
                m.anim.keys.erase(m.anim.keys.begin() + g_animSelKey);
                g_animSelKey = std::clamp(g_animSelKey, 0, (int)m.anim.keys.size() - 1);
            }
        }

        ImGui::Separator();
        ImGui::TextWrapped(
            "Flusso rapido: posiziona il modello → Set AnimateFrame → si avanza di Step frame → ripeti → Play.");
        ImGui::TextDisabled("Lua: AnimateFrame(frame, Vector3.new(x,y,z))");
    }
    ImGui::End();
    if (!g_showAnimEditor) {
        g_animPlaying = false;
        g_animEditModel = -1;
    }
}

static void drawStudioUi(AnimatedModel& fbx, bool fbxOk) {
    g_studioUiTime = (float)glfwGetTime();

    // In Test: niente menu/toolbar (rubano il mouse e bloccano la visuale)
    if (g_studioTesting) {
        ImGui::SetNextWindowPos({70, 12}, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.45f);
        ImGui::Begin("##TestHud", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoInputs);
        ImGui::TextColored({0.3f, 1.f, 0.5f, 1.f}, "%s", L("test.playing"));
        ImGui::TextDisabled("%s  ·  Esc/F1 stop", g_world.placeName.c_str());
        ImGui::End();
        drawHealthBarHud();
        drawPlayerHeadHealthBar();
        drawChatBubbleHud();
        drawFpCrosshair();
        drawSpeechBubbles();
        drawInventoryHud();
        drawWorldLabels();
        if (g_showChat) {
            ImGui::SetNextWindowPos({12, 112}, ImGuiCond_Appearing);
            ImGui::SetNextWindowSize({360, 220}, ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowBgAlpha(0.92f);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.08f, 0.22f, 0.48f, 0.95f});
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.f);
            ImGui::Begin("Chat##testchat", &g_showChat);
            ImGui::BeginChild("##tlog", {0, -36}, false);
            for (const auto& line : g_menu.chatLines)
                ImGui::TextWrapped("%s", line.c_str());
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.f)
                ImGui::SetScrollHereY(1.f);
            ImGui::EndChild();
            bool send = ImGui::InputText("##tmsg", g_studioChatBuf, sizeof(g_studioChatBuf),
                ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::SameLine();
            if ((ImGui::Button("Invia") || send) && g_studioChatBuf[0]) {
                sendPlayChat(g_studioChatBuf);
                g_studioChatBuf[0] = 0;
            }
            ImGui::End();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
        }
        drawSpeechBubbles();
        return;
    }

    if (ImGui::BeginMainMenuBar()) {
        ImGui::TextColored({0.35f, 0.78f, 1.f, 1.f}, "Blocky");
        ImGui::SameLine();
        ImGui::TextColored({0.95f, 0.75f, 0.25f, 1.f}, "Studio");
        ImGui::SameLine();
        ImGui::TextDisabled("  ·  %s", g_world.placeName.c_str());
        ImGui::SameLine();
        ImGui::Spacing();
        if (ImGui::BeginMenu(L("file"))) {
            if (ImGui::MenuItem(L("menu.new_place"), "Ctrl+N")) {
                requestNav(glfwGetCurrentContext(), PendingNav::NewPlace);
            }
            if (ImGui::BeginMenu(L("menu.new_map"))) {
                if (ImGui::MenuItem(L("map.baseplate"))) applyMapTemplate(0);
                if (ImGui::MenuItem(L("map.garden"))) applyMapTemplate(1);
                if (ImGui::MenuItem(L("map.obby"))) applyMapTemplate(2);
                if (ImGui::MenuItem(L("map.arena"))) applyMapTemplate(3);
                if (ImGui::MenuItem(L("map.park"))) applyMapTemplate(4);
                if (ImGui::MenuItem(L("map.city"))) applyMapTemplate(5);
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem(L("menu.open_place"), "Ctrl+O")) doOpenPlace();
            if (ImGui::MenuItem(L("menu.save_place"), "Ctrl+S")) doSavePlace();
            if (ImGui::MenuItem(L("menu.save_place_as"), "Ctrl+Shift+S")) doSavePlaceAs();
            if (ImGui::MenuItem("Salva per pubblicare", "Ctrl+Shift+U")) doSaveForPublish();
            if (ImGui::MenuItem(L("menu.save_project"))) doSaveProject();
            ImGui::Separator();
            if (ImGui::MenuItem(L("menu.export_place"), "Ctrl+E")) doExport();
            if (ImGui::MenuItem(L("menu.publish"), "Ctrl+Shift+P")) {
                if (publishCurrentGame()) {
                    refreshGamesList();
                    CrashHandler::note("Publish da menu File OK");
                }
            }
            ImGui::TextDisabled("Salva · Salva per pubblicare (bozza) · Pubblica");
            ImGui::TextDisabled("Max %d giochi / 2 mesi per account", kMaxPublishedGamesPerUser);
            if (g_publishDraftActive)
                ImGui::TextColored({0.35f, 0.9f, 0.55f, 1.f}, "Autosave bozza publish ON");
            if (!g_placeFilePath.empty())
                ImGui::TextDisabled("%s", g_placeFilePath.c_str());
            ImGui::Separator();
            if (kIsGameEngine) {
                if (ImGui::MenuItem("Home"))
                    requestNav(glfwGetCurrentContext(), PendingNav::GoHome);
                if (ImGui::MenuItem(L("menu.exit_engine")))
                    requestNav(glfwGetCurrentContext(), PendingNav::QuitApp);
            } else {
                if (ImGui::MenuItem(L("menu.back_blocky")))
                    returnToMenu(glfwGetCurrentContext());
                if (ImGui::MenuItem(L("menu.exit")))
                    glfwSetWindowShouldClose(glfwGetCurrentContext(), true);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(L("insert"))) {
            if (ImGui::MenuItem(L("menu.part"), "Ctrl+P")) insertNewPart();
            if (ImGui::MenuItem("Ball (sfera)")) insertShapedPart(PartShape::Ball);
            if (ImGui::MenuItem("Cylinder (cilindro)")) insertShapedPart(PartShape::Cylinder);
            if (ImGui::MenuItem("Wedge (triangolo/rampa)")) insertShapedPart(PartShape::Wedge);
            if (ImGui::MenuItem("Neon Part (lucente)")) insertNeonPart();
            if (ImGui::MenuItem("PointLight")) insertPointLightPart();
            if (ImGui::MenuItem("Explode Part")) insertExplodePart();
            if (ImGui::MenuItem("Player amico")) insertFriendPlayer();
            if (ImGui::MenuItem(L("menu.model"), "Ctrl+M")) insertImportedModel();
            if (ImGui::MenuItem(L("menu.textlabel"), "Ctrl+Shift+T")) insertNewTextLabel();
            if (ImGui::MenuItem(L("menu.label"), "Ctrl+T")) insertNewLabel();
            if (ImGui::MenuItem(L("menu.textbutton"), "Ctrl+B")) insertNewTextButton();
            if (ImGui::MenuItem(L("menu.sound"), "Ctrl+U")) insertSound();
            ImGui::Separator();
            if (ImGui::BeginMenu("Particles")) {
                glm::vec3 at = g_cam.pos + g_cam.front() * 8.f;
                if (g_selectedPart >= 0 && g_selectedPart < (int)g_world.parts.size())
                    at = g_world.parts[g_selectedPart].position + glm::vec3(0, 2, 0);
                if (ImGui::MenuItem("Sparks"))
                    g_particles.emitPreset(ParticlePreset::Sparks, at);
                if (ImGui::MenuItem("Smoke"))
                    g_particles.emitPreset(ParticlePreset::Smoke, at);
                if (ImGui::MenuItem("Coins"))
                    g_particles.emitPreset(ParticlePreset::Coins, at);
                if (ImGui::MenuItem("Burst"))
                    g_particles.emitPreset(ParticlePreset::Burst, at);
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Backpack Item...")) {
                static int hue = 0;
                g_world.addBackpackItem("Item", hue++);
                g_exportMsg = "Aggiunto al Backpack — si vede in Test inventario";
            }
            if (ImGui::BeginMenu("Backpack presets")) {
                if (ImGui::MenuItem("Sword")) g_world.addBackpackItem("Sword", 3, BackpackTool::Sword);
                if (ImGui::MenuItem("Pistola")) g_world.addBackpackItem("Pistola", 4, BackpackTool::Pistol);
                if (ImGui::MenuItem("Potion")) g_world.addBackpackItem("Potion", 1);
                if (ImGui::MenuItem("Key")) g_world.addBackpackItem("Key", 2);
                if (ImGui::MenuItem("Shield")) g_world.addBackpackItem("Shield", 0);
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::BeginMenu(L("menu.props"))) {
                if (ImGui::MenuItem(L("menu.tree"))) insertPropPreset(0);
                if (ImGui::MenuItem(L("menu.house"))) insertPropPreset(1);
                if (ImGui::MenuItem(L("menu.ramp"))) insertPropPreset(2);
                if (ImGui::MenuItem(L("menu.tower"))) insertPropPreset(3);
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(L("edit"))) {
            if (ImGui::MenuItem("Annulla", "Ctrl+Z", false, g_undo.canUndo())) {
                g_undo.applyUndo(g_world, g_selectedPart);
                g_exportMsg = "Annullato";
            }
            if (ImGui::MenuItem("Ripeti", "Ctrl+Y", false, g_undo.canRedo())) {
                g_undo.applyRedo(g_world, g_selectedPart);
                g_exportMsg = "Ripetuto";
            }
            ImGui::Separator();
            if (ImGui::MenuItem(L("menu.dup"), "Ctrl+D")) duplicateSelected();
            if (ImGui::MenuItem(L("menu.script_part"), "Ctrl+L")) addScriptToSelected();
            if (ImGui::MenuItem(L("menu.delete"), "Del")) deleteSelected();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(L("player"))) {
            if (ImGui::MenuItem(L("menu.avatar_catalog"), "F8"))
                enterAvatarPublisher();
            if (ImGui::MenuItem(L("menu.rig_player"), "Ctrl+Shift+R"))
                beginRigPlayer();
            if (ImGui::MenuItem(L("menu.publish_anim"), "Ctrl+Shift+I"))
                g_showPlayerAnimPublisher = true;
            ImGui::Separator();
            if (ImGui::MenuItem(L("menu.play_idle"))) g_animator.play("Idle");
            if (ImGui::MenuItem(L("menu.play_walk"))) g_animator.play("Walk");
            if (ImGui::MenuItem(L("menu.stop_anim"))) g_animator.stop();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(L("animation"))) {
            if (ImGui::MenuItem(L("menu.animate_model"), "Ctrl+Shift+A"))
                beginAnimateSelectedModel();
            ImGui::Separator();
            ImGui::TextDisabled("Player (corpo R6):");
            if (ImGui::MenuItem(L("menu.play_idle"))) g_animator.play("Idle");
            if (ImGui::MenuItem(L("menu.play_walk"))) g_animator.play("Walk");
            if (ImGui::MenuItem(L("menu.stop_anim"))) g_animator.stop();
            ImGui::MenuItem("Dock clip Player", nullptr, &g_showAnimDock);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(L("view"))) {
            ImGui::MenuItem(L("menu.explorer"), nullptr, &g_showExplorer);
            ImGui::MenuItem(L("menu.properties"), nullptr, &g_showProps);
            ImGui::MenuItem(L("menu.create_panel"), nullptr, &g_showCreatePanel);
            ImGui::MenuItem(L("menu.global_script"), nullptr, &g_showScript);
            ImGui::MenuItem("Guida Lua Blocky", "F2", &g_showLuaGuide);
            ImGui::MenuItem(L("menu.engine_settings"), nullptr, &g_showPlatformPanel);
            ImGui::MenuItem(L("menu.chat"), nullptr, &g_showChat);
            ImGui::Separator();
            ImGui::TextDisabled("%s", L("lang"));
            drawLanguageSelector("studiolang");
            ImGui::EndMenu();
        }
        ImGui::SameLine(ImGui::GetWindowWidth() - 320);
        drawLanguageButton("menulang");
        ImGui::SameLine();
        ImGui::TextDisabled("%.0f FPS", g_fps);
        ImGui::EndMainMenuBar();
    }

    const float menuH = ImGui::GetFrameHeight();

    // Toolbar essenziale: tool + test + Model/Anima (il resto nei menu)
    ImGui::SetNextWindowPos({(float)g_w * 0.5f - 340.f, menuH + 6}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({680, 48}, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {6.f, 4.f});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.07f, 0.09f, 0.14f, 0.94f});
    ImGui::Begin("##BuildBar", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus);

    auto toolBtn = [](const char* label, StudioTool t) {
        bool on = (g_tool == t);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.f);
        if (on) {
            ImGui::PushStyleColor(ImGuiCol_Button, {0.18f, 0.48f, 0.90f, 1.f});
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.28f, 0.58f, 1.f, 1.f});
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, {0.16f, 0.20f, 0.28f, 1.f});
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.22f, 0.30f, 0.42f, 1.f});
        }
        if (ImGui::Button(label, {0, 30})) {
            g_tool = t;
            g_scaleDragging = false;
            g_moveDragging = false;
            g_rotateDragging = false;
        }
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar();
    };
    toolBtn(LB("tool.select", "tsel"), StudioTool::Select);
    ImGui::SameLine();
    toolBtn(LB("tool.move", "tmov"), StudioTool::Move);
    ImGui::SameLine();
    toolBtn(LB("tool.scale", "tscl"), StudioTool::Scale);
    ImGui::SameLine();
    toolBtn(LB("tool.rotate", "trot"), StudioTool::Rotate);
    ImGui::SameLine();
    ImGui::Dummy({10, 0});
    ImGui::SameLine();
    if (!g_studioTesting) {
        ImGui::PushStyleColor(ImGuiCol_Button, {0.12f, 0.68f, 0.34f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.18f, 0.80f, 0.42f, 1.f});
        if (ImGui::Button(LB("test", "testbtn"), {72, 30}))
            startStudioTest(glfwGetCurrentContext());
        ImGui::PopStyleColor(2);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("F5 · Esc = stop");
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, {0.82f, 0.22f, 0.18f, 1.f});
        if (ImGui::Button(LB("stop", "stopbtn"), {72, 30}))
            stopStudioTest(glfwGetCurrentContext());
        ImGui::PopStyleColor();
    }
    ImGui::SameLine();
    ImGui::Dummy({8, 0});
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, {0.18f, 0.42f, 0.32f, 1.f});
    if (ImGui::Button(LB("btn.part", "bpart"), {0, 30})) insertNewPart();
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, {0.75f, 0.35f, 0.35f, 1.f});
    if (ImGui::Button("Sfera", {0, 30})) insertShapedPart(PartShape::Ball);
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Ball — sfera");
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, {0.3f, 0.5f, 0.85f, 1.f});
    if (ImGui::Button("Cilindro", {0, 30})) insertShapedPart(PartShape::Cylinder);
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Cylinder — asse Y");
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, {0.35f, 0.7f, 0.35f, 1.f});
    if (ImGui::Button("Wedge", {0, 30})) insertShapedPart(PartShape::Wedge);
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Wedge — triangolo / rampa");
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, {0.12f, 0.72f, 0.55f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.20f, 0.85f, 0.65f, 1.f});
    if (ImGui::Button("Neon", {0, 30})) insertNeonPart();
    ImGui::PopStyleColor(2);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Parte lucente + luce dinamica");
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, {0.85f, 0.72f, 0.18f, 1.f});
    if (ImGui::Button("Light", {0, 30})) insertPointLightPart();
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("PointLight dinamico");
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, {0.82f, 0.22f, 0.15f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.95f, 0.32f, 0.2f, 1.f});
    if (ImGui::Button("Boom", {0, 30})) insertExplodePart();
    ImGui::PopStyleColor(2);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Explode Part — tocco = boom");
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, {0.22f, 0.62f, 0.38f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.30f, 0.75f, 0.48f, 1.f});
    if (ImGui::Button("Amico", {0, 30})) insertFriendPlayer();
    ImGui::PopStyleColor(2);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Player amico — in Test ti segue");
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, {0.28f, 0.42f, 0.72f, 1.f});
    if (ImGui::Button(LB("btn.model", "bmodel"), {0, 30})) insertImportedModel();
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, {0.85f, 0.45f, 0.18f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.95f, 0.55f, 0.25f, 1.f});
    if (ImGui::Button(LB("btn.anima", "banima"), {0, 30}))
        beginAnimateSelectedModel();
    ImGui::PopStyleColor(2);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Anima Model · Ctrl+Shift+A · Move + Set Frame (K)");
    ImGui::SameLine();
    if (ImGui::Button(LB("more", "insMore"), {0, 30}))
        ImGui::OpenPopup("##studioMore");
    if (ImGui::BeginPopup("##studioMore")) {
        if (ImGui::MenuItem(L("menu.textlabel"))) insertNewTextLabel();
        if (ImGui::MenuItem(L("menu.label"))) insertNewLabel();
        if (ImGui::MenuItem(L("menu.textbutton"))) insertNewTextButton();
        if (ImGui::MenuItem(L("menu.sound"))) insertSound();
        if (ImGui::MenuItem("Player amico")) insertFriendPlayer();
        ImGui::Separator();
        if (ImGui::MenuItem(L("menu.dup"), "Ctrl+D")) duplicateSelected();
        if (ImGui::MenuItem(L("menu.script_part"), "Ctrl+L")) addScriptToSelected();
        if (ImGui::MenuItem(L("menu.delete"), "Del")) deleteSelected();
        ImGui::Separator();
        if (ImGui::MenuItem(L("btn.avatar"), "F8")) enterAvatarPublisher();
        if (ImGui::MenuItem(L("btn.publish_anim"))) g_showPlayerAnimPublisher = true;
        if (ImGui::MenuItem(L("btn.create"))) g_showCreatePanel = true;
        ImGui::EndPopup();
    }
    if (!g_exportMsg.empty()) {
        ImGui::SameLine();
        ImGui::TextColored({1.f, 0.85f, 0.4f, 1.f}, "  %s", g_exportMsg.c_str());
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    // Rig Player / Model Animator — sempre visibili
    drawPlayerRigWindows();
    drawModelAnimWindows();

    // Pannello Create animato (map / props / label / model)
    if (g_showCreatePanel && !g_studioTesting) {
        float appear = studioEaseOut(std::min(1.f, g_studioUiTime * 0.8f));
        ImGui::SetNextWindowPos({(float)g_w * 0.5f - 340.f, menuH + 72}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({680, 168}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.92f * appear);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.f);
        ImGui::Begin("Create##create", &g_showCreatePanel);
        ImGui::TextDisabled("Mappe e props — chiudi con X se non ti serve");
        ImGui::Separator();
        ImGui::Text("Mappe");
        if (ImGui::Button(LB("map.baseplate", "mbp"), {90, 28})) applyMapTemplate(0);
        ImGui::SameLine();
        if (ImGui::Button(LB("map.garden", "mgd"), {80, 28})) applyMapTemplate(1);
        ImGui::SameLine();
        if (ImGui::Button(LB("map.obby", "mob"), {70, 28})) applyMapTemplate(2);
        ImGui::SameLine();
        if (ImGui::Button(LB("map.arena", "mar"), {70, 28})) applyMapTemplate(3);
        ImGui::SameLine();
        if (ImGui::Button(LB("map.park", "mpk"), {70, 28})) applyMapTemplate(4);
        ImGui::SameLine();
        if (ImGui::Button(LB("map.city", "mct"), {70, 28})) applyMapTemplate(5);

        ImGui::Spacing();
        ImGui::Text("Props");
        if (ImGui::Button(LB("menu.tree", "ptree"), {70, 28})) insertPropPreset(0);
        ImGui::SameLine();
        if (ImGui::Button(LB("menu.house", "phouse"), {70, 28})) insertPropPreset(1);
        ImGui::SameLine();
        if (ImGui::Button(LB("menu.ramp", "pramp"), {70, 28})) insertPropPreset(2);
        ImGui::SameLine();
        if (ImGui::Button(LB("menu.tower", "ptower"), {70, 28})) insertPropPreset(3);
        ImGui::End();
        ImGui::PopStyleVar();
    }

    if (g_showAnimDock && !g_studioTesting) {
        ImGui::SetNextWindowPos({(float)g_w - 280.f, (float)g_h - 160.f}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({260, 130}, ImGuiCond_FirstUseEver);
        ImGui::Begin("Clip rapide##animdock", &g_showAnimDock);
        if (ImGui::Button("Idle", {56, 26})) g_animator.play("Idle");
        ImGui::SameLine();
        if (ImGui::Button("Walk", {56, 26})) g_animator.play("Walk");
        ImGui::SameLine();
        if (ImGui::Button("Stop", {56, 26})) g_animator.stop();
        if (ImGui::Button("Pubblica AnimationId...", {-1, 28}))
            g_showPlayerAnimPublisher = true;
        ImGui::TextDisabled("%s", g_animator.lastPlayed.c_str());
        ImGui::End();
    }

    if (!g_studioTesting)
        drawStudioPlayerAnimPanel();

    if (g_showExplorer) {
        ImGui::SetNextWindowPos({12, menuH + 56}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({300, 480}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(1.f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.08f, 0.11f, 0.18f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_Text, {0.93f, 0.96f, 1.00f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_TextDisabled, {0.55f, 0.65f, 0.78f, 1.f});
        ImGui::Begin("Explorer", &g_showExplorer);

        if (ImGui::TreeNodeEx("Workspace", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::Selectable("Baseplate##bp", g_selectedBaseplate))
                selectBaseplate();
            if (ImGui::Selectable("SpawnLocation##sp", g_selectedSpawn))
                selectSpawn();

            if (ImGui::TreeNodeEx("Parts", ImGuiTreeNodeFlags_DefaultOpen)) {
                for (int i = 0; i < (int)g_world.parts.size(); ++i) {
                    auto& p = g_world.parts[i];
                    bool sel = (i == g_selectedPart) && !g_editingPartScript;
                    std::string label = p.name;
                    if (p.shape != PartShape::Block)
                        label += std::string(" [") + partShapeName(p.shape) + "]";
                    if (p.isNeon()) label += " [Neon]";
                    else if (p.pointLight) label += " [Light]";
                    else if (p.material != PartMaterial::Plastic)
                        label += std::string(" [") + partMaterialName(p.material) + "]";
                    label += "##part" + std::to_string(i);
                    if (ImGui::Selectable(label.c_str(), sel)) {
                        selectPart(i);
                        g_editingPartScript = false;
                    }
                    if (!p.script.empty()) {
                        ImGui::Indent();
                        bool scriptSel = (i == g_selectedPart) && g_editingPartScript;
                        std::string slabel = std::string("Script##sc") + std::to_string(i);
                        if (ImGui::Selectable(slabel.c_str(), scriptSel)) {
                            selectPart(i);
                            g_editingPartScript = true;
                            g_showProps = true;
                        }
                        ImGui::Unindent();
                    }
                }
                ImGui::TreePop();
            }

            if (ImGui::TreeNodeEx("Models", ImGuiTreeNodeFlags_DefaultOpen)) {
                for (int i = 0; i < (int)g_world.models.size(); ++i) {
                    auto& m = g_world.models[i];
                    std::string label = m.name + "##model" + std::to_string(i);
                    if (ImGui::Selectable(label.c_str(), i == g_selectedModel)) {
                        selectModel(i);
                        g_showProps = true;
                    }
                }
                if (ImGui::SmallButton("+ Import Model")) insertImportedModel();
                ImGui::TreePop();
            }

            if (ImGui::TreeNodeEx("StarterGui", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (ImGui::TreeNodeEx("ScreenGui", ImGuiTreeNodeFlags_DefaultOpen)) {
                    for (int i = 0; i < (int)g_world.labels.size(); ++i) {
                        auto& l = g_world.labels[i];
                        std::string label = l.className + " · " + l.name + "##lab" + std::to_string(i);
                        if (ImGui::Selectable(label.c_str(), i == g_selectedLabel))
                            selectLabel(i);
                    }
                    if (ImGui::SmallButton("+ TextLabel")) insertNewTextLabel();
                    ImGui::SameLine();
                    if (ImGui::SmallButton("+ Label")) insertNewLabel();
                    ImGui::SameLine();
                    if (ImGui::SmallButton("+ TextButton")) insertNewTextButton();
                    ImGui::TreePop();
                }
                ImGui::TreePop();
            }

            if (ImGui::TreeNodeEx("Backpack", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::TextDisabled("Item → inventario circolare in Test/Play");
                for (int i = 0; i < (int)g_world.backpack.size(); ++i) {
                    auto& b = g_world.backpack[i];
                    bool eq = (g_world.equippedBackpack == i);
                    std::string label = (eq ? "* " : "") + b.name + "##bp" + std::to_string(i);
                    if (ImGui::Selectable(label.c_str(), i == g_selectedBackpack)) {
                        g_selectedBackpack = i;
                        g_showProps = true;
                    }
                }
                if (ImGui::SmallButton("+ Item")) {
                    g_world.addBackpackItem("Item", (int)g_world.backpack.size());
                    g_selectedBackpack = (int)g_world.backpack.size() - 1;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("+ Sword"))
                    g_world.addBackpackItem("Sword", 3, BackpackTool::Sword);
                ImGui::SameLine();
                if (ImGui::SmallButton("+ Pistola"))
                    g_world.addBackpackItem("Pistola", 4, BackpackTool::Pistol);
                ImGui::TreePop();
            }

            if (ImGui::TreeNodeEx("Players", ImGuiTreeNodeFlags_DefaultOpen)) {
                bool playerOpen = ImGui::TreeNodeEx("LocalPlayer",
                    ImGuiTreeNodeFlags_DefaultOpen
                    | (g_selectedPlayer && g_selectedPlayerPart < 0 ? ImGuiTreeNodeFlags_Selected : 0));
                if (ImGui::IsItemClicked())
                    selectPlayer(-1);
                if (playerOpen) {
                    ImGui::TextDisabled(g_player.humanoid.userRigged ? "Rig: OK" : "Rig: mancante");
                    if (ImGui::SmallButton("Riggare..."))
                        beginRigPlayer();
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Anim ID..."))
                        g_showPlayerAnimPublisher = true;

                    if (ImGui::TreeNodeEx("Humanoid", ImGuiTreeNodeFlags_DefaultOpen)) {
                        if (ImGui::IsItemClicked())
                            selectPlayer(-1);
                        for (int i = 0; i < (int)g_player.humanoid.parts.size(); ++i) {
                            auto& bp = g_player.humanoid.parts[i];
                            bool sel = g_selectedPlayer && g_selectedPlayerPart == i;
                            if (ImGui::Selectable(bp.name.c_str(), sel))
                                selectPlayer(i);
                        }
                        ImGui::TreePop();
                    }
                    ImGui::TreePop();
                }
                ImGui::TreePop();
            }

            ImGui::TreePop();
        }

        ImGui::Separator();
        if (ImGui::TreeNodeEx("Services", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::Selectable("Lighting##svc0", g_selectedService == 0)) selectService(0);
            if (ImGui::Selectable("RunService##svc1", g_selectedService == 1)) selectService(1);
            if (ImGui::Selectable("TweenService##svc2", g_selectedService == 2)) selectService(2);
            if (ImGui::Selectable("UserInputService##svc3", g_selectedService == 3)) selectService(3);
            if (ImGui::Selectable("Chat##svc4", g_selectedService == 4)) selectService(4);
            if (ImGui::Selectable("Debris##svc5", g_selectedService == 5)) selectService(5);
            if (ImGui::Selectable("ReplicatedStorage##svc6", g_selectedService == 6)) selectService(6);
            if (ImGui::Selectable("StarterGui##svc7", g_selectedService == 7)) selectService(7);
            if (ImGui::TreeNodeEx("SoundService", ImGuiTreeNodeFlags_DefaultOpen)) {
                for (int i = 0; i < (int)g_sounds.sounds.size(); ++i) {
                    auto& s = g_sounds.sounds[i];
                    std::string label = s.name + (s.playing ? " [playing]" : "") + "##snd" + std::to_string(i);
                    if (ImGui::Selectable(label.c_str(), i == g_selectedSound)) {
                        selectSound(i);
                        g_showProps = true;
                    }
                }
                if (ImGui::SmallButton("+ Sound")) insertSound();
                ImGui::TreePop();
            }
            bool animSel = g_selectedService < 0 && !g_selectedPlayer && !g_selectedBaseplate && !g_selectedSpawn
                && g_selectedPart < 0 && g_selectedModel < 0 && g_selectedSound < 0
                && g_selectedLabel < 0;
            if (ImGui::Selectable("Animator##svc", animSel)) {
                clearAllSelection();
                g_showProps = true;
            }
            ImGui::TextDisabled("clip: %s", g_animator.lastPlayed.c_str());
            ImGui::TreePop();
        }

        ImGui::Separator();
        if (ImGui::Button("Aggiungi Script al blocco", {-1, 0}))
            addScriptToSelected();
        if (g_showFbx && fbxOk) ImGui::BulletText("FBX (legacy)");
        ImGui::End();
        ImGui::PopStyleColor(3);
    }

    if (g_showProps) {
        ImGui::SetNextWindowPos({(float)g_w - 340, menuH + 56}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({328, 560}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(1.f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.08f, 0.11f, 0.18f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_ChildBg, {0.10f, 0.14f, 0.22f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_Text, {0.93f, 0.96f, 1.00f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_TextDisabled, {0.55f, 0.65f, 0.78f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_FrameBg, {0.12f, 0.18f, 0.30f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_Border, {0.22f, 0.34f, 0.52f, 1.f});
        ImGui::Begin("Properties", &g_showProps);

        if (g_selectedBaseplate) {
            ImGui::TextColored({0.35f, 0.85f, 0.45f, 1.f}, "Workspace · Baseplate");
            ImGui::Checkbox("Enabled", &g_world.hasBaseplate);
            ImGui::DragFloat3("Size", &g_world.baseplateSize.x, 1.f, 8.f, 512.f);
            ImGui::ColorEdit3("Color", &g_world.baseplateColor.x);
            drawPartColorPicker(g_world.baseplateColor);
            ImGui::TextDisabled("Clicca la base nel viewport o qui in Explorer.");
        } else if (g_selectedService == 0) {
            ImGui::TextColored({1.f, 0.88f, 0.35f, 1.f}, "Services · Lighting");

            // —— Giorno / notte del place ——
            ImGui::TextColored({0.55f, 0.85f, 1.f, 1.f}, "Giorno / Notte");
            int mode = (int)g_lighting.dayNight;
            bool modeChanged = false;
            modeChanged |= ImGui::RadioButton("Ciclo giorno/notte", &mode, 0);
            modeChanged |= ImGui::RadioButton("Solo giorno", &mode, 1);
            ImGui::SameLine();
            modeChanged |= ImGui::RadioButton("Solo notte", &mode, 2);
            if (modeChanged) {
                g_lighting.dayNight = (DayNightMode)mode;
                markPlaceDirty();
            }
            if (g_lighting.dayNight == DayNightMode::Cycle) {
                ImGui::SliderFloat("Durata giornata", &g_lighting.dayLengthMinutes,
                                   0.25f, 60.f, "%.2f min");
                ImGui::TextDisabled("Un giro completo 0h → 24h in %.1f min",
                                    g_lighting.dayLengthMinutes);
                ImGui::TextColored({0.55f, 0.85f, 1.f, 1.f},
                    "Il tempo scorre solo in Test (F5) e in Play.");
                ImGui::TextDisabled("Qui in Studio resta fermo su ClockTime.");
            } else if (g_lighting.dayNight == DayNightMode::DayOnly) {
                ImGui::SliderFloat("Ora fissa", &g_lighting.fixedDayClock, 7.f, 17.f, "%.1f h");
            } else {
                ImGui::SliderFloat("Ora fissa", &g_lighting.fixedNightClock, 0.f, 24.f, "%.1f h");
                ImGui::TextDisabled("Sopra l'orizzonte c'è la luna: ombre più deboli.");
            }
            ImGui::Checkbox("Ombre da sole/luna", &g_lighting.globalShadows);
            if (g_engine.quality.shadowOccluders <= 0)
                ImGui::TextColored({1.f, 0.65f, 0.3f, 1.f},
                                   "Preset grafico Low: ombre disattivate");
            ImGui::Separator();

            ImGui::SliderFloat("ClockTime", &g_lighting.clockTime, 0.f, 24.f, "%.1f h");
            if (g_lighting.dayNight != DayNightMode::Cycle)
                ImGui::TextDisabled("(sovrascritto dall'ora fissa qui sopra)");
            ImGui::SliderFloat("Brightness", &g_lighting.brightness, 0.f, 10.f, "%.2f");
            ImGui::SliderFloat("Ambient", &g_lighting.ambient, 0.f, 1.f, "%.2f");
            ImGui::ColorEdit3("OutdoorAmbient", &g_lighting.outdoorAmbient.x);
            ImGui::ColorEdit3("FogColor", &g_lighting.fogColor.x);
            ImGui::DragFloat("FogStart", &g_lighting.fogStart, 1.f, 0.f, 5000.f);
            ImGui::DragFloat("FogEnd", &g_lighting.fogEnd, 1.f, 1.f, 8000.f);
            if (ImGui::Button("Mezzogiorno (12)")) g_lighting.clockTime = 12.f;
            ImGui::SameLine();
            if (ImGui::Button("Tramonto (19)")) g_lighting.clockTime = 19.f;
            ImGui::SameLine();
            if (ImGui::Button("Notte (22)")) g_lighting.clockTime = 22.f;
            g_lighting.clampAll();
            ImGui::TextDisabled("Sole: %s · notte %.0f%%",
                                g_lighting.sunIsUp() ? "sopra l'orizzonte" : "tramontato",
                                g_lighting.nightAmount() * 100.f);
            ImGui::TextDisabled("Lua: game.Lighting:SetClockTime(14)");
        } else if (g_selectedService == 1) {
            ImGui::TextColored({0.45f, 0.85f, 1.f, 1.f}, "Services · RunService");
            ImGui::TextWrapped("Heartbeat via game.RunService.Heartbeat:Connect(fn).");
            ImGui::TextDisabled("Usato dagli script in Test/Play.");
        } else if (g_selectedService == 2) {
            ImGui::TextColored({0.75f, 0.55f, 1.f, 1.f}, "Services · TweenService");
            ImGui::TextWrapped("TweenService:Create(...):Play() — stub compatibile (espansione in corso).");
        } else if (g_selectedService == 3) {
            ImGui::TextColored({0.55f, 0.95f, 0.65f, 1.f}, "Services · UserInputService");
            ImGui::TextWrapped("DisableJump / EnableJump da Lua.");
        } else if (g_selectedService == 4) {
            ImGui::TextColored({0.95f, 0.75f, 0.35f, 1.f}, "Services · Chat");
            ImGui::TextWrapped("Chat in-game (menu Chat). GetService(\"Chat\") disponibile.");
        } else if (g_selectedService == 5) {
            ImGui::TextColored({0.9f, 0.5f, 0.4f, 1.f}, "Services · Debris");
            ImGui::TextWrapped("Debris:AddItem(part, seconds) — rimozione parti.");
        } else if (g_selectedService == 6) {
            ImGui::TextColored({0.5f, 0.8f, 1.f, 1.f}, "Services · ReplicatedStorage");
            ImGui::TextWrapped("Alias Events — condivisione dati/script.");
        } else if (g_selectedService == 7) {
            ImGui::TextColored({0.6f, 0.9f, 1.f, 1.f}, "Services · StarterGui");
            ImGui::TextWrapped("Contiene ScreenGui → TextLabel / Label / TextButton.");
        } else if (g_selectedLabel >= 0 && g_selectedLabel < (int)g_world.labels.size()) {
            auto& l = g_world.labels[g_selectedLabel];
            ImGui::TextColored({0.35f, 0.85f, 1.f, 1.f}, "ScreenGui · %s",
                               l.className.empty() ? "TextLabel" : l.className.c_str());
            if (ImGui::InputText("Name", g_labelNameBuf, sizeof(g_labelNameBuf)))
                l.name = g_labelNameBuf;
            if (ImGui::InputText("Text", g_labelTextBuf, sizeof(g_labelTextBuf)))
                l.text = g_labelTextBuf;
            {
                int ck = 0;
                if (l.className == "Label") ck = 1;
                else if (l.isButton()) ck = 2;
                const char* kinds[] = { "TextLabel", "Label", "TextButton" };
                if (ImGui::Combo("Class", &ck, kinds, 3)) {
                    l.className = kinds[ck];
                    if (l.isButton()) {
                        if (l.bgTransparency > 0.9f) l.bgTransparency = 0.f;
                        if (l.bgColor.x + l.bgColor.y + l.bgColor.z < 0.05f)
                            l.bgColor = {0.55f, 0.55f, 0.58f};
                        if (l.sizeW < 1.f) l.sizeW = 140.f;
                        if (l.sizeH < 1.f) l.sizeH = 40.f;
                    }
                }
            }
            ImGui::SliderFloat("Anchor X", &l.anchorX, 0.f, 1.f);
            ImGui::SliderFloat("Anchor Y", &l.anchorY, 0.f, 1.f);
            ImGui::DragFloat("Offset X (px)", &l.offsetX, 1.f);
            ImGui::DragFloat("Offset Y (px)", &l.offsetY, 1.f);
            ImGui::DragFloat("Size W (px)", &l.sizeW, 1.f, 0.f, 2000.f);
            ImGui::DragFloat("Size H (px)", &l.sizeH, 1.f, 0.f, 2000.f);
            if (ImGui::IsItemHovered() || ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("0 = auto dal testo · trascina i vertici nel viewport");
            ImGui::SliderFloat("Font scale", &l.fontScale, 0.5f, 2.5f);
            ImGui::ColorEdit3("Text Color", &l.color.x);
            ImGui::ColorEdit3("Background Color", &l.bgColor.x);
            ImGui::SliderFloat("Background Transparency", &l.bgTransparency, 0.f, 1.f, "%.2f");
            ImGui::TextDisabled("0 = bg opaco · 1 = solo testo + outline");
            ImGui::Checkbox("Visible", &l.visible);
            if (l.isButton())
                ImGui::TextDisabled("TextButton: cliccabile in Test/Play (cursore libero)");
            else
                ImGui::TextDisabled("Trascina per muovere · vertici per ridimensionare");
            if (studioAccentButton("Rimuovi", {-1, 32}, {0.75f, 0.2f, 0.18f, 1.f}))
                deleteSelected();
        } else if (g_selectedBackpack >= 0 && g_selectedBackpack < (int)g_world.backpack.size()) {
            auto& b = g_world.backpack[g_selectedBackpack];
            ImGui::TextColored({0.95f, 0.75f, 0.25f, 1.f}, "Backpack · Item");
            static char bpNameBuf[64] = {};
            static int bpEditIdx = -1;
            if (bpEditIdx != g_selectedBackpack) {
                bpEditIdx = g_selectedBackpack;
                std::snprintf(bpNameBuf, sizeof(bpNameBuf), "%s", b.name.c_str());
            }
            if (ImGui::InputText("Name", bpNameBuf, sizeof(bpNameBuf)))
                b.name = bpNameBuf;
            ImGui::SliderInt("Colore icona", &b.iconHue, 0, 5);
            {
                int tk = (int)b.tool;
                const char* toolNames[] = { "None", "Sword", "Pistol" };
                if (ImGui::Combo("Tool", &tk, toolNames, 3))
                    b.tool = (BackpackTool)tk;
            }
            if (!b.meshPath.empty())
                ImGui::TextDisabled("Mesh: %s", b.meshPath.c_str());
            bool eq = (g_world.equippedBackpack == g_selectedBackpack);
            if (ImGui::Checkbox("Equipaggiato (default)", &eq)) {
                if (eq) g_world.equippedBackpack = g_selectedBackpack;
                else if (g_world.equippedBackpack == g_selectedBackpack)
                    g_world.equippedBackpack = -1;
            }
            ImGui::TextDisabled("Test: 1-9 equip · Sword/Pistol = tool in mano");
            if (studioAccentButton("Rimuovi dal Backpack", {-1, 32}, {0.75f, 0.2f, 0.18f, 1.f})) {
                int idx = g_selectedBackpack;
                g_world.backpack.erase(g_world.backpack.begin() + idx);
                if (g_world.equippedBackpack == idx) g_world.equippedBackpack = -1;
                else if (g_world.equippedBackpack > idx) g_world.equippedBackpack--;
                g_selectedBackpack = -1;
                bpEditIdx = -1;
            }
        } else if (g_selectedSpawn) {
            ImGui::TextColored({0.95f, 0.75f, 0.25f, 1.f}, "Workspace · SpawnLocation");
            ImGui::DragFloat3("Position", &g_world.spawn.position.x, 0.25f);
            ImGui::DragFloat("Yaw °", &g_world.spawn.yaw, 1.f);
            if (ImGui::Button("Metti spawn qui (camera)", {-1, 32})) {
                g_world.spawn.position = g_cam.pos;
                g_world.spawn.yaw = g_cam.yaw;
            }
            if (ImGui::Button("Metti spawn sul Player", {-1, 28})) {
                g_world.spawn.position = g_playerPos;
                g_world.spawn.yaw = g_playerYaw;
            }
        } else if (g_selectedPlayer) {
            auto& h = g_player.humanoid;
            ImGui::TextColored({0.45f, 0.85f, 1.f, 1.f}, "Players · LocalPlayer");
            ImGui::TextDisabled(h.userRigged ? "Rig R6 attivo" : "Non riggato");
            ImGui::DragFloat3("Position (world)", &g_playerPos.x, 0.1f);
            ImGui::DragFloat("Yaw °", &g_playerYaw, 1.f);
            ImGui::SliderFloat("WalkSpeed", &h.walkSpeed, 4.f, 40.f);
            ImGui::SliderFloat("JumpPower", &h.jumpPower, 20.f, 90.f);
            if (g_selectedPlayerPart >= 0 && g_selectedPlayerPart < (int)h.parts.size()) {
                auto& p = h.parts[g_selectedPlayerPart];
                ImGui::Separator();
                ImGui::TextColored({0.95f, 0.75f, 0.3f, 1.f}, "Part: %s", p.name.c_str());
                ImGui::DragFloat3("Offset", &p.offset.x, 0.05f);
                ImGui::DragFloat3("Size", &p.size.x, 0.05f, 0.2f, 8.f);
                ImGui::DragFloat("Rot X", &p.rotX, 1.f);
                ImGui::DragFloat("Rot Z", &p.rotZ, 1.f);
                ImGui::ColorEdit3("Color", &p.color.x);
            }
            ImGui::Separator();
            ImGui::PushStyleColor(ImGuiCol_Button, {0.15f, 0.65f, 0.40f, 1.f});
            if (ImGui::Button(h.userRigged ? "Apri Rigging Player..." : "Riggare il Player...", {-1, 40}))
                beginRigPlayer();
            ImGui::PopStyleColor();
            ImGui::PushStyleColor(ImGuiCol_Button, {0.85f, 0.45f, 0.20f, 1.f});
            if (ImGui::Button("Play Wave", {-1, 28})) g_animator.play("Wave");
            ImGui::PopStyleColor();
        } else if (g_selectedModel >= 0 && g_selectedModel < (int)g_world.models.size()) {
            auto& m = g_world.models[g_selectedModel];
            ImGui::TextColored({0.45f, 0.75f, 1.f, 1.f}, "Model");
            char nameBuf[64];
            std::snprintf(nameBuf, sizeof(nameBuf), "%s", m.name.c_str());
            if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) m.name = nameBuf;
            ImGui::TextWrapped("%s", m.sourcePath.c_str());
            if (m.rig.fileHasBones)
                ImGui::TextColored({0.3f, 0.85f, 0.45f, 1.f}, "Rig: scheletro nel file");
            else if (m.rig.simpleRig)
                ImGui::TextColored({0.3f, 0.75f, 0.95f, 1.f}, "Rig: Root (semplice)");
            else
                ImGui::TextColored({1.f, 0.55f, 0.25f, 1.f}, "Non riggato");
            // Cosa è stato davvero importato: submesh, texture, materiali
            if (g_selectedModel < (int)g_wsMeshes.size() && g_wsMeshes[g_selectedModel].loaded) {
                const auto& sm = g_wsMeshes[g_selectedModel];
                int withTex = 0, emissive = 0, twoSided = 0, transp = 0;
                for (const auto& s : sm.subs) {
                    if (s.texId) ++withTex;
                    if (s.emissiveTex || s.emissive != glm::vec3(0.f)) ++emissive;
                    if (s.twoSided) ++twoSided;
                    if (s.alpha < 0.99f) ++transp;
                }
                ImGui::Separator();
                ImGui::TextColored({0.55f, 0.85f, 1.f, 1.f}, "Materiali importati");
                ImGui::Text("%d submesh · %d con texture · %d verts",
                            (int)sm.subs.size(), withTex, sm.vertexCount);
                if (emissive) ImGui::TextDisabled("%d emissivi", emissive);
                if (twoSided) ImGui::TextDisabled("%d double-sided", twoSided);
                if (transp)   ImGui::TextDisabled("%d semi-trasparenti", transp);
                int missing = sm.missingTextureCount();
                if (missing > 0) {
                    ImGui::TextColored({1.f, 0.45f, 0.3f, 1.f},
                        "%d texture non trovate (es. %s)", missing,
                        sm.firstMissingTexture().c_str());
                    ImGui::TextDisabled("Mettile accanto al modello o in ./textures");
                }
                if (ImGui::TreeNode("Dettaglio submesh")) {
                    for (const auto& s : sm.subs) {
                        ImGui::BulletText("%s%s%s", s.name.c_str(),
                                          s.matName.empty() ? "" : " · mat: ",
                                          s.matName.c_str());
                        ImGui::SameLine();
                        if (s.texId) ImGui::TextColored({0.3f, 0.85f, 0.45f, 1.f}, "[tex]");
                        else if (!s.missingTex.empty())
                            ImGui::TextColored({1.f, 0.45f, 0.3f, 1.f}, "[tex mancante]");
                        else ImGui::TextDisabled("[solo colore]");
                    }
                    ImGui::TreePop();
                }
                ImGui::Separator();
            }
            bool xformEdited = false;
            xformEdited |= ImGui::DragFloat3("Position", &m.position.x, 0.25f);
            xformEdited |= ImGui::DragFloat3("Rotation °", &m.rotation.x, 1.f);
            xformEdited |= ImGui::DragFloat3("Scale", &m.scale.x, 0.05f, 0.05f, 80.f);
            if (xformEdited) {
                m.anim.reseedIfStatic(m.position, m.rotation, m.scale);
                markPlaceDirty();
            }
            ImGui::ColorEdit3("Tint", &m.tint.x);
            ImGui::PushStyleColor(ImGuiCol_Button, {0.85f, 0.45f, 0.20f, 1.f});
            if (ImGui::Button("Anima questo Model...", {-1, 40}))
                beginAnimateSelectedModel();
            ImGui::PopStyleColor();
            if (!m.rig.hasAnimRig()) {
                ImGui::PushStyleColor(ImGuiCol_Button, {0.15f, 0.65f, 0.40f, 1.f});
                if (ImGui::Button("Crea Rig Root ora", {-1, 32}))
                    openModelAnimEditor(g_selectedModel);
                ImGui::PopStyleColor();
            }
            ImGui::TextDisabled("1) Anima  2) Move  3) Set Frame (K)  4) Play");
            ImGui::PushStyleColor(ImGuiCol_Button, {0.75f, 0.18f, 0.15f, 1.f});
            if (ImGui::Button("Rimuovi Model", {-1, 0})) deleteSelected();
            ImGui::PopStyleColor();
        } else if (g_selectedSound >= 0 && g_selectedSound < (int)g_sounds.sounds.size()) {
            auto& s = g_sounds.sounds[g_selectedSound];
            ImGui::TextColored({0.75f, 0.55f, 1.f, 1.f}, "SoundService · Sound");
            char nameBuf[64];
            std::snprintf(nameBuf, sizeof(nameBuf), "%s", s.name.c_str());
            if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) s.name = nameBuf;
            if (ImGui::InputText("SoundId (path)", g_soundPathBuf, sizeof(g_soundPathBuf)))
                s.soundId = g_soundPathBuf;
            ImGui::Checkbox("Looped", &s.looped);
            ImGui::SliderFloat("Volume", &s.volume, 0.f, 1.f);
            if (ImGui::Button("Play", {100, 0})) {
                s.soundId = g_soundPathBuf;
                if (!g_sounds.play(s))
                    g_exportMsg = g_sounds.lastError;
            }
            ImGui::SameLine();
            if (ImGui::Button("Stop", {100, 0})) g_sounds.stop(s);
            ImGui::TextDisabled("WAV consigliato · %s", s.playing ? "in riproduzione" : "fermo");
            ImGui::PushStyleColor(ImGuiCol_Button, {0.75f, 0.18f, 0.15f, 1.f});
            if (ImGui::Button("Rimuovi Sound", {-1, 0})) deleteSelected();
            ImGui::PopStyleColor();
        } else if (g_selectedPart < 0 || g_selectedPart >= (int)g_world.parts.size()) {
            ImGui::TextColored({0.9f, 0.7f, 0.3f, 1.f}, "Animator");
            ImGui::InputText("Clip", g_animNameBuf, sizeof(g_animNameBuf));
            ImGui::TextDisabled("Idle · Walk · Wave · Dance · Jump");
            if (ImGui::Button("Play Clip", {-1, 32}))
                g_animator.play(g_animNameBuf[0] ? g_animNameBuf : "Wave");
            if (ImGui::Button("Stop / Idle", {-1, 0})) g_animator.stop();
            ImGui::Text("Attivo: %s", g_animator.lastPlayed.c_str());
            ImGui::Separator();
            ImGui::TextDisabled("Nessuna Part/Model selezionata.");
            ImGui::Separator();
            ImGui::TextColored({0.45f, 0.85f, 1.f, 1.f}, "Place / Pubblica");
            ImGui::TextDisabled("Salva · Salva per pubblicare · Pubblica");
            ImGui::InputText("Nome gioco", g_gameNameBuf, sizeof(g_gameNameBuf));
            g_world.placeName = g_gameNameBuf[0] ? g_gameNameBuf : g_world.placeName;
            ImGui::PushStyleColor(ImGuiCol_Text, {1.f, 1.f, 1.f, 1.f});
            if (ImGui::Button("Apri Place...", { -1, 0 })) doOpenPlace();
            if (ImGui::Button("Salva Place", { -1, 0 })) doSavePlace();
            ImGui::PushStyleColor(ImGuiCol_Button, {0.22f, 0.48f, 0.78f, 1.f});
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.30f, 0.58f, 0.90f, 1.f});
            if (ImGui::Button("Salva per pubblicare", { -1, 34 })) doSaveForPublish();
            ImGui::PopStyleColor(2);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Bozza + autosave ogni 30s — pubblichi dopo quando vuoi");
            if (g_publishDraftActive)
                ImGui::TextColored({0.35f, 0.9f, 0.55f, 1.f}, "Autosave bozza publish ON");
            if (ImGui::Button("Salva Progetto", { -1, 0 })) doSaveProject();
            ImGui::SliderInt("Max giocatori", &g_maxPlayers, 1, 32);
            ImGui::TextDisabled("Limite server multiplayer (1–32)");
            {
                int usedQ = 0;
                long long nowQ = (long long)std::time(nullptr);
                std::string cr = g_menu.username[0] ? g_menu.username : "Builder";
                std::string gid = sanitizeGameId(g_gameNameBuf[0] ? g_gameNameBuf : g_world.placeName);
                bool okSlot = canPublishGame(g_games, gid, cr, nowQ, &usedQ);
                ImGui::TextDisabled("Slot giochi: %d / %d (2 mesi)", usedQ, kMaxPublishedGamesPerUser);
                if (!okSlot)
                    ImGui::TextColored({1.f, 0.45f, 0.3f, 1.f}, "Limite raggiunto — aggiorna un gioco esistente");
            }
            ImGui::PushStyleColor(ImGuiCol_Button, {0.12f, 0.62f, 0.42f, 1.f});
            if (ImGui::Button("Pubblica nei Games", {-1, 40})) {
                if (publishCurrentGame())
                    refreshGamesList();
            }
            ImGui::PopStyleColor();
            if (ImGui::Button("Avvia Server dedicato", {-1, 32})) {
                if (publishCurrentGame())
                    refreshGamesList();
                launchDedicatedServer(g_maxPlayers, g_gameNameBuf[0] ? g_gameNameBuf : "Blocky");
            }
            if (!g_publishMsg.empty())
                ImGui::TextWrapped("%s", g_publishMsg.c_str());
            if (!g_exportMsg.empty())
                ImGui::TextWrapped("%s", g_exportMsg.c_str());
            ImGui::Separator();
            ImGui::Text("Place: %s", g_world.placeName.c_str());
            if (!g_placeFilePath.empty())
                ImGui::TextDisabled("%s", g_placeFilePath.c_str());
            if (g_publishDraftActive && !g_publishDraftPath.empty())
                ImGui::TextDisabled("Bozza: %s", g_publishDraftPath.c_str());
            ImGui::SliderFloat("FOV", &g_cam.fov, 40.f, 100.f);
            ImGui::Checkbox("VSync", &g_vsync);
            ImGui::PopStyleColor(); // Text bianco bottoni Place
        } else {
            auto& p = g_world.parts[g_selectedPart];
            ImGui::TextColored({0.45f, 0.85f, 1.f, 1.f}, "Part · %s", partShapeName(p.shape));
            if (ImGui::InputText("Name", g_partNameBuf, sizeof(g_partNameBuf)))
                p.name = g_partNameBuf;
            {
                int sh = (int)p.shape;
                if (sh < 0 || sh >= (int)PartShape::Count) sh = 0;
                const char* shapes[] = { "Block", "Ball", "Cylinder", "Wedge" };
                if (ImGui::Combo("Shape", &sh, shapes, IM_ARRAYSIZE(shapes))) {
                    p.shape = (PartShape)sh;
                    if (p.shape != PartShape::Block) p.useStud = false;
                    markPlaceDirty();
                }
            }

            ImGui::Separator();
            ImGui::Text("Transform");
            ImGui::DragFloat3("Position", &p.position.x, 0.25f);
            ImGui::DragFloat3("Size", &p.size.x, 0.25f, 0.5f, 80.f);
            ImGui::DragFloat3("Rotation °", &p.rotation.x, 1.f);
            if (g_tool == StudioTool::Move)
                ImGui::TextWrapped("Muovi: frecce RGB. Snap 0.25 · Shift=0.05 · Ctrl=libero.");
            else if (g_tool == StudioTool::Scale)
                ImGui::TextWrapped("Scala: cubetti sulle facce. Snap 0.25 · Shift=0.1 · Ctrl=libero.");
            else if (g_tool == StudioTool::Rotate)
                ImGui::TextWrapped("Ruota: anelli RGB + tick 15°. Snap 5° · Shift=1° · Ctrl=libero.");
            else
                ImGui::TextWrapped("Seleziona: scegli Muovi / Scala / Ruota dalla toolbar.");
            if (ImGui::Button("Allarga X")) p.size.x = std::min(80.f, p.size.x + 1.f);
            ImGui::SameLine();
            if (ImGui::Button("Allarga Z")) p.size.z = std::min(80.f, p.size.z + 1.f);
            ImGui::SameLine();
            if (ImGui::Button("Alza Y")) p.size.y = std::min(80.f, p.size.y + 1.f);
            if (ImGui::Button("Ingrandisci x1.25")) scaleSelected(1.25f);
            ImGui::SameLine();
            if (ImGui::Button("Riduci x0.8")) scaleSelected(0.8f);

            ImGui::Separator();
            drawPartColorPicker(p.color);
            ImGui::Separator();
            ImGui::Checkbox("Anchored", &p.anchored);
            if (p.anchored)
                ImGui::TextDisabled("Anchored = ferma in Play (in Studio puoi comunque muovere)");
            ImGui::Checkbox("CanCollide", &p.canCollide);
            if (!p.canCollide)
                ImGui::TextColored({1.f, 0.55f, 0.2f, 1.f}, "No Collision — solo questa Part");
            else
                ImGui::TextDisabled("Collisione attiva solo su questa Part");
            ImGui::Checkbox("Stud texture", &p.useStud);

            ImGui::Separator();
            ImGui::TextColored({0.85f, 0.75f, 0.35f, 1.f}, "Material");
            {
                int matIdx = (int)p.material;
                if (matIdx < 0 || matIdx >= (int)PartMaterial::Count) matIdx = 0;
                const char* mats[] = {
                    "Plastic", "SmoothPlastic", "Neon", "Metal", "Wood",
                    "Concrete", "Grass", "Glass", "Ice", "Brick", "ForceField"
                };
                if (ImGui::Combo("Material##mat", &matIdx, mats, IM_ARRAYSIZE(mats)))
                    p.setMaterial((PartMaterial)matIdx);
            }
            ImGui::SliderFloat("Emissive", &p.emissive, 0.f, 2.f, "%.2f");
            ImGui::Checkbox("PointLight (illumina intorno)", &p.pointLight);
            if (p.pointLight) {
                ImGui::SliderFloat("Light Range", &p.lightRange, 4.f, 60.f, "%.0f");
                ImGui::SliderFloat("Light Brightness", &p.lightBrightness, 0.2f, 4.f, "%.2f");
                ImGui::TextDisabled("Colore luce = colore Part · max 8 luci attive");
            }

            ImGui::Separator();
            ImGui::TextColored({1.f, 0.45f, 0.3f, 1.f}, "Distruzione");
            ImGui::Checkbox("CanDestroy", &p.canDestroy);
            ImGui::Checkbox("BoomOnTouch", &p.boomOnTouch);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("In Test: 1 avvicinamento = boom");
            ImGui::SliderFloat("BoomAutoSecs", &p.boomAutoSecs, 0.f, 15.f, "%.1f s");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("0 = solo tocco; >0 = esplode da solo dopo N secondi in Test");
            if (!p.canDestroy)
                ImGui::TextDisabled("Destroy / Explode / spada non la rimuovono");
            else {
                ImGui::SliderFloat("Health", &p.health, 1.f, 500.f, "%.0f");
                p.maxHealth = std::max(p.maxHealth, p.health);
                if (ImGui::Button("Explodi ora", {180, 0})) {
                    glm::vec3 boomPos = p.position;
                    if (g_studioTesting || g_screen == AppScreen::Playing)
                        doBoomAt(boomPos, 14.f, 55.f, 24);
                    else
                        g_particles.spawnExplosion(boomPos, 14.f, 24);
                    g_exportMsg = "BOOM!";
                    if ((g_studioTesting || g_screen == AppScreen::Playing) && p.canDestroy) {
                        int idx = g_selectedPart;
                        selectPart(-1);
                        if (idx >= 0 && idx < (int)g_world.parts.size())
                            g_world.parts.erase(g_world.parts.begin() + idx);
                    } else if (!g_studioTesting && g_screen == AppScreen::Studio) {
                        g_exportMsg += " (solo FX in editor — Test per danno/distruggi)";
                    }
                }
            }

            ImGui::Separator();
            ImGui::PushStyleColor(ImGuiCol_Button, {0.75f, 0.18f, 0.15f, 1.f});
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.90f, 0.28f, 0.20f, 1.f});
            if (ImGui::Button("Rimuovi Part", {-1, 0})) deleteSelected();
            ImGui::PopStyleColor(2);

            ImGui::Separator();
            ImGui::TextColored({0.9f, 0.7f, 0.2f, 1.f}, "Script del blocco");
            ImGui::TextWrapped(
                "Non e' Lua vanilla: usi l'API Blocky (tipo Roblox). "
                "Esplosione = part:Explode(). Apri View → Guida Lua Blocky.");
            if (p.script.empty()) {
                ImGui::TextDisabled("Nessuno script — prova un preset.");
                ImGui::PushStyleColor(ImGuiCol_Button, {0.20f, 0.65f, 0.85f, 1.f});
                if (ImGui::Button(LB("btn.jump_block", "jmpblk"), {-1, 36})) {
                    p.script = makeJumpBlockScript(p);
                    syncPartEditors();
                    g_editingPartScript = true;
                    g_luaRunMsg = "Script salto — Test (F5)";
                }
                ImGui::PopStyleColor();
                ImGui::PushStyleColor(ImGuiCol_Button, {0.85f, 0.25f, 0.12f, 1.f});
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.95f, 0.35f, 0.18f, 1.f});
                if (ImGui::Button(LB("btn.explode_block", "expblk"), {-1, 36})) {
                    p.script = makeExplodeBlockScript(p);
                    p.color = {0.85f, 0.2f, 0.12f};
                    p.boomOnTouch = false;
                    p.boomAutoSecs = 0.f;
                    p.canDestroy = true;
                    p.canCollide = false;
                    syncPartEditors();
                    g_editingPartScript = true;
                    g_luaRunMsg = "Script esplosione Lua (monete + boom) — Test (F5)";
                }
                ImGui::PopStyleColor(2);
                ImGui::PushStyleColor(ImGuiCol_Button, {0.12f, 0.72f, 0.55f, 1.f});
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.20f, 0.85f, 0.65f, 1.f});
                if (ImGui::Button("Script Neon (Material)", {-1, 36})) {
                    p.script = makeNeonBlockScript(p);
                    p.setMaterial(PartMaterial::Neon);
                    p.pointLight = true;
                    p.color = {0.25f, 1.f, 0.85f};
                    syncPartEditors();
                    g_editingPartScript = true;
                    g_luaRunMsg = "Script Neon — Test (F5) per pulse + luce";
                }
                ImGui::PopStyleColor(2);
                ImGui::PushStyleColor(ImGuiCol_Button, {0.20f, 0.55f, 0.90f, 1.f});
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.30f, 0.65f, 1.0f, 1.f});
                if (ImGui::Button("Script Stamina (sprint)", {-1, 36})) {
                    applyStaminaToSelectedPart();
                }
                ImGui::PopStyleColor(2);
                ImGui::PushStyleColor(ImGuiCol_Button, {0.85f, 0.65f, 0.12f, 1.f});
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.95f, 0.75f, 0.20f, 1.f});
                ImGui::PushStyleColor(ImGuiCol_Text, {0.1f, 0.1f, 0.1f, 1.f});
                if (ImGui::Button(LB("btn.add_script", "addsc"), {-1, 28}))
                    addScriptToSelected();
                ImGui::PopStyleColor(3);
            } else {
                g_editingPartScript = true;
                ImGui::InputTextMultiline("##partscript", g_partScriptBuf, sizeof(g_partScriptBuf),
                    {-1, 140}, ImGuiInputTextFlags_AllowTabInput);
                p.script = g_partScriptBuf;
                if (ImGui::SmallButton("Inserisci Explode")) {
                    std::string cur = g_partScriptBuf;
                    cur += "\npart:Explode()\n";
                    std::snprintf(g_partScriptBuf, sizeof(g_partScriptBuf), "%s", cur.c_str());
                    p.script = g_partScriptBuf;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Inserisci Touched→Explode")) {
                    std::snprintf(g_partScriptBuf, sizeof(g_partScriptBuf), "%s",
                        makeExplodeBlockScript(p).c_str());
                    p.script = g_partScriptBuf;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Apri guida"))
                    g_showLuaGuide = true;
                if (ImGui::SmallButton("Carica Stamina (sprint)"))
                    applyStaminaToSelectedPart();
                ImGui::TextDisabled("%s", L("hint.scripts_test"));
                if (ImGui::Button(LB("btn.save_script", "savesc")))
                    p.script = g_partScriptBuf;
                ImGui::PushStyleColor(ImGuiCol_Button, {0.75f, 0.18f, 0.15f, 1.f});
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.90f, 0.28f, 0.20f, 1.f});
                if (ImGui::Button(LB("btn.remove_script", "rmsc"), {-1, 0}))
                    removeScriptFromSelected();
                ImGui::PopStyleColor(2);
            }
            if (!g_luaRunMsg.empty()) {
                ImGui::Separator();
                ImGui::TextWrapped("%s", g_luaRunMsg.c_str());
            }
        }
        ImGui::End();
        ImGui::PopStyleColor(6);
    }

    if (g_showScript) {
        ImGui::SetNextWindowPos({12, (float)g_h - 280}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({480, 260}, ImGuiCond_FirstUseEver);
        ImGui::Begin("Script (Safe Bytecode)", &g_showScript);
        ImGui::TextColored({0.45f, 0.9f, 0.7f, 1.f}, "Logica di gioco = Lua Blocky → .bbc sigillato");
        ImGui::TextWrapped(
            "Lua 5.4 da solo NON ha Explode / Particles / Touched. "
            "Quelle sono funzioni Blocky (come Roblox). Apri la Guida Lua.");
        if (ImGui::Button("Apri Guida Lua Blocky"))
            g_showLuaGuide = true;
        ImGui::SameLine();
        ImGui::TextDisabled("OpenGL = solo grafica");
        ImGui::TextWrapped("Place: %s", g_world.placeName.c_str());
        ImGui::InputTextMultiline("##luaedit", g_luaEditBuf, sizeof(g_luaEditBuf),
            {-1, 100}, ImGuiInputTextFlags_AllowTabInput);
        ImGui::TextDisabled("%s", L("hint.scripts_test"));
        if (ImGui::Button(LB("btn.compile_bbc", "bbc"))) {
            std::string id = sanitizeGameId(g_gameNameBuf[0] ? g_gameNameBuf : g_world.placeName);
            std::string dir = BlockyProject::joinPath(BlockyProject::projectsDir(), id);
            BlockyProject::mkDir(dir);
            BlockyProject::mkDir(BlockyProject::joinPath(dir, "scripts"));
            std::string out = BlockyProject::joinPath(dir, "scripts\\main.bbc");
            std::string err;
            if (LuaRuntime::compileSealedFile(g_luaEditBuf, id, out, err)) {
                // salva anche sorgente per edit
                BlockyProject::writeTextFile(
                    BlockyProject::joinPath(dir, "scripts\\main.lua"), g_luaEditBuf);
                g_luaRunMsg = "Safe Bytecode OK: " + out;
                g_exportMsg = g_luaRunMsg;
            } else {
                g_luaRunMsg = "Compile fallita: " + err;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Esempio AnimateFrame")) {
            std::string path = resolveScriptPath("scripts/animate_example.lua");
            std::string src = readTextFile(path);
            if (src.empty()) {
                src =
                    "-- AnimateFrame(frame, position)\n"
                    "AnimateFrame(0, Vector3.new(0, 2, 0))\n"
                    "AnimateFrame(15, Vector3.new(4, 5, 0))\n"
                    "AnimateFrame(30, Vector3.new(0, 2, 0))\n"
                    "print(\"AnimateFrame impostato — apri Animator e Play\")\n";
            }
            std::snprintf(g_luaEditBuf, sizeof(g_luaEditBuf), "%s", src.c_str());
            g_luaRunMsg = "Caricato esempio AnimateFrame (serve un Model)";
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("API completa (Test)", &g_loadApiCompleta)) {
            if (g_loadApiCompleta)
                g_luaRunMsg = "Al Test (F5) carica lua_api_completo.lua — pad effetti a z=12";
        }
        ImGui::SameLine();
        if (ImGui::Button("Carica stamina.lua")) {
            std::string src = loadStaminaScriptSource();
            if (!src.empty()) {
                std::snprintf(g_luaEditBuf, sizeof(g_luaEditBuf), "%s", src.c_str());
                g_loadStamina = true;
                g_luaRunMsg = "Stamina caricata — premi Test (F5), poi CTRL+WASD";
            } else {
                g_loadStamina = false;
                g_luaRunMsg = "File stamina.lua non trovato in assets/scripts";
            }
        }
        if (g_loadStamina) {
            ImGui::SameLine();
            if (ImGui::SmallButton("Rimuovi stamina")) {
                g_loadStamina = false;
                g_luaRunMsg = "Stamina disattivata";
            }
        }
        ImGui::SameLine();
        ImGui::TextDisabled("part:Explode · Particles · Humanoid · barre UI · raycast...");
        if (!g_luaRunMsg.empty()) ImGui::TextWrapped("%s", g_luaRunMsg.c_str());
        if (!g_lua.lastError.empty())
            ImGui::TextColored({1,.4f,.4f,1}, "%s", g_lua.lastError.c_str());
        ImGui::End();
    }

    if (g_showLuaGuide) {
        ImGui::SetNextWindowSize({520, 420}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos({(float)g_w - 540.f, 90.f}, ImGuiCond_FirstUseEver);
        ImGui::Begin("Guida Lua Blocky", &g_showLuaGuide);
        ImGui::TextColored({1.f, 0.85f, 0.35f, 1.f}, "Non e' Lua standard");
        ImGui::TextWrapped(
            "Il linguaggio e' Lua 5.4, ma Explode / Particles / Touched / Humanoid "
            "sono API Blocky (stile Roblox). Senza di esse non puoi far esplodere nulla.");
        ImGui::Separator();
        ImGui::TextColored({0.45f, 0.9f, 1.f, 1.f}, "Esplosione (1 riga)");
        ImGui::BulletText("part:Explode()     — boom + sparisce");
        ImGui::BulletText("part:Explode(60)   — boom piu' grosso");
        ImGui::BulletText("part:Destroy()     — rimuove (se CanDestroy)");
        ImGui::BulletText("part.Material      — Neon/Metal/Glass/Wood/...");
        ImGui::BulletText("part.CanDestroy    — true/false");
        ImGui::BulletText("game.Particles:Explode(pos) — solo effetti");
        ImGui::Separator();
        ImGui::TextColored({0.45f, 0.9f, 1.f, 1.f}, "Part (script sul blocco)");
        ImGui::BulletText("local part = script.Parent");
        ImGui::BulletText("part.Position / Size / Orientation");
        ImGui::BulletText("part.Touched:Connect(function() ... end)");
        ImGui::BulletText("part:Destroy()  ·  part:Explode()");
        ImGui::Separator();
        ImGui::TextColored({0.45f, 0.9f, 1.f, 1.f}, "Effetti");
        ImGui::BulletText("game.Particles:Sparks(pos, n)");
        ImGui::BulletText("game.Particles:Smoke(pos, n)");
        ImGui::BulletText("game.Particles:Emit(pos, n, Color3.new(r,g,b))");
        ImGui::BulletText("game.Particles:Coins(pos, n)");
        ImGui::Separator();
        ImGui::TextColored({0.45f, 0.9f, 1.f, 1.f}, "Tempo / loop");
        ImGui::BulletText("task.wait(1)  ·  task.spawn(fn)  ·  task.delay(t, fn)");
        ImGui::BulletText("game.RunService.Heartbeat:Connect(function(dt) end)");
        ImGui::Separator();
        ImGui::TextColored({0.45f, 0.9f, 1.f, 1.f}, "Player");
        ImGui::BulletText("Humanoid.SetWalkSpeed(16)");
        ImGui::BulletText("Humanoid.SetJumpPower(50)");
        ImGui::BulletText("Humanoid.SetJumpEnabled(false)");
        ImGui::Separator();
        ImGui::TextColored({0.45f, 0.9f, 1.f, 1.f}, "Mondo");
        ImGui::BulletText("workspace:Raycast(origin, direction)");
        ImGui::BulletText("workspace:FindFirstChild(\"Nome\")");
        ImGui::BulletText("Vector3.new(x,y,z)  ·  Color3.new(r,g,b)");
        ImGui::Separator();
        ImGui::TextDisabled("Preset Properties: Blocco esplosivo / Blocco che salta");
        ImGui::TextDisabled("Gli script girano solo in Test (F5) o Play.");
        if (ImGui::Button("Chiudi", {-1, 28}))
            g_showLuaGuide = false;
        ImGui::End();
    }

    if (g_showPlatformPanel) {
        ImGui::SetNextWindowPos({(float)g_w * 0.5f - 200.f, (float)g_h - 250}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({400, 230}, ImGuiCond_FirstUseEver);
        ImGui::Begin("Blocky Game Engine", &g_showPlatformPanel);
        ImGui::TextColored({0.45f, 0.85f, 1.f, 1.f}, "%s", kAppBrand);
        ImGui::TextDisabled("Editor di giochi — parti, Lua, publish su Blocky");
        ImGui::Separator();
        ImGui::Text("Piattaforma: %s",
                    g_engine.platform == TargetPlatform::Unknown
                        ? "PC" : targetPlatformName(g_engine.platform));
        ImGui::Text("%s: %s", L("lang"), locLangLabel(loc().lang));
        drawLanguageSelector("enginepanel_lang");
        ImGui::Text("Graphics:   %s", gfxApiName(g_engine.gfxApi));
        ImGui::Text("Profilo:    %s", g_engine.quality.name);
        ImGui::Separator();
        ImGui::TextDisabled("Ottimizza per questa sessione:");
        if (ImGui::Button("Low"))    g_engine.setPreset(QualityPreset::Low);
        ImGui::SameLine();
        if (ImGui::Button("Medium")) g_engine.setPreset(QualityPreset::Medium);
        ImGui::SameLine();
        if (ImGui::Button("High"))   g_engine.setPreset(QualityPreset::High);
        ImGui::SameLine();
        if (ImGui::Button("Ultra"))  g_engine.setPreset(QualityPreset::Ultra);
        ImGui::SameLine();
        if (ImGui::Button("Default")) g_engine.setPreset(QualityPreset::PlatformDefault);
        ImGui::SliderFloat("Render scale", &g_engine.quality.renderScale, 0.5f, 1.25f, "%.2f");
        ImGui::SliderFloat("Draw distance", &g_engine.quality.maxDrawDistance, 50.f, 1200.f, "%.0f");
        ImGui::SliderInt("Max parts", &g_engine.quality.maxVisibleParts, 100, 12000);
        ImGui::SliderFloat("Physics Hz", &g_engine.quality.physicsHz, 20.f, 120.f, "%.0f");
        ImGui::SliderInt("Lua budget us", &g_engine.quality.luaBudgetUs, 500, 12000);
        ImGui::Checkbox("Stud textures", &g_engine.quality.studTextures);
        ImGui::Checkbox("VSync", &g_engine.quality.vsync);
        g_vsync = g_engine.quality.vsync;
        ImGui::Separator();
        ImGui::TextColored({1.f, 0.55f, 0.35f, 1.f}, "AntiCheat / AntiTamper");
        ImGui::Checkbox("Enabled", &antiCheat().cfg.enabled);
        ImGui::Checkbox("Anti-tamper", &antiCheat().cfg.antiTamper);
        ImGui::Checkbox("Anti-debug", &antiCheat().cfg.antiDebug);
        ImGui::Checkbox("Kill tool cheat", &antiCheat().cfg.killHostileProcs);
        if (ImGui::IsItemDeactivatedAfterEdit() || ImGui::IsItemEdited())
            acSealConfig();
        ImGui::Text("Strikes: %d  ·  Tamper: %d  ·  Kill: %d",
                    antiCheat().strikes, antiCheat().tamperHits, antiCheat().hostileKills);
        ImGui::TextDisabled("Seal %s · HB %u",
            antiCheat().integrityOk ? "OK" : "FAIL",
            (unsigned)antiCheat().heartbeat);
        if (!antiCheat().lastReason.empty())
            ImGui::TextDisabled("%s", antiCheat().lastReason.c_str());
        if (antiCheat().debuggerSeen)
            ImGui::TextColored({1.f, 0.25f, 0.2f, 1.f}, "DEBUGGER RILEVATO");
        if (antiCheat().softKick || (float)glfwGetTime() < antiCheat().hardMuteUntil)
            ImGui::TextColored({1.f, 0.3f, 0.3f, 1.f}, "MUTE / SOFT-KICK attivo");
        if (ImGui::SmallButton("Reset AntiCheat")) { acReset(); acSealConfig(); }
        ImGui::SameLine();
        if (ImGui::SmallButton("Re-seal")) acSealConfig();
        ImGui::TextDisabled("Moderazione: odio/swear/spam/mute · Lua sandbox");
        ImGui::TextDisabled("PC · Mobile · iOS · PS5 — Blocky su ogni piattaforma");
        ImGui::End();
    }

    if (g_showChat) {
        ImGui::SetNextWindowPos({(float)g_w - 340, (float)g_h - 280}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({320, 250}, ImGuiCond_FirstUseEver);
        ImGui::Begin("Chat", &g_showChat);
        ImGui::TextColored({0.45f, 0.85f, 1.f, 1.f}, "Blocky Chat");
        ImGui::TextDisabled("Parolacce → ****  ·  Report nel tab Chat del menu");
        ImGui::Separator();
        ImGui::BeginChild("##chatlog", {0, -36}, true);
        for (const auto& line : g_menu.chatLines)
            ImGui::TextWrapped("%s", line.c_str());
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.f)
            ImGui::SetScrollHereY(1.f);
        ImGui::EndChild();
        bool send = ImGui::InputText("##msg", g_studioChatBuf, sizeof(g_studioChatBuf),
            ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if ((ImGui::Button("Invia") || send) && g_studioChatBuf[0]) {
            sendPlayChat(g_studioChatBuf);
            g_studioChatBuf[0] = 0;
        }
        ImGui::End();
    }

    // Nome Part sopra la selezione (colore 3D invariato)
    drawSelectedPartLabel();
}

static bool importAccessoryFromPath(const std::string& path) {
    if (path.empty()) {
        g_catalog.status = "Percorso vuoto";
        return false;
    }
    auto mod = Moderation::checkAvatarOrModelName(path);
    if (mod.verdict != Moderation::Verdict::Ok) {
        g_catalog.status = mod.reason;
        g_menu.chatLines.push_back(mod.reason);
        return false;
    }
    AvatarAccessory acc;
    acc.slot = g_catalog.pendingSlot;
    acc.replacesBody = g_catalog.pendingAsBodyReplace;
    acc.offset = acc.replacesBody ? bodyReplaceOffsetForSlot(acc.slot)
                                  : defaultOffsetForSlot(acc.slot);
    acc.scale = {1, 1, 1};
    auto slash = path.find_last_of("\\/");
    acc.name = (slash == std::string::npos) ? path : path.substr(slash + 1);
    // Copia subito in libreria Blocky → resta equipaggiato anche senza Pubblica
    std::string lib = copyModelToLibrary(path, acc.name);
    acc.sourcePath = lib.empty() ? path : lib;
    if (!acc.mesh.load(acc.sourcePath)) {
        if (acc.sourcePath != path && acc.mesh.load(path))
            acc.sourcePath = path;
        else {
            g_catalog.status = "Import fallito (formato o file): " + path;
            return false;
        }
    }
    // Una sola sostituzione per slot: togli le replace precedenti sullo stesso limb
    if (acc.replacesBody) {
        for (int i = (int)g_catalog.items.size() - 1; i >= 0; --i) {
            if (g_catalog.items[i].slot == acc.slot && g_catalog.items[i].replacesBody)
                g_catalog.items.erase(g_catalog.items.begin() + i);
        }
        g_catalog.hideDefaultPart[(int)acc.slot] = true;
    }
    g_catalog.items.push_back(std::move(acc));
    g_catalog.selected = (int)g_catalog.items.size() - 1;
    const auto& back = g_catalog.items.back();
    upsertOwnedAvatarInventory(back);
    saveShopItems(g_menu);
    saveEquippedAvatar(g_catalog);
    syncEquippedToMenu();
    g_catalog.status = back.replacesBody
        ? (std::string("Equipaggiata — ") + back.name + " sostituisce " + avatarSlotName(back.slot)
           + " (anche senza Pubblica)")
        : (std::string("Equipaggiato — ") + back.name + " su " + avatarSlotName(back.slot)
           + " (" + std::to_string(back.mesh.vertexCount) + " vertici)");
    g_catalog.camDist = 10.f;
    g_catalog.camPitch = -12.f;
    offerBackpackForGunImport(path.empty() ? back.name : path);
    return true;
}

static void* publisherHwnd() {
#ifdef _WIN32
    GLFWwindow* w = glfwGetCurrentContext();
    return w ? (void*)glfwGetWin32Window(w) : nullptr;
#else
    return nullptr;
#endif
}

/** Studio: importa animazione Player → pubblica → AnimationId da usare in script. */
static void drawStudioPlayerAnimPanel() {
    if (!g_showPlayerAnimPublisher) return;
    ImGui::SetNextWindowPos({(float)g_w * 0.5f - 220.f, 90.f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({440, 520}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Animazioni Player — Pubblica ID##playerAnimPub",
                      &g_showPlayerAnimPublisher)) {
        ImGui::End();
        return;
    }

    ImGui::TextColored({0.95f, 0.75f, 0.25f, 1.f}, "Flusso Studio");
    ImGui::TextWrapped(
        "1) (Opzionale) Scarica R6 e anima in Blender/Maya\n"
        "2) Importa FBX/GLB con keyframe\n"
        "3) Scegli AnimationId → Pubblica\n"
        "4) Usa l'ID nello script: game.Animator:Play(\"Id\")\n\n"
        "Pistola: basta animare Right Arm (1 arto ok). "
        "In Blender rinomina il bone/object: Right Arm");

    if (!g_catalog.status.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, {0.55f, 0.9f, 1.f, 1.f});
        ImGui::TextWrapped("%s", g_catalog.status.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Button, {0.15f, 0.48f, 0.88f, 1.f});
    if (ImGui::Button("1. Scarica Player R6 default (OBJ)", {-1, 36}))
        downloadDefaultR6PlayerModel();
    ImGui::PopStyleColor();

    ImGui::InputText("Nome Action/clip (Blender/Maya)", g_catalog.walkClipPrefBuf,
                     sizeof(g_catalog.walkClipPrefBuf));
    ImGui::TextDisabled("Es. FancyWalk, Shoot, WalkingAnimation...");

    ImGui::PushStyleColor(ImGuiCol_Button, {0.55f, 0.28f, 0.85f, 1.f});
    if (ImGui::Button("2. Importa animazione (FBX / GLB)", {-1, 40}))
        importWalkingAnimationFromFile();
    ImGui::PopStyleColor();

    if (g_catalog.hasPendingWalk && g_catalog.pendingWalk.valid()) {
        ImGui::TextColored({0.4f, 1.f, 0.55f, 1.f}, "Importata: %s · %d keyframe",
                           g_catalog.pendingWalk.clipName.c_str(),
                           (int)g_catalog.pendingWalk.keys.size());
        if (!g_catalog.pendingClipNames.empty()) {
            ImGui::TextDisabled("Clip nel file:");
            for (const auto& n : g_catalog.pendingClipNames)
                ImGui::BulletText("%s", n.c_str());
        }
        ImGui::InputText("AnimationId da pubblicare", g_catalog.walkNameBuf,
                         sizeof(g_catalog.walkNameBuf));
        ImGui::Checkbox("Imposta come walk di default", &g_catalog.pendingAsWalkDefault);
        if (g_catalog.pendingAsWalkDefault && Animator::looksLikeToolAnim(g_catalog.pendingWalk))
            ImGui::TextColored({1.f, 0.75f, 0.3f, 1.f},
                "Attenzione: sembra un'anim pistola/tool — meglio NON usarla come walk");
        ImGui::Checkbox("Anteprima sul player", &g_catalog.previewWalk);

        ImGui::PushStyleColor(ImGuiCol_Button, {0.12f, 0.62f, 0.42f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.18f, 0.75f, 0.52f, 1.f});
        if (ImGui::Button("3. Pubblica → ottieni AnimationId", {-1, 48}))
            releaseWalkingAnimation();
        ImGui::PopStyleColor(2);
    }

    if (g_lastPublishedAnimId[0]) {
        ImGui::Separator();
        ImGui::TextColored({0.35f, 1.f, 0.55f, 1.f}, "AnimationId pubblicato");
        ImGui::PushStyleColor(ImGuiCol_ChildBg, {0.12f, 0.18f, 0.14f, 1.f});
        ImGui::BeginChild("##pubIdBox", {0, 72}, true);
        ImGui::SetWindowFontScale(1.35f);
        ImGui::Text("%s", g_lastPublishedAnimId);
        ImGui::SetWindowFontScale(1.f);
        ImGui::TextDisabled("Incolla questo ID nello script");
        ImGui::EndChild();
        ImGui::PopStyleColor();

        if (ImGui::Button("Copia AnimationId", {-1, 32}))
            ImGui::SetClipboardText(g_lastPublishedAnimId);
        if (ImGui::Button("Copia: Play(\"...\")", {-1, 28})) {
            std::string snip = std::string("game.Animator:Play(\"")
                + g_lastPublishedAnimId + "\")";
            ImGui::SetClipboardText(snip.c_str());
        }
        if (ImGui::Button("Copia: SetWalkAnim(\"...\")", {-1, 28})) {
            std::string snip = std::string("game.Animator:SetWalkAnim(\"")
                + g_lastPublishedAnimId + "\")";
            ImGui::SetClipboardText(snip.c_str());
        }
        if (ImGui::Button("Copia: SetJumpAnim(\"...\")", {-1, 28})) {
            std::string snip = std::string("game.Animator:SetJumpAnim(\"")
                + g_lastPublishedAnimId + "\")";
            ImGui::SetClipboardText(snip.c_str());
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Prova Play"))
            g_animator.play(g_lastPublishedAnimId);
    }

    if (!g_catalog.walkClips.empty()) {
        ImGui::Separator();
        ImGui::Text("Tutti gli ID pubblicati");
        ImGui::BeginChild("##pubList", {0, 140}, true);
        for (int i = 0; i < (int)g_catalog.walkClips.size(); ++i) {
            const auto& c = g_catalog.walkClips[i];
            ImGui::PushID(3100 + i);
            const char* showId = c.displayName.empty() ? c.id.c_str() : c.displayName.c_str();
            ImGui::Text("%s", showId);
            ImGui::SameLine();
            if (ImGui::SmallButton("Copia"))
                ImGui::SetClipboardText(showId);
            ImGui::SameLine();
            if (ImGui::SmallButton("Play"))
                g_animator.play(showId);
            ImGui::SameLine();
            if (ImGui::SmallButton("Walk")) {
                g_catalog.equippedWalkId = c.id;
                syncEquippedWalkToAnimator();
                r6SaveLibrary(g_catalog.walkClips, g_catalog.equippedWalkId);
                animLibrary().setDefaultWalk(showId);
                std::snprintf(g_lastPublishedAnimId, sizeof(g_lastPublishedAnimId), "%s", showId);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Jump")) {
                animLibrary().setDefaultJump(c.id);
                g_animator.setJumpAnimId(c.id);
                std::snprintf(g_lastPublishedAnimId, sizeof(g_lastPublishedAnimId), "%s", showId);
            }
            ImGui::PopID();
        }
        ImGui::EndChild();
    }

    ImGui::Separator();
    ImGui::TextDisabled("Esempio script:");
    ImGui::TextWrapped(
        "game.Animator:Play(\"Shoot\")\n"
        "game.Animator:SetWalkAnim(\"FancyWalk\")\n"
        "game.Animator:SetJumpAnim(\"ClassicJump\")");
    ImGui::End();
}

static void drawAvatarPublisherUi() {
    const float menuH = 8.f;
    ImGui::SetNextWindowPos({12, menuH});
    ImGui::SetNextWindowSize({360, (float)g_h - 24}, ImGuiCond_Always);
    ImGui::Begin("Avatar Catalog", nullptr, ImGuiWindowFlags_NoCollapse);

    ImGui::TextColored({0.25f, 0.55f, 0.95f, 1.f}, "BLOCKY");
    ImGui::SameLine();
    ImGui::TextColored({0.95f, 0.75f, 0.25f, 1.f}, kIsGameEngine ? "Game Engine" : "");
    ImGui::TextColored({0.95f, 0.95f, 0.98f, 1.f}, "Avatar Catalog");
    ImGui::TextDisabled("Importa GLB/FBX · puoi NASCONDERE i cubi R6 e mettere braccia custom");
    ImGui::Separator();

    if (!g_catalog.status.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, {0.55f, 0.85f, 1.f, 1.f});
        ImGui::TextWrapped("%s", g_catalog.status.c_str());
        ImGui::PopStyleColor();
    }

    // —— Nascondi / elimina parti corpo default ——
    ImGui::Spacing();
    ImGui::TextColored({1.f, 0.75f, 0.35f, 1.f}, "Parti del corpo (R6)");
    ImGui::TextDisabled("Togli i cubi di default, poi importa la parte custom");
    {
        bool changed = false;
        const char* hideLabs[] = {
            "Nascondi Head", "Nascondi Torso",
            "Nascondi Braccio SX", "Nascondi Braccio DX",
            "Nascondi Gamba SX", "Nascondi Gamba DX"
        };
        for (int i = 0; i < (int)AvatarSlot::Count; ++i) {
            if (ImGui::Checkbox(hideLabs[i], &g_catalog.hideDefaultPart[i]))
                changed = true;
        }
        if (ImGui::Button("Nascondi tutte", {160, 28})) {
            for (int i = 0; i < (int)AvatarSlot::Count; ++i)
                g_catalog.hideDefaultPart[i] = true;
            changed = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Mostra tutte##showbody", {160, 28})) {
            for (int i = 0; i < (int)AvatarSlot::Count; ++i)
                g_catalog.hideDefaultPart[i] = false;
            changed = true;
        }
        if (changed) {
            saveEquippedAvatar(g_catalog);
            g_catalog.status = "Parti corpo aggiornate";
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Checkbox("Prossimo import = SOSTITUISCE la parte (braccio/gamba custom)",
                    &g_catalog.pendingAsBodyReplace);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("ON: nasconde il cubo R6 e mette la mesh al posto del limb.\nOFF: accessorio sopra (cappello, arma…).");

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button, {0.12f, 0.62f, 0.42f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.18f, 0.75f, 0.52f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_Text, {1, 1, 1, 1});
    if (ImGui::Button(g_catalog.pendingAsBodyReplace ? "Aggiungi parte custom" : "Aggiungi modello", {-1, 52})) {
        std::string path;
        if (openModelFileDialog(path, publisherHwnd())) {
            g_catalog.pendingImportPath = path;
            g_catalog.wizardStep = 0;
            if (path.size() < sizeof(g_importPathBuf))
                std::snprintf(g_importPathBuf, sizeof(g_importPathBuf), "%s", path.c_str());
            auto slash = path.find_last_of("\\/");
            std::string name = (slash == std::string::npos) ? path : path.substr(slash + 1);
            g_catalog.status = g_catalog.pendingAsBodyReplace
                ? ("Parte custom: " + name + " — scegli quale limb sostituire")
                : ("File: " + name + " — scegli dove metterlo");
        } else {
            g_catalog.status = "Import annullato";
        }
    }
    ImGui::PopStyleColor(3);

    ImGui::Spacing();
    ImGui::Separator();

    if (g_catalog.wizardStep == 0) {
        if (!g_catalog.pendingImportPath.empty()) {
            ImGui::TextColored({0.4f, 1.f, 0.55f, 1.f},
                g_catalog.pendingAsBodyReplace ? "Parte custom — quale limb?" : "Modello — dove lo metti?");
            auto slash = g_catalog.pendingImportPath.find_last_of("\\/");
            std::string name = (slash == std::string::npos)
                ? g_catalog.pendingImportPath
                : g_catalog.pendingImportPath.substr(slash + 1);
            ImGui::TextWrapped("%s", name.c_str());
            ImGui::Spacing();
        } else {
            ImGui::Text(g_catalog.pendingAsBodyReplace ? "Scegli limb da sostituire" : "Passo 1 — Dove lo metti?");
            ImGui::Spacing();
        }
        auto slotBtn = [&](AvatarSlot s, const char* label, const char* hint) {
            ImGui::PushStyleColor(ImGuiCol_Button, {0.18f, 0.38f, 0.72f, 1.f});
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.28f, 0.50f, 0.88f, 1.f});
            if (ImGui::Button(label, {-1, 40})) {
                g_catalog.pendingSlot = s;
                g_catalog.wizardStep = 1;
                if (!g_catalog.pendingImportPath.empty()) {
                    std::string path = g_catalog.pendingImportPath;
                    g_catalog.pendingImportPath.clear();
                    if (!importAccessoryFromPath(path)) {
                        g_catalog.status = std::string("Slot: ") + avatarSlotName(s)
                            + " — import fallito, riprova";
                    }
                } else {
                    g_catalog.status = std::string("Slot: ") + avatarSlotName(s) + " — importa un modello";
                }
            }
            ImGui::PopStyleColor(2);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", hint);
        };
        slotBtn(AvatarSlot::Head, "Head  —  Testa", "Parte custom testa / cappelli");
        slotBtn(AvatarSlot::Torso, "Torso", "Torso custom / corazza");
        slotBtn(AvatarSlot::LeftArm, "Left Arm  —  Braccio SX", "Braccio custom SX");
        slotBtn(AvatarSlot::RightArm, "Right Arm  —  Braccio DX", "Braccio custom DX");
        slotBtn(AvatarSlot::LeftLeg, "Left Leg  —  Gamba SX", "Gamba custom SX");
        slotBtn(AvatarSlot::RightLeg, "Right Leg  —  Gamba DX", "Gamba custom DX");
    } else {
        ImGui::Text("Passo 2 — Slot: %s %s", avatarSlotName(g_catalog.pendingSlot),
                    g_catalog.pendingAsBodyReplace ? "(SOSTITUISCE)" : "(accessorio)");
        if (ImGui::Button("← Cambia slot")) {
            g_catalog.wizardStep = 0;
            g_catalog.status = "Scegli dove mettere il modello";
        }
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, {0.12f, 0.62f, 0.42f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.18f, 0.75f, 0.52f, 1.f});
        if (ImGui::Button("Scegli file (GLB / GLTF / FBX / OBJ)", {-1, 44})) {
            std::string path;
            if (openModelFileDialog(path, publisherHwnd())) {
                importAccessoryFromPath(path);
                if (!path.empty() && path.size() < sizeof(g_importPathBuf))
                    std::snprintf(g_importPathBuf, sizeof(g_importPathBuf), "%s", path.c_str());
            } else {
                g_catalog.status = "Import annullato — puoi anche incollare il percorso sotto";
            }
        }
        ImGui::PopStyleColor(2);

        ImGui::TextDisabled("Oppure incolla il percorso completo:");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##importPath", g_importPathBuf, sizeof(g_importPathBuf));
        if (ImGui::Button("Carica da percorso", {-1, 32})) {
            importAccessoryFromPath(g_importPathBuf);
        }
    }

    ImGui::Separator();
    ImGui::TextColored({0.45f, 0.85f, 1.f, 1.f}, "Sull'avatar ora (%d)", (int)g_catalog.items.size());
    ImGui::BeginChild("##accList", {0, g_catalog.wizardStep == 0 ? 120.f : 100.f}, true);
    if (g_catalog.items.empty()) {
        ImGui::TextDisabled("Nessun pezzo. Aggiungi modello / parte custom.");
    }
    for (int i = 0; i < (int)g_catalog.items.size(); ++i) {
        auto& it = g_catalog.items[i];
        bool sel = (i == g_catalog.selected);
        ImGui::PushID(i);
        std::string lab = it.name + "  [" + avatarSlotName(it.slot) + "]";
        if (it.replacesBody) lab += "  ★parte";
        if (ImGui::Selectable(lab.c_str(), sel))
            g_catalog.selected = i;
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, {0.75f, 0.25f, 0.22f, 1.f});
        if (ImGui::SmallButton("Togli")) {
            g_catalog.items.erase(g_catalog.items.begin() + i);
            if (g_catalog.selected == i) g_catalog.selected = -1;
            else if (g_catalog.selected > i) g_catalog.selected--;
            saveEquippedAvatar(g_catalog);
            g_catalog.status = "Accessorio rimosso";
            ImGui::PopStyleColor();
            ImGui::PopID();
            break;
        }
        ImGui::PopStyleColor();
        ImGui::PopID();
    }
    ImGui::EndChild();
    if (!g_catalog.items.empty() && ImGui::Button("Togli tutto dall'avatar", {-1, 0})) {
        g_catalog.items.clear();
        g_catalog.selected = -1;
        saveEquippedAvatar(g_catalog);
        g_catalog.status = "Tutti gli accessori rimossi";
    }

    if (g_catalog.selected >= 0 && g_catalog.selected < (int)g_catalog.items.size()) {
        auto& a = g_catalog.items[g_catalog.selected];
        ImGui::Separator();
        ImGui::TextColored({0.35f, 0.80f, 1.f, 1.f}, "Modifica accessorio");
        ImGui::TextDisabled("Attaccato a %s — trascina LMB nella vista", avatarSlotName(a.slot));

        // Buffer stabile: cambia solo quando cambi selezione (altrimenti la moderazione
        // azzerava ogni lettera e non si poteva rinominare).
        static int renameSel = -1;
        static char renameBuf[128] = {};
        if (renameSel != g_catalog.selected) {
            renameSel = g_catalog.selected;
            std::snprintf(renameBuf, sizeof(renameBuf), "%s", a.name.c_str());
        }
        ImGui::Text("Rinomina");
        ImGui::SetNextItemWidth(-1);
        bool renameEnter = ImGui::InputText("##accRename", renameBuf, sizeof(renameBuf),
            ImGuiInputTextFlags_EnterReturnsTrue);
        bool renameDone = renameEnter || ImGui::IsItemDeactivatedAfterEdit();
        if (renameDone && renameBuf[0]) {
            auto mod = Moderation::checkAvatarOrModelName(renameBuf);
            if (mod.verdict == Moderation::Verdict::Ok) {
                a.name = renameBuf;
                saveEquippedAvatar(g_catalog);
                g_catalog.status = "Rinominato: " + a.name;
            } else {
                g_catalog.status = mod.reason;
                std::snprintf(renameBuf, sizeof(renameBuf), "%s", a.name.c_str());
            }
        }
        if (ImGui::Button("Applica nome", {-1, 28}) && renameBuf[0]) {
            auto mod = Moderation::checkAvatarOrModelName(renameBuf);
            if (mod.verdict == Moderation::Verdict::Ok) {
                a.name = renameBuf;
                saveEquippedAvatar(g_catalog);
                g_catalog.status = "Rinominato: " + a.name;
            } else {
                g_catalog.status = mod.reason;
                std::snprintf(renameBuf, sizeof(renameBuf), "%s", a.name.c_str());
            }
        }

        ImGui::Spacing();
        if (ImGui::Checkbox("Sostituisce parte del corpo (nasconde cubo R6)", &a.replacesBody)) {
            if (a.replacesBody) {
                a.offset = bodyReplaceOffsetForSlot(a.slot);
                g_catalog.hideDefaultPart[(int)a.slot] = true;
                // una sola replace per slot
                for (int j = 0; j < (int)g_catalog.items.size(); ++j) {
                    if (j == g_catalog.selected) continue;
                    if (g_catalog.items[j].slot == a.slot && g_catalog.items[j].replacesBody)
                        g_catalog.items[j].replacesBody = false;
                }
            }
            saveEquippedAvatar(g_catalog);
            g_catalog.status = a.replacesBody
                ? (std::string("Ora sostituisce ") + avatarSlotName(a.slot))
                : "Ora e' un accessorio sopra";
        }

        ImGui::Text("Tool  (1 Muovi · 2 Ruota · 3 Scala)");
        if (ImGui::RadioButton("Muovi", g_catalog.tool == AccelTool::Move)) g_catalog.tool = AccelTool::Move;
        ImGui::SameLine();
        if (ImGui::RadioButton("Ruota", g_catalog.tool == AccelTool::Rotate)) g_catalog.tool = AccelTool::Rotate;
        ImGui::SameLine();
        if (ImGui::RadioButton("Scala", g_catalog.tool == AccelTool::Scale)) g_catalog.tool = AccelTool::Scale;
        ImGui::TextDisabled("Ruota: trascina sul gizmo (X/Y liberi). Shift+anello = un asse. Snap · Ctrl=libero");

        ImGui::DragFloat3("Offset", &a.offset.x, 0.02f);
        ImGui::DragFloat3("Rotation °", &a.rotation.x, 1.f);
        ImGui::DragFloat3("Scale", &a.scale.x, 0.02f, 0.05f, 8.f);
        if (ImGui::Button("Ingrandisci")) a.scale *= 1.15f;
        ImGui::SameLine();
        if (ImGui::Button("Rimpicciolisci")) a.scale *= 0.87f;
        ImGui::ColorEdit3("Tint", &a.tint.x);

        ImGui::PushStyleColor(ImGuiCol_Button, {0.75f, 0.22f, 0.20f, 1.f});
        if (ImGui::Button("Disequipaggia / Rimuovi", {-1, 0})) {
            AvatarSlot removedSlot = a.slot;
            bool wasReplace = a.replacesBody;
            g_catalog.items.erase(g_catalog.items.begin() + g_catalog.selected);
            g_catalog.selected = -1;
            renameSel = -1;
            if (wasReplace) {
                bool still = false;
                for (const auto& x : g_catalog.items)
                    if (x.slot == removedSlot && x.replacesBody) { still = true; break; }
                if (!still) g_catalog.hideDefaultPart[(int)removedSlot] = false;
            }
            g_catalog.status = "Pezzo rimosso";
            saveEquippedAvatar(g_catalog);
        }
        ImGui::PopStyleColor();
    }

    // —— Animazioni Player (stesso flusso Studio: importa → pubblica → ID) ——
    ImGui::Separator();
    ImGui::TextColored({0.95f, 0.75f, 0.25f, 1.f}, "Animazioni Player (AnimationId)");
    if (ImGui::Button("Apri guida web (HTML)", {-1, 32}))
        openWalkingAnimWebGuide();
    ImGui::TextWrapped(
        "In Studio: Animation → Importa/Pubblica · oppure bottone Anim ID\n"
        "1) Scarica R6 → Blender/Maya  2) Importa FBX/GLB  3) Pubblica → copia ID");

    ImGui::PushStyleColor(ImGuiCol_Button, {0.15f, 0.48f, 0.88f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.25f, 0.58f, 0.98f, 1.f});
    if (ImGui::Button("1. Scarica modello Player R6 (default)", {-1, 44}))
        downloadDefaultR6PlayerModel();
    ImGui::PopStyleColor(2);
    ImGui::TextDisabled("OBJ + MTL · Head / Torso / Left_Arm / Right_Arm / Left_Leg / Right_Leg");

    ImGui::InputText("Action/clip in Blender-Maya", g_catalog.walkClipPrefBuf,
                     sizeof(g_catalog.walkClipPrefBuf));
    ImGui::TextDisabled("Es. WalkingAnimation, FancyWalk, Shoot, Reload...");
    ImGui::PushStyleColor(ImGuiCol_Button, {0.55f, 0.28f, 0.85f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.68f, 0.38f, 0.95f, 1.f});
    if (ImGui::Button("2. Importa animazione (FBX/GLB)", {-1, 40}))
        importWalkingAnimationFromFile();
    ImGui::PopStyleColor(2);

    if (g_catalog.hasPendingWalk && g_catalog.pendingWalk.valid()) {
        ImGui::TextColored({0.4f, 1.f, 0.55f, 1.f}, "Clip: %s · %d keyframe",
                           g_catalog.pendingWalk.clipName.c_str(),
                           (int)g_catalog.pendingWalk.keys.size());
        if (!g_catalog.pendingClipNames.empty()) {
            ImGui::TextDisabled("Clip nel file:");
            for (const auto& n : g_catalog.pendingClipNames)
                ImGui::BulletText("%s", n.c_str());
        }
        ImGui::InputText("AnimationId (scegli tu)", g_catalog.walkNameBuf, sizeof(g_catalog.walkNameBuf));
        ImGui::Checkbox("Usa come walk di default", &g_catalog.pendingAsWalkDefault);
        if (g_catalog.pendingAsWalkDefault && Animator::looksLikeToolAnim(g_catalog.pendingWalk))
            ImGui::TextColored({1.f, 0.75f, 0.3f, 1.f},
                "Attenzione: sembra pistola/tool — togli la spunta walk");
        ImGui::Checkbox("Anteprima", &g_catalog.previewWalk);
        ImGui::TextWrapped("Dopo Pubblica: game.Animator:Play(\"%s\")",
                           g_catalog.walkNameBuf[0] ? g_catalog.walkNameBuf : "Id");
        ImGui::PushStyleColor(ImGuiCol_Button, {0.12f, 0.62f, 0.42f, 1.f});
        if (ImGui::Button("3. Pubblica e ottieni AnimationId", {-1, 44}))
            releaseWalkingAnimation();
        ImGui::PopStyleColor();
    }

    if (g_lastPublishedAnimId[0]) {
        ImGui::Spacing();
        ImGui::TextColored({0.35f, 1.f, 0.55f, 1.f}, "Ultimo ID pubblicato:");
        ImGui::Text("  %s", g_lastPublishedAnimId);
        if (ImGui::SmallButton("Copia ID##cat"))
            ImGui::SetClipboardText(g_lastPublishedAnimId);
        ImGui::SameLine();
        if (ImGui::SmallButton("Copia script Play##cat")) {
            std::string snip = std::string("game.Animator:Play(\"") + g_lastPublishedAnimId + "\")";
            ImGui::SetClipboardText(snip.c_str());
        }
    }

    if (!g_catalog.walkClips.empty()) {
        ImGui::Spacing();
        ImGui::Text("Animazioni pubblicate (ID)");
        for (int i = 0; i < (int)g_catalog.walkClips.size(); ++i) {
            const auto& c = g_catalog.walkClips[i];
            ImGui::PushID(2000 + i);
            bool eq = (c.id == g_catalog.equippedWalkId);
            const char* showId = c.displayName.empty() ? c.id.c_str() : c.displayName.c_str();
            if (eq)
                ImGui::TextColored({0.3f, 0.9f, 0.5f, 1.f}, "* %s", showId);
            else
                ImGui::Text("%s", showId);
            ImGui::SameLine();
            if (ImGui::SmallButton("Copia ID"))
                ImGui::SetClipboardText(showId);
            ImGui::SameLine();
            if (ImGui::SmallButton("Play"))
                g_animator.play(showId);
            if (!eq) {
                ImGui::SameLine();
                if (ImGui::SmallButton("Walk default")) {
                    g_catalog.equippedWalkId = c.id;
                    syncEquippedWalkToAnimator();
                    r6SaveLibrary(g_catalog.walkClips, g_catalog.equippedWalkId);
                    animLibrary().setDefaultWalk(showId);
                    g_catalog.status = "Walk default = " + std::string(showId);
                }
            } else {
                ImGui::SameLine();
                if (ImGui::SmallButton("Classic")) {
                    g_catalog.equippedWalkId.clear();
                    syncEquippedWalkToAnimator();
                    r6SaveLibrary(g_catalog.walkClips, g_catalog.equippedWalkId);
                    g_catalog.status = "Walk: Classic (procedurale)";
                }
            }
            ImGui::PopID();
        }
    }

    ImGui::Separator();
    if (ImGui::CollapsingHeader("Pacchetti animazioni (pubblica set)")) {
        drawAnimPackPublisher(g_animPackUi);
        ImGui::Separator();
    }

        ImGui::TextDisabled("Import = gia' sull'avatar (anche senza Pubblica). Pubblica = metti in vendita nello Shop.");
        ImGui::TextColored({1.f, 0.85f, 0.25f, 1.f}, "Saldo: %d B$", g_menu.shopCoins);
    ImGui::PushStyleColor(ImGuiCol_Button, {0.12f, 0.62f, 0.42f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.18f, 0.75f, 0.52f, 1.f});
    if (ImGui::Button("Pubblica nello Shop", {-1, 44})) {
        int n = publishAccessoriesToShop();
        if (n > 0) {
            g_catalog.open = false;
            g_catalog.status = "Pubblicato nello Shop!";
            returnToStudioEditor(glfwGetCurrentContext());
        } else {
            if (g_catalog.status.find("moderazione") != std::string::npos) {
            } else if (g_catalog.items.empty()) {
                g_catalog.status = "Niente da pubblicare — importa prima un modello";
            } else {
                g_catalog.status = "Nessun nuovo item (già nello Shop o bloccato)";
            }
        }
    }
    ImGui::PopStyleColor(2);
    ImGui::PushStyleColor(ImGuiCol_Button, {0.15f, 0.55f, 0.95f, 1.f});
    if (ImGui::Button("Salva e torna all'editor", {-1, 36})) {
        g_catalog.open = false;
        syncCatalogItemsToInventory();
        syncEquippedToMenu();
        g_catalog.status = "Salvato sul personaggio (" + std::to_string(g_catalog.items.size()) + " item)";
        saveEquippedAvatar(g_catalog);
        returnToStudioEditor(glfwGetCurrentContext());
    }
    ImGui::PopStyleColor();
    if (ImGui::Button(kIsGameEngine ? "Torna all'editor" : "Torna al menu", {-1, 0})) {
        g_catalog.open = false;
        if (kIsGameEngine)
            returnToStudioEditor(glfwGetCurrentContext());
        else
            returnToMenu(glfwGetCurrentContext());
    }
    ImGui::End();
}

static void setPlayMenuOpen(GLFWwindow* win, bool open) {
    g_playMenuOpen = open;
    if (g_screen == AppScreen::Playing || g_studioTesting)
        syncPlayCursor(win);
}

/** Bubble chat in alto a sinistra (stile Roblox) — apre/chiude la chat. */
/** Invia chat + (in Play/Test) mostra fumetto sopra il player. */
static bool sendPlayChat(const char* msg) {
    if (!msg || !msg[0]) return false;
    const char* uname = g_menu.username[0] ? g_menu.username : "Player";
    if (!Moderation::tryAppendChat(g_menu.chatLines, g_menu.mod, uname, msg))
        return false;
    const bool inWorld = (g_screen == AppScreen::Playing)
        || (g_screen == AppScreen::Studio && g_studioTesting);
    if (inWorld && !g_menu.mod.history.empty()) {
        const auto& e = g_menu.mod.history.back();
        if (!e.blocked && !e.display.empty()) {
            g_speechText = e.display;
            if (g_speechText.size() > 90)
                g_speechText = g_speechText.substr(0, 87) + "...";
            g_speechUntil = glfwGetTime() + 5.0;
        }
    }
    return true;
}

/** Fumetto stile Roblox sopra la testa del player locale. */
static void drawSpeechBubbles() {
    if (g_speechText.empty() || glfwGetTime() > g_speechUntil) return;
    if (!(g_screen == AppScreen::Playing
          || (g_screen == AppScreen::Studio && g_studioTesting)))
        return;
    // Sopra la Head R6 (~4.7 + metà testa)
    glm::vec3 anchor = g_playerPos + glm::vec3(0.f, 6.35f, 0.f);
    float sx = 0.f, sy = 0.f;
    if (!worldToScreen(anchor, sx, sy)) return;

    const double left = g_speechUntil - glfwGetTime();
    float a = left < 0.7 ? (float)(left / 0.7) : 1.f;
    a = glm::clamp(a, 0.f, 1.f);
    const int ai = (int)(a * 255.f);

    ImFont* font = ImGui::GetFont();
    const float fs = ImGui::GetFontSize() * 0.95f;
    const float wrapW = 220.f;
    ImVec2 textSize = font->CalcTextSizeA(fs, wrapW, wrapW, g_speechText.c_str());
    const float padX = 12.f, padY = 8.f;
    const float tipH = 10.f;
    ImVec2 boxMin{sx - textSize.x * 0.5f - padX, sy - textSize.y - padY * 2.f - tipH};
    ImVec2 boxMax{sx + textSize.x * 0.5f + padX, sy - tipH};
    // Clamp ai bordi schermo
    if (boxMin.x < 8.f) {
        float d = 8.f - boxMin.x;
        boxMin.x += d; boxMax.x += d;
    }
    if (boxMax.x > (float)g_w - 8.f) {
        float d = boxMax.x - ((float)g_w - 8.f);
        boxMin.x -= d; boxMax.x -= d;
    }
    if (boxMin.y < 8.f) {
        float d = 8.f - boxMin.y;
        boxMin.y += d; boxMax.y += d;
    }

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const float round = 12.f;
    // Ombra soft (stile Roblox) — niente outline bianca
    dl->AddRectFilled({boxMin.x + 2.f, boxMin.y + 3.f},
                      {boxMax.x + 2.f, boxMax.y + 3.f},
                      IM_COL32(0, 0, 0, (int)(a * 45.f)), round);
    dl->AddRectFilled(boxMin, boxMax, IM_COL32(255, 255, 255, ai), round);
    // Puntina verso la testa (+ ombra leggera)
    const float midX = (boxMin.x + boxMax.x) * 0.5f;
    dl->AddTriangleFilled(
        {midX - 7.f, boxMax.y - 0.5f},
        {midX + 7.f, boxMax.y - 0.5f},
        {sx + 1.5f, boxMax.y + tipH + 1.5f},
        IM_COL32(0, 0, 0, (int)(a * 40.f)));
    dl->AddTriangleFilled(
        {midX - 8.f, boxMax.y - 1.f},
        {midX + 8.f, boxMax.y - 1.f},
        {sx, boxMax.y + tipH},
        IM_COL32(255, 255, 255, ai));
    dl->AddText(font, fs, {boxMin.x + padX, boxMin.y + padY},
                IM_COL32(25, 25, 25, ai), g_speechText.c_str(), nullptr, wrapW);
}

static void drawChatBubbleHud() {
    const float r = 22.f;
    const float x = 14.f;
    const float y = 56.f;
    ImGui::SetNextWindowPos({x, y}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({r * 2.f + 8.f, r * 2.f + 8.f}, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {4.f, 4.f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, r + 4.f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.f, 0.f, 0.f, 0.f});
    ImGui::Begin("##ChatBubble", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 c{origin.x + r, origin.y + r};
    if (ImGui::InvisibleButton("##chatbubclick", {r * 2.f, r * 2.f})) {
        g_showChat = !g_showChat;
        if (GLFWwindow* w = glfwGetCurrentContext())
            syncPlayCursor(w);
    }
    const bool hover = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const bool on = g_showChat || hover;
    ImU32 fill = on ? IM_COL32(0, 162, 255, 245) : IM_COL32(0, 132, 232, 230);
    // Stile Roblox: cerchio blu pieno, niente alone/outline bianca
    dl->AddCircleFilled({c.x + 1.5f, c.y + 2.f}, r, IM_COL32(0, 0, 0, 55), 28);
    dl->AddCircleFilled(c, r, fill, 28);
    // fumetto bianco interno (icona)
    dl->AddRectFilled({c.x - 10.f, c.y - 8.f}, {c.x + 10.f, c.y + 4.f},
                      IM_COL32(255, 255, 255, 250), 5.f);
    dl->AddTriangleFilled({c.x - 4.f, c.y + 4.f}, {c.x + 2.f, c.y + 4.f},
                          {c.x - 6.f, c.y + 11.f}, IM_COL32(255, 255, 255, 250));
    // tre puntini blu
    dl->AddCircleFilled({c.x - 5.f, c.y - 2.f}, 1.5f, IM_COL32(0, 120, 210, 255), 8);
    dl->AddCircleFilled({c.x, c.y - 2.f}, 1.5f, IM_COL32(0, 120, 210, 255), 8);
    dl->AddCircleFilled({c.x + 5.f, c.y - 2.f}, 1.5f, IM_COL32(0, 120, 210, 255), 8);
    if (hover)
        ImGui::SetTooltip("Chat");
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

/** Health bar sopra la testa: visibile solo se HP < 90 (o morto). */
static void drawPlayerHeadHealthBar() {
    const auto& h = g_player.humanoid;
    if (!h.dead && h.health >= 90.f) return;
    if (!(g_screen == AppScreen::Playing
          || (g_screen == AppScreen::Studio && g_studioTesting)))
        return;
    if (g_firstPerson && !g_ragdoll.active) return; // in FP non serve sopra la testa

    glm::vec3 head = g_playerPos + glm::vec3(0.f, 5.6f, 0.f);
    float sx = 0.f, sy = 0.f;
    if (!worldToScreen(head, sx, sy)) return;

    const float barW = 56.f, barH = 7.f;
    float x = sx - barW * 0.5f;
    float y = sy - 18.f;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    dl->AddRectFilled({x - 1.5f, y - 1.5f}, {x + barW + 1.5f, y + barH + 1.5f},
                      IM_COL32(0, 0, 0, 170), 3.f);
    dl->AddRectFilled({x, y}, {x + barW, y + barH}, IM_COL32(30, 30, 35, 220), 2.f);

    float pct = h.dead ? 0.f : h.healthPct();
    ImU32 fillCol;
    if (h.dead || h.health <= 0.f) {
        fillCol = IM_COL32(110, 110, 115, 255);
        pct = 1.f;
    } else if (pct > 0.5f) {
        float t = (pct - 0.5f) * 2.f;
        fillCol = IM_COL32((int)((0.25f + (1.f - t) * 0.75f) * 255), (int)(0.85f * 255), 50, 255);
    } else {
        float t = pct * 2.f;
        fillCol = IM_COL32(242, (int)((0.20f + t * 0.65f) * 255), 40, 255);
    }
    float fillW = barW * pct;
    if (fillW > 0.5f)
        dl->AddRectFilled({x, y}, {x + fillW, y + barH}, fillCol, 2.f);
}

static void syncFriendsToWorld() {
    g_world.friends.clear();
    g_world.friends.reserve(g_friends.size());
    for (const auto& f : g_friends) {
        WorkspaceFriend w;
        w.name = f.name;
        w.position = f.pos;
        w.yaw = f.yaw;
        g_world.friends.push_back(std::move(w));
    }
}

static void syncFriendsFromWorld() {
    g_friends.clear();
    g_friends.reserve(g_world.friends.size());
    int maxN = 0;
    for (const auto& w : g_world.friends) {
        FriendPlayer f;
        f.name = w.name.empty() ? "Amico" : w.name;
        f.pos = w.position;
        f.pos.y = 0.f;
        f.yaw = w.yaw;
        // Amico3 → seq
        if (f.name.size() > 5 && f.name.compare(0, 5, "Amico") == 0) {
            int n = std::atoi(f.name.c_str() + 5);
            if (n > maxN) maxN = n;
        }
        g_friends.push_back(std::move(f));
    }
    if (maxN >= g_friendSeq) g_friendSeq = maxN + 1;
}

static void insertFriendPlayer() {
    FriendPlayer f;
    f.name = "Amico" + std::to_string(g_friendSeq++);
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
    g_exportMsg = "Player amico aggiunto — in Test ti segue";
}

static void updateFriendPlayers(float dt) {
    if (dt <= 0.f) return;
    if (!(g_screen == AppScreen::Playing
          || (g_screen == AppScreen::Studio && g_studioTesting)))
        return;
    for (auto& f : g_friends) {
        glm::vec3 to = g_playerPos - f.pos;
        to.y = 0.f;
        float dist = glm::length(to);
        bool walking = false;
        if (dist > 3.5f && dist < 80.f) {
            glm::vec3 dir = to / dist;
            float spd = (dist > 12.f) ? 14.f : 9.f;
            f.pos += dir * spd * dt;
            f.yaw = glm::degrees(std::atan2(dir.x, dir.z));
            walking = true;
        }
        f.pos.y = 0.f;
        f.body.update(dt, walking);
    }
}

static void drawFriendPlayers(GLuint vao, GLuint partP, const glm::mat4& view,
                              const glm::mat4& proj, const glm::vec3& sun) {
    if (g_friends.empty()) return;
    GLuint playerVao = g_bevelCube.vao ? g_bevelCube.vao : vao;
    BlockDrawContext bctx{ playerVao, partP, view, proj, sun };
    for (auto& f : g_friends) {
        f.body.draw(bctx, drawCubePlayer, f.pos, f.yaw, nullptr);
        glm::vec3 namePos = f.pos + glm::vec3(0.f, 5.4f, 0.f);
        drawBillboardText(namePos, f.name.c_str(), {0.45f, 1.f, 0.55f}, 0.85f, true, false);
    }
}

/** Health bar stile Roblox: colore da vita; grigia se morto. */
static void drawHealthBarHud() {
    const auto& h = g_player.humanoid;
    const float barW = 220.f, barH = 18.f;
    const float x = (float)g_w - barW - 16.f;
    const float y = 56.f;
    ImDrawList* dl = ImGui::GetForegroundDrawList();

    // bordo / sfondo
    dl->AddRectFilled({x - 3, y - 3}, {x + barW + 3, y + barH + 3},
                      IM_COL32(0, 0, 0, 160), 4.f);
    dl->AddRectFilled({x, y}, {x + barW, y + barH}, IM_COL32(35, 35, 40, 220), 3.f);

    float pct = h.dead ? 0.f : h.healthPct();
    ImU32 fillCol;
    if (h.dead || h.health <= 0.f) {
        fillCol = IM_COL32(110, 110, 115, 255); // grigio = morto
    } else {
        // verde → giallo → rosso (come Roblox)
        float r, g, b;
        if (pct > 0.5f) {
            float t = (pct - 0.5f) * 2.f; // 0..1
            r = 0.25f + (1.f - t) * 0.75f;
            g = 0.85f;
            b = 0.20f;
        } else {
            float t = pct * 2.f;
            r = 0.95f;
            g = 0.20f + t * 0.65f;
            b = 0.15f;
        }
        fillCol = IM_COL32((int)(r * 255), (int)(g * 255), (int)(b * 255), 255);
    }
    float fillW = std::max(0.f, barW * pct);
    if (h.dead)
        fillW = barW; // barra piena grigia da morto
    if (fillW > 0.5f)
        dl->AddRectFilled({x, y}, {x + fillW, y + barH}, fillCol, 3.f);

    char lab[64];
    if (h.dead)
        std::snprintf(lab, sizeof(lab), "%s", L("hud.dead"));
    else
        std::snprintf(lab, sizeof(lab), "%s  %.0f / %.0f", L("hud.health"), h.health, h.maxHealth);
    ImVec2 ts = ImGui::CalcTextSize(lab);
    dl->AddText({x + (barW - ts.x) * 0.5f, y + (barH - ts.y) * 0.5f},
                IM_COL32(255, 255, 255, 240), lab);
}

/** Item equipaggiato nel hotbar (se esiste e ha tool). */
static const BackpackItem* equippedBackpackItem() {
    int i = g_world.equippedBackpack;
    if (i < 0 || i >= (int)g_world.backpack.size()) return nullptr;
    return &g_world.backpack[i];
}

static bool equippedSword() {
    const BackpackItem* it = equippedBackpackItem();
    return it && it->tool == BackpackTool::Sword;
}

/** Disegna spada sul braccio destro se Sword equipaggiata. */
static void drawEquippedSword(GLuint vao, GLuint partP, const glm::mat4& view,
                              const glm::mat4& proj, const glm::vec3& sun) {
    if (!equippedSword() || g_player.humanoid.dead) return;

    const HumanoidPartDef* arm = nullptr;
    for (const auto& p : g_player.humanoid.parts) {
        if (p.name == "Right Arm") { arm = &p; break; }
    }
    if (!arm) return;

    float walkArm = humanoidArmSwing(g_player.humanoid);
    float jL = 0.f, jR = 0.f, jl = 0.f, jr = 0.f;
    humanoidJumpPose(g_player.humanoid, jL, jR, jl, jr);
    float bob = humanoidBob(g_player.humanoid);
    float rx = arm->rotX;
    float rz = arm->rotZ;
    bool animDriven = (std::fabs(arm->rotX) + std::fabs(arm->rotZ)) > 0.01f;
    if (!animDriven)
        rx = -walkArm * (1.f - g_player.humanoid.jumpArmBlend) + jR;

    // Swing: ruota la spada in avanti
    float swingDeg = g_swordSwing * 70.f;

    glm::mat4 root = glm::rotate(glm::translate(glm::mat4(1.f), g_playerPos),
                                 glm::radians(g_playerYaw), glm::vec3(0, 1, 0));
    glm::vec3 c = arm->offset;
    glm::mat4 armM = glm::translate(root, c + arm->pivot);
    if (rx != 0) armM = glm::rotate(armM, glm::radians(rx), glm::vec3(1, 0, 0));
    if (rz != 0) armM = glm::rotate(armM, glm::radians(rz), glm::vec3(0, 0, 1));
    armM = glm::translate(armM, -arm->pivot);
    // Grip nella mano (basso braccio) + swing
    armM = glm::translate(armM, {0.15f, -0.85f, 0.35f});
    armM = glm::rotate(armM, glm::radians(-90.f + swingDeg), glm::vec3(1, 0, 0));
    armM = glm::rotate(armM, glm::radians(8.f), glm::vec3(0, 0, 1));

    const glm::vec3 steel{0.75f, 0.78f, 0.85f};
    const glm::vec3 hilt{0.35f, 0.22f, 0.12f};
    const glm::vec3 guard{0.55f, 0.55f, 0.60f};

    auto drawBit = [&](glm::vec3 local, glm::vec3 size, glm::vec3 col) {
        glm::mat4 m = glm::translate(armM, local) * glm::scale(glm::mat4(1.f), size);
        drawCube(vao, partP, m, view, proj, col, sun, false, 0);
    };
    drawBit({0, 0.15f, 0}, {0.18f, 0.45f, 0.18f}, hilt);       // impugnatura
    drawBit({0, 0.42f, 0}, {0.55f, 0.12f, 0.18f}, guard);      // guardia
    drawBit({0, 1.35f, 0}, {0.14f, 1.6f, 0.06f}, steel);       // lama
    (void)bob;
}

/** Attacca con Sword (LMB / pad X). */
static void trySwordAttack() {
    if (!equippedSword() || g_player.humanoid.dead) return;
    if (g_swordCooldown > 0.f) return;
    g_swordCooldown = 0.45f;
    g_swordSwing = 1.f;

    // Colpisci parti non-anchored davanti al player (range corto)
    glm::vec3 fwd = camFlatForward();
    glm::vec3 origin = g_playerPos + glm::vec3(0, 2.5f, 0) + fwd * 1.2f;
    const float reach = 4.5f;
    bool hit = false;
    for (auto& p : g_world.parts) {
        if (!p.canDestroy) continue;
        glm::vec3 to = p.position - origin;
        float dist = glm::length(to);
        if (dist > reach || dist < 0.01f) continue;
        float align = glm::dot(glm::normalize(to), fwd);
        if (align < 0.25f) continue;
        p.health -= 40.f;
        p.position += fwd * 0.8f;
        p.position.y += 0.25f;
        g_particles.emitPreset(ParticlePreset::Sparks, p.position, 22, {-1,-1,-1}, fwd);
        if (p.health <= 0.f) {
            g_particles.spawnExplosion(p.position, 8.f, 16);
            g_lua.applyExplosionPlayerDamage(p.position, 8.f, 25.f);
            for (int i = 0; i < (int)g_world.parts.size(); ++i) {
                if (&g_world.parts[i] == &p) {
                    g_world.parts.erase(g_world.parts.begin() + i);
                    if (g_selectedPart == i) selectPart(-1);
                    else if (g_selectedPart > i) --g_selectedPart;
                    break;
                }
            }
        }
        hit = true;
        break; // un colpo per swing
    }
    if (!hit)
        g_particles.emitPreset(ParticlePreset::Sparks, origin + fwd * 2.2f, 10, {-1,-1,-1}, fwd);
}

/** Colore fill cerchio inventario da iconHue. */
static ImU32 backpackHueColor(int hue) {
    static const ImU32 cols[] = {
        IM_COL32(70, 140, 255, 230),
        IM_COL32(80, 200, 90, 230),
        IM_COL32(240, 180, 50, 230),
        IM_COL32(230, 80, 70, 230),
        IM_COL32(180, 90, 220, 230),
        IM_COL32(90, 210, 210, 230),
    };
    int i = hue % 6;
    if (i < 0) i += 6;
    return cols[i];
}

/**
 * Switch inventario — SOLO GLFW. Slot 1-9 sempre (anche Backpack vuoto).
 * Toggle: rosso selezionato · di nuovo → bianco.
 */
static void pollBackpackSwitch(GLFWwindow* win) {
    if (!win) return;

    static const int kRow[] = {
        GLFW_KEY_1, GLFW_KEY_2, GLFW_KEY_3, GLFW_KEY_4, GLFW_KEY_5,
        GLFW_KEY_6, GLFW_KEY_7, GLFW_KEY_8, GLFW_KEY_9
    };
    static const int kPad[] = {
        GLFW_KEY_KP_1, GLFW_KEY_KP_2, GLFW_KEY_KP_3, GLFW_KEY_KP_4, GLFW_KEY_KP_5,
        GLFW_KEY_KP_6, GLFW_KEY_KP_7, GLFW_KEY_KP_8, GLFW_KEY_KP_9
    };

    for (int i = 0; i < DataModel::kHotbarSlots; ++i) {
        bool rd = glfwGetKey(win, kRow[i]) == GLFW_PRESS;
        bool pd = glfwGetKey(win, kPad[i]) == GLFW_PRESS;
        bool pressed = (rd && !g_bpKeyWas[i]) || (pd && !g_bpKpWas[i]);
        g_bpKeyWas[i] = rd;
        g_bpKpWas[i] = pd;
        if (pressed)
            g_world.equipBackpackSlot(i, true);
    }

    // No Tab — ImGui Nav lo usa e a volte blocca la visuale
    static bool leftWas = false, rightWas = false;
    bool left = glfwGetKey(win, GLFW_KEY_LEFT) == GLFW_PRESS;
    bool right = glfwGetKey(win, GLFW_KEY_RIGHT) == GLFW_PRESS;
    if (right && !rightWas) g_world.cycleBackpack(+1);
    if (left && !leftWas) g_world.cycleBackpack(-1);
    leftWas = left; rightWas = right;

    const auto& pad = gamepad();
    if (pad.connected) {
        if (pad.rbPressed) g_world.cycleBackpack(+1);
        if (pad.rtPressed || pad.lbPressed) g_world.cycleBackpack(-1);
    }
}

/**
 * Hotbar 1-9. Item solo se messi in Backpack (nome); altrimenti empty ma premibile.
 * Bianco = non selezionato · Rosso = selezionato (anche slot vuoto).
 */
static void drawInventoryHud() {
    auto& bp = g_world.backpack;
    const int nItems = (int)bp.size();
    // Compatto in basso — non copre il centro della visuale
    const float slotR = 22.f;
    const float gap = 6.f;
    const int showN = DataModel::kHotbarSlots;
    const float totalW = showN * (slotR * 2.f) + (showN - 1) * gap;
    const float x0 = ((float)g_w - totalW) * 0.5f + slotR;
    const float y0 = (float)g_h - slotR - 14.f;

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImGui::SetNextWindowPos({x0 - slotR - 12, y0 - slotR - 28}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({totalW + 40, slotR * 2.f + 48}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.f);
    ImGui::Begin("##InvCircle", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoInputs);

    dl->AddText({x0 - slotR, y0 - slotR - 22}, IM_COL32(255, 255, 255, 220), L("hud.inventory"));
    if (nItems == 0)
        dl->AddText({x0 - slotR + 100, y0 - slotR - 22}, IM_COL32(170, 170, 180, 170),
                    "empty  ·  1-9");
    else
        dl->AddText({x0 - slotR + 100, y0 - slotR - 22}, IM_COL32(200, 205, 220, 180),
                    "1-9  (di nuovo = togli)");

    for (int i = 0; i < showN; ++i) {
        float cx = x0 + i * (slotR * 2.f + gap);
        float cy = y0;
        bool has = i < nItems;
        bool eq = (g_world.equippedBackpack == i);

        if (has)
            dl->AddCircleFilled({cx, cy}, slotR, backpackHueColor(bp[i].iconHue), 48);
        else
            dl->AddCircleFilled({cx, cy}, slotR, IM_COL32(32, 34, 40, 220), 48);

        // Anello: bianco / rosso anche su slot empty
        ImU32 ring = eq ? IM_COL32(230, 40, 40, 255) : IM_COL32(255, 255, 255, 255);
        dl->AddCircle({cx, cy}, slotR, ring, 48, eq ? 5.0f : 2.8f);
        if (eq)
            dl->AddCircle({cx, cy}, slotR + 6.f, IM_COL32(255, 50, 50, 160), 48, 2.2f);

        char num[4];
        std::snprintf(num, sizeof(num), "%d", i + 1);
        ImVec2 ns = ImGui::CalcTextSize(num);
        ImU32 numCol = eq ? IM_COL32(255, 45, 45, 255) : IM_COL32(255, 255, 255, 230);
        dl->AddText({cx - ns.x * 0.5f, cy - slotR + 4}, numCol, num);

        // Nome solo se c'è un item nel Backpack
        if (has) {
            char shortN[16];
            std::snprintf(shortN, sizeof(shortN), "%.6s", bp[i].name.c_str());
            ImVec2 ts = ImGui::CalcTextSize(shortN);
            dl->AddText({cx - ts.x * 0.5f, cy - ts.y * 0.5f + 4},
                        IM_COL32(255, 255, 255, 250), shortN);
        } else {
            ImVec2 es = ImGui::CalcTextSize("—");
            dl->AddText({cx - es.x * 0.5f, cy - es.y * 0.5f + 4},
                        IM_COL32(120, 120, 130, 180), "—");
        }
    }
    ImGui::End();
}

static void drawPlayingHud(GLFWwindow* win) {
    // Barra in alto: titolo + Menu
    ImGui::SetNextWindowPos({0, 0}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({(float)g_w, 48}, ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.07f, 0.28f, 0.58f, 0.92f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::Begin("##PlayTopBar", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse);
    ImGui::TextColored({1.f, 1.f, 1.f, 1.f}, "BLOCKY");
    ImGui::SameLine();
    ImGui::TextColored({0.75f, 0.88f, 1.f, 1.f}, "  %s", g_activeGame.title.c_str());
    if (g_shiftLock) {
        ImGui::SameLine();
        ImGui::TextColored({1.f, 0.88f, 0.30f, 1.f}, "  [SHIFT LOCK]");
    }
    ImGui::SameLine((float)g_w - 290);
    ImGui::TextColored({1.f, 0.90f, 0.40f, 1.f}, "%s  ·  %d B$",
                       g_menu.username[0] ? g_menu.username : "Player", g_menu.shopCoins);
    ImGui::SameLine((float)g_w - 96);
    ImGui::PushStyleColor(ImGuiCol_Button, {1.f, 1.f, 1.f, 0.20f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {1.f, 1.f, 1.f, 0.35f});
    ImGui::PushStyleColor(ImGuiCol_Text, {1, 1, 1, 1});
    if (ImGui::Button("Menu", {78, 30}))
        setPlayMenuOpen(win, !g_playMenuOpen);
    ImGui::PopStyleColor(3);
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    // Health + Inventario + chat bubble + fumetto sopra player
    drawHealthBarHud();
    drawPlayerHeadHealthBar();
    drawChatBubbleHud();
    drawSpeechBubbles();
    drawFpCrosshair();
    drawWorldLabels();
    if (!g_playMenuOpen)
        drawInventoryHud();

    // Hint basso a sinistra (sotto la bubble)
    if (!g_playMenuOpen) {
        ImGui::SetNextWindowPos({12, 112}, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.55f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.06f, 0.20f, 0.42f, 0.75f});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.f);
        ImGui::Begin("##PlayHints", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoInputs);
        {
            const auto& padHint = gamepad();
            if (padHint.connected)
                ImGui::TextColored({0.80f, 0.90f, 1.f, 1.f},
                    "Pad: %s · Start menu · nel menu: LS punta · A/X/L clicca", padHint.name);
            else
                ImGui::TextColored({0.80f, 0.90f, 1.f, 1.f},
                    "WASD · Space · zoom/V = 1ª persona · RMB guarda in 3ª");
            if (g_net.role != NetRole::Offline)
                ImGui::TextColored({0.35f, 0.95f, 0.55f, 1.f}, "%s", g_net.status);
        }
        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }

    // Menu Esc: Leave + Chat
    if (g_playMenuOpen) {
        ImGui::SetNextWindowPos({(float)g_w * 0.5f - 180, (float)g_h * 0.5f - 235}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({360, 470}, ImGuiCond_Always);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.08f, 0.22f, 0.48f, 0.97f});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 16.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f);
        ImGui::Begin("Menu di gioco", nullptr,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);

        ImGui::TextColored({1.f, 1.f, 1.f, 1.f}, "BLOCKY");
        ImGui::SameLine();
        ImGui::TextColored({0.75f, 0.88f, 1.f, 1.f}, "— %s", g_activeGame.title.c_str());
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Button, {0.10f, 0.72f, 0.40f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.16f, 0.85f, 0.50f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_Text, {1, 1, 1, 1});
        if (ImGui::Button("Riprendi", {-1, 42}))
            setPlayMenuOpen(win, false);
        ImGui::PopStyleColor(3);

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, {0.16f, 0.42f, 0.82f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.24f, 0.55f, 0.95f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_Text, {1, 1, 1, 1});
        if (ImGui::Button(g_shiftLock ? "Shift Lock: ON" : "Shift Lock: OFF", {-1, 36})) {
            g_shiftLock = !g_shiftLock;
            if (g_shiftLock) g_firstPerson = false;
        }
        ImGui::PopStyleColor(3);

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, {0.22f, 0.55f, 0.78f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.30f, 0.65f, 0.92f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_Text, {1, 1, 1, 1});
        if (ImGui::Button(g_firstPerson ? "Prima persona: ON" : "Prima persona: OFF", {-1, 36})) {
            g_firstPerson = !g_firstPerson;
            if (g_firstPerson) {
                g_shiftLock = false;
                g_playerYaw = yawFromDir(camFlatForward());
                g_orbitDist = kFpEnterDist;
            } else {
                g_orbitDist = std::max(g_orbitDist, kFpExitDist);
            }
            syncPlayCursor(win);
        }
        ImGui::TextDisabled("Oppure zoom (rotella) fino in fondo");
        ImGui::PopStyleColor(3);

        // Prestazioni grafiche: Low / Medium / High
        ImGui::Spacing();
        ImGui::TextColored({0.75f, 0.88f, 1.f, 1.f}, "Grafica / Prestazioni");
        {
            const QualityPreset presets[3] = {
                QualityPreset::Low, QualityPreset::Medium, QualityPreset::High };
            const char* labels[3] = { "Low", "Medium", "High" };
            float bw = (ImGui::GetContentRegionAvail().x
                        - ImGui::GetStyle().ItemSpacing.x * 2.f) / 3.f;
            for (int i = 0; i < 3; ++i) {
                bool on = (g_engine.preset == presets[i]);
                ImGui::PushStyleColor(ImGuiCol_Button,
                    on ? ImVec4{0.10f, 0.72f, 0.40f, 1.f} : ImVec4{0.16f, 0.36f, 0.66f, 1.f});
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                    on ? ImVec4{0.16f, 0.85f, 0.50f, 1.f} : ImVec4{0.24f, 0.50f, 0.85f, 1.f});
                ImGui::PushStyleColor(ImGuiCol_Text, {1, 1, 1, 1});
                if (ImGui::Button(labels[i], {bw, 34}))
                    applyGfxPreset(presets[i]);
                ImGui::PopStyleColor(3);
                if (i < 2) ImGui::SameLine();
            }
        }
        ImGui::TextDisabled("%s · vista %.0f · parti %d%s",
                            g_engine.quality.name,
                            g_engine.quality.maxDrawDistance,
                            g_engine.quality.maxVisibleParts,
                            g_engine.quality.studTextures ? "" : " · stud off");
        ImGui::TextDisabled("Ombre luci Neon: %s · MSAA al riavvio",
                            g_engine.quality.shadowOccluders > 0 ? "sì" : "no");

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, {0.18f, 0.52f, 0.95f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.28f, 0.62f, 1.f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_Text, {1, 1, 1, 1});
        if (ImGui::Button(g_showChat ? LB("hub.hide_chat", "hchat") : LB("hub.show_chat", "schat"), {-1, 40})) {
            g_showChat = !g_showChat;
            if (g_showChat)
                setPlayMenuOpen(win, false);
        }
        ImGui::PopStyleColor(3);

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, {0.82f, 0.22f, 0.22f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.95f, 0.32f, 0.28f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_Text, {1, 1, 1, 1});
        if (ImGui::Button("Leave", {-1, 44})) {
            g_playMenuOpen = false;
            g_shiftLock = false;
            g_firstPerson = false;
            g_moveVel = {0, 0, 0};
            returnToMenu(win);
        }
        ImGui::PopStyleColor(3);

        ImGui::Spacing();
        ImGui::TextColored({0.70f, 0.82f, 0.98f, 1.f}, "Pad: LS punta · A / X / L clicca · Start chiude");
        ImGui::TextColored({0.70f, 0.82f, 0.98f, 1.f}, "3ª = mouse libero (RMB guarda) · 1ª = mirino");
        ImGui::End();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
    }

    // Chat in Play (bubble in alto a sinistra)
    if (g_showChat) {
        ImGui::SetNextWindowPos({12, 112}, ImGuiCond_Appearing);
        ImGui::SetNextWindowSize({360, 220}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.92f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.08f, 0.22f, 0.48f, 0.95f});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.f);
        ImGui::Begin("Chat", &g_showChat);
        ImGui::BeginChild("##plog", {0, -36}, false);
        for (const auto& line : g_menu.chatLines)
            ImGui::TextWrapped("%s", line.c_str());
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.f)
            ImGui::SetScrollHereY(1.f);
        ImGui::EndChild();
        bool send = ImGui::InputText("##pmsg", g_studioChatBuf, sizeof(g_studioChatBuf),
            ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if ((ImGui::Button("Invia") || send) && g_studioChatBuf[0]) {
            sendPlayChat(g_studioChatBuf);
            g_studioChatBuf[0] = 0;
        }
        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }
}

static void onGlfwError(int code, const char* desc) {
    char buf[256];
    std::snprintf(buf, sizeof(buf), "GLFW error %d: %s", code, desc ? desc : "?");
    CrashHandler::note(buf);
    std::fprintf(stderr, "[GLFW] %s\n", buf);
}

static void updateCrashContext() {
    const char* screen = "Unknown";
    switch (g_screen) {
    case AppScreen::MainMenu: screen = "MainMenu"; break;
    case AppScreen::Loading: screen = "Loading"; break;
    case AppScreen::Playing: screen = "Playing"; break;
    case AppScreen::Studio: screen = g_studioTesting ? "Studio/Test" : "Studio"; break;
    case AppScreen::AvatarPublisher: screen = "AvatarPublisher"; break;
    }
    char buf[256];
    std::snprintf(buf, sizeof(buf), "%s | place=%s | user=%s | fps=%.0f",
                  screen,
                  g_world.placeName.empty() ? "-" : g_world.placeName.c_str(),
                  g_menu.username[0] ? g_menu.username : "-",
                  g_fps);
    CrashHandler::setContext(buf);
}

int main() {
    CrashHandler::install(kAppBrand, "1.0");
    CrashHandler::note(kIsGameEngine ? "Avvio Game Engine" : "Avvio Blocky hub");

    // Log su file: debug persistente, si legge dopo un crash o un rifiuto.
#ifdef BLOCKY_DEV
    blog::setLevel(blog::Debug);
#endif
    blog::toFile(blockyDataDir() + "/blocky.log");
    LOG_INFO("boot", "Blocky avviato su %s", plat::name());

    // Presto, prima di caricare qualunque roba di terze parti: la baseline
    // della .text deve essere presa quando il processo e' ancora pulito.
    intgBoot();
    std::snprintf(g_net.localFingerprint, sizeof(g_net.localFingerprint),
                  "%s", gateClientFingerprint().c_str());

    loc().load();

    g_engine = EngineConfig::createDefault();
    g_vsync = g_engine.quality.vsync;
    loadGfxPreset(); // preset scelto nel menu di gioco (prima della finestra: MSAA/scale)
    g_engine.logBanner();

    glfwSetErrorCallback(onGlfwError);
    if (!glfwInit()) {
        CrashHandler::note("glfwInit fallita");
        return -1;
    }
    initGamepad(); // Xbox XInput + DualSense/PS5 USB (hotplug)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    int msaa = g_engine.quality.msaaSamples;
    if (msaa < 0) msaa = 0;
    if (msaa > 8) msaa = 8;
    glfwWindowHint(GLFW_SAMPLES, msaa);

    // Finestra leggermente ridotta se renderScale < 1 (mobile profile)
    int winW = (int)(g_w * std::clamp(g_engine.quality.renderScale, 0.6f, 1.f));
    int winH = (int)(g_h * std::clamp(g_engine.quality.renderScale, 0.6f, 1.f));
    if (g_engine.platform == TargetPlatform::PC) { winW = g_w; winH = g_h; }

    char title[128];
    if (kIsGameEngine)
        std::snprintf(title, sizeof(title), "%s", kAppBrand);
    else
        std::snprintf(title, sizeof(title), "Blocky — %s", targetPlatformName(g_engine.platform));
    GLFWwindow* win = glfwCreateWindow(winW, winH, title, nullptr, nullptr);
    if (!win) {
        CrashHandler::note("glfwCreateWindow fallita");
        return -1;
    }
    CrashHandler::note("Finestra creata");
    g_mainWin = win;
    g_w = winW; g_h = winH;
    glfwMakeContextCurrent(win);
    setWindowIconB(win); // dopo il context — icona titolo + taskbar
    glfwSwapInterval(g_vsync ? 1 : 0);
    glfwSetFramebufferSizeCallback(win, onResize);
    glfwSetCursorPosCallback(win, onMouse);
    glfwSetMouseButtonCallback(win, onMouseBtn);
    glfwSetScrollCallback(win, onScroll);
    glfwSetWindowCloseCallback(win, onWindowCloseRequest);
    glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    // Chiusura improvvisa (X / Alt+F4 / Ctrl+C / spegnimento)
    // no atexit: persistFastExit solo a fine loop (altrimenti chiusura lenta)
    std::signal(SIGINT, onFatalSignal);
    std::signal(SIGTERM, onFatalSignal);
#ifdef _WIN32
    SetConsoleCtrlHandler(onConsoleCtrl, TRUE);
#endif

    if (!gladLoadGL(glfwGetProcAddress)) {
        CrashHandler::note("gladLoadGL fallita");
        return -1;
    }
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; // pad nei menu, senza UI settings
    if (kIsGameEngine)
        applyStudioTheme();
    else
        applyClassicRobloxTheme();
    acBoot();
    if (loadUserProfile(g_menu)) {
        std::printf("[Blocky] Username caricato: %s (%d B$) loggedIn=%d\n",
                    g_menu.username, g_menu.shopCoins, (int)g_menu.loggedIn);
        if (g_menu.loggedIn) {
            g_menu.chatLines.clear();
            g_menu.chatLines.push_back("[Sistema] Bentornato, " + std::string(g_menu.username) + "!");
            g_menu.chatLines.push_back("[Sistema] Accesso automatico — username salvato.");
            if (economy().tampered)
                g_menu.chatLines.push_back("[Sicurezza] " + economy().lastError);
        }
    } else {
        ecoBoot(g_menu.username[0] ? g_menu.username : "Player", EconomyState::kStartCoins);
        ecoSyncMenu(g_menu);
    }
    ImGui_ImplGlfw_InitForOpenGL(win, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    refreshGamesList();
    seedDefaultShop(g_menu);
    if (loadEquippedAvatar(g_catalog)) {
        std::printf("[Blocky] Avatar caricato: %d accessori\n", (int)g_catalog.items.size());
        syncCatalogItemsToInventory();
        syncEquippedToMenu();
        if (g_menu.loggedIn) {
            g_menu.chatLines.push_back("[Sistema] Avatar e Shop ripristinati dal salvataggio.");
        }
    }
    loadWalkLibraryIntoCatalog();
    if (g_animator.hasWalkOverride)
        std::printf("[Blocky] Walking Animation: %s\n", g_animator.walkOverride.displayName.c_str());
    std::printf("[Blocky] Shop: %d item | Games pubblicati in %s\n",
        (int)g_menu.shopItems.size(), publishedManifestPath().c_str());
    g_world.loadBaseplatePlace();

    GLuint vao = makeCubeVAO();
    g_bevelCube.build(0.11f); // limbs / corpo beveled
    g_partMeshes.buildAll(); // Ball / Cylinder / Wedge
    GLuint partP = makeProg(kPartVS, kPartFS);
    GLuint studP = makeProg(kPartVS, kStudFS);
    GLuint skinP = makeProg(kSkinVS, kPartFS);
    GLuint meshP = makeProg(kMeshTexVS, kMeshTexFS);
    // Pay-link B$: se i link sono placeholder → pagina demo locale
#ifdef _WIN32
    {
        char exePath[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, exePath, MAX_PATH);
        std::string dir(exePath);
        auto slash = dir.find_last_of("\\/");
        if (slash != std::string::npos) dir = dir.substr(0, slash + 1);
        std::string html = dir + "assets\\web\\buy_bucks.html";
        if (GetFileAttributesA(html.c_str()) != INVALID_FILE_ATTRIBUTES) {
            std::string url = std::string("file:///") + html;
            for (char& c : url) if (c == '\\') c = '/';
            bucksSetFallbackPayPage(url);
        }
        bucksEnsureLoaded();
    }
#else
    bucksEnsureLoaded();
#endif
    Texture2D studTex;
    if (!studTex.load(findStudTexture(), true))
        std::fprintf(stderr, "Avviso: stud.png non trovato.\n");
    glm::vec3 sun = g_lighting.sunDirection();
    (void)g_lighting.sunTint(); // pronto per tint shader futuro
    g_lighting.clampAll();
    SunSky sunSky;
    if (!sunSky.init())
        std::fprintf(stderr, "Avviso: Sole/cielo 2D non inizializzato.\n");
    // Mappa più piccola sui preset leggeri (Medium = 4 occlusori)
    if (!g_shadowMap.init(g_engine.quality.shadowOccluders >= 8 ? 2048 : 1024))
        std::fprintf(stderr, "Avviso: shadow map non inizializzata (ombre sole off).\n");
    if (!g_particles.init())
        std::fprintf(stderr, "Avviso: Particles non inizializzato.\n");

    AnimatedModel fbx;
    std::string fbxPath = findFbx();
    if (!fbxPath.empty()) fbx.load(fbxPath);
    bool fbxOk = fbx.loaded;

    AnimatedModel noobGlb;
    std::string noobPath = findNoobGlb();
    if (!noobPath.empty()) {
        noobGlb.load(noobPath);
        std::fprintf(stdout, "Noob GLB: %s (%s)\n", noobPath.c_str(), noobGlb.loaded ? "OK" : "FAIL");
    }
    if (!noobGlb.loaded) g_useGlbAvatar = false;

    ensureDefaultBeepWav();
    // Icona B su exe + file .blocky in Explorer
    // Solo il Game Engine registra i .blocky: e' lui che li apre in Studio.
    // Se anche il player (Blocky.exe) li registrasse, l'ultimo avviato
    // rubererebbe l'associazione e il doppio clic aprirebbe l'app sbagliata.
    if (kIsGameEngine)
        BlockyFileAssoc::registerPlaceAssociation();

    std::string launchPlace = BlockyFileAssoc::placePathFromArgs();

    if (kIsGameEngine) {
        // App dedicata: Home (non entra subito in Studio)
        if (!g_menu.loggedIn) {
            if (g_menu.username[0] == '\0')
                std::snprintf(g_menu.username, sizeof(g_menu.username), "Builder");
            g_menu.loggedIn = true;
            g_menu.showLogin = false;
        }
        g_screen = AppScreen::MainMenu;
        g_startupHomeDone = false;
        applyStudioTheme();
        std::printf("[%s] Home — SoundService · Animator · Model import\n", kAppBrand);

        // Doppio click su file .blocky: NON aprirlo qui (prima del loop, GL e
        // mesh non ancora caldi -> crash sul primo render). Lo si rimanda al
        // primo frame del loop, quando il motore e' completamente avviato.
        if (!launchPlace.empty())
            g_pendingOpenPlace = launchPlace;
    }
#ifdef _WIN32
    else if (!launchPlace.empty()) {
        // Il player (Blocky.exe) ha ricevuto un .blocky: i place si aprono nel
        // Game Engine, non qui. Lo passa a BlockyGameEngine.exe (stessa cartella)
        // e si chiude. Cosi' il doppio clic finisce sempre nel posto giusto,
        // qualunque exe Windows abbia lanciato.
        std::string dir = plat::selfDir();
        std::string ge = dir + "\\BlockyGameEngine.exe";
        if (plat::fileExists(ge)) {
            std::string params = "\"" + launchPlace + "\"";
            ShellExecuteA(nullptr, "open", ge.c_str(), params.c_str(), dir.c_str(), SW_SHOWNORMAL);
            glfwDestroyWindow(win);
            glfwTerminate();
            return 0;
        }
    }
#endif

    float lastT = (float)glfwGetTime();
    int frames = 0;
    float fpsAcc = 0.f;

    while (!glfwWindowShouldClose(win)) {
        glfwPollEvents(); // prima: joystick/hotplug aggiornati prima del poll pad

        // Apertura .blocky da doppio-clic: qui, non allo startup. A questo punto
        // il motore ha gia' fatto almeno un giro, GL e mesh sono pronti.
        if (!g_pendingOpenPlace.empty() && kIsGameEngine) {
            std::string path = g_pendingOpenPlace;
            g_pendingOpenPlace.clear();
            g_startupHomeDone = true;
            if (openPlacePath(path)) {
                returnToStudioEditor(win);
                CrashHandler::note(("Aperto .blocky: " + path).c_str());
            }
        }

        float now = (float)glfwGetTime();
        float dt = std::min(now - lastT, 0.05f);
        lastT = now;
        fpsAcc += dt; frames++;
        if (fpsAcc >= 0.5f) { g_fps = frames / fpsAcc; frames = 0; fpsAcc = 0.f; }

        updateCrashContext();

        // Autosave leggero ogni 30s (prima: full persist ogni 5s → freeze a caso)
        g_autosaveAcc += dt;
        if (g_autosaveAcc >= 30.f) {
            g_autosaveAcc = 0.f;
            persistAutosavePlaceOnly();
        }
        // Autosave bozza "Salva per pubblicare" (pubblica in futuro)
        if (g_publishDraftActive) {
            g_publishDraftAcc += dt;
            if (g_publishDraftAcc >= 30.f) {
                g_publishDraftAcc = 0.f;
                persistPublishDraftOnly();
            }
        }

        // Esc edge-trigger: evita chiusura accidentale / spam returnToMenu
        static bool escWasDown = false;
        bool escDown = glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS;
        bool escPressed = escDown && !escWasDown;
        escWasDown = escDown;
        if (escPressed) {
            if (kIsGameEngine && g_screen == AppScreen::MainMenu) {
                // resta in Home (Esc non fa nulla / non forza Studio)
            }
            else if (g_screen == AppScreen::MainMenu)
                glfwSetWindowShouldClose(win, true);
            else if (g_screen == AppScreen::Studio && g_studioTesting) {
                stopStudioTest(win);
            }
            else if (g_screen == AppScreen::AvatarPublisher) {
                returnToStudioEditor(win);
            }
            else if (g_screen == AppScreen::Playing) {
                setPlayMenuOpen(win, !g_playMenuOpen);
            }
            else if (g_screen == AppScreen::Studio && kIsGameEngine) {
                requestNav(win, PendingNav::GoHome);
            }
            else if (g_screen != AppScreen::Loading && !kIsGameEngine)
                returnToMenu(win);
        }

        // Game Engine: Home consentita; blocca solo Play/Loading hub player
        if (kIsGameEngine && (g_screen == AppScreen::Loading || g_screen == AppScreen::Playing)) {
            returnToStudioEditor(win);
        }

        g_walking = false;
        pollGamepad();
        const auto& pad = gamepad();

        // Start / Back = menu Esc (Play / Test) — senza UI controller dedicata
        if (pad.startPressed || pad.backPressed) {
            if (g_screen == AppScreen::Playing)
                setPlayMenuOpen(win, !g_playMenuOpen);
            else if (g_screen == AppScreen::Studio && g_studioTesting)
                setPlayMenuOpen(win, !g_playMenuOpen);
            else if (g_screen == AppScreen::Studio && !g_studioTesting && pad.startPressed)
                startStudioTest(win); // Start in editor = Test Play
        }

        if (g_screen == AppScreen::AvatarPublisher) {
            if (!io.WantCaptureKeyboard) {
                // Orbit / zoom camera preview (Q/E + scroll/WASD)
                if (glfwGetKey(win, GLFW_KEY_Q) == GLFW_PRESS)
                    g_catalog.camYaw -= 70.f * dt;
                if (glfwGetKey(win, GLFW_KEY_E) == GLFW_PRESS)
                    g_catalog.camYaw += 70.f * dt;
                if (glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS)
                    g_catalog.camDist = std::max(4.f, g_catalog.camDist - 8.f * dt);
                if (glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS)
                    g_catalog.camDist = std::min(40.f, g_catalog.camDist + 8.f * dt);
                // Hotkeys tool accessorio
                if (ImGui::IsKeyPressed(ImGuiKey_1)) g_catalog.tool = AccelTool::Move;
                if (ImGui::IsKeyPressed(ImGuiKey_2)) g_catalog.tool = AccelTool::Rotate;
                if (ImGui::IsKeyPressed(ImGuiKey_3)) g_catalog.tool = AccelTool::Scale;
                // Trascina maniglie gizmo (come Studio)
                {
                    double mx = 0, my = 0;
                    glfwGetCursorPos(win, &mx, &my);
                    bool lmb = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
                    if (g_accGizmoDrag) {
                        if (lmb && !g_rmbLook) updateCatalogGizmo((float)mx, (float)my);
                        else endCatalogGizmo();
                    }
                }
                if (g_catalog.selected >= 0 && g_catalog.selected < (int)g_catalog.items.size()) {
                    auto& a = g_catalog.items[g_catalog.selected];
                    float step = io.KeyShift ? 0.15f : 0.04f;
                    if (g_catalog.tool == AccelTool::Move) {
                        if (glfwGetKey(win, GLFW_KEY_LEFT) == GLFW_PRESS) a.offset.x -= step;
                        if (glfwGetKey(win, GLFW_KEY_RIGHT) == GLFW_PRESS) a.offset.x += step;
                        if (glfwGetKey(win, GLFW_KEY_UP) == GLFW_PRESS) a.offset.y += step;
                        if (glfwGetKey(win, GLFW_KEY_DOWN) == GLFW_PRESS) a.offset.y -= step;
                        if (glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS) a.offset.z -= step;
                        if (glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS) a.offset.z += step;
                    } else if (g_catalog.tool == AccelTool::Rotate) {
                        // Y = girare dx/sx, X = inclinazione avanti/dietro, Z = roll (A/D)
                        if (glfwGetKey(win, GLFW_KEY_LEFT) == GLFW_PRESS) a.rotation.y -= 60.f * dt;
                        if (glfwGetKey(win, GLFW_KEY_RIGHT) == GLFW_PRESS) a.rotation.y += 60.f * dt;
                        if (glfwGetKey(win, GLFW_KEY_UP) == GLFW_PRESS) a.rotation.x -= 60.f * dt;
                        if (glfwGetKey(win, GLFW_KEY_DOWN) == GLFW_PRESS) a.rotation.x += 60.f * dt;
                        if (glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS) a.rotation.z -= 60.f * dt;
                        if (glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS) a.rotation.z += 60.f * dt;
                    } else {
                        if (glfwGetKey(win, GLFW_KEY_UP) == GLFW_PRESS) a.scale *= (1.f + 1.2f * dt);
                        if (glfwGetKey(win, GLFW_KEY_DOWN) == GLFW_PRESS) a.scale *= (1.f - 1.2f * dt);
                        a.scale = glm::clamp(a.scale, glm::vec3(0.05f), glm::vec3(8.f));
                    }
                }
            }
            g_player.humanoid.stepPhysics(dt, g_playerPos.y, 0.f);
            // Anteprima WalkingAnimation sul personaggio in catalog
            if (g_catalog.previewWalk) {
                if (g_catalog.hasPendingWalk && g_catalog.pendingWalk.valid())
                    g_animator.setWalkOverride(g_catalog.pendingWalk);
                else
                    syncEquippedWalkToAnimator();
                g_player.humanoid.grounded = true;
                g_player.humanoid.walkPhase += dt * 8.f;
                g_animator.apply(g_player.humanoid, dt, true);
                g_player.update(dt, true);
            } else {
                syncEquippedWalkToAnimator();
                g_animator.apply(g_player.humanoid, dt, false);
                g_player.update(dt, false);
            }
        }
        else if (g_screen == AppScreen::Playing || g_screen == AppScreen::Studio) {
            // Trascina maniglie in Studio (non in Test)
            if (g_screen == AppScreen::Studio && !g_studioTesting) {
                double mx = 0, my = 0;
                glfwGetCursorPos(win, &mx, &my);
                bool lmb = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
                if (g_scaleDragging) {
                    if (lmb) updateScaleDrag((float)mx, (float)my);
                    else endScaleDrag();
                }
                if (g_moveDragging) {
                    if (lmb) updateMoveDrag((float)mx, (float)my);
                    else endMoveDrag();
                }
                if (g_rotateDragging) {
                    if (lmb) updateRotateDrag((float)mx, (float)my);
                    else endRotateDrag();
                }
                if (g_labelDragging) {
                    if (lmb) updateLabelDrag((float)mx, (float)my);
                    else endLabelDrag();
                }
                updateGuiButtonHover((float)mx, (float)my);
                if (!io.WantCaptureKeyboard) {
                    if (ImGui::IsKeyPressed(ImGuiKey_1)) g_tool = StudioTool::Select;
                    if (ImGui::IsKeyPressed(ImGuiKey_2)) g_tool = StudioTool::Move;
                    if (ImGui::IsKeyPressed(ImGuiKey_3)) g_tool = StudioTool::Scale;
                    if (ImGui::IsKeyPressed(ImGuiKey_4)) g_tool = StudioTool::Rotate;
                    if (ImGui::IsKeyPressed(ImGuiKey_F8))
                        enterAvatarPublisher();
                    if (ImGui::IsKeyPressed(ImGuiKey_F2))
                        g_showLuaGuide = !g_showLuaGuide;
                }
            }
            // Tastiera (WASD) NON bloccata dal cursore pad — solo dal menu Esc / campi testo
            const bool playUiOpen = (g_screen == AppScreen::Playing && g_playMenuOpen)
                || (g_screen == AppScreen::Studio && g_studioTesting && g_playMenuOpen);
            const bool inPlayMove = (g_screen == AppScreen::Playing && !g_playMenuOpen)
                || (g_screen == AppScreen::Studio && g_studioTesting && !g_playMenuOpen);
            // In Play/Test il cursore menu non deve restare attivo (altrimenti blocca tutto)
            if (inPlayMove)
                menuPadCursor().active = false;

            const bool menuCursor = menuPadCursor().active;
            const bool kbOk = !playUiOpen && !io.WantTextInput;
            const bool padOk = pad.connected && !playUiOpen && !menuCursor;

            // Inventario 1-9: subito qui (GLFW), prima di ImGui
            if (inPlayMove)
                pollBackpackSwitch(win);

            if (g_screen == AppScreen::Playing || g_studioTesting)
                syncPlayCursor(win);

            if (kbOk || padOk) {
                // Shift Lock / Prima persona
                if (g_screen == AppScreen::Playing || g_studioTesting) {
                    static bool shiftWas = false;
                    bool shiftKey = kbOk && (glfwGetKey(win, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS
                                 || glfwGetKey(win, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);
                    if ((shiftKey && !shiftWas) || (padOk && pad.yPressed)) {
                        g_shiftLock = !g_shiftLock;
                        if (g_shiftLock) {
                            g_firstPerson = false;
                            g_playerYaw = yawFromDir(camFlatForward());
                            g_orbitDist = glm::clamp(g_orbitDist, 8.f, 12.f);
                        }
                    }
                    shiftWas = shiftKey;

                    static bool vWas = false;
                    bool vKey = kbOk && glfwGetKey(win, GLFW_KEY_V) == GLFW_PRESS;
                    if (vKey && !vWas) {
                        g_firstPerson = !g_firstPerson;
                        if (g_firstPerson) {
                            g_shiftLock = false;
                            g_playerYaw = yawFromDir(camFlatForward());
                            g_orbitDist = kFpEnterDist;
                        } else {
                            g_orbitDist = std::max(g_orbitDist, kFpExitDist);
                        }
                        syncPlayCursor(win);
                    }
                    vWas = vKey;
                }

                float baseSpd = g_player.humanoid.walkSpeed;
                bool sprintKey = (kbOk && (glfwGetKey(win, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS
                           || glfwGetKey(win, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS))
                           || (padOk && pad.rt > 0.35f);
                // Se uno script Lua controlla WalkSpeed (stamina), niente moltiplicatore C++
                bool luaOwnsSpeed = scriptControls().walkSpeedOverride >= 0.f;
                float maxSpd = baseSpd * ((!luaOwnsSpeed && sprintKey) ? 1.65f : 1.f);

                // Look stick destro (destra=destra, su=su)
                if (padOk && (std::fabs(pad.rx) > 0.01f || std::fabs(pad.ry) > 0.01f)) {
                    float lookSpd = (g_shiftLock || g_firstPerson) ? 160.f : 140.f;
                    g_orbitYaw -= pad.rx * lookSpd * dt;   // stick destra → guarda destra
                    g_orbitPitch += pad.ry * lookSpd * dt; // stick su → guarda su
                    g_orbitPitch = glm::clamp(g_orbitPitch, g_firstPerson ? -85.f : -80.f,
                                             g_firstPerson ? 80.f : 80.f);
                    if ((g_shiftLock || g_firstPerson)
                        && (g_screen == AppScreen::Playing || g_studioTesting))
                        syncPlayerYawToCamera();
                    if (g_screen == AppScreen::Studio && g_mode == CamMode::Studio && !g_studioTesting) {
                        g_cam.yaw -= pad.rx * lookSpd * dt;
                        g_cam.pitch += pad.ry * (lookSpd * 0.85f) * dt;
                        g_cam.pitch = glm::clamp(g_cam.pitch, -89.f, 89.f);
                    }
                }

                // Freccia / Q-E / bumpers: ruota camera (stesso senso della visuale)
                if (!(g_screen == AppScreen::Playing && (g_shiftLock || g_firstPerson))) {
                    if ((kbOk && (glfwGetKey(win, GLFW_KEY_Q) == GLFW_PRESS || glfwGetKey(win, GLFW_KEY_LEFT) == GLFW_PRESS))
                        || (padOk && (pad.lb || pad.dpadL)))
                        g_orbitYaw += 90.f * dt; // sinistra
                    if ((kbOk && (glfwGetKey(win, GLFW_KEY_E) == GLFW_PRESS || glfwGetKey(win, GLFW_KEY_RIGHT) == GLFW_PRESS))
                        || (padOk && (pad.rb || pad.dpadR)))
                        g_orbitYaw -= 90.f * dt; // destra
                }

                if (g_screen == AppScreen::Studio && g_mode == CamMode::Studio && !g_studioTesting) {
                    float spd = maxSpd * dt;
                    glm::vec3 f = g_cam.front(), r = glm::normalize(glm::cross(f, glm::vec3(0,1,0)));
                    glm::vec3 move(0);
                    if (kbOk && glfwGetKey(win, GLFW_KEY_W)) move += f;
                    if (kbOk && glfwGetKey(win, GLFW_KEY_S)) move -= f;
                    if (kbOk && glfwGetKey(win, GLFW_KEY_A)) move -= r;
                    if (kbOk && glfwGetKey(win, GLFW_KEY_D)) move += r;
                    // Stick sinistro in Studio fly (solo se non siamo già in Test)
                    if (padOk) {
                        move += f * pad.ly;
                        move += r * pad.lx;
                    }
                    if (glm::length(move) > 0.01f) g_cam.pos += glm::normalize(move) * spd;
                    if ((kbOk && glfwGetKey(win, GLFW_KEY_SPACE)) || (padOk && pad.a)) g_cam.pos.y += spd;
                    if ((kbOk && glfwGetKey(win, GLFW_KEY_LEFT_CONTROL)) || (padOk && pad.b)) g_cam.pos.y -= spd;
                    if (padOk && pad.dpadU) g_orbitDist = std::max(4.f, g_orbitDist - 12.f * dt);
                    if (padOk && pad.dpadD) g_orbitDist = std::min(60.f, g_orbitDist + 12.f * dt);
                } else {
                    // Movimento player (Play / Test Play)
                    glm::vec3 fwd = camFlatForward();
                    glm::vec3 right = camFlatRight();
                    glm::vec3 wish(0);
                    if (kbOk && glfwGetKey(win, GLFW_KEY_W)) wish += fwd;
                    if (kbOk && glfwGetKey(win, GLFW_KEY_S)) wish -= fwd;
                    if (kbOk && glfwGetKey(win, GLFW_KEY_A)) wish -= right; // sinistra
                    if (kbOk && glfwGetKey(win, GLFW_KEY_D)) wish += right; // destra
                    if (padOk) {
                        wish += fwd * pad.ly;
                        wish += right * pad.lx;
                    }
                    if (!scriptControls().walkEnabled || g_player.humanoid.dead)
                        wish = glm::vec3(0.f);
                    float wishLen = glm::length(wish);
                    if (wishLen > 1.f)
                        wish /= wishLen; // stick: mantieni magnitudine ≤1
                    else if (wishLen > 0.001f)
                        wish = glm::normalize(wish);
                    else
                        wish = glm::vec3(0.f);

                    // Esposto a Lua (stamina sprint, ecc.)
                    scriptControls().moveHeld = wishLen > 0.001f;
                    scriptControls().sprintHeld = sprintKey && scriptControls().moveHeld;

                    // Stile Roblox: vai verso velocità target (niente ice-skate)
                    bool grounded = g_player.humanoid.grounded;
                    glm::vec3 targetVel = wish * maxSpd;
                    const bool hasInput = wishLen > 0.001f;
                    // A terra: accel rapida + freno netto. In aria: controllo leggero.
                    float rate = grounded
                        ? (hasInput ? 22.f : 28.f)   // stop più veloce del run → no slippery
                        : (hasInput ? 8.f  : 3.5f);
                    float t = 1.f - std::exp(-rate * dt);
                    g_moveVel.x += (targetVel.x - g_moveVel.x) * t;
                    g_moveVel.z += (targetVel.z - g_moveVel.z) * t;

                    float hSpd = std::sqrt(g_moveVel.x * g_moveVel.x + g_moveVel.z * g_moveVel.z);
                    if (hSpd > maxSpd * 1.02f) {
                        float s = maxSpd / hSpd;
                        g_moveVel.x *= s;
                        g_moveVel.z *= s;
                        hSpd = maxSpd;
                    }
                    if (!hasInput && hSpd < 0.35f)
                        g_moveVel.x = g_moveVel.z = 0.f;

                    g_playerPos.x += g_moveVel.x * dt;
                    g_playerPos.z += g_moveVel.z * dt;
                    // Animazione: solo con input (non durante la frenata)
                    g_walking = hasInput && grounded && !g_player.humanoid.dead;

                    if (g_shiftLock || g_firstPerson) {
                        // Shift Lock / FP: corpo = direzione camera
                        syncPlayerYawToCamera();
                    } else if (hasInput) {
                        // Terra o aria: gira verso dove ti muovi (WASD rispetto alla visuale)
                        float targetYaw = yawFromDir(wish);
                        float diff = targetYaw - g_playerYaw;
                        while (diff > 180.f) diff -= 360.f;
                        while (diff < -180.f) diff += 360.f;
                        float turn = grounded ? 22.f : 16.f;
                        g_playerYaw += diff * std::min(1.f, turn * dt);
                    }
                    // Senza input: tiene l'ultimo facing (anche in salto)

                    static bool spaceWas = false;
                    bool spaceKey = kbOk && glfwGetKey(win, GLFW_KEY_SPACE) == GLFW_PRESS;
                    // Lua può disattivare il jump (UserInputService.DisableJump / Humanoid.SetJumpEnabled)
                    if (scriptControls().jumpEnabled) {
                        if ((spaceKey && !spaceWas) || (padOk && pad.aPressed))
                            g_player.humanoid.tryJump();
                    }
                    spaceWas = spaceKey;

                    // Sword: cooldown / anim + pad X attacca
                    if (g_swordCooldown > 0.f) g_swordCooldown = std::max(0.f, g_swordCooldown - dt);
                    if (g_swordSwing > 0.f) g_swordSwing = std::max(0.f, g_swordSwing - dt * 4.5f);
                    if (padOk && pad.xPressed)
                        trySwordAttack();
                }
                // F5 = Test / F1 = Stop test (edge)
                static bool f5Was = false, f1Was = false;
                bool f5 = kbOk && glfwGetKey(win, GLFW_KEY_F5) == GLFW_PRESS;
                bool f1 = kbOk && glfwGetKey(win, GLFW_KEY_F1) == GLFW_PRESS;
                if (g_screen == AppScreen::Studio) {
                    if (f5 && !f5Was) {
                        if (g_studioTesting) stopStudioTest(win);
                        else startStudioTest(win);
                    }
                    if (f1 && !f1Was && g_studioTesting)
                        stopStudioTest(win);
                }
                f5Was = f5;
                f1Was = f1;
            }

            if (inPlayMove) {
                double mx = 0, my = 0;
                glfwGetCursorPos(win, &mx, &my);
                updateGuiButtonHover((float)mx, (float)my);
            }

            if (kbOk && g_screen == AppScreen::Studio && g_mode == CamMode::Studio && !g_studioTesting) {
                if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_P)) insertNewPart();
                if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_M)) insertImportedModel();
                if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_T))
                    insertNewTextLabel();
                else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_T))
                    insertNewLabel();
                if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_B))
                    insertNewTextButton();
                if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_U)) insertSound();
                if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_A))
                    beginAnimateSelectedModel();
                if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_R))
                    beginRigPlayer();
                if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_I))
                    g_showPlayerAnimPublisher = true;
                if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
                    if (g_undo.applyUndo(g_world, g_selectedPart))
                        g_exportMsg = "Annullato";
                }
                if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y)) {
                    if (g_undo.applyRedo(g_world, g_selectedPart))
                        g_exportMsg = "Ripetuto";
                }
                if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D)) duplicateSelected();
                if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_L)) addScriptToSelected();
                if (ImGui::IsKeyPressed(ImGuiKey_Delete)) deleteSelected();
                // Ctrl+0 = reset testo/UI (FontGlobalScale + Label font)
                if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_0)) {
                    io.FontGlobalScale = 1.f;
                    if (g_selectedLabel >= 0 && g_selectedLabel < (int)g_world.labels.size())
                        g_world.labels[g_selectedLabel].fontScale = 1.f;
                    g_exportMsg = "Testo UI riportato a normale (Ctrl+0)";
                }
                // + / - : Label → font · Part → size (solo con Shift, evita ingrandimenti accidentali)
                if (ImGui::IsKeyPressed(ImGuiKey_Equal) || ImGui::IsKeyPressed(ImGuiKey_KeypadAdd)) {
                    if (g_selectedLabel >= 0 && g_selectedLabel < (int)g_world.labels.size())
                        g_world.labels[g_selectedLabel].fontScale =
                            std::min(2.5f, g_world.labels[g_selectedLabel].fontScale * 1.15f);
                    else if (io.KeyShift)
                        scaleSelected(1.25f);
                }
                if (ImGui::IsKeyPressed(ImGuiKey_Minus) || ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract)) {
                    if (g_selectedLabel >= 0 && g_selectedLabel < (int)g_world.labels.size())
                        g_world.labels[g_selectedLabel].fontScale =
                            std::max(0.5f, g_world.labels[g_selectedLabel].fontScale / 1.15f);
                    else if (io.KeyShift)
                        scaleSelected(0.8f);
                }
            }
            if (kbOk && g_screen == AppScreen::Studio && !g_studioTesting) {
                if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_S))
                    doSavePlaceAs();
                else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S))
                    doSavePlace();
                if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_U))
                    doSaveForPublish();
                if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O))
                    doOpenPlace();
                if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_P)) {
                    if (publishCurrentGame())
                        refreshGamesList();
                }
            }
            if (kbOk && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_E)) doExport();

            // Fisica a passo fisso (QualityProfile.physicsHz)
            acClampHumanoid(g_player.humanoid);
            acSetLinkedUser(g_menu.username);
            acTickIntegrity(dt, (float)glfwGetTime());
            {
                std::string warn;
                if (acTakePlayerWarning(warn)) {
                    g_menu.chatLines.push_back("[Sicurezza] " + warn);
                    g_exportMsg = warn;
                    std::fprintf(stderr, "%s\n", warn.c_str());
                }
                if (economy().banned && g_screen == AppScreen::Playing) {
                    g_menu.chatLines.push_back("[Sicurezza] Account bannato — ritorno al menu.");
                    returnToMenu(win);
                }
            }
            {
                static float physAcc = 0.f;
                float hz = std::max(20.f, g_engine.quality.physicsHz);
                float step = 1.f / hz;
                physAcc += dt;
                int guard = 0;
                while (physAcc >= step && guard++ < 8) {
                    if (g_screen == AppScreen::Playing ||
                        (g_screen == AppScreen::Studio && g_mode == CamMode::Play)) {
                        stepPlayerPhysicsWithCollision(step);
                    } else {
                        g_player.humanoid.stepPhysics(step, g_playerPos.y, 0.f);
                    }
                    physAcc -= step;
                }
                if (physAcc > step * 2.f) physAcc = 0.f;
            }
            acValidateMovement(g_playerPos, g_moveVel, dt, (float)glfwGetTime(),
                               g_player.humanoid.grounded, g_player.humanoid.velocityY);
            g_player.update(dt, g_walking);
            updateFriendPlayers(dt);
            // Multiplayer LAN: invia/ricevi trasformazioni
            if (g_screen == AppScreen::Playing && g_net.role != NetRole::Offline) {
                g_net.poll((float)glfwGetTime());
                // Il registro degli script vive nel runtime Lua: qui lo si
                // travasa alla rete solo quando cambia, cosi' un chunk
                // iniettato a meta' partita finisce nel report successivo.
                if (g_lua.scriptSetChanged) {
                    g_net.localScriptHashes = g_lua.scriptHashes;
                    g_lua.scriptSetChanged = false;
                    g_net.scriptReportAcc = 99.f;   // manda subito, non fra 3s
                }
                g_net.sendLocalState(dt, g_playerPos, g_playerYaw, g_walking);
                g_net.updateDisplay(dt);
                if (g_remoteBodies.size() < g_net.remotes.size())
                    g_remoteBodies.resize(g_net.remotes.size());
                for (size_t i = 0; i < g_net.remotes.size(); ++i)
                    g_remoteBodies[i].update(dt, g_net.remotes[i].walking);
                std::snprintf(g_menu.netStatus, sizeof(g_menu.netStatus), "%s (%d)",
                              g_net.status, g_net.playerCount());
            }

            // --- GATE ONLINE ---
            // FUORI dal blocco di rete di proposito: se la rete e' Offline
            // (init fallita, server chiuso, host di se stesso) quel blocco non
            // gira, e prima il gate veniva saltato -> si entrava lo stesso.
            // Qui gira a ogni frame in Playing, qualunque sia lo stato di rete.
            if (g_screen == AppScreen::Playing && !g_gatePassed && !gateOfflineAllowed()) {
                // La DECISIONE e' un bool: niente stringhe che descrivono i
                // server. Il codice-motivo serve solo al log di sviluppo, e
                // le sue stringhe spariscono dal binario di distribuzione.
                bool denied = false;
                int reasonCode = 0;   // 1=offline 2=host 3=reject 4=badclient 5=timeout
                if (g_net.role == NetRole::Offline)      { denied = true; reasonCode = 1; }
                else if (g_net.role == NetRole::Host)    { denied = true; reasonCode = 2; }
                else if (g_net.rejected)                 { denied = true; reasonCode = 3; }
                else if (g_net.connected) {
                    GateResult gr = gateEvaluate(g_net);
                    if (gr.state == GateState::Allowed) g_gatePassed = true;
                    else { denied = true; reasonCode = 4; }
                } else if (glfwGetTime() > g_gateDeadline) { denied = true; reasonCode = 5; }

                if (denied) {
                    // Schermata: sempre generica, uguale per ogni motivo.
                    std::snprintf(g_gateTitle, sizeof(g_gateTitle), "Blocky needs Wi-Fi");
                    std::snprintf(g_gateMsg, sizeof(g_gateMsg),
                                  "Controlla la connessione e riprova.");
                    // Il dettaglio va solo nel canale sicurezza: in distribuzione
                    // sparisce dal binario, cosi' non si legge nulla sui server.
                    LOG_SEC("gate", "negato codice=%d", reasonCode);
                    g_net.shutdown();
                    returnToMenu(win);
                }
            }
            // In Rigging editor non sovrascrivere le rotazioni che stai editando
            if (!g_showPlayerRigEditor && !g_player.humanoid.dead)
                g_animator.apply(g_player.humanoid, dt, g_walking);
            updateModelAnimPlayback(dt);
            // Heartbeat Lua solo in Play / Test — mai nell'editor Studio
            if (g_screen == AppScreen::Playing
                || (g_screen == AppScreen::Studio && g_studioTesting)) {
                g_lua.budgetUs = g_engine.quality.luaBudgetUs;
                g_lua.fireHeartbeat(dt);
                pollPartTouched();
                tickBoomAutoFuse(dt);
                g_lua.drainRemoteEvents();
            }
        }

        g_particles.update(dt);

        // Ciclo giorno/notte solo nelle schermate col mondo 3D: hub e publisher
        // restano sempre in pieno giorno (l'avatar deve vedersi bene).
        const bool worldScreen = (g_screen != AppScreen::MainMenu
                               && g_screen != AppScreen::AvatarPublisher);
        glm::vec3 toSunNow;
        glm::vec3 skyZenith, skyHorizon;
        // Il ciclo scorre solo giocando o in Test: in Studio l'ora resta ferma
        const bool timeRuns = (g_screen == AppScreen::Playing)
            || (g_screen == AppScreen::Studio && g_studioTesting);
        if (worldScreen) {
            g_lighting.tick(dt, timeRuns);
            toSunNow = g_lighting.toSun();
            sun = g_lighting.sunDirection();
            g_night = g_lighting.nightAmount();
            g_lighting.skyColors(skyZenith, skyHorizon);
        } else {
            g_lighting.clampAll();
            toSunNow = glm::normalize(glm::vec3(-0.35f, 0.85f, 0.28f));
            sun = -toSunNow;
            g_night = 0.f;
            skyZenith = {0.42f, 0.65f, 0.95f};
            skyHorizon = {0.69f, 0.84f, 0.95f};
        }
        {
            glm::vec3 sky = glm::mix(skyHorizon, skyZenith, 0.35f);
            sky = glm::mix(sky, g_lighting.outdoorAmbient, g_lighting.ambient * 0.35f * (1.f - g_night));
            glClearColor(sky.r, sky.g, sky.b, 1.f);
        }
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float aspect = g_w / (float)g_h;
        glm::mat4 proj = glm::perspective(glm::radians(g_cam.fov), aspect, 0.1f, 800.f);
        auto camFromView = [](const glm::mat4& view) {
            return glm::vec3(glm::inverse(view)[3]);
        };
        auto drawSunSky = [&](const glm::mat4& view) {
            sunSky.draw(view, proj, toSunNow, camFromView(view),
                        g_night, skyZenith, skyHorizon);
        };

        if (g_screen == AppScreen::MainMenu) {
            clearDynamicLights();
            g_menu.charSpin += dt * 40.f;
            glm::mat4 view = glm::lookAt(glm::vec3(8, 6, 14), glm::vec3(0, 2, 0), glm::vec3(0, 1, 0));
            drawSunSky(view);
            glm::mat4 bm = glm::scale(glm::translate(glm::mat4(1.f), {0,-1,0}), {20,1,20});
            drawCube(vao, studP, bm, view, proj, {0.55f,0.56f,0.55f}, sun, true, studTex.id);
            GLuint playerVao = g_bevelCube.vao ? g_bevelCube.vao : vao;
            BlockDrawContext bctx{ playerVao, partP, view, proj, sun };
            bool hideBody[(int)AvatarSlot::Count] = {};
            fillBodyHideMask(g_catalog, hideBody);
            g_player.draw(bctx, drawCubePlayer, {0, 0, 0}, g_menu.charSpin - 30.f, hideBody);
            drawAvatarAccessories(meshP, view, proj, sun, {0,0,0}, g_menu.charSpin - 30.f, g_player.humanoid, vao, partP);
            g_particles.draw(view, proj);
            drawExplosionCubes(vao, partP, view, proj, sun);
        }
        else if (g_screen == AppScreen::Loading) {
            g_loadTimer += dt;
            if (g_loadTimer >= g_loadDur) finishLoading();
        }
        else if (g_screen == AppScreen::AvatarPublisher) {
            clearDynamicLights();
            float yawR = glm::radians(g_catalog.camYaw);
            float pitchR = glm::radians(g_catalog.camPitch);
            glm::vec3 target{0, 2.8f, 0};
            glm::vec3 eye = target + glm::vec3(
                g_catalog.camDist * cosf(pitchR) * sinf(yawR),
                g_catalog.camDist * (-sinf(pitchR)),
                g_catalog.camDist * cosf(pitchR) * cosf(yawR));
            glm::mat4 view = glm::lookAt(eye, target, {0,1,0});
            g_lastView = view;
            g_lastProj = proj;
            drawSunSky(view);
            glm::mat4 bm = glm::scale(glm::translate(glm::mat4(1.f), {0,-1,0}), {30,1,30});
            drawCube(vao, studP, bm, view, proj, {0.55f,0.56f,0.55f}, sun, true, studTex.id);
            GLuint playerVao = g_bevelCube.vao ? g_bevelCube.vao : vao;
            BlockDrawContext bctx{ playerVao, partP, view, proj, sun };
            bool hideBody[(int)AvatarSlot::Count] = {};
            fillBodyHideMask(g_catalog, hideBody);
            g_player.draw(bctx, drawCubePlayer, {0, 0, 0}, 180.f, hideBody);
            drawAvatarAccessories(meshP, view, proj, sun, {0,0,0}, 180.f, g_player.humanoid, vao, partP, true);
            drawCatalogGizmos(vao, partP, view, proj, sun);
            g_particles.draw(view, proj);
            drawExplosionCubes(vao, partP, view, proj, sun);
        }
        else {
            glm::mat4 view = (g_screen == AppScreen::Studio && g_mode == CamMode::Studio && !g_studioTesting)
                ? g_cam.view() : playCameraView();
            drawSunSky(view);
            renderWorld3D(vao, studP, partP, studTex.id, skinP, meshP, &noobGlb, g_useGlbAvatar && noobGlb.loaded,
                          view, proj, sun);
            if (g_showFbx && fbxOk && g_screen == AppScreen::Studio && !g_studioTesting) {
                fbx.update(dt);
                glm::mat4 fm = fbx.placement(6.f, 0.f, {12.f, 0.f, 0.f});
                glUseProgram(skinP);
                glUniformMatrix4fv(glGetUniformLocation(skinP,"uModel"),1,GL_FALSE,glm::value_ptr(fm));
                glUniformMatrix4fv(glGetUniformLocation(skinP,"uView"),1,GL_FALSE,glm::value_ptr(view));
                glUniformMatrix4fv(glGetUniformLocation(skinP,"uProj"),1,GL_FALSE,glm::value_ptr(proj));
                glUniform3fv(glGetUniformLocation(skinP,"uSun"),1,glm::value_ptr(sun));
                fbx.draw(skinP);
            }
            g_particles.draw(view, proj);
            drawExplosionCubes(vao, partP, view, proj, sun);
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        // Cursore pad nei menu (dopo GLFW mouse, prima di NewFrame)
        {
            bool menuPad = gamepad().connected && (
                g_screen == AppScreen::MainMenu
                || (g_screen == AppScreen::Playing && g_playMenuOpen)
                || (g_screen == AppScreen::Studio && g_studioTesting && g_playMenuOpen)
                || (g_screen == AppScreen::Studio && !g_studioTesting)
                || g_screen == AppScreen::AvatarPublisher);
            applyMenuPadCursor(menuPad, dt, (float)g_w, (float)g_h);
        }
        ImGui::NewFrame();
        // Evita testo UI enorme se FontGlobalScale è stato toccato per sbaglio
        if (io.FontGlobalScale < 0.85f || io.FontGlobalScale > 1.15f)
            io.FontGlobalScale = 1.f;
        // Play/Test: no Nav tastiera (Tab/frecce altrimenti rubano focus e bloccano look)
        if (playCameraActive()) {
            io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
            io.WantCaptureMouse = false;
            io.WantCaptureKeyboard = false;
        } else {
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        }

        if (g_screen == AppScreen::MainMenu) {
            if (kIsGameEngine) {
                if (!g_startupHomeDone) {
                    g_startupHomeDone = true;
                    g_world.loadBaseplatePlace();
                    g_placeFilePath.clear();
                    clearPlaceDirty();
                    if (autosaveFileExists())
                        g_showAutosavePrompt = true;
                }
                drawGameEngineHome(win, dt);
            } else {
            // Hub: se un tool si aggancia / è aperto → kill + warning (+ ban al 2°)
            acSetLinkedUser(g_menu.username);
            acScanInjectAndWarn(now);
            {
                std::string warn;
                if (acTakePlayerWarning(warn))
                    g_menu.chatLines.push_back("[Sicurezza] " + warn);
                if (economy().banned) {
                    g_menu.loggedIn = false;
                    g_menu.showLogin = true;
                    g_menu.loginError = "BANNATO PERMANENTE — cheat / inject ripetuto";
                }
            }
            syncEquippedToMenu();
            drawClassicMainMenu(g_menu, g_games, dt, (float)g_w, (float)g_h, now);

            // Esito del gate: senza questo torneresti al menu senza sapere perche'.
            if (g_gateMsg[0]) {
                ImGui::SetNextWindowPos({(float)g_w * 0.5f, (float)g_h * 0.5f},
                                        ImGuiCond_Always, {0.5f, 0.5f});
                ImGui::SetNextWindowSize({420, 0});
                ImGui::Begin("##gate", nullptr,
                             ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
                ImGui::PushStyleColor(ImGuiCol_Text, {1.f, 0.45f, 0.4f, 1.f});
                ImGui::SetWindowFontScale(1.35f);
                ImGui::TextWrapped("%s", g_gateTitle);
                ImGui::SetWindowFontScale(1.0f);
                ImGui::PopStyleColor();
                ImGui::Spacing();
                ImGui::TextWrapped("%s", g_gateMsg);
                ImGui::Spacing();
                // Niente riferimenti a server / .bat / IP: il giocatore non
                // deve vedere come e' fatta l'infrastruttura.
                if (ImGui::Button("OK", {-1, 34}))
                    g_gateMsg[0] = '\0';
                ImGui::End();
            }
            if (g_menu.wantPlay && g_menu.launchGame >= 0) {
                startLoadingGame(g_menu.launchGame);
                g_menu.wantPlay = false;
            }
            if (g_menu.wantJoinIp && g_menu.launchGame >= 0) {
                g_joinByIp = true;
                startLoadingGame(g_menu.launchGame);
                g_menu.wantJoinIp = false;
            }
            // Studio / Avatar Catalog rimossi da Blocky — solo in BlockyGameEngine.exe
            g_menu.wantBuild = false;
            g_menu.wantCatalog = false;
            g_menu.wantImportModel = false;
            if (g_menu.wantEquipShop >= 0) {
                equipShopItem(g_menu.wantEquipShop);
                g_menu.wantEquipShop = -1;
            }
            if (g_menu.wantUnequipShop >= 0) {
                unequipShopItem(g_menu.wantUnequipShop);
                g_menu.wantUnequipShop = -1;
            }
            if (g_menu.wantUnequipEquipped != -1) {
                unequipEquippedIndex(g_menu.wantUnequipEquipped);
                g_menu.wantUnequipEquipped = -1;
            }
            if (g_menu.wantQuit) glfwSetWindowShouldClose(win, true);
            }
        }
        else if (g_screen == AppScreen::Loading) {
            drawLoadingScreen(g_loadTimer / g_loadDur, g_activeGame.title, (float)g_w, (float)g_h, now);
        }
        else if (g_screen == AppScreen::Studio) {
            drawStudioUi(fbx, fbxOk);
            drawGizmoOverlayHud();
            drawSavePlacePrompt(win);
        }
        else if (g_screen == AppScreen::AvatarPublisher) {
            drawAvatarPublisherUi();
        }
        else if (g_screen == AppScreen::Playing) {
            drawPlayingHud(win);
        }

        drawMenuPadCursor();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapInterval(g_vsync ? 1 : 0);
        glfwSwapBuffers(win);
    }

    persistFastExit();
    g_sounds.stopAll();
    g_net.shutdown();
    // Skip destroy GL pesanti: il processo sta uscendo (OS libera tutto)
    g_lua.shutdown();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    return 0;
}
