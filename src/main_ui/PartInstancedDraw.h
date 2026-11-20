#pragma once

/**
 * Part opache disegnate "a gruppi" (GPU instancing).
 *
 * Prima ogni part era un draw a se': cambio programma, ~15 uniform (matrici,
 * colore, materiale...), bind della forma, glDrawArrays. Sul telefono di prova
 * con 419 part erano ~8 ms solo di "Mondo 3D", quasi tutti di chiamate al driver.
 *
 * Qui le part opache con la stessa forma, lo stesso materiale e lo stesso
 * programma (stud / liscio) vanno in un solo glDrawArraysInstanced: matrice e
 * colore di ogni part arrivano dal buffer delle istanze, non da uniform.
 * Gli shader sono gli stessi di sempre (kPartVS / kPartFS / kStudFS): qui sotto
 * vengono solo riscritti per leggere uModel / uColor / uEmissive dall'istanza.
 * Se la riscrittura non riesce (shader esterno in assets/shaders diverso) o il
 * programma non si linka, si torna al disegno una part alla volta.
 */

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

struct PartInstance {
    glm::mat4 model;
    glm::vec4 colorEm;   // rgb = colore, a = emissive
};

static GLuint g_partInstP = 0, g_studInstP = 0;
static bool g_partInstancingOk = false;
static GLuint g_partInstBuf = 0;
static int g_partInstBatchesLastFrame = 0;   // per la diagnostica

static bool partInstReplace(std::string& s, const std::string& from, const std::string& to) {
    const size_t at = s.find(from);
    if (at == std::string::npos) return false;
    s.replace(at, from.size(), to);
    return true;
}

/** kPartVS -> stessa cosa, ma uModel e colore vengono dal buffer delle istanze. Vuota se non si puo'. */
static std::string instancedPartVS(std::string s) {
    if (!partInstReplace(s, "uniform mat4 uModel,uView,uProj;",
            "layout(location=4) in mat4 iModel;\n"
            "layout(location=8) in vec4 iColorEm;\n"
            "uniform mat4 uView,uProj;\n"
            "out vec4 vInstColEm;\n"
            "#define uModel iModel\n"))
        return {};
    if (!partInstReplace(s, "void main(){", "void main(){\n  vInstColEm=iColorEm;"))
        return {};
    return s;
}

/** kPartFS / kStudFS -> uColor e uEmissive letti dall'istanza. Vuota se non si puo'. */
static std::string instancedPartFS(std::string s) {
    if (!partInstReplace(s, "uniform vec3 uColor,uSun,uCamPos;",
            "uniform vec3 uSun,uCamPos;\nin vec4 vInstColEm;\n#define uColor (vInstColEm.rgb)\n"))
        return {};
    if (!partInstReplace(s, "uniform float uEmissive,uSpecular,uRoughness,uAlpha;",
            "uniform float uSpecular,uRoughness,uAlpha;\n#define uEmissive (vInstColEm.a)\n"))
        return {};
    return s;
}

static bool partInstProgramLinked(GLuint p) {
    if (!p) return false;
    GLint ok = GL_FALSE;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    return ok == GL_TRUE;
}

struct PartInstItem {
    GLuint prog;
    GLuint vao;
    int verts;
    PartMaterial mat;
    int order;           // posizione nella lista gia' ordinata
    float alpha = 1.f;
    bool transparent = false;
    PartInstance inst;
};

static std::vector<PartInstItem> g_partInstItems;

static void partInstBegin() { g_partInstItems.clear(); }

/** True se questa part (con questo materiale finale) puo' andare nei gruppi. */
static bool partInstCanBatch(PartMaterial mat) {
    return g_partInstancingOk && (int)mat >= 0 && mat < PartMaterial::Count;
}

static void partInstAdd(bool stud, GLuint vao, int verts, PartMaterial mat,
                        const glm::mat4& model, const glm::vec3& col, float emissive) {
    float spec = 0.f, rough = 0.f, emBias = 0.f, alpha = 1.f;
    partMaterialShading(mat, spec, rough, emBias, alpha);
    PartInstItem it;
    it.prog = stud ? g_studInstP : g_partInstP;
    it.vao = vao;
    it.verts = verts;
    it.mat = mat;
    it.order = (int)g_partInstItems.size();
    it.alpha = alpha;
    it.transparent = alpha < 0.99f;
    it.inst.model = model;
    it.inst.colorEm = glm::vec4(col, std::max(emissive, emBias));
    g_partInstItems.push_back(it);
}

