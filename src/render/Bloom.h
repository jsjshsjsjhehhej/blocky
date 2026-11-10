#pragma once

#include <glad/gl.h>
#include "render/GlslPortable.h"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cstdio>

// Bloom vero: la scena si renderizza in un framebuffer offscreen (invece
// che direttamente sullo schermo), poi le zone sopra soglia (Neon,
// ForceField, torce/luci molto vicine) vengono estratte, sfocate a
// risoluzione dimezzata e sommate sopra l'immagine originale — la luce
// "si spande" nel buio invece di restare solo un colore acceso a bordi
// netti. L'ImGui/HUD resta disegnato DOPO, direttamente sullo schermo
// reale, quindi non viene mai sfocato.
struct Bloom {
    GLuint sceneFbo = 0, sceneColorTex = 0, sceneDepthRbo = 0;
    GLuint brightFbo = 0, brightTex = 0;
    GLuint pingFbo[2] = {0, 0}, pingTex[2] = {0, 0};
    int fullW = 0, fullH = 0, halfW = 0, halfH = 0;
    GLuint quadVao = 0, quadVbo = 0;
    GLuint brightProg = 0, blurProg = 0, compositeProg = 0;
    bool ready = false;

    static GLuint compile(GLenum type, const char* src) {
        GLuint s = glCreateShader(type);
        const std::string portableSrc = glslForTarget(src);
        const char* srcPtr = portableSrc.c_str();
        glShaderSource(s, 1, &srcPtr, nullptr);
        glCompileShader(s);
        GLint ok = 0;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[512];
            glGetShaderInfoLog(s, 512, nullptr, log);
            std::fprintf(stderr, "[Bloom] shader: %s\n", log);
        }
        return s;
    }
    static GLuint link(GLuint vs, GLuint fs) {
        GLuint p = glCreateProgram();
        glAttachShader(p, vs);
        glAttachShader(p, fs);
        glLinkProgram(p);
        glDeleteShader(vs);
        glDeleteShader(fs);
        return p;
    }

    void makeQuad() {
        const float quad[] = {
            -1.f, -1.f,  1.f, -1.f,  1.f, 1.f,
            -1.f, -1.f,  1.f,  1.f, -1.f, 1.f,
        };
        glGenVertexArrays(1, &quadVao);
        glGenBuffers(1, &quadVbo);
        glBindVertexArray(quadVao);
        glBindBuffer(GL_ARRAY_BUFFER, quadVbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        glBindVertexArray(0);
    }

    static GLuint makeTex(int w, int h) {
        GLuint t;
        glGenTextures(1, &t);
        glBindTexture(GL_TEXTURE_2D, t);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        return t;
    }

    bool createTargets(int w, int h) {
        fullW = std::max(1, w);
        fullH = std::max(1, h);
        halfW = std::max(1, fullW / 2);
        halfH = std::max(1, fullH / 2);

        sceneColorTex = makeTex(fullW, fullH);
        glGenRenderbuffers(1, &sceneDepthRbo);
        glBindRenderbuffer(GL_RENDERBUFFER, sceneDepthRbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, fullW, fullH);
        glGenFramebuffers(1, &sceneFbo);
        glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneColorTex, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, sceneDepthRbo);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::fprintf(stderr, "[Bloom] scene FBO incompleto\n");
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return false;
        }

        brightTex = makeTex(halfW, halfH);
        glGenFramebuffers(1, &brightFbo);
        glBindFramebuffer(GL_FRAMEBUFFER, brightFbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, brightTex, 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::fprintf(stderr, "[Bloom] bright FBO incompleto\n");
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return false;
        }

        for (int i = 0; i < 2; ++i) {
            pingTex[i] = makeTex(halfW, halfH);
            glGenFramebuffers(1, &pingFbo[i]);
            glBindFramebuffer(GL_FRAMEBUFFER, pingFbo[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingTex[i], 0);
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                std::fprintf(stderr, "[Bloom] ping FBO %d incompleto\n", i);
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                return false;
            }
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return true;
    }

    void destroyTargets() {
        GLuint texs[] = {sceneColorTex, brightTex, pingTex[0], pingTex[1]};
        glDeleteTextures(4, texs);
        GLuint fbos[] = {sceneFbo, brightFbo, pingFbo[0], pingFbo[1]};
        glDeleteFramebuffers(4, fbos);
        if (sceneDepthRbo) glDeleteRenderbuffers(1, &sceneDepthRbo);
        sceneColorTex = brightTex = pingTex[0] = pingTex[1] = 0;
        sceneFbo = brightFbo = pingFbo[0] = pingFbo[1] = 0;
        sceneDepthRbo = 0;
    }

    bool init(int w, int h) {
        makeQuad();

        static const char* fsQuadVS = R"(#version 330 core
layout(location=0) in vec2 aPos;
out vec2 vUV;
void main(){ vUV = aPos * 0.5 + 0.5; gl_Position = vec4(aPos, 0.0, 1.0); })";

        static const char* brightFS = R"(#version 330 core
