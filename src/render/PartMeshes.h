#pragma once

#include <glad/gl.h>
#include <cmath>
#include <vector>

/**
 * Mesh unitarie (-0.5..0.5) pos.xyz + n.xyz — stesso layout del cubo Part.
 * Ball / Cylinder / Wedge (rampa triangolare stile Roblox).
 */
struct ShapeMesh {
    GLuint vao = 0;
    GLuint vbo = 0;
    int vertexCount = 0;

    void destroy() {
        if (vbo) { glDeleteBuffers(1, &vbo); vbo = 0; }
        if (vao) { glDeleteVertexArrays(1, &vao); vao = 0; }
        vertexCount = 0;
    }

    void upload(const std::vector<float>& v) {
        destroy();
        if (v.empty()) return;
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
};

struct PartMeshes {
    ShapeMesh ball;
    ShapeMesh cylinder;
    ShapeMesh wedge;

    void destroy() {
        ball.destroy();
        cylinder.destroy();
        wedge.destroy();
    }

    void buildAll() {
        buildBall(24, 16);
        buildCylinder(28);
        buildWedge();
    }

    void buildBall(int slices, int stacks) {
        std::vector<float> v;
        auto push = [&](float x, float y, float z, float nx, float ny, float nz) {
            v.push_back(x); v.push_back(y); v.push_back(z);
            v.push_back(nx); v.push_back(ny); v.push_back(nz);
        };
        const float R = 0.5f;
        for (int i = 0; i < stacks; ++i) {
            float v0 = (float)i / (float)stacks;
            float v1 = (float)(i + 1) / (float)stacks;
            float phi0 = v0 * 3.14159265f;
            float phi1 = v1 * 3.14159265f;
            float y0 = std::cos(phi0), y1 = std::cos(phi1);
            float r0 = std::sin(phi0), r1 = std::sin(phi1);
            for (int j = 0; j < slices; ++j) {
                float u0 = (float)j / (float)slices;
                float u1 = (float)(j + 1) / (float)slices;
                float th0 = u0 * 6.2831853f;
                float th1 = u1 * 6.2831853f;
                float c0 = std::cos(th0), s0 = std::sin(th0);
                float c1 = std::cos(th1), s1 = std::sin(th1);
                float x00 = R * r0 * c0, z00 = R * r0 * s0;
                float x01 = R * r0 * c1, z01 = R * r0 * s1;
                float x10 = R * r1 * c0, z10 = R * r1 * s0;
                float x11 = R * r1 * c1, z11 = R * r1 * s1;
                float ny0 = y0, ny1 = y1;
                // due triangoli per quad
                push(x00, R * y0, z00, r0 * c0, ny0, r0 * s0);
                push(x10, R * y1, z10, r1 * c0, ny1, r1 * s0);
                push(x11, R * y1, z11, r1 * c1, ny1, r1 * s1);
                push(x00, R * y0, z00, r0 * c0, ny0, r0 * s0);
                push(x11, R * y1, z11, r1 * c1, ny1, r1 * s1);
                push(x01, R * y0, z01, r0 * c1, ny0, r0 * s1);
            }
        }
        ball.upload(v);
    }

