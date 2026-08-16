#include "renderer.h"

void rendererBeginFrame() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void rendererDraw(const Shader& shader, const FrameUniforms& frame, const DrawItem& item) {
    if (!item.mesh || !shader.program) return;

    shaderUse(shader);

    // Matrices
    glUniformMatrix4fv(shader.uModel, 1, GL_FALSE, item.model.data());
    glUniformMatrix4fv(shader.uView,  1, GL_FALSE, frame.view.data());
    glUniformMatrix4fv(shader.uProj,  1, GL_FALSE, frame.proj.data());

    // Lighting / material
    Vec3 sun = normalize(frame.sunDir);
    glUniform3f(shader.uSunDir, sun.x, sun.y, sun.z);
    glUniform3f(shader.uCamPos, frame.camPos.x, frame.camPos.y, frame.camPos.z);
    glUniform3f(shader.uColor, item.color.x, item.color.y, item.color.z);

    meshBindAttributes(*item.mesh);
    meshDraw(*item.mesh);
    meshUnbindAttributes();
}

void rendererDrawSceneSkeleton(const Shader& plastic,
                               const FrameUniforms& frame,
                               const DrawItem* items,
                               int count) {
    rendererBeginFrame();
    for (int i = 0; i < count; ++i)
        rendererDraw(plastic, frame, items[i]);
}
