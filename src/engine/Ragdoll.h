#pragma once

/**
 * Ragdoll R6 — alla morte i cubi del player cadono con fisica leggera.
 * Niente engine fisico pieno: gravity + bounce + soft-link al torso.
 */

#include "Humanoid.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

struct RagdollPart {
    std::string name;
    glm::vec3 pos{0};
    glm::vec3 vel{0};
    glm::vec3 angVel{0}; // rad/s
    glm::vec3 euler{0};  // gradi
    glm::vec3 size{1};
    glm::vec3 color{1};
};

struct Ragdoll {
    bool active = false;
    int torsoIdx = -1;
    float groundY = 0.f;
    std::vector<RagdollPart> parts;

    void clear() {
        active = false;
        torsoIdx = -1;
        parts.clear();
    }

    glm::vec3 focusPos() const {
        if (torsoIdx >= 0 && torsoIdx < (int)parts.size())
            return parts[(size_t)torsoIdx].pos;
        if (!parts.empty()) return parts[0].pos;
        return {0, 0, 0};
    }

    static float frand(float a, float b) {
        return a + (b - a) * ((float)(std::rand() % 10001) / 10000.f);
    }

    void start(const Humanoid& h, const glm::vec3& rootPos, float yawDeg,
               const glm::vec3& horizVel, float floorY = 0.f) {
        clear();
        active = true;
        groundY = floorY;
        const float yawR = glm::radians(yawDeg);
        glm::mat4 root = glm::rotate(glm::translate(glm::mat4(1.f), rootPos),
                                     yawR, glm::vec3(0, 1, 0));

        for (const auto& p : h.parts) {
            if (p.name == "HumanoidRootPart") continue;

            RagdollPart rp;
            rp.name = p.name;
            rp.size = p.size;
            rp.color = p.color;

            glm::mat4 m = glm::translate(root, p.offset + p.pivot);
            if (std::fabs(p.rotX) > 0.01f)
                m = glm::rotate(m, glm::radians(p.rotX), glm::vec3(1, 0, 0));
            if (std::fabs(p.rotZ) > 0.01f)
                m = glm::rotate(m, glm::radians(p.rotZ), glm::vec3(0, 0, 1));
            m = glm::translate(m, -p.pivot);
            rp.pos = glm::vec3(m[3]);

            rp.euler = {
                p.rotX + frand(-25.f, 25.f),
                yawDeg + frand(-40.f, 40.f),
                p.rotZ + frand(-50.f, 50.f)
            };

            glm::vec3 kick = horizVel * 0.55f;
            kick.y = 0.f;
            rp.vel = kick + glm::vec3(frand(-7.f, 7.f), frand(6.f, 16.f), frand(-7.f, 7.f));
            rp.angVel = {frand(-9.f, 9.f), frand(-6.f, 6.f), frand(-11.f, 11.f)};

            if (p.name == "Torso") {
                torsoIdx = (int)parts.size();
                rp.vel.y += 3.f;
                rp.angVel.x += frand(3.f, 7.f);
                rp.angVel.z += frand(-5.f, 5.f);
            } else if (p.name == "Head") {
                rp.vel.y += 5.f;
                rp.angVel *= 1.4f;
            } else if (p.name.find("Arm") != std::string::npos) {
                rp.vel += glm::vec3(frand(-4.f, 4.f), frand(2.f, 8.f), frand(-4.f, 4.f));
            } else if (p.name.find("Leg") != std::string::npos) {
                rp.vel.y += frand(1.f, 5.f);
                rp.angVel.x += frand(-4.f, 4.f);
            }

            parts.push_back(std::move(rp));
        }
    }

    void update(float dt) {
        if (!active || parts.empty()) return;
        const float gravity = 58.f;
        const float linDamp = std::pow(0.978f, dt * 60.f);
        const float angDamp = std::pow(0.965f, dt * 60.f);

        glm::vec3 torso = focusPos();

        for (auto& p : parts) {
            p.vel.y -= gravity * dt;
            p.vel *= linDamp;
            p.pos += p.vel * dt;
            p.euler += glm::degrees(p.angVel) * dt;
            p.angVel *= angDamp;

            // Soft-link: arti restano vicini al torso (effetto floppy, non esplosione)
            if (p.name != "Torso") {
                float maxDist = (p.name == "Head") ? 2.4f : 3.8f;
                glm::vec3 d = torso - p.pos;
                float dist = glm::length(d);
                if (dist > maxDist && dist > 1e-4f) {
                    glm::vec3 n = d / dist;
                    float pull = (dist - maxDist) * 0.4f;
                    p.pos += n * pull;
                    p.vel += n * (14.f * dt);
                }
            }

            const float half = p.size.y * 0.45f;
            if (p.pos.y - half < groundY) {
                p.pos.y = groundY + half;
                if (p.vel.y < 0.f)
                    p.vel.y = -p.vel.y * 0.28f;
                p.vel.x *= 0.72f;
                p.vel.z *= 0.72f;
                p.angVel *= 0.55f;
                // Si adagia a terra
                p.euler.x = glm::mix(p.euler.x, (p.euler.x > 0.f ? 90.f : -90.f), 0.08f);
            }
        }

        // Aggiorna riferimento torso dopo il passo
        if (torsoIdx >= 0 && torsoIdx < (int)parts.size())
            torso = parts[(size_t)torsoIdx].pos;
    }

    template <typename DrawCubeFn>
    void draw(DrawCubeFn drawCube, GLuint vao, GLuint prog,
              const glm::mat4& view, const glm::mat4& proj,
              const glm::vec3& sun) const {
        if (!active) return;
        for (const auto& p : parts) {
            glm::mat4 m = glm::translate(glm::mat4(1.f), p.pos);
            m = glm::rotate(m, glm::radians(p.euler.y), glm::vec3(0, 1, 0));
            m = glm::rotate(m, glm::radians(p.euler.x), glm::vec3(1, 0, 0));
            m = glm::rotate(m, glm::radians(p.euler.z), glm::vec3(0, 0, 1));
            m = glm::scale(m, p.size);
            drawCube(vao, prog, m, view, proj, p.color, sun);
        }
    }
};
