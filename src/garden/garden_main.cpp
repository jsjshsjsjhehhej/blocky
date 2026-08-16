// GardenGame - prototipo stile "Grow a Garden" in OpenGL
// Menu → Loading animato → Mondo 3D con player cartoony + shop
//
// Controlli (Playing):
//   WASD     muovi il personaggio
//   E        pianta / raccogli sul plot piu' vicino
//   B        apri/chiudi shop
//   1 / 2    compra item nello shop
//   ESC      esci (Menu: avvia gioco)

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../BlockPlayer.h"
#include "../render/Texture2D.h"
#include "../render/GlShaders.h"
#include "../render/WindowIcon.h"
#include "GameTypes.h"

#include <cstdio>
#include <cmath>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

static int g_width = 1280;
static int g_height = 720;

struct Part {
    glm::vec3 position;
    glm::vec3 size;
    glm::vec3 color;
};

static constexpr float kStudTile = 4.f; // texture 4x4 stud

#ifndef GARDEN_GLES
static const char* kGardenVS = kPartVS;
static const char* kGardenFS = kStudFS;
#else
static const char* kGardenVS = kGlesPartVS;
static const char* kGardenFS = kGlesStudAtmosFS;
#endif

static const char* kUiVS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
uniform vec2 uScreen;
void main() {
    vec2 ndc = (aPos / uScreen) * 2.0 - 1.0;
    ndc.y = -ndc.y;
    gl_Position = vec4(ndc, 0.0, 1.0);
}
)";

static const char* kUiFS = R"(#version 330 core
out vec4 FragColor;
uniform vec4 uColor;
void main() { FragColor = uColor; }
)";

static GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        std::fprintf(stderr, "Shader error: %s\n", log);
    }
    return s;
}

static GLuint makeProgram(const char* vs, const char* fs) {
    GLuint v = compileShader(GL_VERTEX_SHADER, vs);
    GLuint f = compileShader(GL_FRAGMENT_SHADER, fs);
    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    glDeleteShader(v);
    glDeleteShader(f);
    return p;
}

static GLuint makeCubeVAO() {
    const float v[] = {
        -0.5f,-0.5f, 0.5f,  0,0,1,   0.5f,-0.5f, 0.5f, 0,0,1,   0.5f, 0.5f, 0.5f, 0,0,1,
        -0.5f,-0.5f, 0.5f,  0,0,1,   0.5f, 0.5f, 0.5f, 0,0,1,  -0.5f, 0.5f, 0.5f, 0,0,1,
        -0.5f,-0.5f,-0.5f,  0,0,-1, -0.5f, 0.5f,-0.5f, 0,0,-1,  0.5f, 0.5f,-0.5f, 0,0,-1,
        -0.5f,-0.5f,-0.5f,  0,0,-1,  0.5f, 0.5f,-0.5f, 0,0,-1,  0.5f,-0.5f,-0.5f, 0,0,-1,
         0.5f,-0.5f,-0.5f,  1,0,0,   0.5f, 0.5f,-0.5f, 1,0,0,   0.5f, 0.5f, 0.5f, 1,0,0,
         0.5f,-0.5f,-0.5f,  1,0,0,   0.5f, 0.5f, 0.5f, 1,0,0,   0.5f,-0.5f, 0.5f, 1,0,0,
        -0.5f,-0.5f,-0.5f, -1,0,0,  -0.5f,-0.5f, 0.5f,-1,0,0,  -0.5f, 0.5f, 0.5f,-1,0,0,
        -0.5f,-0.5f,-0.5f, -1,0,0,  -0.5f, 0.5f, 0.5f,-1,0,0,  -0.5f, 0.5f,-0.5f,-1,0,0,
        -0.5f, 0.5f,-0.5f,  0,1,0,  -0.5f, 0.5f, 0.5f, 0,1,0,   0.5f, 0.5f, 0.5f, 0,1,0,
        -0.5f, 0.5f,-0.5f,  0,1,0,   0.5f, 0.5f, 0.5f, 0,1,0,   0.5f, 0.5f,-0.5f, 0,1,0,
        -0.5f,-0.5f,-0.5f,  0,-1,0,  0.5f,-0.5f,-0.5f, 0,-1,0,  0.5f,-0.5f, 0.5f, 0,-1,0,
        -0.5f,-0.5f,-0.5f,  0,-1,0,  0.5f,-0.5f, 0.5f, 0,-1,0, -0.5f,-0.5f, 0.5f, 0,-1,0,
    };
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    return vao;
}

