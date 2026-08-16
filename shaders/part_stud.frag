#version 450

layout(set = 0, binding = 0) uniform FrameUBO {
    mat4 view;
    mat4 proj;
    vec3 sunDir;
    vec3 camPos;
    float studScale;
    float _pad;
} frame;

layout(set = 1, binding = 0) uniform sampler2D uStud;

layout(push_constant) uniform Push {
    mat4 model;
    vec3 color;
} pc;

layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec3 vWorld;
layout(location = 0) out vec4 outColor;

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(-frame.sunDir);
    float diff = max(dot(N, L), 0.0);
    float lit = 0.42 + diff * 0.78;

    vec3 base = pc.color;

    // Stud SOLO sulla faccia superiore (normal up)
    if (N.y > 0.85) {
        vec2 uv = vWorld.xz / frame.studScale;
        vec3 stud = texture(uStud, uv).rgb;
        base = pc.color * stud * 1.15;
    }

    outColor = vec4(base * lit, 1.0);
}
