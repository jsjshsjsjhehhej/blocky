#pragma once
/**
 * Minimal GLES 2.0 shader loader (compile + link + error logs).
 */

#include <GLES2/gl2.h>

struct Shader {
    GLuint program = 0;

    // Cached uniform locations (fill after link)
    GLint uModel      = -1;
    GLint uView       = -1;
    GLint uProj       = -1;
    GLint uColor      = -1;
    GLint uSunDir     = -1;
    GLint uCamPos     = -1;

    // Attribute locations (match shader layout)
    GLint aPosition = -1;
    GLint aNormal   = -1;
};

/** Compile one shader stage; returns 0 on failure. */
GLuint shaderCompile(GLenum type, const char* source);

/** Link vertex + fragment into a program; returns 0 on failure. */
GLuint shaderLink(GLuint vs, GLuint fs);

/**
 * Build the Roblox-2015 plastic brick shader from embedded sources.
 * Fills uniform/attribute locations. Returns false on error.
 */
bool shaderCreatePlastic(Shader& out);

void shaderDestroy(Shader& s);

void shaderUse(const Shader& s);
