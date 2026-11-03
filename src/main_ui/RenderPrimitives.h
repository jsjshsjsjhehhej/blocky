#pragma once

/**
 * NON e' un header riusabile come quelli in src/render/ o src/engine/ — e'
 * il pezzo di main.cpp con le primitive di disegno base e le luci dinamiche,
 * spostato qui solo per accorciare main.cpp: raccolta luci dinamiche e
 * shadow occluders (collectDynamicLights/collectShadowOccluders/
 * clearDynamicLights/uploadDynamicLights), disegno cubo con materiali/luci
 * (drawCube), varianti (drawCubePlayer, drawExplosionCubes), e il
 * contorno blu di selezione (drawSelectedPartBlueOutline).
 * Legge/scrive le static globali di main.cpp (g_dynLights, g_occCenter,
 * g_world, g_engine, ecc., dichiarate prima di questo punto). Compila SOLO
 * se incluso da main.cpp, esattamente nel punto in cui questo codice viveva
 * prima — mai da studio_main.cpp, BlockyApi.cpp o altre TU.
 */

// Texture reali per materiale (PNG/JPG in assets/textures/materials/).
// Caricate pigramente al primo uso; cache per stem file.
static GLuint materialTextureId(PartMaterial mat) {
    const char* stem = partMaterialTextureStem(mat);
    if (!stem) return 0;
    static std::unordered_map<std::string, Texture2D> cache;
    static std::unordered_map<std::string, bool> tried;
    std::string key(stem);
    if (!tried[key]) {
        tried[key] = true;
        Texture2D tex;
        tex.load(findMaterialTexture(key), true);
        cache[key] = std::move(tex);
    }
    auto it = cache.find(key);
    return (it != cache.end()) ? it->second.id : 0;
}

// Cresce ogni volta che cambia qualcosa di "scena" (luci raccolte, occluder,
// shadow map, ora del giorno): le uniform di scena dei programmi Part si
// ricaricano solo allora (vedi uploadDynamicLights), non a ogni cubo.
static uint32_t g_sceneUniformVersion = 1;
static inline void bumpSceneUniforms() { ++g_sceneUniformVersion; }

static float neonLightRange(const WorkspacePart& p) {
    float maxDim = std::max(p.size.x, std::max(p.size.y, p.size.z));
    return std::clamp(8.f + maxDim * 2.2f, 8.f, 42.f);
}

static void collectDynamicLights() {
    bumpSceneUniforms();
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
    bumpSceneUniforms();
    g_occCount = 0;
    g_bodyOccCount = 0;
    const int budget = std::min(kMaxShadowBoxes, g_engine.quality.shadowOccluders);
    if (budget <= 0) return;
    if (g_dynLightCount == 0 && !pipelineSunShadowsEnabled(g_engine.quality, g_lighting)) return;

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
    bumpSceneUniforms();
    g_dynLightCount = 0;
    g_occCount = 0;
}

// ---- Cache delle uniform location --------------------------------------
// glGetUniformLocation e' una ricerca per stringa dentro il driver: prima
// drawCube ne faceva ~55 per OGNI cubo (piu' 24 snprintf), cioe' decine di
// migliaia a frame con qualche centinaio di part. Qui le location si
// risolvono una volta per programma e le uniform "di scena" (luci, nebbia,
// cielo, occluder, shadow map) si caricano una volta per frame per programma
// invece che a ogni draw. Vale per tutti i giochi: e' il costo CPU base di
// ogni Part disegnata.
struct CubeProgLocs {
    GLint model = -1, view = -1, proj = -1, color = -1, sun = -1, camPos = -1;
    GLint emissive = -1, specular = -1, roughness = -1, alpha = -1, material = -1;
    GLint stud = -1, studScale = -1, materialTex = -1, hasMaterialTex = -1;
    GLint lightCount = -1;
    GLint lightPos[kMaxDynLights], lightCol[kMaxDynLights], lightRange[kMaxDynLights];
    GLint night = -1, sunColor = -1, ambientTint = -1, skyZenith = -1, skyHorizon = -1;
    GLint fogColor = -1, fogStart = -1, fogEnd = -1;
    GLint occCount = -1, aoOccStart = -1, sunShadow = -1, occCenter = -1, occHalf = -1, occRot = -1;
    GLint shadowMap = -1, lightVP = -1, shadowTexel = -1;
    uint32_t sceneVersion = 0;   // ultima versione delle uniform di scena caricata
};