static GLuint g_studTex = 0;

static void drawCube(GLuint prog, GLuint vao, const glm::mat4& model, const glm::mat4& view,
                     const glm::mat4& proj, const glm::vec3& color, const glm::vec3& sunDir,
                     const glm::vec3& camPos = {}) {
    glUseProgram(prog);
    glUniformMatrix4fv(glGetUniformLocation(prog, "uModel"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(prog, "uView"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(prog, "uProj"), 1, GL_FALSE, glm::value_ptr(proj));
    glUniform3fv(glGetUniformLocation(prog, "uColor"), 1, glm::value_ptr(color));
    GLint sunLoc = glGetUniformLocation(prog, "uSun");
    if (sunLoc < 0) sunLoc = glGetUniformLocation(prog, "uSunDir");
    glUniform3fv(sunLoc, 1, glm::value_ptr(sunDir));
    if (g_studTex) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_studTex);
        glUniform1i(glGetUniformLocation(prog, "uStud"), 0);
        glUniform1f(glGetUniformLocation(prog, "uStudScale"), kStudTile);
    }
#ifdef GARDEN_GLES
    glUniform3fv(glGetUniformLocation(prog, "uCamPos"), 1, glm::value_ptr(camPos));
#endif
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 36);
}

static void drawCubeForPlayer(GLuint vao, GLuint prog, const glm::mat4& model, const glm::mat4& view,
                              const glm::mat4& proj, const glm::vec3& color, const glm::vec3& sunDir) {
    drawCube(prog, vao, model, view, proj, color, sunDir);
}

static void drawUiRect(GLuint uiProg, GLuint uiVao, float x, float y, float w, float h,
                       glm::vec4 color, float sw, float sh) {
    float verts[] = {
        x,     y,
        x + w, y,
        x + w, y + h,
        x,     y,
        x + w, y + h,
        x,     y + h,
    };
    glDisable(GL_DEPTH_TEST);
    glUseProgram(uiProg);
    glUniform2f(glGetUniformLocation(uiProg, "uScreen"), sw, sh);
    glUniform4fv(glGetUniformLocation(uiProg, "uColor"), 1, glm::value_ptr(color));
    glBindVertexArray(uiVao);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    GLuint tmp;
    glGenBuffers(1, &tmp);
    glBindBuffer(GL_ARRAY_BUFFER, tmp);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STREAM_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDeleteBuffers(1, &tmp);
    glEnable(GL_DEPTH_TEST);
}

static glm::vec3 playerForward(float yawDeg) {
    float y = glm::radians(yawDeg);
    return glm::normalize(glm::vec3(cosf(y), 0.0f, sinf(y)));
}

static glm::mat4 thirdPersonView(const Player& player) {
    glm::vec3 fwd = playerForward(player.yawDeg);
    glm::vec3 camPos = player.position - fwd * 12.0f + glm::vec3(0.0f, 7.0f, 0.0f);
    glm::vec3 lookAt = player.position + glm::vec3(0.0f, 3.0f, 0.0f);
    return glm::lookAt(camPos, lookAt, glm::vec3(0, 1, 0));
}

static int nearestPlot(const Player& player, const std::vector<Plot>& plots) {
    int best = -1;
    float bestD = 999999.0f;
    for (int i = 0; i < (int)plots.size(); ++i) {
        glm::vec3 d = plots[i].center - player.position;
        d.y = 0.0f;
        float dist = glm::length(d);
        if (dist < bestD) { bestD = dist; best = i; }
    }
    return bestD < 4.5f ? best : -1;
}

static void updatePlots(std::vector<Plot>& plots, float dt) {
    for (Plot& p : plots) {
        if (p.type == PlantType::None) continue;
        if (p.growth < 1.0f) {
            p.growth += dt / plantGrowSeconds(p.type);
            if (p.growth > 1.0f) p.growth = 1.0f;
        }
    }
}

