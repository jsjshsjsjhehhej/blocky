#pragma once

/**
 * Shadow map direzionale per sole / luna.
 * Depth pass ortografico centrato davanti alla camera: l'ombra ha la forma
 * vera della mesh (sfere, cilindri, wedge, modelli importati, avatar), non
 * del suo bounding box.
 */

#include <glad/gl.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cstdio>

struct ShadowMap {
    GLuint fbo = 0;
    GLuint depthTex = 0;
    GLuint depthProg = 0;
    int size = 0;
    /** Estensione (raggio in unità mondo) coperta dalla mappa. */
    float extent = 90.f;
    glm::mat4 lightVP{1.f};
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
            std::fprintf(stderr, "[ShadowMap] shader: %s\n", log);
        }
        return s;
    }

    bool init(int mapSize = 2048) {
        size = mapSize;

        // Depth-only: legge solo la posizione (location 0), va bene per ogni VAO
        static const char* vs = R"(#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 uModel;
uniform mat4 uLightVP;
void main(){ gl_Position = uLightVP * uModel * vec4(aPos, 1.0); })";
        static const char* fs = R"(#version 330 core
void main(){})";

        depthProg = glCreateProgram();
        GLuint v = compile(GL_VERTEX_SHADER, vs);
        GLuint f = compile(GL_FRAGMENT_SHADER, fs);
        glAttachShader(depthProg, v);
        glAttachShader(depthProg, f);
        glLinkProgram(depthProg);
        glDeleteShader(v);
        glDeleteShader(f);

        glGenTextures(1, &depthTex);
        glBindTexture(GL_TEXTURE_2D, depthTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, size, size, 0,
                     GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        // Fuori mappa = profondità massima = mai in ombra
        const float border[4] = {1.f, 1.f, 1.f, 1.f};
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
        // Confronto hardware: sampler2DShadow con PCF gratis in bilineare
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTex, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        if (st != GL_FRAMEBUFFER_COMPLETE) {
            std::fprintf(stderr, "[ShadowMap] FBO incompleto (0x%X)\n", st);
            return false;
        }
        GLint linked = GL_FALSE;
        glGetProgramiv(depthProg, GL_LINK_STATUS, &linked);
        ready = linked && depthTex && fbo;
        return ready;
    }

    void destroy() {
        if (fbo) glDeleteFramebuffers(1, &fbo);
        if (depthTex) glDeleteTextures(1, &depthTex);
        if (depthProg) glDeleteProgram(depthProg);
        fbo = depthTex = depthProg = 0;
        ready = false;
    }

    /**
     * Prepara la matrice luce e apre il depth pass.
     * toLight = direzione VERSO l'astro. focus = punto guardato (player/camera).
     * Ritorna false se non c'è nulla da fare (mappa non pronta).
     */
    bool begin(const glm::vec3& toLight, const glm::vec3& focus) {
        if (!ready) return false;
        glm::vec3 L = glm::normalize(toLight);
        if (std::fabs(L.y) < 0.05f) // astro rasente: evita matrice degenere
            L = glm::normalize(glm::vec3(L.x, L.y >= 0.f ? 0.05f : -0.05f, L.z));

        // "far"/"near" sono macro di windows.h: nomi espliciti
        float farPlane = extent * 4.f;
        glm::vec3 eye = focus + L * (extent * 2.f);
        glm::vec3 up = std::fabs(L.y) > 0.95f ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
        glm::mat4 lightView = glm::lookAt(eye, focus, up);
        glm::mat4 lightProj = glm::ortho(-extent, extent, -extent, extent, 0.5f, farPlane);

        // Snap al texel: evita lo sfarfallio dell'ombra quando la camera si muove
        glm::mat4 vp = lightProj * lightView;
        glm::vec4 origin = vp * glm::vec4(0, 0, 0, 1);
        float texelsPerUnit = (float)size / (extent * 2.f);
        glm::vec2 snapped{
            std::floor(origin.x * 0.5f * (float)size) / (0.5f * (float)size),
            std::floor(origin.y * 0.5f * (float)size) / (0.5f * (float)size)};
        glm::vec2 offset = snapped - glm::vec2(origin.x, origin.y);
        (void)texelsPerUnit;
        lightProj[3][0] += offset.x;
        lightProj[3][1] += offset.y;
        lightVP = lightProj * lightView;

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, size, size);
        glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        // Niente culling: si salva la superficie PIÙ VICINA alla luce.
        // (col front-face culling si salvava quella lontana, cioè il fondo
        //  dell'oggetto: per una mesh appoggiata a terra l'ombra spariva
        //  proprio al contatto — peter-panning.)
        // L'acne si evita con il depth bias scalato sulla pendenza.
        glDisable(GL_CULL_FACE);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.8f, 3.f);
        glUseProgram(depthProg);
        glUniformMatrix4fv(glGetUniformLocation(depthProg, "uLightVP"), 1, GL_FALSE,
                           glm::value_ptr(lightVP));
        return true;
    }

    /** Model matrix del prossimo oggetto da scrivere nella mappa. */
    void setModel(const glm::mat4& m) const {
        glUniformMatrix4fv(glGetUniformLocation(depthProg, "uModel"), 1, GL_FALSE,
                           glm::value_ptr(m));
    }

    void end(int screenW, int screenH) {
        glDisable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(0.f, 0.f);
        glCullFace(GL_BACK);
        glDisable(GL_CULL_FACE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, screenW, screenH);
    }

    /** Lega la mappa a un'unità texture e imposta le uniform del programma. */
    void bindForRead(GLuint prog, int unit = 3) const {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, depthTex);
        glUniform1i(glGetUniformLocation(prog, "uShadowMap"), unit);
        glUniformMatrix4fv(glGetUniformLocation(prog, "uLightVP"), 1, GL_FALSE,
                           glm::value_ptr(lightVP));
        glUniform1f(glGetUniformLocation(prog, "uShadowTexel"), 1.f / (float)size);
        glActiveTexture(GL_TEXTURE0);
    }
};
