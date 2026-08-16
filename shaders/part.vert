#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;

layout(set = 0, binding = 0) uniform FrameUBO {
    mat4 view;
    mat4 proj;
    vec3 sunDir;
    vec3 camPos;
    float studScale;
    float _pad;
} frame;

layout(push_constant) uniform Push {
    mat4 model;
    vec3 color;
} pc;

layout(location = 0) out vec3 vNormal;
layout(location = 1) out vec3 vWorld;

void main() {
    vec4 world = pc.model * vec4(inPos, 1.0);
    vWorld = world.xyz;
    vNormal = mat3(pc.model) * inNormal;
    gl_Position = frame.proj * frame.view * world;
}