    /** Cilindro asse Y: altezza 1, raggio 0.5 (poi scale Part.size). */
    void buildCylinder(int slices) {
        std::vector<float> v;
        auto push = [&](float x, float y, float z, float nx, float ny, float nz) {
            v.push_back(x); v.push_back(y); v.push_back(z);
            v.push_back(nx); v.push_back(ny); v.push_back(nz);
        };
        const float R = 0.5f;
        const float Y0 = -0.5f, Y1 = 0.5f;
        for (int j = 0; j < slices; ++j) {
            float u0 = (float)j / (float)slices;
            float u1 = (float)(j + 1) / (float)slices;
            float th0 = u0 * 6.2831853f;
            float th1 = u1 * 6.2831853f;
            float c0 = std::cos(th0), s0 = std::sin(th0);
            float c1 = std::cos(th1), s1 = std::sin(th1);
            float x0 = R * c0, z0 = R * s0;
            float x1 = R * c1, z1 = R * s1;
            // lato
            push(x0, Y0, z0, c0, 0.f, s0);
            push(x1, Y0, z1, c1, 0.f, s1);
            push(x1, Y1, z1, c1, 0.f, s1);
            push(x0, Y0, z0, c0, 0.f, s0);
            push(x1, Y1, z1, c1, 0.f, s1);
            push(x0, Y1, z0, c0, 0.f, s0);
            // coperchio top
            push(0.f, Y1, 0.f, 0.f, 1.f, 0.f);
            push(x0, Y1, z0, 0.f, 1.f, 0.f);
            push(x1, Y1, z1, 0.f, 1.f, 0.f);
            // coperchio bottom
            push(0.f, Y0, 0.f, 0.f, -1.f, 0.f);
            push(x1, Y0, z1, 0.f, -1.f, 0.f);
            push(x0, Y0, z0, 0.f, -1.f, 0.f);
        }
        cylinder.upload(v);
    }

    /** Wedge / rampa: pieno a +Z, spigolo basso a -Z (stile WedgePart). */
    void buildWedge() {
        std::vector<float> v;
        auto push = [&](float x, float y, float z, float nx, float ny, float nz) {
            v.push_back(x); v.push_back(y); v.push_back(z);
            v.push_back(nx); v.push_back(ny); v.push_back(nz);
        };
        auto tri = [&](float ax, float ay, float az,
                       float bx, float by, float bz,
                       float cx, float cy, float cz,
                       float nx, float ny, float nz) {
            push(ax, ay, az, nx, ny, nz);
            push(bx, by, bz, nx, ny, nz);
            push(cx, cy, cz, nx, ny, nz);
        };
        auto quad = [&](float ax, float ay, float az,
                        float bx, float by, float bz,
                        float cx, float cy, float cz,
                        float dx, float dy, float dz,
                        float nx, float ny, float nz) {
            tri(ax, ay, az, bx, by, bz, cx, cy, cz, nx, ny, nz);
            tri(ax, ay, az, cx, cy, cz, dx, dy, dz, nx, ny, nz);
        };

        const float h = 0.5f;
        // bottom
        quad(-h, -h, -h,  h, -h, -h,  h, -h,  h, -h, -h,  h,  0, -1, 0);
        // back (+Z) full
        quad(-h, -h,  h,  h, -h,  h,  h,  h,  h, -h,  h,  h,  0, 0, 1);
        // left (-X) triangle
        tri(-h, -h, -h, -h, -h,  h, -h,  h,  h,  -1, 0, 0);
        // right (+X) triangle
        tri( h, -h, -h,  h,  h,  h,  h, -h,  h,   1, 0, 0);
        // slope: from front-bottom to back-top (normale verso fuori/alto)
        {
            float ax = -h, ay = -h, az = -h;
            float bx =  h, by = -h, bz = -h;
            float cx =  h, cy =  h, cz =  h;
            float dx = -h, dy =  h, dz =  h;
            float ux = bx - ax, uy = by - ay, uz = bz - az;
            float vx = dx - ax, vy = dy - ay, vz = dz - az;
            float nx = uy * vz - uz * vy;
            float ny = uz * vx - ux * vz;
            float nz = ux * vy - uy * vx;
            float len = std::sqrt(nx * nx + ny * ny + nz * nz);
            if (len > 1e-6f) { nx /= len; ny /= len; nz /= len; }
            if (ny < 0.f) { nx = -nx; ny = -ny; nz = -nz; }
            quad(ax, ay, az, bx, by, bz, cx, cy, cz, dx, dy, dz, nx, ny, nz);
        }
        wedge.upload(v);
    }
};
