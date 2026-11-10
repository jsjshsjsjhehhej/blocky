#pragma once

/**
 * NON e' un header riusabile come quelli in src/render/ o src/engine/ — e'
 * il pezzo di main.cpp con le primitive di disegno base e le luci dinamiche,
 * spostato qui solo per accorciare main.cpp: raccolta luci dinamiche e
 * shadow occluders (collectDynamicLights/collectShadowOccluders/
 * clearDynamicLights/uploadDynamicLights), disegno cubo con materiali/luci
 * (drawCube), varianti (drawCubePlayer, drawExplosionCubes — stub), e il
 * contorno blu di selezione (drawSelectedPartBlueOutline).
 * Legge/scrive le static globali di main.cpp (g_dynLights, g_occCenter,
 * g_world, g_engine, ecc., dichiarate prima di questo punto). Compila SOLO
 * se incluso da main.cpp, esattamente nel punto in cui questo codice viveva
 * prima — mai da studio_main.cpp, BlockyApi.cpp o altre TU.
 */

// Texture reali per materiale (oggi solo Wood) invece del pattern
// procedurale a bande dello shader. Caricata pigramente al primo uso.
static GLuint materialTextureId(PartMaterial mat) {
    static Texture2D wood;
    static bool woodTried = false;
    if (mat == PartMaterial::Wood) {
        if (!woodTried) {
            woodTried = true;
            wood.load(findMaterialTexture("wood"), true);
        }
        return wood.id;
    }
    return 0;
}

static void bumpCubeUniformSerial();

static float neonLightRange(const WorkspacePart& p) {
    float maxDim = std::max(p.size.x, std::max(p.size.y, p.size.z));
    return std::clamp(8.f + maxDim * 2.2f, 8.f, 42.f);
}

static void collectDynamicLights() {
    bumpCubeUniformSerial();
    g_dynLightCount = 0;
    struct Cand { glm::vec3 pos, col; float range, d2; };
    static std::vector<Cand> cands;
    cands.clear();
    float maxD = g_engine.quality.maxDrawDistance;
    int lightCap = kMaxDynLights;
    if (antilag::engine().enabled()) {
        maxD = std::min(maxD, antilag::engine().plan().maxDrawDistance);
        lightCap = std::min(lightCap, std::max(1, antilag::engine().plan().maxLights));
    }
    for (const auto& p : g_world.parts) {
        const bool neon = p.isNeon();
        if (!p.pointLight && !neon) continue;
        Cand c;
        c.pos = p.position;
        if (p.pointLight) {
            c.col = p.color * std::max(0.05f, p.lightBrightness);
            c.range = std::max(1.f, p.lightRange);
        } else {
            c.col = p.color * std::clamp(0.55f + p.effectiveEmissive() * 0.55f, 0.35f, 1.6f);
            c.range = neonLightRange(p);
            // Leggero flicker stile neon/torcia: fase stabile per part (da
            // posizione, cosi' non "salta" a ogni frame) cosi' piu' neon
            // vicini non pulsano in sincrono.
            float phase = std::fmod(std::fabs(p.position.x * 12.9898f
                + p.position.y * 78.233f + p.position.z * 37.719f), 6.2831853f);
            float t = (float)glfwGetTime();
            float flicker = 1.f + 0.05f * std::sin(t * 9.f + phase)
                               + 0.025f * std::sin(t * 23.f + phase * 1.7f);
            c.col *= std::clamp(flicker, 0.85f, 1.15f);
        }
        glm::vec3 d = c.pos - g_renderCamPos;
        c.d2 = glm::dot(d, d);
        float reach = c.range + maxD;
        if (c.d2 > reach * reach) continue;
        cands.push_back(c);
    }
    if ((int)cands.size() > lightCap) {
        std::nth_element(cands.begin(), cands.begin() + lightCap, cands.end(),
            [](const Cand& a, const Cand& b) { return a.d2 < b.d2; });
        cands.resize(lightCap);
    }
    for (const auto& c : cands) {
        DynLight& L = g_dynLights[g_dynLightCount++];
        L.pos = c.pos;
        L.color = c.col;
        L.range = c.range;
    }
}

