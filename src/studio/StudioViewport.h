#pragma once

/**
 * Viewport 3D di Blocky Studio.
 *
 * Usa gli stessi shader, mesh e texture di BlockyGameEngine (GlShaders.h,
 * BeveledCube.h, stud.png) invece di averne di propri: quello che si vede
 * nell'editor deve essere quello che si vedrà in gioco, altrimenti si
 * costruisce alla cieca. L'unica cosa in più è la griglia dell'editor.
 *
 * Il rendering avviene in un framebuffer offscreen che finisce in una
 * ImGui::Image, così il 3D sta dentro il layout a pannelli.
 */

#include <glad/gl.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "BlockPlayer.h"
#include "engine/DataModel.h"
#include "engine/Humanoid.h"
#include "engine/LightingService.h"
#include "engine/PartMaterial.h"
#include "render/BeveledCube.h"
#include "render/GlShaders.h"
#include "render/PartMeshes.h"
#include "render/Texture2D.h"

namespace studio {

constexpr float kStudTile = 4.f;   // stessa scala del gioco

/** Griglia dell'editor: l'unico shader che il gioco non ha. */
inline const char* kGridVS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uViewProj;
uniform vec3 uOrigin;
uniform float uExtent;
out vec3 vWorld;
void main() {
    vec3 p = vec3(aPos.x * uExtent + uOrigin.x, 0.02, aPos.z * uExtent + uOrigin.z);
    vWorld = p;
    gl_Position = uViewProj * vec4(p, 1.0);
}
)";

inline const char* kGridFS = R"(#version 330 core
in vec3 vWorld;
uniform vec3 uCamPos;
uniform float uExtent;
out vec4 FragColor;

// Spessore costante in pixel: derivata dello spazio mondo per pixel.
float gridLine(vec2 p, float step, float thickness) {
    vec2 g = abs(fract(p / step - 0.5) - 0.5) / fwidth(p / step);
    return 1.0 - min(min(g.x, g.y) / thickness, 1.0);
}

void main() {
    vec2 p = vWorld.xz;
    float fine   = gridLine(p, 4.0, 1.0);
    float coarse = gridLine(p, 20.0, 1.4);

    // Dissolvenza col raggio: in lontananza le linee sarebbero solo aliasing.
    float fade = 1.0 - smoothstep(uExtent * 0.30, uExtent * 0.48, length(p - uCamPos.xz));

    vec3 color = mix(vec3(0.30, 0.33, 0.38), vec3(0.55, 0.60, 0.68), coarse);
    float alpha = max(fine * 0.30, coarse * 0.55) * fade;

    if (abs(p.y) < 0.06) color = vec3(0.85, 0.30, 0.30);
    if (abs(p.x) < 0.06) { color = vec3(0.30, 0.50, 0.90); alpha = max(alpha, 0.7 * fade); }

    if (alpha < 0.004) discard;
    FragColor = vec4(color, alpha);
}
)";

inline GLuint compileShader(GLenum type, const char* src, std::string* log) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = GL_FALSE;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[2048];
        glGetShaderInfoLog(s, sizeof(buf), nullptr, buf);
        if (log) { *log += buf; *log += "\n"; }
        glDeleteShader(s);
        return 0;
    }
    return s;
}

