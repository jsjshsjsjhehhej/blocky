#pragma once

#include <glad/gl.h>
#include "render/GlslPortable.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

struct Particle {
    glm::vec3 pos{0};
    glm::vec3 vel{0};
    glm::vec3 color{1.f};
    float life = 1.f;
    float maxLife = 1.f;
    float size0 = 0.4f;
    float size1 = 0.05f;
    float drag = 1.5f;
    float gravity = -6.f;
};

enum class ParticlePreset : int {
    Burst = 0,
    Sparks,
    Smoke,
    Coins,
};

struct ExplosionBall {
    glm::vec3 pos{0};
    float age = 0.f;
    float duration = 0.65f;
    float maxRadius = 16.f;
};

class ParticleSystem {
public:
    static constexpr int kMaxParticles = 64;

    std::vector<Particle> particles;
    std::vector<ExplosionBall> explosions;
    GLuint prog = 0;
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint tex = 0;
    bool ready = false;
    GLint locMVP = -1, locColor = -1, locAlpha = -1, locTex = -1;

    static GLuint compile(GLenum type, const char* src) {
        GLuint s = glCreateShader(type);
        const std::string portableSrc = glslForTarget(src);
        const char* srcPtr = portableSrc.c_str();
        glShaderSource(s, 1, &srcPtr, nullptr);
        glCompileShader(s);
        GLint ok = 0;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[512];
            glGetShaderInfoLog(s, 512, nullptr, log);
            std::fprintf(stderr, "Particles shader: %s\n", log);
        }
        return s;
    }

    bool init() {
        static const char* vs = R"(#version 330 core
layout(location=0) in vec2 aPos;
out vec2 vUV;
uniform mat4 uMVP;
void main(){
  vUV = aPos * 0.5 + 0.5;
  gl_Position = uMVP * vec4(aPos, 0.0, 1.0);
})";
        static const char* fs = R"(#version 330 core