static void drawWorld3D(GLuint cubeVAO, GLuint partProg,
                        const glm::mat4& view, const glm::mat4& proj,
                        const glm::vec3& sunDir, const std::vector<Plot>& plots,
                        BlockPlayer& avatar, const Player& player, bool walking, float dt) {
    glm::vec3 camPos = player.position - playerForward(player.yawDeg) * 12.f + glm::vec3(0, 7, 0);
    Part grass{ glm::vec3(0, -1, 0), glm::vec3(120, 2, 120), glm::vec3(0.25f, 0.62f, 0.28f) };
    drawCube(partProg, cubeVAO,
             glm::scale(glm::translate(glm::mat4(1.0f), grass.position), grass.size),
             view, proj, grass.color, sunDir, camPos);

    for (const Plot& plot : plots) {
        glm::mat4 soil = glm::scale(
            glm::translate(glm::mat4(1.0f), plot.center + glm::vec3(0, 0.15f, 0)),
            glm::vec3(5.5f, 0.3f, 5.5f));
        drawCube(partProg, cubeVAO, soil, view, proj, glm::vec3(0.45f, 0.28f, 0.12f), sunDir);

        if (plot.type != PlantType::None) {
            float h = 0.5f + plot.growth * 2.5f;
            glm::mat4 plant = glm::scale(
                glm::translate(glm::mat4(1.0f), plot.center + glm::vec3(0, h * 0.5f + 0.3f, 0)),
                glm::vec3(0.8f, h, 0.8f));
            drawCube(partProg, cubeVAO, plant, view, proj, plantColor(plot.type, plot.growth), sunDir);
        }
    }

    // Recinto decorativo stile Grow a Garden
    for (int i = -1; i <= 1; i += 2) {
        drawCube(partProg, cubeVAO,
                 glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(i * 28.0f, 1.5f, 0)), glm::vec3(1, 3, 56)),
                 view, proj, glm::vec3(0.55f, 0.35f, 0.18f), sunDir);
        drawCube(partProg, cubeVAO,
                 glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0, 1.5f, i * 28.0f)), glm::vec3(56, 3, 1)),
                 view, proj, glm::vec3(0.55f, 0.35f, 0.18f), sunDir);
    }

    avatar.update(dt, walking);
    BlockDrawContext ctx{ cubeVAO, partProg, view, proj, sunDir };
    avatar.draw(ctx, drawCubeForPlayer, player.position, player.yawDeg);
}

static void drawMenuUi(GLuint uiProg, GLuint uiVao, float sw, float sh, float pulse) {
    drawUiRect(uiProg, uiVao, 0, 0, sw, sh, glm::vec4(0.12f, 0.45f, 0.22f, 1.0f), sw, sh);
    drawUiRect(uiProg, uiVao, sw * 0.2f, sh * 0.18f, sw * 0.6f, sh * 0.12f,
               glm::vec4(0.95f, 0.85f, 0.2f, 1.0f), sw, sh);
    float btnGlow = 0.55f + 0.25f * sinf(pulse * 3.0f);
    drawUiRect(uiProg, uiVao, sw * 0.32f, sh * 0.52f, sw * 0.36f, sh * 0.09f,
               glm::vec4(0.2f, 0.75f, 0.35f, btnGlow), sw, sh);
    drawUiRect(uiProg, uiVao, sw * 0.25f, sh * 0.72f, sw * 0.5f, sh * 0.04f,
               glm::vec4(1, 1, 1, 0.25f), sw, sh);
}

static void drawLoadingUi(GLuint uiProg, GLuint uiVao, float sw, float sh, float progress) {
    drawUiRect(uiProg, uiVao, 0, 0, sw, sh, glm::vec4(0.08f, 0.12f, 0.18f, 1.0f), sw, sh);
    float barW = sw * 0.5f;
    float barX = sw * 0.25f;
    float barY = sh * 0.55f;
    drawUiRect(uiProg, uiVao, barX, barY, barW, sh * 0.03f, glm::vec4(0.25f, 0.25f, 0.3f, 1.0f), sw, sh);
    drawUiRect(uiProg, uiVao, barX, barY, barW * progress, sh * 0.03f,
               glm::vec4(0.3f, 0.85f, 0.45f, 1.0f), sw, sh);
    // "Spinner" semplice
    float spin = progress * 6.283f;
    drawUiRect(uiProg, uiVao, sw * 0.48f + cosf(spin) * 40.0f, sh * 0.35f + sinf(spin) * 40.0f,
               24, 24, glm::vec4(0.95f, 0.75f, 0.2f, 1.0f), sw, sh);
}

