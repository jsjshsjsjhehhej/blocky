#include "mesh.h"

// Stride: 6 floats (pos.xyz + normal.xyz)
static const GLsizei kStride = 6 * sizeof(float);

static Mesh uploadMesh(const float* verts, size_t vertFloats,
                       const unsigned short* indices, size_t indexCount) {
    Mesh mesh;
    mesh.indexCount = static_cast<GLsizei>(indexCount);

    glGenBuffers(1, &mesh.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertFloats * sizeof(float)),
                 verts, GL_STATIC_DRAW);

    glGenBuffers(1, &mesh.ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(indexCount * sizeof(unsigned short)),
                 indices, GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    return mesh;
}

void meshBindAttributes(const Mesh& mesh) {
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ibo);

    // Location 0: position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, kStride,
                          reinterpret_cast<const void*>(0));

    // Location 1: normal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, kStride,
                          reinterpret_cast<const void*>(3 * sizeof(float)));
}

void meshUnbindAttributes() {
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

Mesh meshCreateCube() {
    // 6 faces × 4 verts, each: px,py,pz, nx,ny,nz
    const float v[] = {
        // +Z
        -0.5f,-0.5f, 0.5f,  0,0,1,   0.5f,-0.5f, 0.5f,  0,0,1,
         0.5f, 0.5f, 0.5f,  0,0,1,  -0.5f, 0.5f, 0.5f,  0,0,1,
        // -Z
         0.5f,-0.5f,-0.5f,  0,0,-1, -0.5f,-0.5f,-0.5f,  0,0,-1,
        -0.5f, 0.5f,-0.5f,  0,0,-1,  0.5f, 0.5f,-0.5f,  0,0,-1,
        // +X
         0.5f,-0.5f, 0.5f,  1,0,0,   0.5f,-0.5f,-0.5f,  1,0,0,
         0.5f, 0.5f,-0.5f,  1,0,0,   0.5f, 0.5f, 0.5f,  1,0,0,
        // -X
        -0.5f,-0.5f,-0.5f, -1,0,0,  -0.5f,-0.5f, 0.5f, -1,0,0,
        -0.5f, 0.5f, 0.5f, -1,0,0,  -0.5f, 0.5f,-0.5f, -1,0,0,
        // +Y (stud face / top)
        -0.5f, 0.5f, 0.5f,  0,1,0,   0.5f, 0.5f, 0.5f,  0,1,0,
         0.5f, 0.5f,-0.5f,  0,1,0,  -0.5f, 0.5f,-0.5f,  0,1,0,
        // -Y
        -0.5f,-0.5f,-0.5f,  0,-1,0,  0.5f,-0.5f,-0.5f,  0,-1,0,
         0.5f,-0.5f, 0.5f,  0,-1,0, -0.5f,-0.5f, 0.5f,  0,-1,0,
    };

    unsigned short idx[36];
    for (int f = 0; f < 6; ++f) {
        unsigned short b = static_cast<unsigned short>(f * 4);
        idx[f * 6 + 0] = b + 0;
        idx[f * 6 + 1] = b + 1;
        idx[f * 6 + 2] = b + 2;
        idx[f * 6 + 3] = b + 0;
        idx[f * 6 + 4] = b + 2;
        idx[f * 6 + 5] = b + 3;
    }

    return uploadMesh(v, sizeof(v) / sizeof(float), idx, 36);
}

Mesh meshCreatePlane() {
    // XZ plane, Y = 0, facing +Y (baseplate)
    const float v[] = {
        -0.5f, 0.f, -0.5f,  0,1,0,
         0.5f, 0.f, -0.5f,  0,1,0,
         0.5f, 0.f,  0.5f,  0,1,0,
        -0.5f, 0.f,  0.5f,  0,1,0,
    };
    const unsigned short idx[] = { 0, 1, 2, 0, 2, 3 };
    return uploadMesh(v, sizeof(v) / sizeof(float), idx, 6);
}

void meshDestroy(Mesh& mesh) {
    if (mesh.vbo) glDeleteBuffers(1, &mesh.vbo);
    if (mesh.ibo) glDeleteBuffers(1, &mesh.ibo);
    mesh.vbo = mesh.ibo = 0;
    mesh.indexCount = 0;
}

void meshDraw(const Mesh& mesh) {
    glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_SHORT, nullptr);
}
