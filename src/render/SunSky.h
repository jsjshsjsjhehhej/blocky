#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <cstdio>
#include <algorithm>
#include <vector>

/**
 * Cielo Roblox-like: gradient 2D a schermo intero + sole billboard con texture 2D
 * e shading (alone caldo + alone soft).
 */
struct SunSky {
    GLuint skyProg = 0;
    GLuint sunProg = 0;
    GLuint quadVao = 0;
    GLuint quadVbo = 0;
    GLuint sunTex = 0;
    GLuint moonTex = 0;
    int texSize = 256;
    bool ready = false;

    static GLuint compile(GLenum type, const char* src) {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        GLint ok = 0;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[512];
            glGetShaderInfoLog(s, 512, nullptr, log);
            std::fprintf(stderr, "SunSky shader: %s\n", log);
        }
        return s;
    }

    static GLuint link(GLuint vs, GLuint fs) {
        GLuint p = glCreateProgram();
        glAttachShader(p, vs);
        glAttachShader(p, fs);
        glLinkProgram(p);
        glDeleteShader(vs);
        glDeleteShader(fs);
        return p;
    }

    /** Texture 2D procedurale: disco sole + alone (stile classic Roblox). */
    void buildSunTexture(int size = 256) {
        texSize = size;
        std::vector<unsigned char> px((size_t)size * size * 4);
        float cx = (size - 1) * 0.5f;
        float coreR = size * 0.18f;
        float midR = size * 0.32f;
        float glowR = size * 0.48f;
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                float dx = (float)x - cx;
                float dy = (float)y - cx;
                float r = std::sqrt(dx * dx + dy * dy);
                float a = 0.f;
                float rf = 1.f, gf = 0.92f, bf = 0.55f;
                if (r <= coreR) {
                    float t = r / coreR;
                    a = 1.f;
                    rf = 1.f;
                    gf = 0.98f - t * 0.08f;
                    bf = 0.72f - t * 0.25f;
                } else if (r <= midR) {
                    float t = (r - coreR) / (midR - coreR);
                    a = 1.f - t * 0.15f;
                    rf = 1.f;
                    gf = 0.85f - t * 0.15f;
                    bf = 0.35f - t * 0.15f;
                } else if (r <= glowR) {
                    float t = (r - midR) / (glowR - midR);
                    a = (1.f - t) * (1.f - t) * 0.40f;
                    rf = 1.f;
                    gf = 0.75f;
                    bf = 0.25f;
                }
                size_t i = ((size_t)y * size + x) * 4;
                px[i + 0] = (unsigned char)(std::clamp(rf, 0.f, 1.f) * 255.f);
                px[i + 1] = (unsigned char)(std::clamp(gf, 0.f, 1.f) * 255.f);
                px[i + 2] = (unsigned char)(std::clamp(bf, 0.f, 1.f) * 255.f);
                px[i + 3] = (unsigned char)(std::clamp(a, 0.f, 1.f) * 255.f);
            }
        }
        if (sunTex) glDeleteTextures(1, &sunTex);
        glGenTextures(1, &sunTex);
        glBindTexture(GL_TEXTURE_2D, sunTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    }

    /** Luna: disco freddo con alone tenue e due crateri appena accennati. */
    void buildMoonTexture(int size = 256) {
        std::vector<unsigned char> px((size_t)size * size * 4);
        float cx = (size - 1) * 0.5f;
        float coreR = size * 0.20f;
        float glowR = size * 0.42f;
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                float dx = (float)x - cx;
                float dy = (float)y - cx;
                float r = std::sqrt(dx * dx + dy * dy);
                float a = 0.f;
                float rf = 0.88f, gf = 0.92f, bf = 1.f;
                if (r <= coreR) {
                    a = 1.f;
                    // crateri: due macchie leggermente più scure
                    float c1 = std::sqrt((dx + coreR * 0.30f) * (dx + coreR * 0.30f)
                                       + (dy - coreR * 0.22f) * (dy - coreR * 0.22f));
                    float c2 = std::sqrt((dx - coreR * 0.34f) * (dx - coreR * 0.34f)
                                       + (dy + coreR * 0.30f) * (dy + coreR * 0.30f));
                    float shade = 1.f;
                    if (c1 < coreR * 0.30f) shade = 0.86f;
                    if (c2 < coreR * 0.22f) shade = 0.82f;
                    rf *= shade; gf *= shade; bf *= shade;
                } else if (r <= glowR) {
                    float t = (r - coreR) / (glowR - coreR);
                    a = (1.f - t) * (1.f - t) * 0.28f;
                    rf = 0.72f; gf = 0.80f; bf = 1.f;
                }
                size_t i = ((size_t)y * size + x) * 4;
                px[i + 0] = (unsigned char)(std::clamp(rf, 0.f, 1.f) * 255.f);
                px[i + 1] = (unsigned char)(std::clamp(gf, 0.f, 1.f) * 255.f);
                px[i + 2] = (unsigned char)(std::clamp(bf, 0.f, 1.f) * 255.f);
                px[i + 3] = (unsigned char)(std::clamp(a, 0.f, 1.f) * 255.f);
            }
        }
        if (moonTex) glDeleteTextures(1, &moonTex);
        glGenTextures(1, &moonTex);
        glBindTexture(GL_TEXTURE_2D, moonTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    }

    bool init() {
        static const char* skyVS = R"(#version 330 core
layout(location=0) in vec2 aPos;
out vec2 vUV;
void main(){
  vUV = aPos * 0.5 + 0.5;
  gl_Position = vec4(aPos, 0.999, 1.0);
})";
        static const char* skyFS = R"(#version 330 core
