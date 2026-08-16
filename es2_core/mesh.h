#pragma once
/**
 * Mesh builder — cube (Roblox brick) and plane (baseplate).
 * ES 2.0: VBO + IBO. Attribute binding via glVertexAttribPointer
 * (no core VAO in ES 2.0; bindAttributes() is the "VAO-style" setup).
 */

#include <GLES2/gl2.h>

struct Mesh {
    GLuint vbo = 0;
    GLuint ibo = 0;
    GLsizei indexCount = 0;
};

/**
 * Interleaved vertex: position (3) + normal (3) = 6 floats.
 * Call after binding the shader that uses locations 0/1.
 */
void meshBindAttributes(const Mesh& mesh);

void meshUnbindAttributes();

/** Unit cube centered at origin, size 1x1x1 (scale via model matrix). */
Mesh meshCreateCube();

/** Flat XZ plane centered at origin, size 1x1 (Y up). Scale for baseplate. */
Mesh meshCreatePlane();

void meshDestroy(Mesh& mesh);

/** Draw indexed triangles (attributes must already be bound). */
void meshDraw(const Mesh& mesh);
