#include "shader.h"

#include <cstdio>

// ---------------------------------------------------------------------------
// Embedded GLSL ES 1.00 shaders (OpenGL ES 2.0)
// ---------------------------------------------------------------------------

static const char* kPlasticVS = R"(#version 100
attribute vec3 aPosition;
attribute vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

varying vec3 vWorldPos;
varying vec3 vNormal;

void main() {
    // World-space position for lighting
    vec4 world = uModel * vec4(aPosition, 1.0);
    vWorldPos = world.xyz;

    // Approximate normal (uniform scale bricks OK)
    vNormal = mat3(uModel) * aNormal;

    gl_Position = uProj * uView * world;
}
)";

static const char* kPlasticFS = R"(#version 100
precision mediump float;

varying vec3 vWorldPos;
varying vec3 vNormal;

uniform vec3 uColor;   // BrickColor / Part.Color
uniform vec3 uSunDir;  // Direction toward the sun (normalized)
uniform vec3 uCamPos;  // Camera position (for specular)

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(uSunDir);

    // Lambertian diffuse (classic Roblox-ish)
    float ndotl = max(dot(N, L), 0.0);
    float ambient = 0.35;
    float diffuse = ndotl * 0.55;

    // Simple Blinn-Phong specular — plastic highlight
    vec3 V = normalize(uCamPos - vWorldPos);
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), 32.0) * 0.25;

    vec3 lit = uColor * (ambient + diffuse) + vec3(spec);
    gl_FragColor = vec4(lit, 1.0);
}
)";

GLuint shaderCompile(GLenum type, const char* source) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &source, nullptr);
    glCompileShader(sh);

    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(sh, sizeof(log), nullptr, log);
        std::fprintf(stderr, "Shader compile error:\n%s\n", log);
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

GLuint shaderLink(GLuint vs, GLuint fs) {
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    // Bind attribute names before link (ES 2.0 friendly)
    glBindAttribLocation(prog, 0, "aPosition");
    glBindAttribLocation(prog, 1, "aNormal");
    glLinkProgram(prog);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        std::fprintf(stderr, "Shader link error:\n%s\n", log);
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

bool shaderCreatePlastic(Shader& out) {
    GLuint vs = shaderCompile(GL_VERTEX_SHADER, kPlasticVS);
    GLuint fs = shaderCompile(GL_FRAGMENT_SHADER, kPlasticFS);
    if (!vs || !fs) return false;

    out.program = shaderLink(vs, fs);
    // Shaders can be deleted after successful link
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!out.program) return false;

    out.uModel  = glGetUniformLocation(out.program, "uModel");
    out.uView   = glGetUniformLocation(out.program, "uView");
    out.uProj   = glGetUniformLocation(out.program, "uProj");
    out.uColor  = glGetUniformLocation(out.program, "uColor");
    out.uSunDir = glGetUniformLocation(out.program, "uSunDir");
    out.uCamPos = glGetUniformLocation(out.program, "uCamPos");

    out.aPosition = 0;
    out.aNormal   = 1;
    return true;
}

void shaderDestroy(Shader& s) {
    if (s.program) {
        glDeleteProgram(s.program);
        s.program = 0;
    }
}

void shaderUse(const Shader& s) {
    glUseProgram(s.program);
}
