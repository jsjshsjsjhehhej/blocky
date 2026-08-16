#pragma once

#include <glad/gl.h>
#include <cmath>
#include <vector>

/**
 * Cubo unitario (-0.5..0.5) con spigoli smussati (bevel).
 * 6 facce + 12 edge + 8 corner, winding sempre verso fuori.
 */
struct BeveledCubeMesh {
    GLuint vao = 0;
    GLuint vbo = 0;
    int vertexCount = 0;

    void destroy() {
        if (vbo) { glDeleteBuffers(1, &vbo); vbo = 0; }
        if (vao) { glDeleteVertexArrays(1, &vao); vao = 0; }
        vertexCount = 0;
    }

    /** bevel = quanto smussare (0.05–0.15 tipico). */
    void build(float bevel = 0.10f) {
        destroy();
        bevel = std::fmax(0.02f, std::fmin(bevel, 0.22f));
        const float h = 0.5f;
        const float i = h - bevel; // inset face

        std::vector<float> v; // pos.xyz + n.xyz
        auto push = [&](float x, float y, float z, float nx, float ny, float nz) {
            v.push_back(x); v.push_back(y); v.push_back(z);
            v.push_back(nx); v.push_back(ny); v.push_back(nz);
        };

        // Triangolo con normale verso fuori (dot con centro > 0)
        auto tri = [&](float ax, float ay, float az,
                       float bx, float by, float bz,
                       float cx, float cy, float cz) {
            float ux = bx - ax, uy = by - ay, uz = bz - az;
            float vx = cx - ax, vy = cy - ay, vz = cz - az;
            float nx = uy * vz - uz * vy;
            float ny = uz * vx - ux * vz;
            float nz = ux * vy - uy * vx;
            float len = std::sqrt(nx * nx + ny * ny + nz * nz);
            if (len < 1e-8f) return;
            nx /= len; ny /= len; nz /= len;
            float cx0 = (ax + bx + cx) * (1.f / 3.f);
            float cy0 = (ay + by + cy) * (1.f / 3.f);
            float cz0 = (az + bz + cz) * (1.f / 3.f);
            if (nx * cx0 + ny * cy0 + nz * cz0 < 0.f) {
                // flip winding
                push(ax, ay, az, -nx, -ny, -nz);
                push(cx, cy, cz, -nx, -ny, -nz);
                push(bx, by, bz, -nx, -ny, -nz);
            } else {
                push(ax, ay, az, nx, ny, nz);
                push(bx, by, bz, nx, ny, nz);
                push(cx, cy, cz, nx, ny, nz);
            }
        };

        auto quad = [&](float ax, float ay, float az,
                        float bx, float by, float bz,
                        float cx, float cy, float cz,
                        float dx, float dy, float dz) {
            tri(ax, ay, az, bx, by, bz, cx, cy, cz);
            tri(ax, ay, az, cx, cy, cz, dx, dy, dz);
        };

        // --- 6 facce inset ---
        // +Z -Z +X -X +Y -Y
        quad(-i, -i,  h,   i, -i,  h,   i,  i,  h,  -i,  i,  h);
        quad( i, -i, -h,  -i, -i, -h,  -i,  i, -h,   i,  i, -h);
        quad( h, -i, -i,   h, -i,  i,   h,  i,  i,   h,  i, -i);
        quad(-h, -i,  i,  -h, -i, -i,  -h,  i, -i,  -h,  i,  i);
        quad(-i,  h, -i,  -i,  h,  i,   i,  h,  i,   i,  h, -i);
        quad(-i, -h,  i,  -i, -h, -i,   i, -h, -i,   i, -h,  i);

        // --- 12 edge bevels ---
        // Z+ con bottom / top / -X / +X
        quad(-i, -h,  i,   i, -h,  i,   i, -i,  h,  -i, -i,  h);
        quad(-i,  i,  h,   i,  i,  h,   i,  h,  i,  -i,  h,  i);
        quad(-h, -i,  i,  -i, -i,  h,  -i,  i,  h,  -h,  i,  i);
        quad( i, -i,  h,   h, -i,  i,   h,  i,  i,   i,  i,  h);
        // Z- con bottom / top / -X / +X
        quad( i, -h, -i,  -i, -h, -i,  -i, -i, -h,   i, -i, -h);
        quad( i,  h, -i,  -i,  h, -i,  -i,  i, -h,   i,  i, -h);
        quad(-i, -i, -h,  -h, -i, -i,  -h,  i, -i,  -i,  i, -h);
        quad( h, -i, -i,   i, -i, -h,   i,  i, -h,   h,  i, -i);
        // Top/Bottom con +X / -X  (questi mancavano → buchi / “facce invisibili”)
        quad( i,  h, -i,   i,  h,  i,   h,  i,  i,   h,  i, -i);
        quad(-i,  h,  i,  -i,  h, -i,  -h,  i, -i,  -h,  i,  i);
        quad( i, -h,  i,   i, -h, -i,   h, -i, -i,   h, -i,  i);
        quad(-i, -h, -i,  -i, -h,  i,  -h, -i,  i,  -h, -i, -i);

        // --- 8 corner triangles ---
        for (float sx : {-1.f, 1.f})
            for (float sy : {-1.f, 1.f})
                for (float sz : {-1.f, 1.f}) {
                    float a[3] = { sx * i, sy * i, sz * h };
                    float b[3] = { sx * i, sy * h, sz * i };
                    float c[3] = { sx * h, sy * i, sz * i };
                    tri(a[0], a[1], a[2], b[0], b[1], b[2], c[0], c[1], c[2]);
                }

        vertexCount = (int)(v.size() / 6);
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(v.size() * sizeof(float)), v.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glBindVertexArray(0);
    }

    void draw() const {
        if (!vao || vertexCount <= 0) return;
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, vertexCount);
    }
};