in vec2 vUV; out vec4 FragColor;
uniform vec3 uZenith;
uniform vec3 uHorizon;
uniform vec3 uSunDir; // direzione verso il sole (world, normalizzata)
uniform mat4 uInvViewProj;
uniform float uNight; // 0 = giorno, 1 = notte (stelle + alone spento)
float hash21(vec2 p){
  return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}
void main(){
  // Ray in NDC → world (per gradient + alone cielo)
  vec2 ndc = vUV * 2.0 - 1.0;
  vec4 nearP = uInvViewProj * vec4(ndc, 0.0, 1.0);
  vec4 farP  = uInvViewProj * vec4(ndc, 1.0, 1.0);
  nearP.xyz /= nearP.w; farP.xyz /= farP.w;
  vec3 dir = normalize(farP.xyz - nearP.xyz);
  float h = clamp(dir.y * 0.5 + 0.5, 0.0, 1.0);
  vec3 sky = mix(uHorizon, uZenith, pow(h, 0.85));
  float sunAmt = max(dot(dir, normalize(uSunDir)), 0.0);
  float day = 1.0 - uNight;
  sky += vec3(1.0, 0.85, 0.45) * pow(sunAmt, 32.0) * 0.35 * day;
  sky += vec3(1.0, 0.70, 0.30) * pow(sunAmt, 8.0) * 0.10 * day;
  // Stelle: griglia su direzione, solo sopra l'orizzonte e solo di notte
  if(uNight > 0.02 && dir.y > 0.0){
    vec2 cell = floor(dir.xz / max(abs(dir.y), 0.25) * 46.0);
    float r = hash21(cell);
    if(r > 0.987){
      float tw = 0.65 + 0.35 * hash21(cell + 3.7);
      sky += vec3(0.95, 0.97, 1.0) * tw * uNight * smoothstep(0.0, 0.25, dir.y);
    }
  }
  FragColor = vec4(sky, 1.0);
})";

        static const char* sunVS = R"(#version 330 core
layout(location=0) in vec2 aPos;
out vec2 vUV;
uniform mat4 uMVP;
void main(){
  vUV = aPos * 0.5 + 0.5;
  gl_Position = uMVP * vec4(aPos, 0.0, 1.0);
})";
        static const char* sunFS = R"(#version 330 core
