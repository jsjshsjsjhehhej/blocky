#pragma once

#include <glad/gl.h>
#include <cmath>
#include <vector>

// Geometria del cubo con spigoli smussati (stile "avatar Blocky"), usata sia
// per il rendering (BeveledCubeMesh, con normali per la luce) sia per
// l'export OBJ del player R6 di default (solo posizioni, nessuna GPU) —
// prima le due copie erano identiche ma scritte a mano due volte in due
// file diversi. Qui vive una sola definizione dei triangoli; ognuno dei due
// usi aggiunge solo cio' che gli serve in piu' (normali oppure niente).
inline void pushBeveledCubePositions(std::vector<float>& outPos, float bevel = 0.10f) {
    bevel = std::fmax(0.02f, std::fmin(bevel, 0.22f));
    const float h = 0.5f;
    const float i = h - bevel;

    auto push = [&](float x, float y, float z) {
        outPos.push_back(x); outPos.push_back(y); outPos.push_back(z);
    };
    auto tri = [&](float ax, float ay, float az,
                   float bx, float by, float bz,
                   float cx, float cy, float cz) {
        push(ax, ay, az); push(bx, by, bz); push(cx, cy, cz);
    };
    auto quad = [&](float ax, float ay, float az,
                    float bx, float by, float bz,
                    float cx, float cy, float cz,
                    float dx, float dy, float dz) {
        tri(ax, ay, az, bx, by, bz, cx, cy, cz);
        tri(ax, ay, az, cx, cy, cz, dx, dy, dz);
    };

    quad(-i, -i,  h,   i, -i,  h,   i,  i,  h,  -i,  i,  h);
    quad( i, -i, -h,  -i, -i, -h,  -i,  i, -h,   i,  i, -h);
    quad( h, -i, -i,   h, -i,  i,   h,  i,  i,   h,  i, -i);
    quad(-h, -i,  i,  -h, -i, -i,  -h,  i, -i,  -h,  i,  i);
    quad(-i,  h, -i,  -i,  h,  i,   i,  h,  i,   i,  h, -i);
    quad(-i, -h,  i,  -i, -h, -i,   i, -h, -i,   i, -h,  i);

    quad(-i, -h,  i,   i, -h,  i,   i, -i,  h,  -i, -i,  h);
    quad(-i,  i,  h,   i,  i,  h,   i,  h,  i,  -i,  h,  i);
    quad(-h, -i,  i,  -i, -i,  h,  -i,  i,  h,  -h,  i,  i);
    quad( i, -i,  h,   h, -i,  i,   h,  i,  i,   i,  i,  h);
    quad( i, -h, -i,  -i, -h, -i,  -i, -i, -h,   i, -i, -h);
    quad( i,  h, -i,  -i,  h, -i,  -i,  i, -h,   i,  i, -h);
    quad(-i, -i, -h,  -h, -i, -i,  -h,  i, -i,  -i,  i, -h);
    quad( h, -i, -i,   i, -i, -h,   i,  i, -h,   h,  i, -i);
    quad( i,  h, -i,   i,  h,  i,   h,  i,  i,   h,  i, -i);
    quad(-i,  h,  i,  -i,  h, -i,  -h,  i, -i,  -h,  i,  i);
    quad( i, -h,  i,   i, -h, -i,   h, -i, -i,   h, -i,  i);
    quad(-i, -h, -i,  -i, -h,  i,  -h, -i,  i,  -h, -i, -i);

    for (float sx : {-1.f, 1.f})
        for (float sy : {-1.f, 1.f})
            for (float sz : {-1.f, 1.f}) {
                float a[3] = { sx * i, sy * i, sz * h };
                float b[3] = { sx * i, sy * h, sz * i };
                float c[3] = { sx * h, sy * i, sz * i };
                tri(a[0], a[1], a[2], b[0], b[1], b[2], c[0], c[1], c[2]);
            }
}

struct BeveledCubeMesh {
    GLuint vao = 0;
    GLuint vbo = 0;
    int vertexCount = 0;

    void destroy() {
        if (vbo) { glDeleteBuffers(1, &vbo); vbo = 0; }
        if (vao) { glDeleteVertexArrays(1, &vao); vao = 0; }
        vertexCount = 0;
    }

    void build(float bevel = 0.10f) {
        destroy();

        std::vector<float> pos;
        pushBeveledCubePositions(pos, bevel);

        std::vector<float> v;
        v.reserve(pos.size() * 2);
        for (size_t t = 0; t + 8 < pos.size(); t += 9) {
            float ax = pos[t + 0], ay = pos[t + 1], az = pos[t + 2];
            float bx = pos[t + 3], by = pos[t + 4], bz = pos[t + 5];
            float cx = pos[t + 6], cy = pos[t + 7], cz = pos[t + 8];
            float ux = bx - ax, uy = by - ay, uz = bz - az;
            float vx = cx - ax, vy = cy - ay, vz = cz - az;
            float nx = uy * vz - uz * vy;
            float ny = uz * vx - ux * vz;
            float nz = ux * vy - uy * vx;
            float len = std::sqrt(nx * nx + ny * ny + nz * nz);
            if (len < 1e-8f) continue;
            nx /= len; ny /= len; nz /= len;
            float cx0 = (ax + bx + cx) * (1.f / 3.f);
            float cy0 = (ay + by + cy) * (1.f / 3.f);
            float cz0 = (az + bz + cz) * (1.f / 3.f);
            const bool flip = (nx * cx0 + ny * cy0 + nz * cz0 < 0.f);
            const float sx = flip ? -1.f : 1.f;
            if (!flip) {
                v.insert(v.end(), {ax, ay, az, nx * sx, ny * sx, nz * sx});
                v.insert(v.end(), {bx, by, bz, nx * sx, ny * sx, nz * sx});
                v.insert(v.end(), {cx, cy, cz, nx * sx, ny * sx, nz * sx});
            } else {
                v.insert(v.end(), {ax, ay, az, nx * sx, ny * sx, nz * sx});
                v.insert(v.end(), {cx, cy, cz, nx * sx, ny * sx, nz * sx});
                v.insert(v.end(), {bx, by, bz, nx * sx, ny * sx, nz * sx});
            }
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
