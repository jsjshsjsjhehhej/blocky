# ES2 Core — Roblox-2015 style building blocks (OpenGL ES 2.0 + SDL2)

Minimal modules only. No full demo / game.

## Layout

| File | Role |
|------|------|
| `math.h` | Vec3, Mat4, perspective, lookAt, translate, rotate |
| `window.h/.cpp` | SDL2 window + GLES 2.0 context, depth, sky clear |
| `shader.h/.cpp` | Compile/link + plastic brick shader (embedded) |
| `mesh.h/.cpp` | Cube brick + plane baseplate (VBO/IBO) |
| `camera.h/.cpp` | FPS camera, WASD, mouse/key look, view matrix |
| `renderer.h/.cpp` | Bind uniforms + draw mesh; frame skeleton |
| `shaders/plastic.vert` | Vertex shader source (file copy) |
| `shaders/plastic.frag` | Fragment shader source (file copy) |

## How modules connect

```
windowCreate()          → GLES2 context ready
shaderCreatePlastic()   → program + uniform locations
meshCreateCube/Plane()  → GPU buffers
Camera                  → viewMatrix() each frame
perspective()           → projection matrix
rendererBeginFrame()    → clear
rendererDraw()          → use shader, set MVP/light/color, draw
windowSwap()            → present
```

Typical one-frame skeleton (not a full app):

```cpp
FrameUniforms f;
f.view   = cam.viewMatrix();
f.proj   = perspective(cam.fov, aspect, 0.1f, 500.f);
f.camPos = cam.position;
f.sunDir = normalize(Vec3{-0.4f, 1.f, -0.3f});

DrawItem plate{ &planeMesh, multiply(translate({0,-1,0}), scale({100,1,100})), {0.39f,0.70f,0.30f} };
DrawItem brick{ &cubeMesh,  multiply(translate({0,2,0}), scale({4,4,2})),       {0.77f,0.16f,0.11f} };

rendererDrawSceneSkeleton(plastic, f, items, 2);
windowSwap(app);
```

## ES 2.0 note on VAO

Core OpenGL ES 2.0 has **no VAO**. `meshBindAttributes()` is the ES2-compatible equivalent of binding a VAO (VBO + IBO + `glVertexAttribPointer`).

## Dependencies

- SDL2
- OpenGL ES 2.0 headers (`GLES2/gl2.h`) — on desktop use ANGLE or a GLES2 loader