in vec2 vUV; out vec4 FragColor;
uniform sampler2D uScene;
uniform float uThLow, uThHigh;
void main(){
  vec3 c = texture(uScene, vUV).rgb;
  // Massimo canale, non luminanza percepita: la luminanza pesa il verde
  // ~12x il blu, quindi un Neon blu o rosso saturo (luminoso alla vista,
  // "scuro" per quella formula) restava quasi sempre escluso dal bloom.
  float lum = max(c.r, max(c.g, c.b));
  float k = smoothstep(uThLow, uThHigh, lum);
  FragColor = vec4(c * k, 1.0);
})";

        static const char* blurFS = R"(#version 330 core
in vec2 vUV; out vec4 FragColor;
uniform sampler2D uTex;
uniform vec2 uTexel; // direzione * dimensione texel (uno dei due assi a zero)
void main(){
  vec3 result = texture(uTex, vUV).rgb * 0.227027;
  vec2 off1 = uTexel * 1.3846153846;
  vec2 off2 = uTexel * 3.2307692308;
  result += texture(uTex, vUV + off1).rgb * 0.3162162162;
  result += texture(uTex, vUV - off1).rgb * 0.3162162162;
  result += texture(uTex, vUV + off2).rgb * 0.0702702703;
  result += texture(uTex, vUV - off2).rgb * 0.0702702703;
  FragColor = vec4(result, 1.0);
})";

        // Composite + RICOSTRUZIONE dell'immagine.
        //
        // Quando la scena viene disegnata a risoluzione ridotta (Render scale),
        // qui va riportata a schermo pieno. Con un semplice texture() il
        // ridimensionamento sarebbe bilineare, cioe' sfocato: e' il motivo per
        // cui "abbassa la risoluzione" di solito vuol dire "gioco brutto".
        //
        // Catmull-Rom a 9 prelievi costa quanto 9 texture() ma ricostruisce i
        // bordi in modo netto; RCAS (la nitidezza adattiva di AMD FidelityFX)
        // recupera il dettaglio fine senza esaltare il rumore. Sono shader
        // puri: funzionano su qualsiasi GPU e qualsiasi API, dalla GT 740 in
        // su — a differenza di DLSS, che vuole Tensor Core e DirectX/Vulkan.
        //
        // A risoluzione piena (uUpscale = 0) si salta tutto e si campiona
        // diretto: non ha senso ricostruire cio' che non e' stato ridotto.
        static const char* compositeFS = R"(#version 330 core
in vec2 vUV; out vec4 FragColor;
uniform sampler2D uScene, uBloom;
uniform float uIntensity;
uniform vec2  uSceneSize;   // risoluzione VERA della scena disegnata
uniform float uUpscale;     // 1 = ricostruisci, 0 = campiona diretto
uniform float uSharp;       // 0 = niente nitidezza, 1 = massima

