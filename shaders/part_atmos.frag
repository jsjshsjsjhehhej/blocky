#version 320 es
precision mediump float;

in vec3 vNormal;
in vec3 vWorld;
out vec4 FragColor;

uniform vec3 uColor;
uniform vec3 uSun;
uniform vec3 uCamPos;
uniform sampler2D uStud;
uniform float uStudScale;

// Cielo stile Roblox classico
const vec3 SKY_ZENITH  = vec3(0.53, 0.72, 0.92);
const vec3 SKY_HORIZON = vec3(0.69, 0.84, 0.95);

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(-uSun);
    float diff = max(dot(N, L), 0.0);
    float lit = 0.40 + diff * 0.70;

    vec3 base = uColor;
    if (N.y > 0.85) {
        vec2 uv = vWorld.xz / uStudScale;
        vec3 stud = texture(uStud, uv).rgb;
        base = uColor * stud * 1.12;
    }

    vec3 shaded = base * lit;

    // Atmospheric shading (mobile): fog morbido verso il cielo
    float dist = length(vWorld - uCamPos);
    float fog = 1.0 - exp(-dist * 0.0065);
    vec3 sky = mix(SKY_HORIZON, SKY_ZENITH, clamp(vWorld.y * 0.02 + 0.5, 0.0, 1.0));
    vec3 finalCol = mix(shaded, sky, fog * 0.55);

    FragColor = vec4(finalCol, 1.0);
}
