#include "window.h"

#include <cstdio>

bool windowCreate(AppWindow& out, const char* title, int w, int h) {
    out.width  = w;
    out.height = h;

    // Init SDL video subsystem
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return false;
    }

    // Request OpenGL ES 2.0
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    out.window = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        w, h,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (!out.window) {
        std::fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        return false;
    }

    out.gl = SDL_GL_CreateContext(out.window);
    if (!out.gl) {
        std::fprintf(stderr, "SDL_GL_CreateContext: %s\n", SDL_GetError());
        return false;
    }

    SDL_GL_SetSwapInterval(1); // vsync if available

    // Viewport matches window
    glViewport(0, 0, w, h);

    // Depth test for 3D bricks / baseplate
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // Roblox ~2015 sky blue clear color
    glClearColor(0.53f, 0.72f, 0.92f, 1.0f);

    return true;
}

void windowSwap(AppWindow& app) {
    SDL_GL_SwapWindow(app.window);
}

void windowDestroy(AppWindow& app) {
    if (app.gl) {
        SDL_GL_DeleteContext(app.gl);
        app.gl = nullptr;
    }
    if (app.window) {
        SDL_DestroyWindow(app.window);
        app.window = nullptr;
    }
    SDL_Quit();
}
