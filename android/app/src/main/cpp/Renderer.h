#pragma once

#include <EGL/egl.h>
#include <GLES3/gl3.h>

struct android_app;

// Fase 1: EGL + un triangolo con la VERTEX shader vera del motore
// (kGlesPartVS, src/render/GlShaders.h) — vedi Renderer.cpp per la nota
// sulla fragment (kGlesStudAtmosFS non è collegabile qui as-is, vedi commento).
class Renderer {
public:
    explicit Renderer(android_app* app);
    ~Renderer();

    void render();

private:
    void initEgl();
    void initGl();
    void updateViewport();

    android_app* app_;
    EGLDisplay display_ = EGL_NO_DISPLAY;
    EGLSurface surface_ = EGL_NO_SURFACE;
    EGLContext context_ = EGL_NO_CONTEXT;
    EGLint width_ = 0;
    EGLint height_ = 0;

    GLuint program_ = 0;
    GLuint vbo_ = 0;
    double startTime_ = 0.0;
};
