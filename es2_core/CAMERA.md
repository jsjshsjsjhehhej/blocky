# Camera integration (short)

After creating the camera once:

```cpp
Camera cam;
cam.updateVectors(); // important on startup
```

Each frame (input → update → render):

```cpp
// 1) Look
if (mouseLooking)
    cam.rotate(mouseDx, mouseDy);   // updates vectors internally

// 2) Move (pick one style)
cam.moveWASD(dt, keyW, keyA, keyS, keyD);
// OR: cam.moveForward(dt); cam.moveLeft(dt); ...

// 3) View matrix for shader
Mat4 view = cam.viewMatrix();
Mat4 proj = perspective(cam.fov, aspect, 0.1f, 500.f);
// glUniformMatrix4fv(... view ...);
```

No SDL / no render code here — only pass `dx,dy` and key flags from your app.