static void collectShadowOccluders() {
    g_occCount = 0;
    g_bodyOccCount = 0;
    int budget = std::min(kMaxShadowBoxes, g_engine.quality.shadowOccluders);
    if (antilag::engine().enabled())
        budget = std::min(budget, antilag::engine().plan().shadowOccluders);
    if (budget <= 0) return;
    if (g_dynLightCount == 0 && !g_lighting.globalShadows) return;

    auto add = [&](const glm::vec3& center, const glm::vec3& half, const glm::mat3& invRot) {
        if (g_occCount >= budget) return;
        g_occCenter[g_occCount] = center;
        g_occHalf[g_occCount] = glm::max(half, glm::vec3(0.05f));
        g_occRot[g_occCount] = invRot;
        ++g_occCount;
    };
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
    // I box-corpo servono solo per le ombre finte verso le luci puntuali:
    // sono una scatola unica e grezza che copre torso/braccia/gambe, quindi
    // usarli anche per l'AO di contatto autoscurirebbe il player con la
    // propria sagoma (braccia/busto che sembravano neri/scuri). L'AO
    // rimane calcolata solo dagli occluder da qui in poi (le part vere).
    g_bodyOccCount = g_occCount;

    struct OccCand { int i; float d2; };
    static std::vector<OccCand> cands;
    cands.clear();
    for (int i = 0; i < (int)g_world.parts.size(); ++i) {
        const auto& p = g_world.parts[i];
        if (p.isNeon() || p.pointLight) continue;
        if (p.isTransparent()) continue;
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

static void clearDynamicLights() {
    g_dynLightCount = 0;
    g_occCount = 0;
    bumpCubeUniformSerial();
}

// Posizioni degli uniform del programma cubo. Su desktop e su GLES si cercano
// una volta per programma: glGetUniformLocation per nome, a ogni blocco, e'
// il costo che fa lagare Android (e sull'emulatore genera anche GL 0x502).
struct CubeUniformCache {
    GLuint prog = 0;
    unsigned serial = 0;
    GLint uModel = -1, uView = -1, uProj = -1;
    GLint uColor = -1, uSun = -1, uCamPos = -1;
    GLint uEmissive = -1, uSpecular = -1, uRoughness = -1, uAlpha = -1, uMaterial = -1;
    GLint uStud = -1, uStudScale = -1, uMaterialTex = -1;
    GLint uLightCount = -1, uLightPos = -1, uLightCol = -1, uLightRange = -1;
    GLint uNight = -1, uSunColor = -1, uAmbientTint = -1;
    GLint uSkyZenith = -1, uSkyHorizon = -1;
    GLint uFogColor = -1, uFogStart = -1, uFogEnd = -1;
    GLint uOccCount = -1, uAoOccStart = -1, uSunShadow = -1;
    GLint uOccCenter = -1, uOccHalf = -1, uOccRot = -1;
    GLint uShadowMap = -1, uLightVP = -1, uShadowTexel = -1;
};

static unsigned g_cubeUniformSerial = 1;

static void bumpCubeUniformSerial() {
    if (++g_cubeUniformSerial == 0)
        g_cubeUniformSerial = 1;
}

static void uniform1i(GLint loc, int v) {
    if (loc >= 0) glUniform1i(loc, v);
}
static void uniform1f(GLint loc, float v) {
    if (loc >= 0) glUniform1f(loc, v);
}
static void uniform3(GLint loc, const glm::vec3& v) {
    if (loc >= 0) glUniform3fv(loc, 1, glm::value_ptr(v));
}
static void uniformMat4(GLint loc, const glm::mat4& m) {
    if (loc >= 0) glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(m));
}

static CubeUniformCache& cubeUniforms(GLuint prog) {
    constexpr int kSlots = 8;
    static CubeUniformCache slots[kSlots];
    static int clock = 0;
    for (int i = 0; i < kSlots; ++i)
        if (slots[i].prog == prog) return slots[i];
    CubeUniformCache& slot = slots[clock];
    clock = (clock + 1) % kSlots;
    slot = CubeUniformCache{};
    slot.prog = prog;
    auto loc = [prog](const char* name) { return glGetUniformLocation(prog, name); };
    slot.uModel = loc("uModel");
    slot.uView = loc("uView");
    slot.uProj = loc("uProj");
    slot.uColor = loc("uColor");
    slot.uSun = loc("uSun");
    slot.uCamPos = loc("uCamPos");
    slot.uEmissive = loc("uEmissive");
    slot.uSpecular = loc("uSpecular");
    slot.uRoughness = loc("uRoughness");
    slot.uAlpha = loc("uAlpha");
    slot.uMaterial = loc("uMaterial");
    slot.uStud = loc("uStud");
    slot.uStudScale = loc("uStudScale");
    slot.uMaterialTex = loc("uMaterialTex");
    // Gli array nello shader sono [8], uguale a kMaxDynLights. La location
    // del nome senza indice e' il primo elemento: un solo glUniform*fv
    // carica tutto l'array, su GL e su GLES.
    slot.uLightCount = loc("uLightCount");
    slot.uLightPos = loc("uLightPos");
    slot.uLightCol = loc("uLightCol");
    slot.uLightRange = loc("uLightRange");
    slot.uNight = loc("uNight");
    slot.uSunColor = loc("uSunColor");
    slot.uAmbientTint = loc("uAmbientTint");
    slot.uSkyZenith = loc("uSkyZenith");
    slot.uSkyHorizon = loc("uSkyHorizon");
    slot.uFogColor = loc("uFogColor");
    slot.uFogStart = loc("uFogStart");
    slot.uFogEnd = loc("uFogEnd");
    slot.uOccCount = loc("uOccCount");
    slot.uAoOccStart = loc("uAoOccStart");
    slot.uSunShadow = loc("uSunShadow");
    slot.uOccCenter = loc("uOccCenter");
    slot.uOccHalf = loc("uOccHalf");
    slot.uOccRot = loc("uOccRot");
    slot.uShadowMap = loc("uShadowMap");
    slot.uLightVP = loc("uLightVP");
    slot.uShadowTexel = loc("uShadowTexel");
    return slot;
}

static void uploadDynamicLights(GLuint prog) {
    if (!prog) return;
    CubeUniformCache& u = cubeUniforms(prog);
    if (u.serial != g_cubeUniformSerial) {
        u.serial = g_cubeUniformSerial;
        uniform1i(u.uLightCount, g_dynLightCount);
        if (u.uLightPos >= 0) {
            glm::vec3 pos[kMaxDynLights];
            glm::vec3 col[kMaxDynLights];
            float range[kMaxDynLights];
            for (int i = 0; i < kMaxDynLights; ++i) {
                if (i < g_dynLightCount) {
                    pos[i] = g_dynLights[i].pos;
                    col[i] = g_dynLights[i].color;
                    range[i] = g_dynLights[i].range;
                } else {
                    pos[i] = glm::vec3(0.f);
                    col[i] = glm::vec3(0.f);
                    range[i] = 1.f;
                }
            }
            glUniform3fv(u.uLightPos, kMaxDynLights, glm::value_ptr(pos[0]));
            if (u.uLightCol >= 0)
                glUniform3fv(u.uLightCol, kMaxDynLights, glm::value_ptr(col[0]));
            if (u.uLightRange >= 0)
                glUniform1fv(u.uLightRange, kMaxDynLights, range);
        }
        uniform1f(u.uNight, g_night);
        uniform3(u.uSunColor, g_sunColor);
        uniform3(u.uAmbientTint, g_ambientTint);
        uniform3(u.uSkyZenith, g_skyZenith);
        uniform3(u.uSkyHorizon, g_skyHorizon);
        uniform3(u.uFogColor, g_lighting.fogColor);
        uniform1f(u.uFogStart, g_lighting.fogStart);
        uniform1f(u.uFogEnd, g_lighting.fogEnd);
        uniform1i(u.uOccCount, g_occCount);
        uniform1i(u.uAoOccStart, g_bodyOccCount);
        uniform1i(u.uSunShadow, g_sunShadowOn ? 1 : 0);
        if (g_occCount > 0) {
            if (u.uOccCenter >= 0)
                glUniform3fv(u.uOccCenter, g_occCount, glm::value_ptr(g_occCenter[0]));
            if (u.uOccHalf >= 0)
                glUniform3fv(u.uOccHalf, g_occCount, glm::value_ptr(g_occHalf[0]));
            if (u.uOccRot >= 0)
                glUniformMatrix3fv(u.uOccRot, g_occCount, GL_FALSE, glm::value_ptr(g_occRot[0]));
        }
        if (g_sunShadowOn && u.uShadowMap >= 0) {
            uniform1i(u.uShadowMap, 3);
            uniformMat4(u.uLightVP, g_shadowMap.lightVP);
            uniform1f(u.uShadowTexel, g_shadowMap.size > 0 ? 1.f / (float)g_shadowMap.size : 1.f);
        }
    }
    if (g_sunShadowOn && g_shadowMap.depthTex) {
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, g_shadowMap.depthTex);
        glActiveTexture(GL_TEXTURE0);
    }
}