/** Disegna tutti i gruppi raccolti. Ritorna quante chiamate di disegno sono servite. */
static int partInstFlush(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& sun, GLuint studId) {
    g_partInstBatchesLastFrame = 0;
    if (g_partInstItems.empty()) return 0;
    std::sort(g_partInstItems.begin(), g_partInstItems.end(), [](const PartInstItem& a, const PartInstItem& b) {
        if (a.transparent != b.transparent) return !a.transparent && b.transparent;
        if (a.prog != b.prog) return a.prog < b.prog;
        if (a.vao != b.vao) return a.vao < b.vao;
        if (a.verts != b.verts) return a.verts < b.verts;
        if (a.mat != b.mat) return (int)a.mat < (int)b.mat;
        if (a.alpha != b.alpha) return a.alpha < b.alpha;
        return a.order < b.order;
    });
    static std::vector<PartInstance> data;
    data.resize(g_partInstItems.size());
    for (size_t k = 0; k < g_partInstItems.size(); ++k) data[k] = g_partInstItems[k].inst;

    if (!g_partInstBuf) glGenBuffers(1, &g_partInstBuf);
    glBindBuffer(GL_ARRAY_BUFFER, g_partInstBuf);
    // Un solo caricamento per frame (buffer nuovo ogni volta: il driver non aspetta la GPU).
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(data.size() * sizeof(PartInstance)), data.data(), GL_STREAM_DRAW);

    GLuint curProg = 0;
    int draws = 0;
    size_t start = 0;
    while (start < g_partInstItems.size()) {
        const PartInstItem& f = g_partInstItems[start];
        size_t end = start + 1;
        while (end < g_partInstItems.size()) {
            const PartInstItem& e = g_partInstItems[end];
            if (e.prog != f.prog || e.vao != f.vao || e.verts != f.verts || e.mat != f.mat
                || e.transparent != f.transparent || e.alpha != f.alpha) break;
            ++end;
        }
        const CubeProgLocs& L = cubeProgLocs(f.prog);
        if (f.prog != curProg) {
            curProg = f.prog;
            glUseProgram(curProg);
            glUniformMatrix4fv(L.view, 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(L.proj, 1, GL_FALSE, glm::value_ptr(proj));
            glUniform3fv(L.sun, 1, glm::value_ptr(sun));
            glUniform3fv(L.camPos, 1, glm::value_ptr(g_renderCamPos));
            uploadDynamicLights(curProg);
            if (curProg == g_studInstP && studId) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, studId);
                glUniform1i(L.stud, 0);
                glUniform1f(L.studScale, kStudTile);
            }
        }
        float spec = 0.12f, rough = 0.75f, emBias = 0.f, alpha = 1.f;
        partMaterialShading(f.mat, spec, rough, emBias, alpha);
        (void)alpha;
        glUniform1f(L.specular, spec);
        glUniform1f(L.roughness, rough);
        glUniform1f(L.alpha, f.alpha);
        glUniform1i(L.material, (int)f.mat);
        if (GLuint matTexId = materialTextureId(f.mat)) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, matTexId);
            glUniform1i(L.materialTex, 1);
            glUniform1i(L.hasMaterialTex, 1);
        } else {
            glUniform1i(L.hasMaterialTex, 0);
        }

        if (f.transparent) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);
        }

        glBindVertexArray(f.vao);
        glBindBuffer(GL_ARRAY_BUFFER, g_partInstBuf);
        const GLsizei stride = (GLsizei)sizeof(PartInstance);
        const size_t base = start * sizeof(PartInstance);
        for (int c = 0; c < 4; ++c) {
            glEnableVertexAttribArray(4 + c);
            glVertexAttribPointer(4 + c, 4, GL_FLOAT, GL_FALSE, stride, (void*)(base + sizeof(glm::vec4) * c));
            glVertexAttribDivisor(4 + c, 1);
        }
        glEnableVertexAttribArray(8);
        glVertexAttribPointer(8, 4, GL_FLOAT, GL_FALSE, stride, (void*)(base + offsetof(PartInstance, colorEm)));
        glVertexAttribDivisor(8, 1);

        glDrawArraysInstanced(GL_TRIANGLES, 0, f.verts, (GLsizei)(end - start));
        ++draws;
        if (f.transparent) {
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
        }

        // La forma resta usabile dal disegno normale (ombre, Studio...) come prima.
        for (int a = 4; a <= 8; ++a) {
            glVertexAttribDivisor(a, 0);
            glDisableVertexAttribArray(a);
        }
        start = end;
    }
    glBindVertexArray(0);
    g_partInstBatchesLastFrame = draws;
    return draws;
}