inline GLuint buildProgram(const char* vs, const char* fs, std::string* log) {
    GLuint v = compileShader(GL_VERTEX_SHADER, vs, log);
    if (!v) return 0;
    GLuint f = compileShader(GL_FRAGMENT_SHADER, fs, log);
    if (!f) { glDeleteShader(v); return 0; }

    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    glDeleteShader(v);
    glDeleteShader(f);

    GLint ok = GL_FALSE;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[2048];
        glGetProgramInfoLog(p, sizeof(buf), nullptr, buf);
        if (log) { *log += buf; *log += "\n"; }
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

/** Camera orbitale attorno a un bersaglio, come in ogni editor 3D. */
struct OrbitCamera {
    glm::vec3 target{0.f, 2.f, 0.f};
    // Lo spawn di default guarda verso -X (yaw -90): partendo da questo lato
    // si vede la faccia dell'avatar invece della nuca.
    float yaw = -1.25f;
    float pitch = 0.35f;
    float distance = 26.f;

    glm::vec3 eye() const {
        return target + glm::vec3(distance * std::cos(pitch) * std::sin(yaw),
                                  distance * std::sin(pitch),
                                  distance * std::cos(pitch) * std::cos(yaw));
    }

    void orbit(float dx, float dy) {
        yaw -= dx * 0.008f;
        pitch = std::clamp(pitch + dy * 0.008f, -1.45f, 1.45f);
    }

    void zoom(float steps) {
        // Moltiplicativo: da vicino i passi si accorciano da soli, altrimenti
        // una rotella sola attraversa l'oggetto.
        distance = std::clamp(distance * std::pow(0.88f, steps), 1.5f, 400.f);
    }

    void pan(float dx, float dy) {
        const glm::vec3 fwd = glm::normalize(target - eye());
        const glm::vec3 right = glm::normalize(glm::cross(fwd, {0, 1, 0}));
        const glm::vec3 up = glm::cross(right, fwd);
        target += (-right * dx + up * dy) * (distance * 0.0016f);
    }

    glm::mat4 view() const { return glm::lookAt(eye(), target, {0, 1, 0}); }
};

struct Viewport;

/**
 * BlockPlayer::draw vuole un puntatore a funzione nudo, che non può catturare
 * nulla. Questo puntatore fa da ponte per la durata della sola chiamata: è
 * sicuro perché il rendering è single-thread e viene azzerato subito dopo.
 */
inline Viewport* g_drawViewport = nullptr;

void studioDrawAvatarCube(GLuint vao, GLuint prog, const glm::mat4& model,
                          const glm::mat4& view, const glm::mat4& proj,
                          const glm::vec3& color, const glm::vec3& sun);

struct Viewport {
    GLuint fbo = 0, colorTex = 0, depthRbo = 0;
    int width = 0, height = 0;

    GLuint partProgram = 0;    // kPartVS + kPartFS  — materiali, luci, Neon
    GLuint studProgram = 0;    // kPartVS + kStudFS  — idem, con stud sulla faccia top
    GLuint gridProgram = 0;

    // Il gioco smussa solo il player: le Part del mondo restano spigolose.
    GLuint cubeVao = 0, cubeVbo = 0;   // Part e baseplate
    BeveledCubeMesh bevelCube;         // solo avatar, bevel 0.11 come in gioco
    PartMeshes shapes;                 // ball / cylinder / wedge
    Texture2D studTex;
    bool studReady = false;

    LightingService lighting;
    BlockPlayer blockPlayer;   // stesso avatar R6 del gioco, faccia smile inclusa
    bool showCharacter = true;

    GLuint quadVao = 0, quadVbo = 0;
    OrbitCamera camera;
    float fovY = glm::radians(50.f);
    std::string buildLog;

    // Matrici del frame corrente: gli shader del gioco vogliono view e proj
    // separate, non una MVP già moltiplicata.
    glm::mat4 frameView{1.f};
    glm::mat4 frameProj{1.f};
    glm::vec3 frameEye{0.f};
    glm::vec3 frameSun{0.f, -1.f, 0.f};

    bool init() {
        partProgram = buildProgram(kPartVS, kPartFS, &buildLog);
        studProgram = buildProgram(kPartVS, kStudFS, &buildLog);
        gridProgram = buildProgram(kGridVS, kGridFS, &buildLog);
        if (!partProgram || !gridProgram) return false;

        bevelCube.build(0.11f);   // stesso bevel del player in gioco
        shapes.buildAll();
        buildCube();
        buildQuad();

        studReady = studTex.load(findStudTexture(), true);
        return true;
    }

    void destroy() {
        bevelCube.destroy();
        shapes.destroy();
        studTex.destroy();
        if (cubeVbo) glDeleteBuffers(1, &cubeVbo);
        if (cubeVao) glDeleteVertexArrays(1, &cubeVao);
        if (quadVbo) glDeleteBuffers(1, &quadVbo);
        if (quadVao) glDeleteVertexArrays(1, &quadVao);
        if (partProgram) glDeleteProgram(partProgram);
        if (studProgram) glDeleteProgram(studProgram);
        if (gridProgram) glDeleteProgram(gridProgram);
        destroyTargets();
        quadVbo = quadVao = partProgram = studProgram = gridProgram = 0;
    }

    void destroyTargets() {
        if (depthRbo) { glDeleteRenderbuffers(1, &depthRbo); depthRbo = 0; }
        if (colorTex) { glDeleteTextures(1, &colorTex); colorTex = 0; }
        if (fbo) { glDeleteFramebuffers(1, &fbo); fbo = 0; }
    }

    void resize(int w, int h) {
        w = std::max(w, 16);
        h = std::max(h, 16);
        if (w == width && h == height && fbo) return;

        destroyTargets();
        width = w;
        height = h;

        glGenTextures(1, &colorTex);
        glBindTexture(GL_TEXTURE_2D, colorTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glGenRenderbuffers(1, &depthRbo);
        glBindRenderbuffer(GL_RENDERBUFFER, depthRbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);

        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthRbo);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    float aspect() const { return height > 0 ? (float)width / (float)height : 1.f; }
    glm::mat4 projection() const { return glm::perspective(fovY, aspect(), 0.08f, 2000.f); }
    glm::mat4 viewProjection() const { return projection() * camera.view(); }

    // ---------------------------------------------------------------- render

    void render(const DataModel& world, int selectedPart, bool showGrid) {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, width, height);

        // Stesso colore della foschia del gioco: l'orizzonte non stacca.
        const glm::vec3 sky = lighting.fogColor;
        glClearColor(sky.r, sky.g, sky.b, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);

        frameView = camera.view();
        frameProj = projection();
        frameEye = camera.eye();
        frameSun = lighting.sunDirection();

        collectLights(world);

        if (world.hasBaseplate) {
            const glm::mat4 m = glm::translate(glm::mat4(1.f), {0.f, -world.baseplateSize.y * 0.5f, 0.f})
                              * glm::scale(glm::mat4(1.f), world.baseplateSize);
            drawShape(PartShape::Block, m, world.baseplateColor, 0.f, PartMaterial::Plastic, true);
        }

        for (const WorkspacePart& p : world.parts) {
            if (p.isTransparent()) continue;
            drawShape(p.shape, partModelMatrix(p), p.color, p.emissive, p.material, p.useStud);
        }

        if (showCharacter) drawCharacter(world.spawn.position, world.spawn.yaw);

        if (showGrid) drawGrid();

        // Trasparenti in coda e senza scrivere profondità, altrimenti
        // cancellano quello che sta dietro.
        for (const WorkspacePart& p : world.parts) {
            if (!p.isTransparent()) continue;
            drawShape(p.shape, partModelMatrix(p), p.color, p.emissive, p.material, p.useStud);
        }

        if (selectedPart >= 0 && selectedPart < (int)world.parts.size())
            drawSelectionBox(world.parts[(size_t)selectedPart]);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    /** Point light delle Part → uniform array degli shader del gioco. */
    void collectLights(const DataModel& world) {
        lightPos.clear();
        lightCol.clear();
        lightRange.clear();

        for (const WorkspacePart& p : world.parts) {
            if (!p.pointLight || lightPos.size() >= 8) continue;
            lightPos.push_back(p.position);
            lightCol.push_back(p.color * p.lightBrightness);
            lightRange.push_back(p.lightRange);
        }
    }

    void uploadLights(GLuint prog) const {
        const int n = (int)lightPos.size();
        glUniform1i(glGetUniformLocation(prog, "uLightCount"), n);
        if (n > 0) {
            glUniform3fv(glGetUniformLocation(prog, "uLightPos"), n, glm::value_ptr(lightPos[0]));
            glUniform3fv(glGetUniformLocation(prog, "uLightCol"), n, glm::value_ptr(lightCol[0]));
            glUniform1fv(glGetUniformLocation(prog, "uLightRange"), n, lightRange.data());
        }
    }

    /** Stessa sequenza di uniform di drawCube() in main.cpp. */
    void drawShape(PartShape shape, const glm::mat4& model, const glm::vec3& color,
                   float emissive, PartMaterial material, bool useStud) {
        if ((int)material < 0 || material >= PartMaterial::Count) material = PartMaterial::Plastic;

        float specular = 0.12f, roughness = 0.75f, emBias = 0.f, alpha = 1.f;
        partMaterialShading(material, specular, roughness, emBias, alpha);
        const float em = std::max(emissive, emBias);

        const bool transparent = alpha < 0.99f;
        if (transparent) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);
        }

        const bool wantStud = useStud && studReady && studProgram != 0;
        const GLuint prog = wantStud ? studProgram : partProgram;

        glUseProgram(prog);
        glUniformMatrix4fv(glGetUniformLocation(prog, "uModel"), 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(prog, "uView"), 1, GL_FALSE, glm::value_ptr(frameView));
        glUniformMatrix4fv(glGetUniformLocation(prog, "uProj"), 1, GL_FALSE, glm::value_ptr(frameProj));
        glUniform3fv(glGetUniformLocation(prog, "uColor"), 1, glm::value_ptr(color));
        glUniform3fv(glGetUniformLocation(prog, "uSun"), 1, glm::value_ptr(frameSun));
        glUniform3fv(glGetUniformLocation(prog, "uCamPos"), 1, glm::value_ptr(frameEye));
        glUniform1f(glGetUniformLocation(prog, "uEmissive"), em);
        glUniform1f(glGetUniformLocation(prog, "uSpecular"), specular);
        glUniform1f(glGetUniformLocation(prog, "uRoughness"), roughness);
        glUniform1f(glGetUniformLocation(prog, "uAlpha"), alpha);
        glUniform1i(glGetUniformLocation(prog, "uMaterial"), (int)material);
        uploadLights(prog);

        if (wantStud) {
            glActiveTexture(GL_TEXTURE0);
            studTex.bind(0);
            glUniform1i(glGetUniformLocation(prog, "uStud"), 0);
            glUniform1f(glGetUniformLocation(prog, "uStudScale"), kStudTile);
        }

        switch (shape) {
        case PartShape::Ball:
            bindAndDraw(shapes.ball.vao, shapes.ball.vertexCount);
            break;
        case PartShape::Cylinder:
            bindAndDraw(shapes.cylinder.vao, shapes.cylinder.vertexCount);
            break;
        case PartShape::Wedge:
            bindAndDraw(shapes.wedge.vao, shapes.wedge.vertexCount);
            break;
        default:
            bindAndDraw(cubeVao, 36);   // spigoloso, come le Part in gioco
            break;
        }

        if (transparent) {
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
        }
    }

    /**
     * Avatar R6 nel punto di spawn, disegnato da BlockPlayer — la stessa classe
     * che usa il gioco. Reimplementare la catena degli arti qui dentro avrebbe
     * significato dimenticarsi pezzi (il bob, la faccia smile) e vederli
     * divergere al primo ritocco: meglio chiamare l'originale.
     */
    void drawCharacter(const glm::vec3& position, float yawDegrees) {
        const BlockDrawContext ctx{bevelCube.vao, partProgram, frameView, frameProj, frameSun};
        g_drawViewport = this;
        blockPlayer.draw(ctx, &studioDrawAvatarCube, position, yawDegrees, nullptr);
        g_drawViewport = nullptr;
    }

    void drawGrid() {
        const float extent = 400.f;
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);

        glUseProgram(gridProgram);
        const glm::mat4 vp = frameProj * frameView;
        glUniformMatrix4fv(glGetUniformLocation(gridProgram, "uViewProj"), 1, GL_FALSE,
                           glm::value_ptr(vp));
        glUniform3f(glGetUniformLocation(gridProgram, "uOrigin"), camera.target.x, 0.f, camera.target.z);
        glUniform3fv(glGetUniformLocation(gridProgram, "uCamPos"), 1, glm::value_ptr(frameEye));
        glUniform1f(glGetUniformLocation(gridProgram, "uExtent"), extent);

        glBindVertexArray(quadVao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        glEnable(GL_CULL_FACE);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    void drawSelectionBox(const WorkspacePart& p) {
        const glm::mat4 m = glm::translate(glm::mat4(1.f), p.position)
                          * partRotationMatrix(p.rotation)
                          * glm::scale(glm::mat4(1.f), p.size + glm::vec3(0.06f));

        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glDisable(GL_CULL_FACE);
        glLineWidth(2.f);
        drawShape(PartShape::Block, m, {1.f, 0.78f, 0.15f}, 1.6f, PartMaterial::Neon, false);
        glLineWidth(1.f);
        glEnable(GL_CULL_FACE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    // --------------------------------------------------------------- picking

    glm::vec3 rayDirection(float u, float v) const {
        const float ndcX = u * 2.f - 1.f;
        const float ndcY = 1.f - v * 2.f;
        const float tanHalf = std::tan(fovY * 0.5f);

        const glm::vec3 eye = camera.eye();
        const glm::vec3 fwd = glm::normalize(camera.target - eye);
        const glm::vec3 right = glm::normalize(glm::cross(fwd, {0, 1, 0}));
        const glm::vec3 up = glm::cross(right, fwd);

        return glm::normalize(fwd + right * (ndcX * tanHalf * aspect()) + up * (ndcY * tanHalf));
    }

    int pick(const DataModel& world, float u, float v) const {
        const glm::vec3 origin = camera.eye();
        const glm::vec3 dir = rayDirection(u, v);

        int best = -1;
        float bestT = 1e30f;
        for (size_t i = 0; i < world.parts.size(); ++i) {
            float t;
            if (rayHitsPart(world.parts[i], origin, dir, t) && t < bestT) {
                bestT = t;
                best = (int)i;
            }
        }
        return best;
    }

    /** Slab test in spazio locale: becca anche le Part ruotate, non solo le AABB. */
    static bool rayHitsPart(const WorkspacePart& p, const glm::vec3& origin,
                            const glm::vec3& dir, float& outT) {
        const glm::mat4 inv = glm::inverse(glm::translate(glm::mat4(1.f), p.position)
                                           * partRotationMatrix(p.rotation));
        const glm::vec3 o = glm::vec3(inv * glm::vec4(origin, 1.f));
        const glm::vec3 d = glm::vec3(inv * glm::vec4(dir, 0.f));
        const glm::vec3 half = p.size * 0.5f;

        float tMin = -1e30f, tMax = 1e30f;
        for (int a = 0; a < 3; ++a) {
            if (std::fabs(d[a]) < 1e-6f) {
                if (o[a] < -half[a] || o[a] > half[a]) return false;
                continue;
            }
            float t1 = (-half[a] - o[a]) / d[a];
            float t2 = ( half[a] - o[a]) / d[a];
            if (t1 > t2) std::swap(t1, t2);
            tMin = std::max(tMin, t1);
            tMax = std::min(tMax, t2);
            if (tMin > tMax) return false;
        }
        if (tMax < 0.f) return false;
        outT = tMin >= 0.f ? tMin : tMax;
        return true;
    }

    bool worldToScreen(const glm::vec3& world, glm::vec2& out) const {
        const glm::vec4 clip = viewProjection() * glm::vec4(world, 1.f);
        if (clip.w <= 1e-5f) return false;
        const glm::vec3 ndc = glm::vec3(clip) / clip.w;
        out.x = (ndc.x * 0.5f + 0.5f) * (float)width;
        out.y = (1.f - (ndc.y * 0.5f + 0.5f)) * (float)height;
        return true;
    }

private:
    std::vector<glm::vec3> lightPos, lightCol;
    std::vector<float> lightRange;

    void bindAndDraw(GLuint vao, int count) {
        if (!vao || count <= 0) return;
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, count);
        glBindVertexArray(0);
    }

    /** Cubo unitario spigoloso: posizione + normale, 36 vertici. */
    void buildCube() {
        const float v[] = {
             .5f,-.5f, .5f, 1,0,0,   .5f,-.5f,-.5f, 1,0,0,   .5f, .5f,-.5f, 1,0,0,
             .5f,-.5f, .5f, 1,0,0,   .5f, .5f,-.5f, 1,0,0,   .5f, .5f, .5f, 1,0,0,
            -.5f,-.5f,-.5f,-1,0,0,  -.5f,-.5f, .5f,-1,0,0,  -.5f, .5f, .5f,-1,0,0,
            -.5f,-.5f,-.5f,-1,0,0,  -.5f, .5f, .5f,-1,0,0,  -.5f, .5f,-.5f,-1,0,0,
            -.5f, .5f, .5f, 0,1,0,   .5f, .5f, .5f, 0,1,0,   .5f, .5f,-.5f, 0,1,0,
            -.5f, .5f, .5f, 0,1,0,   .5f, .5f,-.5f, 0,1,0,  -.5f, .5f,-.5f, 0,1,0,
            -.5f,-.5f,-.5f, 0,-1,0,  .5f,-.5f,-.5f, 0,-1,0,  .5f,-.5f, .5f, 0,-1,0,
            -.5f,-.5f,-.5f, 0,-1,0,  .5f,-.5f, .5f, 0,-1,0, -.5f,-.5f, .5f, 0,-1,0,
            -.5f,-.5f, .5f, 0,0,1,   .5f,-.5f, .5f, 0,0,1,   .5f, .5f, .5f, 0,0,1,
            -.5f,-.5f, .5f, 0,0,1,   .5f, .5f, .5f, 0,0,1,  -.5f, .5f, .5f, 0,0,1,
             .5f,-.5f,-.5f, 0,0,-1, -.5f,-.5f,-.5f, 0,0,-1, -.5f, .5f,-.5f, 0,0,-1,
             .5f,-.5f,-.5f, 0,0,-1, -.5f, .5f,-.5f, 0,0,-1,  .5f, .5f,-.5f, 0,0,-1,
        };
        glGenVertexArrays(1, &cubeVao);
        glGenBuffers(1, &cubeVbo);
        glBindVertexArray(cubeVao);
        glBindBuffer(GL_ARRAY_BUFFER, cubeVbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glBindVertexArray(0);
    }

    void buildQuad() {
        const float v[] = {
            -1, 0, -1,   1, 0, -1,   1, 0, 1,
            -1, 0, -1,   1, 0,  1,  -1, 0, 1,
        };
        glGenVertexArrays(1, &quadVao);
        glGenBuffers(1, &quadVbo);
        glBindVertexArray(quadVao);
        glBindBuffer(GL_ARRAY_BUFFER, quadVbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
    }
};

/**
 * Un cubo dell'avatar. Stessa sequenza di uniform di drawCube() in main.cpp,
 * senza materiali né luci: le parti del corpo sono tinte piatte.
 */
inline void studioDrawAvatarCube(GLuint vao, GLuint prog, const glm::mat4& model,
                                 const glm::mat4& view, const glm::mat4& proj,
                                 const glm::vec3& color, const glm::vec3& sun) {
    (void)vao;
    if (!g_drawViewport) return;

    glUseProgram(prog);
    glUniformMatrix4fv(glGetUniformLocation(prog, "uModel"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(prog, "uView"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(prog, "uProj"), 1, GL_FALSE, glm::value_ptr(proj));
    glUniform3fv(glGetUniformLocation(prog, "uColor"), 1, glm::value_ptr(color));
    glUniform3fv(glGetUniformLocation(prog, "uSun"), 1, glm::value_ptr(sun));
    glUniform3fv(glGetUniformLocation(prog, "uCamPos"), 1, glm::value_ptr(g_drawViewport->frameEye));
    glUniform1f(glGetUniformLocation(prog, "uEmissive"), 0.f);
    glUniform1f(glGetUniformLocation(prog, "uSpecular"), 0.12f);
    glUniform1f(glGetUniformLocation(prog, "uRoughness"), 0.75f);
    glUniform1f(glGetUniformLocation(prog, "uAlpha"), 1.f);
    glUniform1i(glGetUniformLocation(prog, "uMaterial"), (int)PartMaterial::Plastic);
    g_drawViewport->uploadLights(prog);

    // Cubi smussati come le Part in gioco, non spigolosi.
    g_drawViewport->bevelCube.draw();
}

} // namespace studio
