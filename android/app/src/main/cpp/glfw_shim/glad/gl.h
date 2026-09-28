#pragma once

/**
 * glad finto per Android — stessa idea dello shim di GLFW qui accanto.
 *
 * Su desktop glad serve a *caricare a runtime* i puntatori alle funzioni
 * OpenGL, perche' Windows espone solo OpenGL 1.1 staticamente. Su Android
 * non serve niente di tutto questo: le funzioni GLES sono nella libreria di
 * sistema (-lGLESv3) e si linkano direttamente. Quindi qui basta includere
 * gli header GLES veri e far diventare gladLoadGL() un "ok, fatto".
 *
 * Il motore continua a scrivere #include <glad/gl.h> e non si accorge di
 * niente.
 */

#include <GLES3/gl32.h>
#include <GLES2/gl2ext.h>

// Firma compatibile con quella vera: main.cpp chiama gladLoadGL(glfwGetProcAddress).
typedef void (*(*GLADloadfunc)(const char*))(void);

inline int gladLoadGL(...) { return 1; }

// Presenti su OpenGL desktop, assenti da GLES: il multisample su Android si
// decide quando si crea la superficie EGL, non con glEnable, quindi il
// glEnable(GL_MULTISAMPLE) del motore diventa innocuo invece di non compilare.
#ifndef GL_MULTISAMPLE
#define GL_MULTISAMPLE 0x809D
#endif
