
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
#include "engine/PoolConfig.h"
#include "engine/NetGate.h"
#include "platform/Log.h"
#include "platform/SystemApi.h"
#include "platform/FilePick.h"
#include "platform/Identity.h"
#include "platform/FrameProfiler.h"
#include "platform/PlatformServices.h"
#include "engine/Ragdoll.h"
#include "engine/BlockyPhysics.h"
#include "engine/R6PlayerExport.h"
#include "engine/UndoStack.h"
#include "platform/BlockyFileAssoc.h"
#include "platform/CrashHandler.h"
#include "platform/TestSessionGuard.h"
#include "platform/ConfigFlags.h"
#include "platform/EngineConfig.h"
#include "platform/GamepadInput.h"
#include "platform/PortMap.h"

#include <atomic>
#include <chrono>
#include <thread>
#include "render/BeveledCube.h"
#include "render/PartMeshes.h"
#include "render/Texture2D.h"
#include "render/GlShaders.h"
#include "render/WindowIcon.h"
#include "render/ShadowMap.h"
#include "render/SunSky.h"
#include "render/Bloom.h"
#include "render/Particles.h"
#include "engine/Purchase.h"

// Dichiarata qui, PRIMA di ClassicMenu.h: quell'header (incluso subito sotto)
// la usa per decidere se mostrare il pulsante "Studio" nella Hub. Prima
// stava piu' in basso nel file, dopo l'#include — non si sarebbe nemmeno
// compilato appena ClassicMenu.h avesse provato a leggerla.
static bool kIsGameEngine = false;

#include "ui/ClassicMenu.h"
#include "ui/StudioChrome.h"
#include "studio/StudioIcons.h"
#include "ui/MenuPadCursor.h"
#include "ui/Loc.h"
#include "ui/TouchControls.h"
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

static const char* kAppBrand = "Blocky";
static const char* kAppShort = "Blocky";

void blockySetEngineMode(bool isGameEngine) {
    kIsGameEngine = isGameEngine;
    if (isGameEngine) {
        kAppBrand = "Blocky Game Engine";
        kAppShort = "Game Engine";
    } else {
        kAppBrand = "Blocky";
        kAppShort = "Blocky";
    }
}

static std::string g_stagedLoadPath;
void blockyStageLoadPath(const char* path) {
    g_stagedLoadPath = path ? path : "";
}

static int g_w = 1280, g_h = 720;

// Risoluzione a cui viene DISEGNATA la scena, che non e piu per forza quella
// della finestra. Il composite del Bloom la riporta a schermo pieno
// ricostruendola (Catmull-Rom + nitidezza adattiva).
//
// Prima il cursore "Render scale" in Studio non faceva assolutamente nulla su
// PC: toccava solo la dimensione della finestra alla creazione, e per PC
// quella riga veniva pure annullata subito dopo. La leva di prestazioni piu
// importante che esista era scollegata.
static int g_renderW = 1280, g_renderH = 720;

// Quanta nitidezza applicare quando si ricostruisce. 0 = nessuna.
static float g_upscaleSharpen = 0.6f;

static float g_fps = 0.f;
// Tabella delle prestazioni per fase (F4). Vedi platform/FrameProfiler.h.
static bool g_showProfiler = false;

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
static int g_selectedService = -1;

static Animator g_animator;
static Ragdoll g_ragdoll;
static ParticleSystem g_particles;
static UndoStack g_undo;
static WorkspacePart g_xformBefore{};
static bool g_xformTrack = false;
static std::vector<uint8_t> g_touching;
static std::vector<StaticMesh> g_wsMeshes;
static std::vector<AnimatedModel> g_wsRigged;
static NavGrid g_nav;
static void rebuildNavGrid();

static constexpr float kWorldModelTargetSize = 5.5f;

#include "main_ui/DebugAndNetUtils.h"
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
static int g_animStepFrames = 5;
static bool g_animLiveCapture = false;
static bool g_animHaveClipboard = false;
static AnimateFrame g_animClipboard{};

static bool g_selectedPlayer = false;
static int g_selectedPlayerPart = -1;
static bool g_selectedSpawn = false;
static int g_selectedLabel = -1;
static int g_selectedBackpack = -1;
// Slot mostrato in mano mentre si regola la presa in Studio; -1 = nessuno.
// Sta qui, fra le globali, perche lo leggono sia il disegno del mondo sia
// il pannello delle proprieta, che vengono inclusi prima dell HUD.
static int g_gripPreviewSlot = -1;
// Regolazione della presa trascinando nel viewport: 0 = spenta, 1 = sposta,
// 2 = ruota. Non e' un gizmo 3D perche l'oggetto in mano non ha una posizione
// nel mondo su cui piantare le frecce — vive attaccato alla mano. Si trascina
// quindi direttamente, con l'oggetto gia' disegnato in mano che segue il mouse.
static int g_gripEditMode = 0;
static bool g_labelDragging = false;
static int g_labelDragCorner = -1;
static float g_labelGrabDX = 0.f, g_labelGrabDY = 0.f;
static float g_labelDragFixedX = 0.f, g_labelDragFixedY = 0.f;
static int g_uiHoverLabel = -1;
static int g_uiPressedLabel = -1;
static bool g_showCreatePanel = false;
static bool g_showPlayerRigPrompt = false;
static bool g_showBackpackOffer = false;
static std::string g_backpackOfferName;
static std::string g_backpackOfferPath;
static char g_labelNameBuf[64] = "Label";
static char g_labelTextBuf[128] = "Hello Blocky";
static char g_labelScriptBuf[16384] = {};
static float g_studioUiTime = 0.f;
static bool g_showPlayerRigEditor = false;
static bool g_playerRigEditorNeedsPlace = false;
static std::vector<GameEntry> g_games;
static ClassicMenuState g_menu;
static GameEntry g_activeGame;
static float g_loadTimer = 0.f;
static float g_loadDur = 1.8f;
static std::string g_exportMsg;
static std::string g_placeFilePath;
static bool g_placeDirty = false;

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
static float g_playerYaw = 180.f;
static float g_orbitYaw = 0.f;
static float g_orbitPitch = -22.f;
static float g_orbitDist = 14.f;
static bool g_walking = false;
static float g_footstepAcc = 0.f;
static bool g_shiftLock = false;
static bool g_firstPerson = false;
static bool g_playCursorLocked = false;
static constexpr float kFpEnterDist = 4.5f;
static constexpr float kFpExitDist = 7.0f;
static glm::vec3 g_moveVel{0, 0, 0};
static bool g_showFbx = false;
static bool g_useGlbAvatar = false;
static bool g_vsync = true;
static bool g_showExplorer = true;
static bool g_showProps = true;
static bool g_showScript = false;
static bool g_showLuaGuide = true;
static bool g_showScriptCreatedGuide = false;
static bool g_showAnimDock = false;
static bool g_showPlayerAnimPublisher = false;
static char g_lastPublishedAnimId[96] = {};
static int g_selectedPart = -1;
static std::string g_pendingOpenPlace;
static bool g_pendingModelImport = false;
static std::string g_pendingModelImportPath;
static char g_partNameBuf[64] = {};
static char g_partScriptBuf[16384] = {};
static char g_modelScriptBuf[16384] = {};

