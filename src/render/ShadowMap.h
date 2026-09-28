#pragma once


#include <glad/gl.h>
#include "render/GlslPortable.h"
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
    float extent = 90.f;
    glm::mat4 lightVP{1.f};
    bool ready = false;

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
            std::fprintf(stderr, "[ShadowMap] shader: %s\n", log);
        }
        return s;
    }

    bool init(int mapSize = 2048) {
        size = mapSize;

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
        // Il tipo GL_FLOAT e' legale su desktop ma NON su GLES 3: li'
        // DEPTH_COMPONENT24 vuole UNSIGNED_INT, e con GL_FLOAT la texture non
        // veniva allocata affatto (GL_INVALID_OPERATION) -> FBO incompleto
        // 0x8CD6. UNSIGNED_INT e' valido su entrambe le piattaforme.
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, size, size, 0,
                     GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
#ifdef __ANDROID__
        // GL_CLAMP_TO_BORDER e il colore di bordo esistono solo da GLES 3.2 in
        // su: su un contesto 3.0/3.1 sarebbero errori silenziosi. CLAMP_TO_EDGE
        // e' l equivalente piu' vicino disponibile ovunque — cambia solo cosa
        // si campiona FUORI dalla mappa ombre, che begin() ricentra ogni frame
        // sulla camera, quindi in pratica e' sempre roba lontana.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#else
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        const float border[4] = {1.f, 1.f, 1.f, 1.f};
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
#endif
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTex, 0);
        // glDrawBuffers (plurale) esiste sia su OpenGL desktop che su GLES 3;
        // glDrawBuffer (singolare) e' solo desktop. Stesso effetto — framebuffer
        // di sola profondita', nessuna uscita colore — ma compila su entrambi.
        const GLenum noColorTarget = GL_NONE;
        glDrawBuffers(1, &noColorTarget);
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

    bool begin(const glm::vec3& toLight, const glm::vec3& focus) {
        if (!ready) return false;
        glm::vec3 L = glm::normalize(toLight);
        if (std::fabs(L.y) < 0.05f)
            L = glm::normalize(glm::vec3(L.x, L.y >= 0.f ? 0.05f : -0.05f, L.z));

        float farPlane = extent * 4.f;
        glm::vec3 eye = focus + L * (extent * 2.f);
        glm::vec3 up = std::fabs(L.y) > 0.95f ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
        glm::mat4 lightView = glm::lookAt(eye, focus, up);
        glm::mat4 lightProj = glm::ortho(-extent, extent, -extent, extent, 0.5f, farPlane);

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
        glDisable(GL_CULL_FACE);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.8f, 3.f);
        glUseProgram(depthProg);
        glUniformMatrix4fv(glGetUniformLocation(depthProg, "uLightVP"), 1, GL_FALSE,
                           glm::value_ptr(lightVP));
        return true;
    }

    void setModel(const glm::mat4& m) const {
        glUniformMatrix4fv(glGetUniformLocation(depthProg, "uModel"), 1, GL_FALSE,
                           glm::value_ptr(m));
    }

    // restoreFbo: il framebuffer su cui si stava disegnando la scena prima
    // di questo pass di profondita' (0 = schermo reale, oppure la scena
    // offscreen del Bloom) — senza questo parametro il pass ombre
    // riporterebbe sempre sullo schermo reale anche quando la scena
    // principale sta disegnando dentro il framebuffer offscreen del Bloom.
    void end(int screenW, int screenH, GLuint restoreFbo = 0) {
        glDisable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(0.f, 0.f);
        glCullFace(GL_BACK);
        glDisable(GL_CULL_FACE);
        glBindFramebuffer(GL_FRAMEBUFFER, restoreFbo);
        glViewport(0, 0, screenW, screenH);
    }

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