static CubeProgLocs& cubeProgLocs(GLuint prog) {
    static std::unordered_map<GLuint, CubeProgLocs> cache;
    auto it = cache.find(prog);
    if (it != cache.end()) return it->second;
    CubeProgLocs L;
    auto u = [&](const char* n) { return glGetUniformLocation(prog, n); };
    L.model = u("uModel"); L.view = u("uView"); L.proj = u("uProj");
    L.color = u("uColor"); L.sun = u("uSun"); L.camPos = u("uCamPos");
    L.emissive = u("uEmissive"); L.specular = u("uSpecular"); L.roughness = u("uRoughness");
    L.alpha = u("uAlpha"); L.material = u("uMaterial");
    L.stud = u("uStud"); L.studScale = u("uStudScale");
    L.materialTex = u("uMaterialTex"); L.hasMaterialTex = u("uHasMaterialTex");
    L.lightCount = u("uLightCount");
    for (int i = 0; i < kMaxDynLights; ++i) {
        char name[32];
        std::snprintf(name, sizeof(name), "uLightPos[%d]", i);   L.lightPos[i] = u(name);
        std::snprintf(name, sizeof(name), "uLightCol[%d]", i);   L.lightCol[i] = u(name);
        std::snprintf(name, sizeof(name), "uLightRange[%d]", i); L.lightRange[i] = u(name);
    }
    L.night = u("uNight"); L.sunColor = u("uSunColor"); L.ambientTint = u("uAmbientTint");
    L.skyZenith = u("uSkyZenith"); L.skyHorizon = u("uSkyHorizon");
    L.fogColor = u("uFogColor"); L.fogStart = u("uFogStart"); L.fogEnd = u("uFogEnd");
    L.occCount = u("uOccCount"); L.aoOccStart = u("uAoOccStart"); L.sunShadow = u("uSunShadow");
    L.occCenter = u("uOccCenter"); L.occHalf = u("uOccHalf"); L.occRot = u("uOccRot");
    L.shadowMap = u("uShadowMap"); L.lightVP = u("uLightVP"); L.shadowTexel = u("uShadowTexel");
    return cache.emplace(prog, L).first->second;
}

static void uploadDynamicLights(GLuint prog) {
    CubeProgLocs& L = cubeProgLocs(prog);
    // La texture della shadow map va ribindata comunque: altri draw possono
    // aver cambiato l'unita' 3 (costa 2 chiamate, nessuna ricerca stringa).
    if (g_sunShadowOn && g_shadowMap.ready) {
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, g_shadowMap.depthTex);
    }
    if (L.sceneVersion == g_sceneUniformVersion) return;
    L.sceneVersion = g_sceneUniformVersion;

    glUniform1i(L.lightCount, g_dynLightCount);
    for (int i = 0; i < kMaxDynLights; ++i) {
        glm::vec3 pos = (i < g_dynLightCount) ? g_dynLights[i].pos : glm::vec3(0);
        glm::vec3 col = (i < g_dynLightCount) ? g_dynLights[i].color : glm::vec3(0);
        float rng = (i < g_dynLightCount) ? g_dynLights[i].range : 1.f;
        glUniform3fv(L.lightPos[i], 1, glm::value_ptr(pos));
        glUniform3fv(L.lightCol[i], 1, glm::value_ptr(col));
        glUniform1f(L.lightRange[i], rng);
    }
    glUniform1f(L.night, g_night);
    glUniform3fv(L.sunColor, 1, glm::value_ptr(g_sunColor));
    glUniform3fv(L.ambientTint, 1, glm::value_ptr(g_ambientTint));
    glUniform3fv(L.skyZenith, 1, glm::value_ptr(g_skyZenith));
    glUniform3fv(L.skyHorizon, 1, glm::value_ptr(g_skyHorizon));
    {
        const glm::vec3 fogC = g_lighting.effectiveFogColor();
        glUniform3fv(L.fogColor, 1, glm::value_ptr(fogC));
        glUniform1f(L.fogStart, g_lighting.fogStart);
        glUniform1f(L.fogEnd, g_lighting.effectiveFogEnd());
    }
    glUniform1i(L.occCount, g_occCount);
    glUniform1i(L.aoOccStart, g_bodyOccCount);
    glUniform1i(L.sunShadow, g_sunShadowOn ? 1 : 0);
    if (g_sunShadowOn && g_shadowMap.ready) {
        glUniform1i(L.shadowMap, 3);
        glUniformMatrix4fv(L.lightVP, 1, GL_FALSE, glm::value_ptr(g_shadowMap.lightVP));
        glUniform1f(L.shadowTexel, 1.f / (float)std::max(1, g_shadowMap.size));
    }
    if (g_occCount > 0) {
        glUniform3fv(L.occCenter, g_occCount, glm::value_ptr(g_occCenter[0]));
        glUniform3fv(L.occHalf, g_occCount, glm::value_ptr(g_occHalf[0]));
        glUniformMatrix3fv(L.occRot, g_occCount, GL_FALSE, glm::value_ptr(g_occRot[0]));
    }
}