static void drawShopUi(GLuint uiProg, GLuint uiVao, float sw, float sh,
                       const std::vector<ShopItem>& shop, const Player& player) {
    drawUiRect(uiProg, uiVao, sw * 0.15f, sh * 0.12f, sw * 0.7f, sh * 0.76f,
               glm::vec4(0.05f, 0.08f, 0.12f, 0.92f), sw, sh);
    drawUiRect(uiProg, uiVao, sw * 0.15f, sh * 0.12f, sw * 0.7f, sh * 0.08f,
               glm::vec4(0.95f, 0.7f, 0.15f, 1.0f), sw, sh);

    float y = sh * 0.24f;
    for (size_t i = 0; i < shop.size(); ++i) {
        drawUiRect(uiProg, uiVao, sw * 0.2f, y, sw * 0.6f, sh * 0.1f,
                   glm::vec4(0.15f, 0.18f, 0.22f, 1.0f), sw, sh);
        drawUiRect(uiProg, uiVao, sw * 0.22f, y + sh * 0.02f, sh * 0.06f, sh * 0.06f,
                   glm::vec4(shop[i].previewColor, 1.0f), sw, sh);
        // Indicatore tasto 1/2
        drawUiRect(uiProg, uiVao, sw * 0.72f, y + sh * 0.025f, sh * 0.05f, sh * 0.05f,
                   glm::vec4(0.3f, 0.8f, 0.4f, 1.0f), sw, sh);
        y += sh * 0.12f;
    }

    // Barra monete
    drawUiRect(uiProg, uiVao, sw * 0.2f, sh * 0.78f, sw * 0.25f, sh * 0.05f,
               glm::vec4(0.85f, 0.65f, 0.1f, 1.0f), sw, sh);
}