// Catmull-Rom con 9 prelievi bilineari invece di 16 puntuali: stessa curva,
// quasi meta' del lavoro. E' il filtro usato dai temporal upscaler moderni.
vec3 sampleCatmullRom(sampler2D tex, vec2 uv, vec2 size) {
  vec2 texel = 1.0 / size;
  vec2 samplePos = uv * size;
  vec2 texPos1 = floor(samplePos - 0.5) + 0.5;
  vec2 f = samplePos - texPos1;

  vec2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f));
  vec2 w1 = 1.0 + f * f * (-2.5 + 1.5 * f);
  vec2 w2 = f * (0.5 + f * (2.0 - 1.5 * f));
  vec2 w3 = f * f * (-0.5 + 0.5 * f);

  vec2 w12 = w1 + w2;
  vec2 offset12 = w2 / max(w12, vec2(1e-5));

  vec2 texPos0  = (texPos1 - 1.0) * texel;
  vec2 texPos3  = (texPos1 + 2.0) * texel;
  vec2 texPos12 = (texPos1 + offset12) * texel;

  vec3 r = vec3(0.0);
  r += texture(tex, vec2(texPos0.x,  texPos0.y )).rgb * w0.x  * w0.y;
  r += texture(tex, vec2(texPos12.x, texPos0.y )).rgb * w12.x * w0.y;
  r += texture(tex, vec2(texPos3.x,  texPos0.y )).rgb * w3.x  * w0.y;
  r += texture(tex, vec2(texPos0.x,  texPos12.y)).rgb * w0.x  * w12.y;
  r += texture(tex, vec2(texPos12.x, texPos12.y)).rgb * w12.x * w12.y;
  r += texture(tex, vec2(texPos3.x,  texPos12.y)).rgb * w3.x  * w12.y;
  r += texture(tex, vec2(texPos0.x,  texPos3.y )).rgb * w0.x  * w3.y;
  r += texture(tex, vec2(texPos12.x, texPos3.y )).rgb * w12.x * w3.y;
  r += texture(tex, vec2(texPos3.x,  texPos3.y )).rgb * w3.x  * w3.y;
  return max(r, vec3(0.0));   // i pesi negativi possono sforare sotto zero
}

// RCAS: nitidezza limitata dai vicini, cosi' non nascono aloni ne' pixel
// bruciati. Il limite e' il minimo e massimo della croce di 4 vicini.
vec3 sharpenRCAS(sampler2D tex, vec2 uv, vec2 size, vec3 center, float amount) {
  vec2 t = 1.0 / size;
  vec3 n = texture(tex, uv + vec2(0.0, -t.y)).rgb;
  vec3 s = texture(tex, uv + vec2(0.0,  t.y)).rgb;
  vec3 w = texture(tex, uv + vec2(-t.x, 0.0)).rgb;
  vec3 e = texture(tex, uv + vec2( t.x, 0.0)).rgb;

  vec3 mn = min(min(min(n, s), min(w, e)), center);
  vec3 mx = max(max(max(n, s), max(w, e)), center);

  vec3 sharp = center * (1.0 + 4.0 * amount) - (n + s + w + e) * amount;
  return clamp(sharp, mn, mx);
}

