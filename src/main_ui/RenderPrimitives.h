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

static void collectShadowOccluders() {
    g_occCount = 0;
    g_bodyOccCount = 0;
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
    glUniform3fv(glGetUniformLocation(prog, "uSunColor"), 1, glm::value_ptr(g_sunColor));
    glUniform3fv(glGetUniformLocation(prog, "uAmbientTint"), 1, glm::value_ptr(g_ambientTint));
    glUniform3fv(glGetUniformLocation(prog, "uSkyZenith"), 1, glm::value_ptr(g_skyZenith));
    glUniform3fv(glGetUniformLocation(prog, "uSkyHorizon"), 1, glm::value_ptr(g_skyHorizon));
    glUniform3fv(glGetUniformLocation(prog, "uFogColor"), 1, glm::value_ptr(g_lighting.fogColor));
    glUniform1f(glGetUniformLocation(prog, "uFogStart"), g_lighting.fogStart);
    glUniform1f(glGetUniformLocation(prog, "uFogEnd"), g_lighting.fogEnd);
    glUniform1i(glGetUniformLocation(prog, "uOccCount"), g_occCount);
    glUniform1i(glGetUniformLocation(prog, "uAoOccStart"), g_bodyOccCount);
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
    if (GLuint matTexId = materialTextureId(mat)) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, matTexId);
        glUniform1i(glGetUniformLocation(prog, "uMaterialTex"), 1);
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