static void drawCube(GLuint vao, GLuint prog, const glm::mat4& m, const glm::mat4& v,
                     const glm::mat4& p, const glm::vec3& col, const glm::vec3& sun,
                     bool studTex = false, GLuint studId = 0, int vertCount = 36,
                     float emissive = 0.f, PartMaterial mat = PartMaterial::Plastic) {
    if (!prog || !vao) return;
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
    CubeUniformCache& u = cubeUniforms(prog);
    uniformMat4(u.uModel, m);
    uniformMat4(u.uView, v);
    uniformMat4(u.uProj, p);
    uniform3(u.uColor, col);
    uniform3(u.uSun, sun);
    uniform3(u.uCamPos, g_renderCamPos);
    uniform1f(u.uEmissive, em);
    uniform1f(u.uSpecular, specular);
    uniform1f(u.uRoughness, roughness);
    uniform1f(u.uAlpha, alpha);
    uniform1i(u.uMaterial, (int)mat);
    uploadDynamicLights(prog);
    if (studTex && studId) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, studId);
        uniform1i(u.uStud, 0);
        uniform1f(u.uStudScale, kStudTile);
    }
    if (GLuint matTexId = materialTextureId(mat)) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, matTexId);
        uniform1i(u.uMaterialTex, 1);
    }
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, vertCount);

    if (transparent) {
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
}

static void drawExplosionCubes(GLuint, GLuint, const glm::mat4&,
                               const glm::mat4&, const glm::vec3&) {
}

static void drawCubePlayer(GLuint vao, GLuint prog, const glm::mat4& m, const glm::mat4& v,
                           const glm::mat4& p, const glm::vec3& col, const glm::vec3& sun) {
    int vc = (g_bevelCube.vao && vao == g_bevelCube.vao) ? g_bevelCube.vertexCount : g_sharpCubeVerts;
    drawCube(vao, prog, m, v, p, col, sun, false, 0, vc);
}

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