static void drawHud(GLuint uiProg, GLuint uiVao, float sw, float sh, const Player& player) {
    drawUiRect(uiProg, uiVao, 16, 16, 180, 36, glm::vec4(0, 0, 0, 0.45f), sw, sh);
    drawUiRect(uiProg, uiVao, 24, 24, 24, 24, glm::vec4(0.95f, 0.75f, 0.1f, 1.0f), sw, sh);
    drawUiRect(uiProg, uiVao, 56, 24, player.coins * 1.2f, 24, glm::vec4(0.95f, 0.75f, 0.1f, 0.35f), sw, sh);
    drawUiRect(uiProg, uiVao, sw - 196, 16, 180, 36, glm::vec4(0, 0, 0, 0.45f), sw, sh);
}

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);

    if (!glfwInit()) return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(g_width, g_height, "Grow a Garden - OpenGL", nullptr, nullptr);
    if (!window) { glfwTerminate(); return -1; }
    setWindowIconR(window);

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glfwSetFramebufferSizeCallback(window, [](GLFWwindow*, int w, int h) {
        g_width = w; g_height = h;
        glViewport(0, 0, w, h);
    });

    if (!gladLoadGL(glfwGetProcAddress)) return -1;
    glEnable(GL_DEPTH_TEST);

    GLuint cubeVAO = makeCubeVAO();
    GLuint partProg = makeProgram(kGardenVS, kGardenFS);
    Texture2D studTex;
    if (studTex.load(findStudTexture())) g_studTex = studTex.id;
    GLuint uiProg = makeProgram(kUiVS, kUiFS);

    GLuint uiVao;
    glGenVertexArrays(1, &uiVao);

    glm::vec3 sunDir = glm::normalize(glm::vec3(-0.4f, -1.0f, -0.3f));

    BlockPlayer avatar;
    GameScreen screen = GameScreen::Menu;
    Player player;
    std::vector<Plot> plots = makeGardenPlots();
    std::vector<ShopItem> shop = defaultShop();

    float loadingTimer = 0.0f;
    float menuPulse = 0.0f;
    bool walking = false;
    bool shopOpen = false;
    bool enterWasDown = false;
    bool eWasDown = false;
    bool bWasDown = false;
    bool key1WasDown = false;
    bool key2WasDown = false;

    float lastTime = (float)glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        float now = (float)glfwGetTime();
        float dt = now - lastTime;
        lastTime = now;
        if (dt > 0.05f) dt = 0.05f;

        bool enterDown = glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS;
        bool escDown = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
        if (escDown) glfwSetWindowShouldClose(window, true);

        glClearColor(0.53f, 0.72f, 0.92f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float sw = (float)g_width;
        float sh = (float)g_height;

        if (screen == GameScreen::Menu) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            menuPulse += dt;
            drawMenuUi(uiProg, uiVao, sw, sh, menuPulse);
            glfwSetWindowTitle(window, "Grow a Garden | ENTER = Gioca | ESC = Esci");

            if (enterDown && !enterWasDown) {
                screen = GameScreen::Loading;
                loadingTimer = 0.0f;
            }
        }
        else if (screen == GameScreen::Loading) {
            loadingTimer += dt;
            float progress = loadingTimer / 2.2f;
            if (progress > 1.0f) progress = 1.0f;
            drawLoadingUi(uiProg, uiVao, sw, sh, progress);
            glfwSetWindowTitle(window, "Caricamento mondo...");

            if (loadingTimer >= 2.2f) {
                screen = GameScreen::Playing;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            }
        }
        else if (screen == GameScreen::Playing) {
            updatePlots(plots, dt);

            glm::vec3 move(0.0f);
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) move += playerForward(player.yawDeg);
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) move -= playerForward(player.yawDeg);
            glm::vec3 right = glm::normalize(glm::cross(playerForward(player.yawDeg), glm::vec3(0, 1, 0)));
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) move -= right;
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) move += right;

            walking = glm::length(move) > 0.01f;
            if (walking) {
                move = glm::normalize(move);
                player.position += move * player.moveSpeed * dt;
                player.yawDeg = glm::degrees(atan2f(move.z, move.x));
            }

            // Limiti recinto
            player.position.x = glm::clamp(player.position.x, -26.0f, 26.0f);
            player.position.z = glm::clamp(player.position.z, -26.0f, 26.0f);

            bool bDown = glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS;
            if (bDown && !bWasDown) shopOpen = !shopOpen;
            bWasDown = bDown;

            bool eDown = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS;
            if (eDown && !eWasDown && !shopOpen) {
                int pi = nearestPlot(player, plots);
                if (pi >= 0) {
                    Plot& plot = plots[pi];
                    if (plot.type == PlantType::None) {
                        if (player.seedsCarrot > 0) {
                            plot.type = PlantType::Carrot;
                            plot.growth = 0.0f;
                            player.seedsCarrot--;
                        } else if (player.seedsTomato > 0) {
                            plot.type = PlantType::Tomato;
                            plot.growth = 0.0f;
                            player.seedsTomato--;
                        }
                    } else if (plot.growth >= 1.0f) {
                        player.coins += plantSellValue(plot.type);
                        plot.type = PlantType::None;
                        plot.growth = 0.0f;
                    }
                }
            }
            eWasDown = eDown;

            if (shopOpen) {
                bool k1 = glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS;
                bool k2 = glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS;
                if (k1 && !key1WasDown && player.coins >= shop[0].price) {
                    player.coins -= shop[0].price;
                    player.seedsCarrot++;
                }
                if (k2 && !key2WasDown && shop.size() > 1 && player.coins >= shop[1].price) {
                    player.coins -= shop[1].price;
                    player.seedsTomato++;
                }
                key1WasDown = k1;
                key2WasDown = k2;
            }

            glm::mat4 view = thirdPersonView(player);
            glm::mat4 proj = glm::perspective(glm::radians(60.0f), sw / sh, 0.1f, 500.0f);
            drawWorld3D(cubeVAO, partProg, view, proj, sunDir, plots, avatar, player, walking, dt);

            drawHud(uiProg, uiVao, sw, sh, player);
            if (shopOpen) drawShopUi(uiProg, uiVao, sw, sh, shop, player);

            char title[256];
            std::snprintf(title, sizeof(title),
                "Grow a Garden | Monete: %d | Carote: %d | Pomodori: %d | E=pianta/raccogli | B=shop",
                player.coins, player.seedsCarrot, player.seedsTomato);
            glfwSetWindowTitle(window, title);
        }

        enterWasDown = enterDown;
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