void main(){
  vec3 scene;
  if (uUpscale > 0.5) {
    scene = sampleCatmullRom(uScene, vUV, uSceneSize);
    if (uSharp > 0.001)
      scene = sharpenRCAS(uScene, vUV, uSceneSize, scene, uSharp * 0.25);
  } else {
    scene = texture(uScene, vUV).rgb;
  }
  vec3 bloom = texture(uBloom, vUV).rgb;
  FragColor = vec4(scene + bloom * uIntensity, 1.0);
})";

        brightProg = link(compile(GL_VERTEX_SHADER, fsQuadVS), compile(GL_FRAGMENT_SHADER, brightFS));
        blurProg = link(compile(GL_VERTEX_SHADER, fsQuadVS), compile(GL_FRAGMENT_SHADER, blurFS));
        compositeProg = link(compile(GL_VERTEX_SHADER, fsQuadVS), compile(GL_FRAGMENT_SHADER, compositeFS));

        bool targetsOk = createTargets(w, h);
        ready = targetsOk && brightProg && blurProg && compositeProg && quadVao;
        return ready;
    }

    void resizeIfNeeded(int w, int h) {
        if (w == fullW && h == fullH) return;
        destroyTargets();
        createTargets(w, h);
    }

    void beginScene(int w, int h) {
        if (!ready) return;
        resizeIfNeeded(w, h);
        glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo);
        glViewport(0, 0, fullW, fullH);
    }

    void drawQuad() {
        glBindVertexArray(quadVao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    // Estrae le zone sopra soglia dalla scena appena disegnata, le sfoca
    // (ping-pong orizzontale/verticale a risoluzione dimezzata) e le somma
    // sopra l'immagine originale nel framebuffer reale (screenFbo=0).
    // Soglia tarata sul canale massimo (non la luminanza) cosi' un Neon
    // saturo qualsiasi (rosso, blu, verde...) supera la soglia allo stesso
    // modo; 0.95-1.5 lascia fuori una superficie chiara in pieno sole
    // (~0.9-1.0 di picco) ma cattura qualunque emissivo (Neon/ForceField
    // arrivano gia' oltre 1.0 anche con colori tenui).
    void endSceneAndComposite(int screenW, int screenH, float intensity = 1.6f,
                              float thLow = 0.95f, float thHigh = 1.5f,
                              float sharpen = 0.6f) {
        if (!ready) {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, screenW, screenH);
            return;
        }
        // Anti-lag: intensity 0 vuol dire "niente alone". Si copia la scena
        // sullo schermo e si saltano i 6 passaggi di blur, che sono il costo
        // vero del post. Il framebuffer della scena va comunque risolto,
        // altrimenti il frame resta nel target offscreen.
        if (intensity <= 0.f) {
            glDisable(GL_DEPTH_TEST);
            glDepthMask(GL_FALSE);
            glDisable(GL_CULL_FACE);
            glDisable(GL_BLEND);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, screenW, screenH);
            glUseProgram(compositeProg);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, sceneColorTex);
            glUniform1i(glGetUniformLocation(compositeProg, "uScene"), 0);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, sceneColorTex);
            glUniform1i(glGetUniformLocation(compositeProg, "uBloom"), 1);
            glUniform1f(glGetUniformLocation(compositeProg, "uIntensity"), 0.f);
            const bool upscaling = (fullW < screenW || fullH < screenH);
            glUniform2f(glGetUniformLocation(compositeProg, "uSceneSize"),
                        (float)fullW, (float)fullH);
            glUniform1f(glGetUniformLocation(compositeProg, "uUpscale"), upscaling ? 1.f : 0.f);
            glUniform1f(glGetUniformLocation(compositeProg, "uSharp"), upscaling ? sharpen : 0.f);
            drawQuad();
            glActiveTexture(GL_TEXTURE0);
            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE);
            return;
        }
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);
        glDisable(GL_BLEND);

        glBindFramebuffer(GL_FRAMEBUFFER, brightFbo);
        glViewport(0, 0, halfW, halfH);
        glUseProgram(brightProg);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sceneColorTex);
        glUniform1i(glGetUniformLocation(brightProg, "uScene"), 0);
        glUniform1f(glGetUniformLocation(brightProg, "uThLow"), thLow);
        glUniform1f(glGetUniformLocation(brightProg, "uThHigh"), thHigh);
        drawQuad();

        GLuint srcTex = brightTex;
        glUseProgram(blurProg);
        const int kPasses = 6; // 3 giri orizzontale+verticale: alone morbido e ampio
        for (int i = 0; i < kPasses; ++i) {
            int dst = i % 2;
            glBindFramebuffer(GL_FRAMEBUFFER, pingFbo[dst]);
            glViewport(0, 0, halfW, halfH);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, srcTex);
            glUniform1i(glGetUniformLocation(blurProg, "uTex"), 0);
            glm::vec2 texel = (i % 2 == 0) ? glm::vec2(1.f / halfW, 0.f) : glm::vec2(0.f, 1.f / halfH);
            glUniform2fv(glGetUniformLocation(blurProg, "uTexel"), 1, glm::value_ptr(texel));
            drawQuad();
            srcTex = pingTex[dst];
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, screenW, screenH);
        glUseProgram(compositeProg);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sceneColorTex);
        glUniform1i(glGetUniformLocation(compositeProg, "uScene"), 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, srcTex);
        glUniform1i(glGetUniformLocation(compositeProg, "uBloom"), 1);
        glUniform1f(glGetUniformLocation(compositeProg, "uIntensity"), intensity);

        // La ricostruzione serve solo se la scena e' piu' piccola dello
        // schermo. Confrontare le dimensioni vere evita di pagare nove
        // prelievi per niente quando si gioca a risoluzione piena.
        const bool upscaling = (fullW < screenW || fullH < screenH);
        glUniform2f(glGetUniformLocation(compositeProg, "uSceneSize"),
                    (float)fullW, (float)fullH);
        glUniform1f(glGetUniformLocation(compositeProg, "uUpscale"), upscaling ? 1.f : 0.f);
        glUniform1f(glGetUniformLocation(compositeProg, "uSharp"), upscaling ? sharpen : 0.f);
        drawQuad();

        glActiveTexture(GL_TEXTURE0);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
    }

    void destroy() {
        destroyTargets();
        if (brightProg) glDeleteProgram(brightProg);
        if (blurProg) glDeleteProgram(blurProg);
        if (compositeProg) glDeleteProgram(compositeProg);
        if (quadVao) glDeleteVertexArrays(1, &quadVao);
        if (quadVbo) glDeleteBuffers(1, &quadVbo);
        brightProg = blurProg = compositeProg = quadVao = quadVbo = 0;
        ready = false;
    }
};