in vec2 vUV; out vec4 FragColor;
uniform sampler2D uTex;
uniform vec3 uColor;
uniform float uAlpha;
void main(){
  float a = texture(uTex, vUV).a * uAlpha;
  if (a < 0.02) discard;
  FragColor = vec4(uColor, a);
})";

        GLuint vsh = compile(GL_VERTEX_SHADER, vs);
        GLuint fsh = compile(GL_FRAGMENT_SHADER, fs);
        prog = glCreateProgram();
        glAttachShader(prog, vsh);
        glAttachShader(prog, fsh);
        glLinkProgram(prog);
        glDeleteShader(vsh);
        glDeleteShader(fsh);

        const float quad[] = {
            -1.f, -1.f,  1.f, -1.f,  1.f, 1.f,
            -1.f, -1.f,  1.f,  1.f, -1.f, 1.f,
        };
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        glBindVertexArray(0);

        const int N = 64;
        std::vector<unsigned char> px((size_t)N * N * 4);
        float c = (N - 1) * 0.5f;
        for (int y = 0; y < N; ++y) {
            for (int x = 0; x < N; ++x) {
                float dx = ((float)x - c) / c;
                float dy = ((float)y - c) / c;
                float r2 = dx * dx + dy * dy;
                float a = std::clamp(1.f - r2, 0.f, 1.f);
                a = a * a;
                size_t i = ((size_t)y * N + x) * 4;
                px[i] = px[i + 1] = px[i + 2] = 255;
                px[i + 3] = (unsigned char)(a * 255.f);
            }
        }
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, N, N, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());

        particles.reserve(kMaxParticles);
        ready = prog && vao && tex;
        if (ready) {
            locMVP = glGetUniformLocation(prog, "uMVP");
            locColor = glGetUniformLocation(prog, "uColor");
            locAlpha = glGetUniformLocation(prog, "uAlpha");
            locTex = glGetUniformLocation(prog, "uTex");
        }
        return ready;
    }

    void destroy() {
        if (prog) glDeleteProgram(prog);
        if (vao) glDeleteVertexArrays(1, &vao);
        if (vbo) glDeleteBuffers(1, &vbo);
        if (tex) glDeleteTextures(1, &tex);
        prog = vao = vbo = tex = 0;
        particles.clear();
        explosions.clear();
        ready = false;
    }

    void clear() {
        particles.clear();
        explosions.clear();
    }

    void spawnExplosion(glm::vec3 origin, float radius = 12.f, int  = 0) {
        if (radius < 4.f) radius = 4.f;
        if (radius > 18.f) radius = 18.f;

        explosions.clear();

        ExplosionBall ball;
        ball.pos = origin;
        ball.age = 0.f;
        ball.duration = 0.28f;
        ball.maxRadius = radius;
        explosions.push_back(ball);
    }

    static float frand(float a, float b) {
        return a + (b - a) * ((float)std::rand() / (float)RAND_MAX);
    }

    void emitOne(const Particle& p) {
        if ((int)particles.size() >= kMaxParticles) {
            particles[(size_t)(std::rand() % kMaxParticles)] = p;
        } else {
            particles.push_back(p);
        }
    }

    void emit(glm::vec3 origin, int count, glm::vec3 color,
              float speedMin, float speedMax, float lifeMin, float lifeMax,
              float size0, float size1, float gravity, float drag,
              glm::vec3 biasDir = {0, 1, 0}, float spread = 1.f) {
        if (count < 1) return;
        if (count > 16) count = 16;
        glm::vec3 bias = glm::length(biasDir) > 1e-4f ? glm::normalize(biasDir) : glm::vec3(0, 1, 0);
        for (int i = 0; i < count; ++i) {
            Particle p;
            p.pos = origin + glm::vec3(frand(-0.15f, 0.15f), frand(-0.1f, 0.2f), frand(-0.15f, 0.15f));
            glm::vec3 rnd = glm::vec3(frand(-1.f, 1.f), frand(-1.f, 1.f), frand(-1.f, 1.f));
            float rl = glm::length(rnd);
            if (rl < 1e-4f) rnd = glm::vec3(0, 1, 0);
            else rnd /= rl;
            glm::vec3 mixv = bias * (1.f - spread * 0.5f) + rnd * spread;
            float ml = glm::length(mixv);
            glm::vec3 dir = ml > 1e-4f ? mixv / ml : bias;
            float spd = frand(speedMin, speedMax);
            p.vel = dir * spd;
            p.color = color * frand(0.85f, 1.1f);
            p.maxLife = frand(lifeMin, lifeMax);
            p.life = p.maxLife;
            p.size0 = size0 * frand(0.8f, 1.2f);
            p.size1 = size1;
            p.gravity = gravity;
            p.drag = drag;
            emitOne(p);
        }
    }

    void emitPreset(ParticlePreset preset, glm::vec3 origin, int count = -1,
                    glm::vec3 colorOverride = {-1, -1, -1},
                    glm::vec3 biasDir = {0, 1, 0}) {
        bool customCol = colorOverride.x >= 0.f;
        switch (preset) {
        case ParticlePreset::Sparks:
            emit(origin, count > 0 ? std::min(count, 10) : 8,
                 customCol ? colorOverride : glm::vec3(1.f, 0.75f, 0.25f),
                 4.f, 11.f, 0.25f, 0.55f, 0.28f, 0.02f, -14.f, 2.2f, biasDir, 1.1f);
            break;
        case ParticlePreset::Smoke:
            emit(origin, count > 0 ? std::min(count, 6) : 5,
                 customCol ? colorOverride : glm::vec3(0.55f, 0.55f, 0.58f),
                 0.4f, 1.6f, 0.8f, 1.6f, 0.55f, 1.4f, 0.8f, 0.6f, {0, 1, 0}, 0.7f);
            break;
        case ParticlePreset::Coins:
            emit(origin, count > 0 ? std::min(count, 8) : 6,
                 customCol ? colorOverride : glm::vec3(1.f, 0.85f, 0.2f),
                 3.f, 7.f, 0.5f, 0.9f, 0.35f, 0.08f, -10.f, 1.2f, {0, 1, 0}, 0.9f);
            break;
        default:
            emit(origin, count > 0 ? std::min(count, 10) : 8,
                 customCol ? colorOverride : glm::vec3(1.f, 1.f, 1.f),
                 2.f, 6.f, 0.4f, 0.9f, 0.4f, 0.05f, -8.f, 1.5f, biasDir, 1.f);
            break;
        }
    }

    // Tiene le particelle con piu' vita rimasta. Le altre hanno gia' fatto
    // il loro effetto: buttarle qui costa meno che disegnarle.
    void trimTo(int keep) {
        if (keep < 0) keep = 0;
        int n = (int)particles.size();
        if (n <= keep) return;
        int drop = n - keep;
        std::nth_element(particles.begin(), particles.begin() + drop, particles.end(),
            [](const Particle& a, const Particle& b) { return a.life < b.life; });
        particles.erase(particles.begin(), particles.begin() + drop);
    }

    void update(float dt) {
        if (dt <= 0.f) return;
        for (auto& p : particles) {
            p.life -= dt;
            p.vel.y += p.gravity * dt;
            p.vel *= std::exp(-p.drag * dt);
            p.pos += p.vel * dt;
        }
        particles.erase(
            std::remove_if(particles.begin(), particles.end(),
                [](const Particle& p) { return p.life <= 0.f; }),
            particles.end());

        for (auto& e : explosions)
            e.age += dt;
        explosions.erase(
            std::remove_if(explosions.begin(), explosions.end(),
                [](const ExplosionBall& e) { return e.age >= e.duration; }),
            explosions.end());
    }

    void draw(const glm::mat4& view, const glm::mat4& proj) {
        if (!ready) return;
        if (particles.empty() && explosions.empty()) return;

        glm::mat4 invView = glm::inverse(view);
        glm::vec3 camRight = glm::normalize(glm::vec3(invView[0]));
        glm::vec3 camUp = glm::normalize(glm::vec3(invView[1]));
        glm::vec3 camFwd = glm::normalize(-glm::vec3(invView[2]));

        glEnable(GL_BLEND);
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);

        glUseProgram(prog);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);
        if (locTex >= 0) glUniform1i(locTex, 0);
        glBindVertexArray(vao);

        auto drawBillboard = [&](glm::vec3 pos, float size, glm::vec3 color, float alpha) {
            if (size < 0.01f || alpha < 0.01f) return;
            glm::mat4 model(1.f);
            model[0] = glm::vec4(camRight * size, 0.f);
            model[1] = glm::vec4(camUp * size, 0.f);
            model[2] = glm::vec4(camFwd * size, 0.f);
            model[3] = glm::vec4(pos, 1.f);
            glm::mat4 mvp = proj * view * model;
            if (locMVP >= 0)
                glUniformMatrix4fv(locMVP, 1, GL_FALSE, glm::value_ptr(mvp));
            if (locColor >= 0)
                glUniform3fv(locColor, 1, glm::value_ptr(color));
            if (locAlpha >= 0)
                glUniform1f(locAlpha, alpha);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        };

        glDisable(GL_DEPTH_TEST);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        for (const auto& e : explosions) {
            float t = std::clamp(e.age / std::max(e.duration, 1e-4f), 0.f, 1.f);
            float grow = 1.f - (1.f - t) * (1.f - t);
            float fade = (1.f - t) * (1.f - t);
            float R = glm::mix(1.2f, e.maxRadius, grow);
            drawBillboard(e.pos, R, glm::vec3(1.f, 0.35f, 0.05f), fade);
            drawBillboard(e.pos, R * 0.45f, glm::vec3(1.f, 0.9f, 0.35f), fade);
        }
        glEnable(GL_DEPTH_TEST);

        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        const int drawMax = 48;
        int drawn = 0;
        for (const auto& p : particles) {
            if (drawn >= drawMax) break;
            float t = 1.f - std::clamp(p.life / std::max(p.maxLife, 1e-4f), 0.f, 1.f);
            float size = glm::mix(p.size0, p.size1, t);
            float alpha = (1.f - t) * (1.f - t) * 0.95f;
            drawBillboard(p.pos, size, p.color, alpha);
            ++drawn;
        }

        glBindVertexArray(0);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glEnable(GL_CULL_FACE);
    }
};
