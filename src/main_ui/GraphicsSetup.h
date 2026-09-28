// Preset di qualita' grafica (salva/carica/applica) e helper per compilare
// shader OpenGL e creare la VAO del cubo base. Non e' un header riusabile:
// usa direttamente le globali di main.cpp (g_engine, g_vsync, ecc.).
// Compila solo se incluso da main.cpp esattamente in questo punto.

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

static void applyGfxPreset(QualityPreset p) {
    g_engine.setPreset(p);
    g_vsync = g_engine.quality.vsync;
    saveGfxPreset();
}

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

static std::string g_speechText;
static double g_speechUntil = 0.0;
static bool g_studioTesting = false;
static bool g_bpKeyWas[9] = {};
static bool g_bpKpWas[9] = {};
static float g_swordSwing = 0.f;
static float g_swordCooldown = 0.f;
static glm::vec3 g_studioCamBackup{0, 12, 28};
static float g_studioYawBackup = -90.f, g_studioPitchBackup = -18.f;
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
static std::string g_pendingIconPath; // icona scelta ma non ancora pubblicata
static int g_maxPlayers = 8;
static std::string g_publishMsg;
static bool g_publishDraftActive = false;
static float g_publishDraftAcc = 0.f;
static std::string g_publishDraftPath;
static AvatarCatalogState g_catalog;
static AnimPackPublisherState g_animPackUi;
static bool g_joinByIp = false;
static bool g_joinByCode = false;

static double g_gateDeadline = 0.0;
static bool g_gatePassed = false;
static char g_gateMsg[256] = {};
static char g_gateTitle[64] = {};
static BeveledCubeMesh g_bevelCube;
static PartMeshes g_partMeshes;
static int g_sharpCubeVerts = 36;

static constexpr float kStudTile = 4.f;

static const char* kSkinVS = R"(#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in ivec4 aBoneIDs;
layout(location=3) in vec4 aWeights;
const int MAX_BONES=100;
uniform mat4 uFinalBones[MAX_BONES],uModel,uView,uProj;
out vec3 vN,vW;
// vLocalN/vLocalPos: kPartFS (che skinP accoppia a questo vertex shader) le
// dichiara per il box-mapping del materiale Wood. Il GL desktop tollera un
// varying letto ma mai scritto; GLES lo rifiuta in fase di link (L0007), quindi
// vanno emesse davvero. Per una mesh skinnata lo spazio "locale" e' quello
// dell oggetto: posizione e normale del vertice prima dello skinning.
out vec3 vLocalN;
out vec3 vLocalPos;
void main(){
  mat4 skin=mat4(0.); float t=0.;
  for(int i=0;i<4;i++){ if(aBoneIDs[i]<0) continue;
    skin+=uFinalBones[aBoneIDs[i]]*aWeights[i]; t+=aWeights[i]; }
  if(t<0.001) skin=mat4(1.);
  vec4 w=uModel*skin*vec4(aPos,1.);
  vW=w.xyz; vN=mat3(uModel)*mat3(skin)*aNormal;
  vLocalN=aNormal; vLocalPos=aPos;
  gl_Position=uProj*uView*w;
})";

// Sorgente shader da assets/shaders/<name>, se esiste — altrimenti la
// stringa GLSL incorporata nel .cpp (fallback), invariata. Un file esterno
// mancante o vuoto non e' un errore: e' semplicemente "usa quella
// incorporata", cosi' il motore parte identico a prima finche' non crei
// davvero il file.
static std::string shaderSrc(const char* name, const char* fallback) {
    std::string path = resolveScriptPath(std::string("shaders/") + name);
    std::ifstream f(path, std::ios::binary);
    if (f) {
        std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        if (!s.empty()) {
            std::fprintf(stdout, "[Shader] esterno: %s\n", path.c_str());
            return s;
        }
    }
    return fallback;
}

static GLuint compileShader(GLenum t, const char* s) {
    GLuint sh = glCreateShader(t);
    // Su Android riscrive la riga #version per GLES (vedi GlslPortable.h);
    // su desktop la sorgente passa invariata.
    const std::string portableSrc = glslForTarget(s);
    const char* srcPtr = portableSrc.c_str();
    glShaderSource(sh, 1, &srcPtr, nullptr);
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