static void drawCube(GLuint vao, GLuint prog, const glm::mat4& m, const glm::mat4& v,
                     const glm::mat4& p, const glm::vec3& col, const glm::vec3& sun,
                     bool studTex = false, GLuint studId = 0, int vertCount = 36,
                     float emissive = 0.f, PartMaterial mat = PartMaterial::Plastic,
                     int firstVert = 0) {
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
    const CubeProgLocs& L = cubeProgLocs(prog);
    glUniformMatrix4fv(L.model, 1, GL_FALSE, glm::value_ptr(m));
    glUniformMatrix4fv(L.view, 1, GL_FALSE, glm::value_ptr(v));
    glUniformMatrix4fv(L.proj, 1, GL_FALSE, glm::value_ptr(p));
    glUniform3fv(L.color, 1, glm::value_ptr(col));
    glUniform3fv(L.sun, 1, glm::value_ptr(sun));
    glUniform3fv(L.camPos, 1, glm::value_ptr(g_renderCamPos));
    glUniform1f(L.emissive, em);
    glUniform1f(L.specular, specular);
    glUniform1f(L.roughness, roughness);
    glUniform1f(L.alpha, alpha);
    glUniform1i(L.material, (int)mat);
    uploadDynamicLights(prog);
    if (studTex && studId) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, studId);
        glUniform1i(L.stud, 0);
        glUniform1f(L.studScale, kStudTile);
    }
    if (GLuint matTexId = materialTextureId(mat)) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, matTexId);
        glUniform1i(L.materialTex, 1);
        glUniform1i(L.hasMaterialTex, 1);
    } else {
        glUniform1i(L.hasMaterialTex, 0);
    }
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, firstVert, vertCount);

    if (transparent) {
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
}

// Pass terrain: view/proj/luci una sola volta, poi solo colore/materiale
// per range. Evita di ripetere ~20 uniform per ogni draw call voxel —
// su mobile e' la differenza tra ~30 e ~45+ fps con molti chunk.
struct TerrainDrawPass {
    GLuint prog = 0;
    GLuint boundVao = 0;
    const CubeProgLocs* L = nullptr;
    bool lite = false; // niente texture materiale (preset Low / mobile)
};

static TerrainDrawPass beginTerrainPass(GLuint prog, const glm::mat4& view,
                                        const glm::mat4& proj, const glm::vec3& sun,
                                        bool liteShading) {
    TerrainDrawPass tp;
    tp.prog = prog;
    tp.lite = liteShading;
    glUseProgram(prog);
    tp.L = &cubeProgLocs(prog);
    const CubeProgLocs& L = *tp.L;
    static const glm::mat4 ident(1.f);
    glUniformMatrix4fv(L.model, 1, GL_FALSE, glm::value_ptr(ident));
    glUniformMatrix4fv(L.view, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(L.proj, 1, GL_FALSE, glm::value_ptr(proj));
    glUniform3fv(L.sun, 1, glm::value_ptr(sun));
    glUniform3fv(L.camPos, 1, glm::value_ptr(g_renderCamPos));
    uploadDynamicLights(prog);
    return tp;
}

static void drawTerrainRange(TerrainDrawPass& tp, GLuint vao, const glm::vec3& col,
                             PartMaterial mat, float emissive, int firstVert, int vertCount) {
    if (!tp.L || vertCount <= 0) return;
    if ((int)mat < 0 || mat >= PartMaterial::Count) mat = PartMaterial::Plastic;
    if (tp.lite) mat = PartMaterial::Plastic; // flat color: niente sample texture
    float specular = 0.12f, roughness = 0.75f, emBias = 0.f, alpha = 1.f;
    partMaterialShading(mat, specular, roughness, emBias, alpha);
    if (tp.lite) { specular = 0.05f; roughness = 0.9f; }
    float em = std::max(emissive, emBias);
    const bool transparent = alpha < 0.99f;
    if (transparent) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
    }
    if (tp.boundVao != vao) {
        glBindVertexArray(vao);
        tp.boundVao = vao;
    }
    const CubeProgLocs& L = *tp.L;
    glUniform3fv(L.color, 1, glm::value_ptr(col));
    glUniform1f(L.emissive, em);
    glUniform1f(L.specular, specular);
    glUniform1f(L.roughness, roughness);
    glUniform1f(L.alpha, alpha);
    glUniform1i(L.material, (int)mat);
    if (!tp.lite) {
        if (GLuint matTexId = materialTextureId(mat)) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, matTexId);
            glUniform1i(L.materialTex, 1);
            glUniform1i(L.hasMaterialTex, 1);
        } else {
            glUniform1i(L.hasMaterialTex, 0);
        }
    } else {
        glUniform1i(L.hasMaterialTex, 0);
    }
    glDrawArrays(GL_TRIANGLES, firstVert, vertCount);
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
