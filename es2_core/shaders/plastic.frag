#version 100
// Fragment — directional sun + plastic specular (OpenGL ES 2.0)
precision mediump float;

varying vec3 vWorldPos;
varying vec3 vNormal;

uniform vec3 uColor;
uniform vec3 uSunDir;
uniform vec3 uCamPos;

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(uSunDir);

    float ndotl = max(dot(N, L), 0.0);
    float ambient = 0.35;
    float diffuse = ndotl * 0.55;

    vec3 V = normalize(uCamPos - vWorldPos);
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), 32.0) * 0.25;

    vec3 lit = uColor * (ambient + diffuse) + vec3(spec);
    gl_FragColor = vec4(lit, 1.0);
}