static void* publisherHwnd();
static bool cameraRayHitParts(const glm::vec3& o, const glm::vec3& dirUnit, float maxDist, float& tOut);
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
static void drawHamburgerMenuHud();
static void drawChatBubbleHud();
static int effectiveHeldSlot();
static void drawSpeechBubbles();
static bool sendPlayChat(const char* msg);
static void drawGizmoOverlayHud();
static void pollBackpackSwitch(GLFWwindow* win);
static void trySwordAttack();
static void tryUseEquippedItem();
static void drawEquippedSword(GLuint vao, GLuint partP, const glm::mat4& view,
                              const glm::mat4& proj, const glm::vec3& sun);
static void drawEquippedTorch(GLuint vao, GLuint partP, const glm::mat4& view,
                              const glm::mat4& proj, const glm::vec3& sun);
static void drawEquippedItemMesh(GLuint vao, GLuint partP, GLuint meshP,
                                 const glm::mat4& view, const glm::mat4& proj,
                                 const glm::vec3& sun);
static bool playCameraActive();
static bool equipShopItem(int idx);
static bool unequipShopItem(int idx);
static void syncEquippedToMenu();

enum class StudioTool { Select, Move, Scale, Rotate };
static StudioTool g_tool = StudioTool::Select;
static bool g_editingPartScript = false;

static bool g_scaleDragging = false;
static int g_scaleAxis = -1;
static int g_scaleSign = 0;
static float g_scaleStartFace = 0.f;
static glm::vec3 g_scaleStartPos{0}, g_scaleStartSize{1};
static constexpr float kHandleSize = 0.48f;

static bool g_moveDragging = false;
static int g_moveAxis = -1;
static glm::vec3 g_moveStartPos{0};
static glm::vec3 g_moveStartHit{0};

static bool g_rotateDragging = false;
static int g_rotateAxis = -1;
static float g_rotateStartAngle = 0.f;
static float g_rotateLiveDelta = 0.f;
static glm::vec3 g_rotateStartRot{0};
static int g_gizmoHoverAxis = -1;
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
static bool g_loadApiCompleta = false;
static bool g_loadStamina = false;
static std::string g_luaRunMsg;
static glm::mat4 g_lastView{1.f}, g_lastProj{1.f};
static EngineConfig g_engine = EngineConfig::createDefault();

/** Aggiorna la risoluzione interna dalla qualita scelta. */
static void updateRenderResolution() {
    const float s = std::clamp(g_engine.quality.renderScale, 0.5f, 1.f);
    g_renderW = std::max(64, (int)(g_w * s));
    g_renderH = std::max(64, (int)(g_h * s));
}
static bool g_showPlatformPanel = false;
static bool g_showChat = false;
static bool g_playMenuOpen = false;

#include "main_ui/LuaSyntaxEditor.h"

#include "main_ui/GraphicsSetup.h"

static constexpr int kMaxDynLights = 8;
struct DynLight {
    glm::vec3 pos{0};
    glm::vec3 color{1};
    float range = 20.f;
};
static DynLight g_dynLights[kMaxDynLights];
static int g_dynLightCount = 0;
static glm::vec3 g_renderCamPos{0, 8, 20};

static constexpr int kMaxShadowBoxes = 8;
static glm::vec3 g_occCenter[kMaxShadowBoxes];
static glm::vec3 g_occHalf[kMaxShadowBoxes];
static glm::mat3 g_occRot[kMaxShadowBoxes];
static int g_occCount = 0;
static int g_bodyOccCount = 0;
static float g_night = 0.f;
static glm::vec3 g_sunColor{1.f};
static glm::vec3 g_ambientTint{1.f};
static glm::vec3 g_skyZenith{0.42f, 0.65f, 0.95f};
static glm::vec3 g_skyHorizon{0.69f, 0.84f, 0.95f};
static ShadowMap g_shadowMap;
static bool g_sunShadowOn = false;
static Bloom g_bloom;

#include "main_ui/RenderPrimitives.h"

#include "main_ui/AssetFinders.h"

#include "main_ui/CameraAndInput.h"

#include "main_ui/SelectionAndInput.h"

#include "main_ui/WorldRenderPipeline.h"

#include "main_ui/ScreenTransitions.h"

#include "main_ui/AvatarShop.h"

#include "main_ui/SaveLoadPublish.h"

#include "main_ui/WorldLabelsUi.h"
#include "main_ui/RigAnimatorUi.h"
#include "main_ui/AvatarRigEditors.h"

#include "main_ui/PlayingHudPanel.h"

#include "main_ui/EngineDiagnostics.h"

// Risorse create una volta sola nell'init di Blocky_RunEngine() e usate ad
// ogni frame da blockyStepFrame() — erano locali nello stack della stessa
// funzione (il while era annidato li' dentro); estraendo il corpo del loop
// in una funzione a parte devono sopravvivere tra una chiamata e l'altra,
// quindi diventano statiche di modulo. L'inizializzazione resta esattamente
// dove/quando era prima, dentro Blocky_RunEngine() (qui sotto sono solo
// dichiarate, coi valori di default che avevano da costruttore).
static GLuint vao = 0;
static GLuint partP = 0, studP = 0, skinP = 0, meshP = 0;
static Texture2D studTex;
static glm::vec3 sun{0.f, -1.f, 0.f};
static SunSky sunSky;
static AnimatedModel fbx;
static bool fbxOk = false;
static AnimatedModel noobGlb;

