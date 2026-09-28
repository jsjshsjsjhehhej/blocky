#include "Renderer.h"
#include "AndroidOut.h"

#include <android/native_window.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <cassert>
#include <cmath>
#include <ctime>
#include <memory>
#include <vector>

// Vertex shader VERA del motore, riusata as-is (non riscritta) — vedi
// android/README.md. La fragment shader "gemella" del motore per questa
// vertex (kGlesStudAtmosFS) si aspetta pero' anche `in vec3 vLocalN` e
// `in vec2 vStudUV`, che kGlesPartVS non emette: e' una cosa gia' presente
// nel motore (usata cosi' anche da src/garden/garden_main.cpp), non un bug
// introdotto qui — per la Fase 1 uso una fragment minima compatibile con
// quello che kGlesPartVS emette davvero (vNormal/vWorld), per non nascondere
// il problema dietro una shader che non compilerebbe/linkerebbe per bene.
#include "render/GlShaders.h"

static const char* kFase1FS = R"(#version 320 es
precision mediump float;
in vec3 vNormal;
in vec3 vWorld;
out vec4 FragColor;
uniform vec3 uColor;
void main(){
  vec3 N = normalize(vNormal);
  float lit = 0.35 + 0.65 * max(dot(N, normalize(vec3(0.4,0.8,0.3))), 0.0);
  FragColor = vec4(uColor * lit, 1.0);
})";

static GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        aout << "Errore compilazione shader: " << log << std::endl;
    }
    return s;
}

static GLuint linkProgram(const char* vs, const char* fs) {
    GLuint v = compileShader(GL_VERTEX_SHADER, vs);
    GLuint f = compileShader(GL_FRAGMENT_SHADER, fs);
    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(p, sizeof(log), nullptr, log);
        aout << "Errore link programma: " << log << std::endl;
    }
    glDeleteShader(v);
    glDeleteShader(f);
    return p;
}

Renderer::Renderer(android_app* app) : app_(app) {
    initEgl();
    initGl();
    startTime_ = (double)std::clock() / CLOCKS_PER_SEC;
}

Renderer::~Renderer() {
    if (program_) glDeleteProgram(program_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (display_ != EGL_NO_DISPLAY) {
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (context_ != EGL_NO_CONTEXT) eglDestroyContext(display_, context_);
        if (surface_ != EGL_NO_SURFACE) eglDestroySurface(display_, surface_);
        eglTerminate(display_);
    }
}

void Renderer::initEgl() {
    constexpr EGLint attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_DEPTH_SIZE, 16,
        EGL_NONE
    };
    display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display_, nullptr, nullptr);

    EGLint numConfigs = 0;
    eglChooseConfig(display_, attribs, nullptr, 0, &numConfigs);
    std::vector<EGLConfig> configs(numConfigs);
    eglChooseConfig(display_, attribs, configs.data(), numConfigs, &numConfigs);
    EGLConfig config = configs[0];

    surface_ = eglCreateWindowSurface(display_, config, app_->window, nullptr);
    constexpr EGLint ctxAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    context_ = eglCreateContext(display_, config, nullptr, ctxAttribs);

    bool ok = eglMakeCurrent(display_, surface_, surface_, context_);
    assert(ok);
    eglQuerySurface(display_, surface_, EGL_WIDTH, &width_);
    eglQuerySurface(display_, surface_, EGL_HEIGHT, &height_);
    aout << "EGL pronto: " << width_ << "x" << height_ << std::endl;
}

void Renderer::initGl() {
    program_ = linkProgram(kGlesPartVS, kFase1FS);

    // Triangolo in clip space (uModel/uView/uProj = identita': niente
    // matematica in piu' per la Fase 1, servono solo per rispettare la
    // firma della vertex shader vera). aPos, aNormal.
    const float verts[] = {
        // x, y, z,      nx, ny, nz
        0.0f, 0.6f, 0.0f,   0.f, 0.f, 1.f,
       -0.6f,-0.5f, 0.0f,   0.f, 0.f, 1.f,
        0.6f,-0.5f, 0.0f,   0.f, 0.f, 1.f,
    };
    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    glClearColor(0.06f, 0.09f, 0.16f, 1.f); // stesso blu scuro del tema di Blocky
}

void Renderer::updateViewport() {
    EGLint w = 0, h = 0;
    eglQuerySurface(display_, surface_, EGL_WIDTH, &w);
    eglQuerySurface(display_, surface_, EGL_HEIGHT, &h);
    if (w != width_ || h != height_) {
        width_ = w;
        height_ = h;
        glViewport(0, 0, width_, height_);
    }
}

void Renderer::render() {
    updateViewport();

    double t = (double)std::clock() / CLOCKS_PER_SEC - startTime_;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(program_);
    const float identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    glUniformMatrix4fv(glGetUniformLocation(program_, "uModel"), 1, GL_FALSE, identity);
    glUniformMatrix4fv(glGetUniformLocation(program_, "uView"), 1, GL_FALSE, identity);
    glUniformMatrix4fv(glGetUniformLocation(program_, "uProj"), 1, GL_FALSE, identity);
    float r = 0.5f + 0.5f * (float)std::sin(t);
    glUniform3f(glGetUniformLocation(program_, "uColor"), r, 0.55f, 0.95f);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glDrawArrays(GL_TRIANGLES, 0, 3);

    eglSwapBuffers(display_, surface_);
}