in vec2 vUV; out vec4 FragColor;
uniform sampler2D uSunTex;
uniform float uBrightness;
uniform float uFade; // 1 = pieno, 0 = sotto l'orizzonte / invisibile
void main(){
  vec4 t = texture(uSunTex, vUV);
  if (t.a < 0.02) discard;
  FragColor = vec4(t.rgb * uBrightness, t.a * uFade);
})";

        skyProg = link(compile(GL_VERTEX_SHADER, skyVS), compile(GL_FRAGMENT_SHADER, skyFS));
        sunProg = link(compile(GL_VERTEX_SHADER, sunVS), compile(GL_FRAGMENT_SHADER, sunFS));

        const float quad[] = {
            -1.f, -1.f,  1.f, -1.f,  1.f, 1.f,
            -1.f, -1.f,  1.f,  1.f, -1.f, 1.f,
        };
        glGenVertexArrays(1, &quadVao);
        glGenBuffers(1, &quadVbo);
        glBindVertexArray(quadVao);
        glBindBuffer(GL_ARRAY_BUFFER, quadVbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        glBindVertexArray(0);

        buildSunTexture(256);
        buildMoonTexture(256);
        ready = skyProg && sunProg && sunTex && moonTex && quadVao;
        return ready;
    }

    void destroy() {
        if (skyProg) glDeleteProgram(skyProg);
        if (sunProg) glDeleteProgram(sunProg);
        if (quadVao) glDeleteVertexArrays(1, &quadVao);
        if (quadVbo) glDeleteBuffers(1, &quadVbo);
        if (sunTex) glDeleteTextures(1, &sunTex);
        if (moonTex) glDeleteTextures(1, &moonTex);
        skyProg = sunProg = quadVao = quadVbo = sunTex = moonTex = 0;
        ready = false;
    }

    /**
     * toSun = direzione VERSO il sole (può stare sotto l'orizzonte: allora sorge la luna).
     * night = 0 pieno giorno … 1 piena notte. zenith/horizon = colori cielo per quell'ora.
     */
    void draw(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& toSun,
              const glm::vec3& camPos, float night = 0.f,
              const glm::vec3& zenith = glm::vec3(0.42f, 0.65f, 0.95f),
              const glm::vec3& horizon = glm::vec3(0.69f, 0.84f, 0.95f)) {
        if (!ready) return;

        glm::vec3 sunDir = glm::normalize(toSun);
        glm::vec3 moonDir = -sunDir;
        night = std::clamp(night, 0.f, 1.f);

        glm::mat4 invVP = glm::inverse(proj * view);

        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);

        // —— Cielo gradient + alone + stelle ——
        glUseProgram(skyProg);
        glUniform3fv(glGetUniformLocation(skyProg, "uZenith"), 1, glm::value_ptr(zenith));
        glUniform3fv(glGetUniformLocation(skyProg, "uHorizon"), 1, glm::value_ptr(horizon));
        glUniform3fv(glGetUniformLocation(skyProg, "uSunDir"), 1, glm::value_ptr(sunDir));
        glUniform1f(glGetUniformLocation(skyProg, "uNight"), night);
        glUniformMatrix4fv(glGetUniformLocation(skyProg, "uInvViewProj"), 1, GL_FALSE,
                           glm::value_ptr(invVP));
        glBindVertexArray(quadVao);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // Basis camera-facing condivisa dai due billboard
        glm::vec3 camRight = glm::normalize(glm::vec3(view[0][0], view[1][0], view[2][0]));
        glm::vec3 camUp = glm::normalize(glm::vec3(view[0][1], view[1][1], view[2][1]));
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(sunProg);
        glBindVertexArray(quadVao);

        auto drawBody = [&](const glm::vec3& dir, GLuint tex, float size, float brightness,
                            float extraFade) {
            // sotto l'orizzonte: sparisce con una dissolvenza corta
            float fade = std::clamp((dir.y + 0.06f) * 9.f, 0.f, 1.f) * extraFade;
            if (fade <= 0.001f) return;
            glm::mat4 model(1.f);
            model[0] = glm::vec4(camRight * size, 0.f);
            model[1] = glm::vec4(camUp * size, 0.f);
            model[2] = glm::vec4(glm::normalize(glm::cross(camRight, camUp)) * size, 0.f);
            model[3] = glm::vec4(camPos + dir * 420.f, 1.f);
            glm::mat4 mvp = proj * view * model;
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, tex);
            glUniform1i(glGetUniformLocation(sunProg, "uSunTex"), 0);
            glUniform1f(glGetUniformLocation(sunProg, "uBrightness"), brightness);
            glUniform1f(glGetUniformLocation(sunProg, "uFade"), fade);
            glUniformMatrix4fv(glGetUniformLocation(sunProg, "uMVP"), 1, GL_FALSE,
                               glm::value_ptr(mvp));
            glDrawArrays(GL_TRIANGLES, 0, 6);
        };

        drawBody(sunDir, sunTex, 32.f, 1.05f, 1.f);
        // di giorno la luna resta appena accennata
        drawBody(moonDir, moonTex, 24.f, 0.95f, 0.15f + 0.85f * night);

        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glBindVertexArray(0);
    }
};