static void blockyStepFrame(GLFWwindow* win) {
    static float lastT = (float)glfwGetTime();
    static int frames = 0;
    static float fpsAcc = 0.f;
    ImGuiIO& io = ImGui::GetIO();

        glfwPollEvents();

        if (!g_pendingOpenPlace.empty() && kIsGameEngine) {
            std::string path = g_pendingOpenPlace;
            g_pendingOpenPlace.clear();
            g_startupHomeDone = true;
            if (openPlacePath(path)) {
                returnToStudioEditor(win);
                CrashHandler::note(("Aperto .blocky: " + path).c_str());
            }
        }

        // Import pesante (parsing Assimp + upload GPU): eseguito qui, a
        // inizio frame, cosi' l'ultimo frame presentato a schermo prima del
        // blocco e' quello con "Importazione in corso..." gia' disegnato
        // (vedi insertImportedModel/processPendingModelImport).
        processPendingModelImport();

        float now = (float)glfwGetTime();
        float dt = std::min(now - lastT, 0.05f);
        lastT = now;
        fpsAcc += dt; frames++;
        if (fpsAcc >= 0.5f) {
            g_fps = frames / fpsAcc; frames = 0; fpsAcc = 0.f;
#ifdef __ANDROID__
            // Su Android non c e una console da guardare: senza questa riga
            // "sembra lento" resta un impressione invece di un numero.
            static float fpsLogAcc = 0.f;
            fpsLogAcc += 0.5f;
            if (fpsLogAcc >= 3.f) {
                fpsLogAcc = 0.f;
                std::printf("[perf] %.1f fps\n", g_fps);
                // Dove va ogni millisecondo, misurato — non e' un'ipotesi:
                // e' lo stesso FrameProfiler che F4 mostra su PC, qui
                // stampato da solo perche' un telefono non ha una tastiera
                // F4 da premere. Serve a scegliere COSA ottimizzare in base
                // a un numero vero, non a una lista di sospetti abituali.
                using namespace plat::prof;
                const State& ps = state();
                std::printf("[perf] frame=%.2fms  non_misurato=%.2fms\n",
                           ps.frameAvgMs, unaccountedMs());
                for (int i = 0; i < (int)Phase::Count; ++i) {
                    if (ps.phase[i].avgMs > 0.05f)
                        std::printf("[perf]   %-14s %.2fms (picco %.2fms)\n",
                                   phaseName((Phase)i), ps.phase[i].avgMs, ps.phase[i].peakMs);
                }
                const bphys::Stats& phs = g_bphys.stats();
                if (phs.bodies > 0)
                    std::printf("[perf] fisica %s: attive %d/%d, congelate %d, lontane %d, statiche %d\n",
                                phs.tier, phs.active, g_bphys.budget().maxActive,
                                phs.frozen, phs.culled, phs.statics);
            }
#endif
        }

        updateCrashContext();

        g_autosaveAcc += dt;
        if (g_autosaveAcc >= 30.f) {
            g_autosaveAcc = 0.f;
            persistAutosavePlaceOnly();
        }
        if (g_publishDraftActive) {
            g_publishDraftAcc += dt;
            if (g_publishDraftAcc >= 30.f) {
                g_publishDraftAcc = 0.f;
                persistPublishDraftOnly();
            }
        }

        static bool escWasDown = false;
        bool escDown = glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS;
        bool escPressed = escDown && !escWasDown;
        escWasDown = escDown;
        if (escPressed) {
            if (kIsGameEngine && g_screen == AppScreen::MainMenu) {
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

        if (kIsGameEngine && (g_screen == AppScreen::Loading || g_screen == AppScreen::Playing)) {
            returnToStudioEditor(win);
        }

        g_walking = false;
        {
            PROF_SCOPE(plat::prof::Phase::Input);
            // Consegna un eventuale file scelto dall utente. Va fatto nel frame,
            // non dal thread che ha portato la risposta: su Android quel thread e
            // quello della copia, e il motore li non e in uno stato coerente.
            plat::pumpFilePicks();
            plat::auth::pumpAuth();
            pollGamepad();
        }
        // I comandi a tocco entrano come se fossero una levetta analogica: il
        // movimento sotto resta quello di sempre, non ce n e' un secondo.
        blk::touch::apply(gamepad());
        const auto& pad = gamepad();

        if (pad.startPressed || pad.backPressed) {
            if (g_screen == AppScreen::Playing)
                setPlayMenuOpen(win, !g_playMenuOpen);
            else if (g_screen == AppScreen::Studio && g_studioTesting)
                setPlayMenuOpen(win, !g_playMenuOpen);
            else if (g_screen == AppScreen::Studio && !g_studioTesting && pad.startPressed)
                startStudioTest(win);
        }

        if (g_screen == AppScreen::AvatarPublisher) {
            if (!io.WantCaptureKeyboard) {
                if (glfwGetKey(win, GLFW_KEY_Q) == GLFW_PRESS)
                    g_catalog.camYaw -= 70.f * dt;
                if (glfwGetKey(win, GLFW_KEY_E) == GLFW_PRESS)
                    g_catalog.camYaw += 70.f * dt;
                if (glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS)
                    g_catalog.camDist = std::max(4.f, g_catalog.camDist - 8.f * dt);
                if (glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS)
                    g_catalog.camDist = std::min(40.f, g_catalog.camDist + 8.f * dt);
                if (ImGui::IsKeyPressed(ImGuiKey_1)) g_catalog.tool = AccelTool::Move;
                if (ImGui::IsKeyPressed(ImGuiKey_2)) g_catalog.tool = AccelTool::Rotate;
                if (ImGui::IsKeyPressed(ImGuiKey_3)) g_catalog.tool = AccelTool::Scale;
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
            const bool playUiOpen = (g_screen == AppScreen::Playing && g_playMenuOpen)
                || (g_screen == AppScreen::Studio && g_studioTesting && g_playMenuOpen);
            const bool inPlayMove = (g_screen == AppScreen::Playing && !g_playMenuOpen)
                || (g_screen == AppScreen::Studio && g_studioTesting && !g_playMenuOpen);
            if (inPlayMove)
                menuPadCursor().active = false;

            const bool menuCursor = menuPadCursor().active;
            const bool kbOk = !playUiOpen && !io.WantTextInput;
            const bool padOk = pad.connected && !playUiOpen && !menuCursor;

            if (inPlayMove)
                pollBackpackSwitch(win);

            if (g_screen == AppScreen::Playing || g_studioTesting)
                syncPlayCursor(win);

            if (kbOk || padOk) {
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
                bool luaOwnsSpeed = scriptControls().walkSpeedOverride >= 0.f;
                float maxSpd = baseSpd * ((!luaOwnsSpeed && sprintKey) ? 1.65f : 1.f);

                if (padOk && (std::fabs(pad.rx) > 0.01f || std::fabs(pad.ry) > 0.01f)) {
                    float lookSpd = (g_shiftLock || g_firstPerson) ? 160.f : 140.f;
                    g_orbitYaw -= pad.rx * lookSpd * dt;
                    g_orbitPitch += pad.ry * lookSpd * dt;
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

                if (!(g_screen == AppScreen::Playing && (g_shiftLock || g_firstPerson))) {
                    if ((kbOk && (glfwGetKey(win, GLFW_KEY_Q) == GLFW_PRESS || glfwGetKey(win, GLFW_KEY_LEFT) == GLFW_PRESS))
                        || (padOk && (pad.lb || pad.dpadL)))
                        g_orbitYaw += 90.f * dt;
                    if ((kbOk && (glfwGetKey(win, GLFW_KEY_E) == GLFW_PRESS || glfwGetKey(win, GLFW_KEY_RIGHT) == GLFW_PRESS))
                        || (padOk && (pad.rb || pad.dpadR)))
                        g_orbitYaw -= 90.f * dt;
                }

                if (g_screen == AppScreen::Studio && g_mode == CamMode::Studio && !g_studioTesting) {
                    float spd = maxSpd * dt;
                    glm::vec3 f = g_cam.front(), r = glm::normalize(glm::cross(f, glm::vec3(0,1,0)));
                    glm::vec3 move(0);
                    if (kbOk && glfwGetKey(win, GLFW_KEY_W)) move += f;
                    if (kbOk && glfwGetKey(win, GLFW_KEY_S)) move -= f;
                    if (kbOk && glfwGetKey(win, GLFW_KEY_A)) move -= r;
                    if (kbOk && glfwGetKey(win, GLFW_KEY_D)) move += r;
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
                    glm::vec3 fwd = camFlatForward();
                    glm::vec3 right = camFlatRight();
                    glm::vec3 wish(0);
                    if (kbOk && glfwGetKey(win, GLFW_KEY_W)) wish += fwd;
                    if (kbOk && glfwGetKey(win, GLFW_KEY_S)) wish -= fwd;
                    if (kbOk && glfwGetKey(win, GLFW_KEY_A)) wish -= right;
                    if (kbOk && glfwGetKey(win, GLFW_KEY_D)) wish += right;
                    if (padOk) {
                        wish += fwd * pad.ly;
                        wish += right * pad.lx;
                    }
                    if (!scriptControls().walkEnabled || g_player.humanoid.dead)
                        wish = glm::vec3(0.f);
                    float wishLen = glm::length(wish);
                    if (wishLen > 1.f)
                        wish /= wishLen;
                    else if (wishLen > 0.001f)
                        wish = glm::normalize(wish);
                    else
                        wish = glm::vec3(0.f);

                    scriptControls().moveHeld = wishLen > 0.001f;
                    scriptControls().sprintHeld = sprintKey && scriptControls().moveHeld;

                    bool grounded = g_player.humanoid.grounded;
                    glm::vec3 targetVel = wish * maxSpd;
                    const bool hasInput = wishLen > 0.001f;
                    float rate = grounded
                        ? (hasInput ? 22.f : 28.f)
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
                    g_walking = hasInput && grounded && !g_player.humanoid.dead;

                    // Suono passi: un "passo" ogni tot stud percorsi (non ogni
                    // tot secondi fissi), cosi' sprint = passi piu' ravvicinati.
                    // g_sounds e' a voce singola (PlaySoundA di WinMM ferma
                    // sempre il suono precedente) — un passo puo' interrompere
                    // un altro effetto in corso mentre cammini, limite del
                    // motore audio attuale, non di questa funzione.
                    static bool g_wasWalking = false;
                    if (g_walking && hSpd > 0.5f) {
                        const float kStrideStuds = 4.5f;
                        bool justStartedWalking = !g_wasWalking;
                        g_footstepAcc += hSpd * dt;
                        if (justStartedWalking || g_footstepAcc >= kStrideStuds) {
                            g_footstepAcc = 0.f;
                            static const char* kFootstepFiles[] = {
                                "sounds/footstep1.wav",
                                "sounds/footstep2.wav",
                                "sounds/footstep3.wav",
                            };
                            int idx = std::rand() % 3;
                            std::string fpath = resolveScriptPath(kFootstepFiles[idx]);
                            g_sounds.playPath(fpath);
                        }
                    } else {
                        g_footstepAcc = 0.f;
                    }
                    g_wasWalking = g_walking && hSpd > 0.5f;

                    if (g_shiftLock || g_firstPerson) {
                        syncPlayerYawToCamera();
                    } else if (hasInput) {
                        float targetYaw = yawFromDir(wish);
                        float diff = targetYaw - g_playerYaw;
                        while (diff > 180.f) diff -= 360.f;
                        while (diff < -180.f) diff += 360.f;
                        float turn = grounded ? 22.f : 16.f;
                        g_playerYaw += diff * std::min(1.f, turn * dt);
                    }

                    static bool spaceWas = false;
                    bool spaceKey = kbOk && glfwGetKey(win, GLFW_KEY_SPACE) == GLFW_PRESS;
                    if (scriptControls().jumpEnabled) {
                        if ((spaceKey && !spaceWas) || (padOk && pad.aPressed)) {
                            if (g_player.humanoid.tryJump())
                                g_sounds.playPath(resolveScriptPath("sounds/jump.wav"));
                        }
                    }
                    spaceWas = spaceKey;

                    if (g_swordCooldown > 0.f) g_swordCooldown = std::max(0.f, g_swordCooldown - dt);
                    if (g_swordSwing > 0.f) g_swordSwing = std::max(0.f, g_swordSwing - dt * 4.5f);
                    if (padOk && pad.xPressed) {
                        trySwordAttack();
                        tryUseEquippedItem();
                    }
                }
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

                static bool f3Was = false;
                const bool f3 = kbOk && glfwGetKey(win, GLFW_KEY_F3) == GLFW_PRESS;
                if (f3 && !f3Was) g_showAnimDebug = !g_showAnimDebug;
                f3Was = f3;

                // F4: dove finiscono i millisecondi. Sempre disponibile,
                // anche nelle build pubblicate: un profiler che esiste solo
                // in sviluppo non misura mai la macchina di chi gioca.
                static bool f4Was = false;
                const bool f4 = kbOk && glfwGetKey(win, GLFW_KEY_F4) == GLFW_PRESS;
                if (f4 && !f4Was) g_showProfiler = !g_showProfiler;
                f4Was = f4;
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
                if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_0)) {
                    // "Normale" vuol dire normale PER QUESTO DISPOSITIVO.
                    io.FontGlobalScale = blk::ui::uiScale();
                    if (g_selectedLabel >= 0 && g_selectedLabel < (int)g_world.labels.size())
                        g_world.labels[g_selectedLabel].fontScale = 1.f;
                    g_exportMsg = "Testo UI riportato a normale (Ctrl+0)";
                }
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
                PROF_SCOPE(plat::prof::Phase::Physics);
                static float physAcc = 0.f;
                float hz = std::max(20.f, g_engine.quality.physicsHz);
                float step = 1.f / hz;
                const bool inPlay = g_screen == AppScreen::Playing ||
                    (g_screen == AppScreen::Studio && g_mode == CamMode::Play);
                if (inPlay) {
                    g_bphys.configure(g_engine.quality);
                    g_bphys.sync(g_world.parts, g_playerPos, dt);
                } else {
                    g_bphys.reset();
                }
                physAcc += dt;
                int guard = 0;
                while (physAcc >= step && guard++ < 8) {
                    if (inPlay) {
                        stepPlayerPhysicsWithCollision(step);
                        g_bphys.step(step);
                    } else {
                        g_player.humanoid.stepPhysics(step, g_playerPos.y, 0.f);
                    }
                    physAcc -= step;
                }
                if (physAcc > step * 2.f) physAcc = 0.f;
                if (inPlay) g_bphys.writeBack(g_world.parts, physAcc / step);
            }
            acValidateMovement(g_playerPos, g_moveVel, dt, (float)glfwGetTime(),
                               g_player.humanoid.grounded, g_player.humanoid.velocityY);
            g_player.update(dt, g_walking);
            updateFriendPlayers(dt);
            if (g_screen == AppScreen::Playing && g_net.role != NetRole::Offline) {
                { PROF_SCOPE(plat::prof::Phase::Network); g_net.poll((float)glfwGetTime()); }
                pollPortMapStatus(g_menu.netStatus, sizeof(g_menu.netStatus));
                if (g_lua.scriptSetChanged) {
                    g_net.localScriptHashes = g_lua.scriptHashes;
                    g_lua.scriptSetChanged = false;
                    g_net.scriptReportAcc = 99.f;
                }
                // Anche "sono in aria": senza, gli altri ci vedono sempre a terra
                // e il salto non compare mai sui loro schermi.
                g_net.sendLocalState(dt, g_playerPos, g_playerYaw, g_walking,
                                     !g_player.humanoid.grounded);
                g_net.updateDisplay(dt);
                // Chat arrivata dalla rete: finisce nello stesso elenco di quella
                // locale, cosi la finestra non deve sapere da dove viene.
                for (const auto& line : g_net.incomingChat)
                    g_menu.chatLines.push_back(line);
                g_net.incomingChat.clear();
                if (g_remoteBodies.size() < g_net.remotes.size())
                    g_remoteBodies.resize(g_net.remotes.size());
                for (size_t i = 0; i < g_net.remotes.size(); ++i) {
                    // La posa di salto la decide grounded dentro l'Humanoid:
                    // basta dirgli cosa e' arrivato dalla rete e l'animazione
                    // che c'era gia' fa il resto, senza codice nuovo.
                    g_remoteBodies[i].humanoid.grounded = !g_net.remotes[i].airborne;
                    g_remoteBodies[i].update(dt, g_net.remotes[i].walking);
                }
                std::snprintf(g_menu.netStatus, sizeof(g_menu.netStatus), "%s (%d)",
                              g_net.status, g_net.playerCount());
            }

            if (g_screen == AppScreen::Playing && !g_gatePassed && !gateOfflineAllowed()) {
                bool denied = false;
                int reasonCode = 0;
                if (g_net.role == NetRole::Offline)      { denied = true; reasonCode = 1; }
                else if (g_net.role == NetRole::Host)    { denied = true; reasonCode = 2; }
                else if (g_net.rejected)                 { denied = true; reasonCode = 3; }
                else if (g_net.connected) {
                    GateResult gr = gateEvaluate(g_net);
                    if (gr.state == GateState::Allowed) g_gatePassed = true;
                    else { denied = true; reasonCode = 4; }
                } else if (glfwGetTime() > g_gateDeadline) { denied = true; reasonCode = 5; }

                // Il gate chiede di essere online con un server Blocky. Finora
                // accettava come prova soltanto una partita multigiocatore in
                // corso, e rifiutava un mondo scaricato dal server dei giochi
                // e giocato da soli — arrivato dalla rete un attimo prima.
                if (denied && blk::remotegames::serverSeenRecently()) {
                    denied = false;
                    g_gatePassed = true;
                }

                if (denied) {
                    std::snprintf(g_gateTitle, sizeof(g_gateTitle), "Blocky needs Wi-Fi");
                    std::snprintf(g_gateMsg, sizeof(g_gateMsg),
                                  "Controlla la connessione e riprova.");
                    LOG_SEC("gate", "negato codice=%d", reasonCode);
                    // Anche sul flusso normale: LOG_SEC in certe build non
                    // scrive nulla, e senza questo motivo un giocatore
                    // scollegato non ha modo di sapere perche.
                    std::printf("[Gate] negato codice=%d (ruolo=%d connesso=%d "
                                "rifiutato=%d integrita=%d)\n",
                                reasonCode, (int)g_net.role, (int)g_net.connected,
                                (int)g_net.rejected, (int)integrity().ok);
                    g_net.shutdown();
                    closePortIfOpen();
                    returnToMenu(win);
                }
            }
            if (!g_showPlayerRigEditor && !g_player.humanoid.dead)
                g_animator.apply(g_player.humanoid, dt, g_walking);
            // Dopo l animatore di sistema: la posa dei keyframe si SOMMA a
            // camminata e salto invece di sostituirli.
            tickRigAnimatorPreview(dt);
            updateModelAnimPlayback(dt);
            if (g_screen == AppScreen::Playing
                || (g_screen == AppScreen::Studio && g_studioTesting)) {
                g_lua.budgetUs = g_engine.quality.luaBudgetUs;
                std::string crashReason;
                bool okFrame = TestSessionGuard::runGuarded([&]() {
                    { PROF_SCOPE(plat::prof::Phase::Scripts); g_lua.fireHeartbeat(dt); }
                    pollPartTouched();
                    pollBackpackEquipChanged();
                    tickBoomAutoFuse(dt);
                    tickCameraShake(dt);
                    g_lua.drainRemoteEvents();
                }, &crashReason);
                if (!okFrame) {
                    // Contenimento: uno script ha fatto crashare qualcosa
                    // di nativo a runtime (bug in una delle funzioni C
                    // esposte a Lua, quasi certamente). Prima si terminava
                    // tutto il processo — ora si esce dalla sessione di
                    // gioco/Test e si torna a un posto sicuro, invece di
                    // continuare a chiamare fireHeartbeat ecc. su uno
                    // stato che potrebbe essere a meta' modificato.
                    CrashHandler::note(("Crash contenuto durante gioco/Test: " + crashReason).c_str());
                    std::fprintf(stderr, "[TestSessionGuard] crash contenuto: %s\n", crashReason.c_str());
                    if (g_screen == AppScreen::Studio && g_studioTesting) {
                        stopStudioTest(win);
                        g_luaRunMsg = "Test interrotto — crash nello script: " + crashReason;
                    } else if (g_screen == AppScreen::Playing) {
                        g_net.shutdown();
                        closePortIfOpen();
                        returnToMenu(win);
                        g_luaRunMsg = "Sessione interrotta — crash nello script: " + crashReason;
                    }
                }
            }
        }

        g_particles.update(dt);

        const bool worldScreen = (g_screen != AppScreen::MainMenu
                               && g_screen != AppScreen::AvatarPublisher);
        glm::vec3 toSunNow;
        glm::vec3 skyZenith, skyHorizon;
        const bool timeRuns = (g_screen == AppScreen::Playing)
            || (g_screen == AppScreen::Studio && g_studioTesting);
        if (worldScreen) {
            g_lighting.tick(dt, timeRuns);
            toSunNow = g_lighting.toSun();
            sun = g_lighting.sunDirection();
            g_night = g_lighting.nightAmount();
            g_sunColor = g_lighting.sunTint();
            g_ambientTint = g_lighting.ambientTint();
            g_lighting.skyColors(skyZenith, skyHorizon);
            g_skyZenith = skyZenith;
            g_skyHorizon = skyHorizon;
        } else {
            g_lighting.clampAll();
            toSunNow = glm::normalize(glm::vec3(-0.35f, 0.85f, 0.28f));
            sun = -toSunNow;
            g_night = 0.f;
            g_sunColor = glm::vec3(1.f);
            g_ambientTint = glm::vec3(1.f);
            skyZenith = {0.42f, 0.65f, 0.95f};
            skyHorizon = {0.69f, 0.84f, 0.95f};
            g_skyZenith = skyZenith;
            g_skyHorizon = skyHorizon;
        }
        // La scena si disegna a g_renderW x g_renderH; il composite la riporta
        // a schermo pieno ricostruendola. Su una GPU modesta abbassare questa
        // sola risoluzione vale piu di qualunque altra impostazione.
        updateRenderResolution();
        g_bloom.beginScene(g_renderW, g_renderH);
        plat::prof::begin(plat::prof::Phase::World);
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
        {
            // Il testo puo allontanarsi dalla base della piattaforma, non da 1.0.
            // Prima il limite era fisso: su un telefono la base giusta e ~1.6, e
            // qualunque ingrandimento veniva annullato al frame successivo — il
            // motivo per cui il testo restava minuscolo sul cellulare.
            const float base = blk::ui::uiScale();
            io.FontGlobalScale = std::clamp(io.FontGlobalScale, base * 0.7f, base * 1.6f);
        }
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
                if (ImGui::Button("OK", {-1, 34}))
                    g_gateMsg[0] = '\0';
                ImGui::End();
            }
            if (g_menu.wantRefreshGameServer) {
                g_menu.wantRefreshGameServer = false;
                blk::remotegames::saveHost(g_menu.gameServer);
                blk::remotegames::startFetch(g_menu.gameServer);
                blk::remotecatalog::startFetch(g_menu.gameServer);
            }
            {
                // I giochi del server arrivano da un thread: si innestano nella
                // lista quando sono pronti, senza aver mai bloccato un frame.
                std::vector<GameEntry> fromServer;
                if (g_menu.wantServerList) {
                    g_menu.wantServerList = false;
                    g_menu.showServerList = true;
                    blk::serverbrowser::startScan(g_menu.joinPort, g_menu.selectedGame);
                }
                if (g_menu.wantSyncGames) {
                    g_menu.wantSyncGames = false;
                    // Si parte dai PACCHETTI su disco e non dall elenco in
                    // memoria: e il pacchetto che contiene il mondo e gli
                    // script, cioe quello che il server deve ricevere.
                    std::vector<std::pair<std::string, std::string>> pkgs;
                    for (const auto& g : BlockyProject::listInstalledGames()) {
                        const std::string dir =
                            BlockyProject::joinPath(BlockyProject::gamesInstallDir(), g.id);
                        if (fileExistsPath(BlockyProject::joinPath(dir, "place.json")))
                            pkgs.emplace_back(g.id, dir);
                    }
                    if (pkgs.empty())
                        g_menu.chatLines.push_back("[Games] Nessun gioco pubblicato da sincronizzare");
                    else
                        blk::remotegames::syncAllGames(g_menu.gameServer, pkgs);
                }
                if (g_menu.wantPublishCatalog) {
                    g_menu.wantPublishCatalog = false;
                    publishCatalogToServer(g_menu, g_menu.gameServer);
                }
                {
                    // Il catalogo scaricato si fonde con quello locale: si
                    // aggiungono solo gli item che qui non ci sono.
                    std::string catJson;
                    if (blk::remotecatalog::takeFetched(catJson)) {
                        const int added = mergeRemoteCatalog(g_menu, catJson);
                        if (added > 0)
                            g_menu.chatLines.push_back("[Catalog] " + std::to_string(added)
                                                       + " item dal server");
                    }
                    const std::string catMsg = blk::remotecatalog::takeMessage();
                    if (!catMsg.empty()) g_menu.chatLines.push_back("[Catalog] " + catMsg);
                }
                const std::string uploadMsg = blk::remotegames::takeUploadMessage();
                if (!uploadMsg.empty()) g_menu.chatLines.push_back("[Games] " + uploadMsg);
                if (blk::remotegames::takeResult(fromServer) && !fromServer.empty()) {
                    mergeGamePackages(g_games, fromServer);
                    g_menu.chatLines.push_back("[Games] " + std::to_string(fromServer.size())
                                               + " giochi dal server");
                }
            }
            {
                // Un gioco del server va prima portato qui: quando i file sono
                // arrivati la voce smette di essere "remota" e punta alle copie
                // locali, dopodiche' si avvia da sola come qualunque altra.
                const std::string doneId = blk::remotegames::takeDownloadedId();
                const std::string dlMsg = blk::remotegames::takeDownloadStatus();
                if (!dlMsg.empty()) g_menu.chatLines.push_back("[Games] " + dlMsg);
                if (!doneId.empty()) {
                    const std::string dir = blk::remotegames::localGameDir(doneId);
                    for (size_t i = 0; i < g_games.size(); ++i) {
                        if (g_games[i].id != doneId) continue;
                        g_games[i].remote = false;
                        g_games[i].placePath = blk::remotegames::localPlacePath(doneId);
                        const std::string thumb = blk::remotegames::thumbnailPath(doneId);
                        if (fileExistsPath(thumb)) g_games[i].iconPath = thumb;
                        const std::string bbc = dir + "/scripts/main.bbc";
                        g_games[i].script = fileExistsPath(bbc) ? bbc : (dir + "/scripts/main.lua");
                        g_menu.launchGame = (int)i;
                        g_menu.wantPlay = true;
                        break;
                    }
                }
            }
            if (g_menu.wantPlay && g_menu.launchGame >= 0
                && g_menu.launchGame < (int)g_games.size()
                && g_games[g_menu.launchGame].remote) {
                g_menu.wantPlay = false;
                const std::string id = g_games[g_menu.launchGame].id;
                if (blk::remotegames::isDownloading()) {
                    g_menu.chatLines.push_back("[Games] Download gia in corso...");
                } else {
                    g_menu.chatLines.push_back("[Games] Scarico \"" + id + "\" dal server...");
                    blk::remotegames::downloadGame(g_menu.gameServer, id);
                }
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
            if (g_menu.wantJoinCode && g_menu.launchGame >= 0) {
                g_joinByCode = true;
                startLoadingGame(g_menu.launchGame);
                g_menu.wantJoinCode = false;
            }
            // Prima veniva solo azzerato: il pulsante "Studio" nella Hub
            // (ClassicMenu.h) lo accende, ma enterStudio() — che esiste da
            // prima, gia' pronta — non veniva mai chiamata da nessuna parte.
            // Studio (e con lui Toolbox, preset di script, editor Lua) era
            // presente nel motore ed era irraggiungibile dall'interfaccia.
            if (g_menu.wantBuild) {
                g_menu.wantBuild = false;
                enterStudio();
            }
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
            // L anteprima della presa vale solo finche il suo pannello e
            // aperto: la si azzera qui e la ridichiara il pannello, se c e.
            g_gripPreviewSlot = -1;
            drawStudioUi(fbx, fbxOk);
            drawRigAnimatorUi();
            drawToolboxWindow();
            drawGizmoOverlayHud();
            drawSavePlacePrompt(win);
        }
        else if (g_screen == AppScreen::AvatarPublisher) {
            drawAvatarPublisherUi();
        }
        else if (g_screen == AppScreen::Playing) {
            drawPlayingHud(win);
            // Dopo l HUD, cosi' joystick e salto restano sopra a tutto il resto.
            blk::touch::draw();
        }

        drawMenuPadCursor();
        if (g_showProfiler) drawFrameProfiler();

        plat::prof::end(plat::prof::Phase::World);
        { PROF_SCOPE(plat::prof::Phase::PostFx);
        g_bloom.endSceneAndComposite(g_w, g_h, 1.6f, 0.95f, 1.5f, g_upscaleSharpen); }

        { PROF_SCOPE(plat::prof::Phase::Ui);
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); }

        glfwSwapInterval(g_vsync ? 1 : 0);
        // Qui la CPU ASPETTA la GPU: se questo numero e grande, il collo di
        // bottiglia e la scheda video, non il codice.
        { PROF_SCOPE(plat::prof::Phase::Present); glfwSwapBuffers(win); }
        plat::prof::endFrame(dt);

        // Limitatore FPS: tiene CPU/GPU a un carico costante invece che al
        // 100% quando vsync/monitor lo permetterebbero — e' la causa piu'
        // diretta di surriscaldamento su mobile. targetFps=0 (default PC) lo
        // disattiva del tutto: non tocca ne' il rendering ne' l'input, solo
        // il tempo che il thread principale passa in sleep a fine frame.
        if (g_engine.quality.targetFps > 0) {
            const double frameBudget = 1.0 / (double)g_engine.quality.targetFps;
            const double elapsed = glfwGetTime() - now;
            const double remaining = frameBudget - elapsed;
            if (remaining > 0.0005)
                std::this_thread::sleep_for(std::chrono::duration<double>(remaining));
        }
}

