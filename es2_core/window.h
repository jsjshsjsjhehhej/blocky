#pragma once
/**
 * Window + OpenGL ES 2.0 context via SDL2.
 */

#include <SDL.h>
#include <GLES2/gl2.h>

struct AppWindow {
    SDL_Window*   window  = nullptr;
    SDL_GLContext gl      = nullptr;
    int           width   = 1280;
    int           height  = 720;
};

/**
 * Create an SDL2 window with an OpenGL ES 2.0 context.
 * Enables depth test and sets Roblox-2015 sky blue clear color.
 * Returns false on failure (check SDL_GetError()).
 */
bool windowCreate(AppWindow& out, const char* title, int w, int h);

/** Swap back buffer and poll for quit (minimal; call each frame). */
void windowSwap(AppWindow& app);

/** Destroy GL context and window. */
void windowDestroy(AppWindow& app);