static int blockyInitEngine(GLFWwindow*& win) {
    CrashHandler::install(kAppBrand, "1.0");
    CrashHandler::note(kIsGameEngine ? "Avvio Game Engine" : "Avvio Blocky hub");

#ifdef BLOCKY_DEV
    blog::setLevel(blog::Debug);
#endif
    blog::toFile(blockyDataDir() + "/blocky.log");
    LOG_INFO("boot", "Blocky avviato su %s", plat::name());

    intgBoot();
    std::snprintf(g_net.localFingerprint, sizeof(g_net.localFingerprint),
                  "%s", gateClientFingerprint().c_str());

    loc().load();

    g_engine = EngineConfig::createDefault();
    g_vsync = g_engine.quality.vsync;
    loadGfxPreset();
    g_engine.logBanner();

    glfwSetErrorCallback(onGlfwError);
    if (!glfwInit()) {
        CrashHandler::note("glfwInit fallita");
        return -1;
    }
    initGamepad();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    int msaa = g_engine.quality.msaaSamples;
    if (msaa < 0) msaa = 0;
    if (msaa > 8) msaa = 8;
    glfwWindowHint(GLFW_SAMPLES, msaa);

    // La finestra sta SEMPRE alla sua dimensione piena.
    //
    // Prima renderScale veniva applicato qui, cioe' rimpiccioliva la finestra:
    // un ripiego nato perche' la vera risoluzione interna non esisteva. E su
    // PC la riga successiva lo annullava comunque, quindi il cursore "Render
    // scale" non faceva nulla. Ora la scala vive dove deve, sul bersaglio di
    // disegno (updateRenderResolution), e la finestra non c'entra piu'.
    const int winW = g_w;
    const int winH = g_h;

    char title[128];
    if (kIsGameEngine)
        std::snprintf(title, sizeof(title), "%s", kAppBrand);
    else
        std::snprintf(title, sizeof(title), "Blocky — %s", targetPlatformName(g_engine.platform));
    win = glfwCreateWindow(winW, winH, title, nullptr, nullptr);
    if (!win) {
        CrashHandler::note("glfwCreateWindow fallita");
        return -1;
    }
    CrashHandler::note("Finestra creata");
    g_mainWin = win;
    g_w = winW; g_h = winH;
    glfwMakeContextCurrent(win);
    setWindowIconB(win);
    glfwSwapInterval(g_vsync ? 1 : 0);
    glfwSetFramebufferSizeCallback(win, onResize);
    glfwSetCursorPosCallback(win, onMouse);
    glfwSetMouseButtonCallback(win, onMouseBtn);
    glfwSetScrollCallback(win, onScroll);
    glfwSetWindowCloseCallback(win, onWindowCloseRequest);
    glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

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
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    // Le misure dell interfaccia vanno calcolate PRIMA del tema: e il tema a
    // riversarle nello stile di ImGui, e con esse l altezza di ogni pulsante.
    blk::ui::update(g_w, g_h);
    io.FontGlobalScale = blk::ui::uiScale();

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
#ifdef __ANDROID__
    // ImGui legge il numero di versione con sscanf e lo confronta contro
    // valori esatti: solo 300 gli dice "sono shader ES" e gli fa emettere
    // le qualificazioni "precision". Con 320 finiva nel ramo desktop
    // glsl_130, e il fragment shader non compilava (S0032, nessuna
    // precision per Frag_UV). La 300 es basta e avanza per la UI.
    ImGui_ImplOpenGL3_Init("#version 300 es");
#else
    ImGui_ImplOpenGL3_Init("#version 330");
#endif

    refreshGamesList();
    {
        // Se un server dei giochi era gia stato impostato, lo si interroga
        // subito all avvio: la richiesta parte in secondo piano, quindi non
        // rallenta l apertura di una virgola.
        const std::string savedServer = blk::remotegames::loadHost();
        if (!savedServer.empty()) {
            std::snprintf(g_menu.gameServer, sizeof(g_menu.gameServer), "%s", savedServer.c_str());
            blk::remotegames::startFetch(savedServer);
            blk::remotecatalog::startFetch(savedServer);
            // Lo stesso computer che ospita i giochi e quasi sempre quello che
            // ospita le partite: chiederglielo direttamente evita di dipendere
            // dal broadcast, che non tutti i router inoltrano.
            {
                std::string h;
                int p = 0;
                blk::remotegames::splitHostPort(savedServer, h, p);
                netExtraDiscoveryHost() = h;
            }
        }
    }
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

    vao = makeCubeVAO();
    g_bevelCube.build(0.11f);
    g_partMeshes.buildAll();
    // Ogni sorgente passa da shaderSrc(): se assets/shaders/<nome> esiste,
    // usa quello (modifichi il file, riavvii, vedi il cambiamento — zero
    // ricompilazione); altrimenti usa la stringa incorporata qui sotto,
    // esattamente come prima.
    partP = makeProg(shaderSrc("part.vert.glsl", kPartVS).c_str(),
                      shaderSrc("part.frag.glsl", kPartFS).c_str());
    studP = makeProg(shaderSrc("part.vert.glsl", kPartVS).c_str(),
                      shaderSrc("stud.frag.glsl", kStudFS).c_str());
    skinP = makeProg(shaderSrc("skin.vert.glsl", kSkinVS).c_str(),
                      shaderSrc("part.frag.glsl", kPartFS).c_str());
    meshP = makeProg(shaderSrc("meshtex.vert.glsl", kMeshTexVS).c_str(),
                      shaderSrc("meshtex.frag.glsl", kMeshTexFS).c_str());
    {
        // Pagina di pagamento di ripiego, servita da un file locale. Era
        // dentro un #ifdef _WIN32 con il calcolo della cartella fatto a mano:
        // fuori da Windows il ripiego semplicemente non esisteva, e nessuno
        // se ne accorgeva finche' non serviva.
        const plat::fs::path html =
            plat::fs::path(plat::selfDir()) / "assets" / "web" / "buy_bucks.html";
        if (plat::fs::exists(html)) {
            std::string url = "file:///" + html.generic_string();  // sempre '/'
            bucksSetFallbackPayPage(url);
        }
        bucksEnsureLoaded();
    }
    if (!studTex.load(findStudTexture(), true))
        std::fprintf(stderr, "Avviso: stud.png non trovato.\n");
    sun = g_lighting.sunDirection();
    g_lighting.clampAll();
    if (!sunSky.init())
        std::fprintf(stderr, "Avviso: Sole/cielo 2D non inizializzato.\n");
    if (!g_shadowMap.init(g_engine.quality.shadowOccluders >= 8 ? 2048 : 1024))
        std::fprintf(stderr, "Avviso: shadow map non inizializzata (ombre sole off).\n");
    if (!g_bloom.init(g_w, g_h))
        std::fprintf(stderr, "Avviso: bloom non inizializzato (niente alone sugli emissivi).\n");
    if (!g_particles.init())
        std::fprintf(stderr, "Avviso: Particles non inizializzato.\n");

    std::string fbxPath = findFbx();
    if (!fbxPath.empty()) fbx.load(fbxPath);
    fbxOk = fbx.loaded;

    std::string noobPath = findNoobGlb();
    if (!noobPath.empty()) {
        noobGlb.load(noobPath);
        std::fprintf(stdout, "Noob GLB: %s (%s)\n", noobPath.c_str(), noobGlb.loaded ? "OK" : "FAIL");
    }
    if (!noobGlb.loaded) g_useGlbAvatar = false;

    ensureDefaultBeepWav();
    if (kIsGameEngine)
        BlockyFileAssoc::registerPlaceAssociation();

    std::string launchPlace = !g_stagedLoadPath.empty()
                             ? g_stagedLoadPath
                             : BlockyFileAssoc::placePathFromArgs();

    if (kIsGameEngine) {
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

        if (!launchPlace.empty())
            g_pendingOpenPlace = launchPlace;
    }
#ifdef _WIN32
    else if (!launchPlace.empty()) {
        std::string dir = plat::selfDir();
        std::string ge = dir + "\\BlockyGameEngine.exe";
        if (plat::fileExists(ge)) {
            std::string params = "\"" + launchPlace + "\"";
            if (plat::sys::launchProcess(ge, params, dir)) {
                glfwDestroyWindow(win);
                glfwTerminate();
                return 0;
            }
        }
    }
#endif
    return 1;
}

static void blockyShutdownEngine() {
    persistFastExit();
    g_sounds.stopAll();
    g_net.shutdown();
    closePortIfOpen();
    g_lua.shutdown();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
}

int Blocky_RunEngine() {
    GLFWwindow* win = nullptr;
    const int ready = blockyInitEngine(win);
    if (ready <= 0) return ready;

    while (!glfwWindowShouldClose(win)) {
        blockyStepFrame(win);
    }

    blockyShutdownEngine();
    return 0;
}

#ifdef __ANDROID__
// Punti d'ingresso per Android: la' la finestra e il contesto li crea
// GameActivity + EGL, e il ciclo lo guida android_main(), quindi il motore
// non puo' possedere un while() suo. Sono le STESSE tre funzioni usate da
// Blocky_RunEngine() qui sopra, solo richiamabili una alla volta.
extern "C" int  blockyAndroidInit()     { GLFWwindow* w = nullptr; return blockyInitEngine(w); }
extern "C" void blockyAndroidFrame()    { blockyStepFrame(blkShimWindow()); }
extern "C" void blockyAndroidShutdown() { blockyShutdownEngine(); }
#endif
