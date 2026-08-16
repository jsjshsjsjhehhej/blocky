#pragma once

/**
 * Input controller USB — Xbox (XInput) + DualSense/PS5 (GLFW + fallback raw).
 * Senza UI dedicata. Hotplug: collega/scollega in qualsiasi momento.
 *
 * LS muovi · RS guarda · A/Cross salta · Y/Triangle Shift Lock
 * RT/R2 sprint · Start/Options menu · LB/RB camera Studio
 */

#include <GLFW/glfw3.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cctype>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <xinput.h>
#endif

enum class GamepadKind {
    None = 0,
    Xbox,
    DualSense,
    Generic
};

struct GamepadState {
    bool connected = false;
    int  jid = -1;           // GLFW joystick id, o -1 se solo XInput
    int  xinputIndex = -1;   // 0..3 se da XInput
    GamepadKind kind = GamepadKind::None;
    char name[96] = {};

    float lx = 0, ly = 0;
    float rx = 0, ry = 0;
    float lt = 0, rt = 0;

    bool a = false, b = false, x = false, y = false;
    bool lb = false, rb = false;
    bool back = false, start = false;
    bool dpadU = false, dpadD = false, dpadL = false, dpadR = false;
    bool l3 = false, r3 = false;

    bool aPressed = false, bPressed = false, xPressed = false, yPressed = false;
    bool startPressed = false, backPressed = false;
    bool dpadUPressed = false, dpadDPressed = false;
    bool dpadLPressed = false, dpadRPressed = false;
    bool lbPressed = false, rbPressed = false; // L1 / R1
    bool ltPressed = false, rtPressed = false; // L2 / R2 (edge)

    bool prevA = false, prevB = false, prevX = false, prevY = false;
    bool prevStart = false, prevBack = false;
    bool prevDU = false, prevDD = false, prevDL = false, prevDR = false;
    bool prevLb = false, prevRb = false;
    float prevLt = 0.f, prevRt = 0.f;
};

inline float gpDeadzone(float v, float dz = 0.12f) {
    if (std::fabs(v) < dz) return 0.f;
    float s = (v > 0.f) ? 1.f : -1.f;
    return s * (std::fabs(v) - dz) / (1.f - dz);
}

inline GamepadState& gamepad() {
    static GamepadState s;
    return s;
}

inline bool gpContainsI(const char* hay, const char* needle) {
    if (!hay || !needle) return false;
    for (const char* h = hay; *h; ++h) {
        const char* a = h;
        const char* b = needle;
        while (*a && *b && (std::tolower((unsigned char)*a) == std::tolower((unsigned char)*b))) {
            ++a; ++b;
        }
        if (!*b) return true;
    }
    return false;
}

inline bool gpIsSonyGuid(const char* guid) {
    // Vendor Sony = 0x054C → bytes "4c05" in GLFW GUID (little-endian hex)
    return guid && std::strstr(guid, "4c05") != nullptr;
}

inline bool gpIsMicrosoftGuid(const char* guid) {
    return guid && std::strstr(guid, "5e04") != nullptr; // 0x045E
}

inline GamepadKind gpDetectKind(const char* name, const char* guid) {
    if (gpIsSonyGuid(guid) || gpContainsI(name, "dualsense") || gpContainsI(name, "ps5")
        || gpContainsI(name, "wireless controller") || gpContainsI(name, "playstation"))
        return GamepadKind::DualSense;
    if (gpIsMicrosoftGuid(guid) || gpContainsI(name, "xbox") || gpContainsI(name, "xinput")
        || gpContainsI(name, "compatible game controller"))
        return GamepadKind::Xbox;
    return GamepadKind::Generic;
}

/** Mapping SDL aggiornati (Windows) — DualSense + Xbox Series + Edge. */
inline void gpLoadBuiltinMappings() {
    static const char* kMaps =
        // DualSense USB / Edge
        "030000004c050000e60c000000000000,PS5 Controller,a:b1,b:b2,back:b8,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b12,leftshoulder:b4,leftstick:b10,lefttrigger:a3,leftx:a0,lefty:a1,misc1:b14,rightshoulder:b5,rightstick:b11,righttrigger:a4,rightx:a2,righty:a5,start:b9,touchpad:b13,x:b0,y:b3,platform:Windows,\n"
        "030000004c050000f20d000000000000,PS5 Controller,a:b1,b:b2,back:b8,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b12,leftshoulder:b4,leftstick:b10,lefttrigger:a3,leftx:a0,lefty:a1,misc1:b14,rightshoulder:b5,rightstick:b11,righttrigger:a4,rightx:a2,righty:a5,start:b9,touchpad:b13,x:b0,y:b3,platform:Windows,\n"
        "030000004c050000e60c000000010000,PS5 Controller,a:b1,b:b2,back:b8,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b12,leftshoulder:b4,leftstick:b10,lefttrigger:a3,leftx:a0,lefty:a1,misc1:b14,rightshoulder:b5,rightstick:b11,righttrigger:a4,rightx:a2,righty:a5,start:b9,touchpad:b13,x:b0,y:b3,platform:Windows,\n"
        "030000004c050000f20d000000010000,PS5 Controller,a:b1,b:b2,back:b8,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b12,leftshoulder:b4,leftstick:b10,lefttrigger:a3,leftx:a0,lefty:a1,misc1:b14,rightshoulder:b5,rightstick:b11,righttrigger:a4,rightx:a2,righty:a5,start:b9,touchpad:b13,x:b0,y:b3,platform:Windows,\n"
        // Xbox Series / One (DirectInput)
        "030000005e040000130b000000000000,Xbox Series Controller,a:b0,b:b1,back:b6,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b10,leftshoulder:b4,leftstick:b8,lefttrigger:a2,leftx:a0,lefty:a1,rightshoulder:b5,rightstick:b9,righttrigger:a5,rightx:a3,righty:a4,start:b7,x:b2,y:b3,platform:Windows,\n"
        "030000005e040000120b000000000000,Xbox Series Controller,a:b0,b:b1,back:b6,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b10,leftshoulder:b4,leftstick:b8,lefttrigger:a2,leftx:a0,lefty:a1,rightshoulder:b5,rightstick:b9,righttrigger:a5,rightx:a3,righty:a4,start:b7,x:b2,y:b3,platform:Windows,\n"
        "030000005e040000fd02000000000000,Xbox One Controller,a:b0,b:b1,back:b6,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b8,leftshoulder:b4,leftstick:b9,lefttrigger:a2,leftx:a0,lefty:a1,rightshoulder:b5,rightstick:b10,righttrigger:a5,rightx:a3,righty:a4,start:b7,x:b2,y:b3,platform:Windows,\n"
        "030000005e040000e002000000000000,Xbox One Controller,a:b0,b:b1,back:b6,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b8,leftshoulder:b4,leftstick:b9,lefttrigger:a2,leftx:a0,lefty:a1,rightshoulder:b5,rightstick:b10,righttrigger:a5,rightx:a3,righty:a4,start:b7,x:b2,y:b3,platform:Windows,\n"
        // XInput virtual GUIDs (GLFW)
        "78696e70757401000000000000000000,XInput Gamepad (GLFW),a:b0,b:b1,back:b6,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b10,leftshoulder:b4,leftstick:b8,lefttrigger:a4,leftx:a0,lefty:a1,rightshoulder:b5,rightstick:b9,righttrigger:a5,rightx:a2,righty:a3,start:b7,x:b2,y:b3,platform:Windows,\n";
    glfwUpdateGamepadMappings(kMaps);
}

/** Se il GUID non e' in DB, registra mapping DualSense/Xbox generico con quel GUID. */
inline void gpEnsureMapped(int jid) {
    if (!glfwJoystickPresent(jid)) return;
    if (glfwJoystickIsGamepad(jid)) return;
    const char* guid = glfwGetJoystickGUID(jid);
    const char* name = glfwGetJoystickName(jid);
    if (!guid || !guid[0]) return;

    GamepadKind k = gpDetectKind(name, guid);
    char line[512];
    if (k == GamepadKind::DualSense) {
        // Layout HID DualSense tipico su Windows USB
        std::snprintf(line, sizeof(line),
            "%s,DualSense USB,a:b1,b:b2,back:b8,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,"
            "guide:b12,leftshoulder:b4,leftstick:b10,lefttrigger:a3,leftx:a0,lefty:a1,"
            "rightshoulder:b5,rightstick:b11,righttrigger:a4,rightx:a2,righty:a5,start:b9,"
            "x:b0,y:b3,platform:Windows,",
            guid);
        glfwUpdateGamepadMappings(line);
        std::printf("[Pad] Mapping DualSense registrato GUID=%s name=%s\n", guid, name ? name : "?");
    } else if (k == GamepadKind::Xbox || k == GamepadKind::Generic) {
        std::snprintf(line, sizeof(line),
            "%s,USB Gamepad,a:b0,b:b1,back:b6,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,"
            "guide:b8,leftshoulder:b4,leftstick:b8,lefttrigger:a2,leftx:a0,lefty:a1,"
            "rightshoulder:b5,rightstick:b9,righttrigger:a5,rightx:a3,righty:a4,start:b7,"
            "x:b2,y:b3,platform:Windows,",
            guid);
        glfwUpdateGamepadMappings(line);
        std::printf("[Pad] Mapping generico registrato GUID=%s name=%s\n", guid, name ? name : "?");
    }
}

inline void gpFinishEdges(GamepadState& g) {
    g.aPressed = g.a && !g.prevA;
    g.bPressed = g.b && !g.prevB;
    g.xPressed = g.x && !g.prevX;
    g.yPressed = g.y && !g.prevY;
    g.startPressed = g.start && !g.prevStart;
    g.backPressed = g.back && !g.prevBack;
    g.dpadUPressed = g.dpadU && !g.prevDU;
    g.dpadDPressed = g.dpadD && !g.prevDD;
    g.dpadLPressed = g.dpadL && !g.prevDL;
    g.dpadRPressed = g.dpadR && !g.prevDR;
    g.lbPressed = g.lb && !g.prevLb;
    g.rbPressed = g.rb && !g.prevRb;
    g.ltPressed = (g.lt > 0.55f) && (g.prevLt <= 0.55f);
    g.rtPressed = (g.rt > 0.55f) && (g.prevRt <= 0.55f);

    g.prevA = g.a; g.prevB = g.b; g.prevX = g.x; g.prevY = g.y;
    g.prevStart = g.start; g.prevBack = g.back;
    g.prevDU = g.dpadU; g.prevDD = g.dpadD;
    g.prevDL = g.dpadL; g.prevDR = g.dpadR;
    g.prevLb = g.lb; g.prevRb = g.rb;
    g.prevLt = g.lt; g.prevRt = g.rt;
}

inline void gpClear(GamepadState& g) {
    g.connected = false;
    g.jid = -1;
    g.xinputIndex = -1;
    g.kind = GamepadKind::None;
    g.name[0] = 0;
    g.lx = g.ly = g.rx = g.ry = g.lt = g.rt = 0.f;
    g.a = g.b = g.x = g.y = false;
    g.lb = g.rb = g.back = g.start = false;
    g.dpadU = g.dpadD = g.dpadL = g.dpadR = false;
    g.l3 = g.r3 = false;
    g.aPressed = g.bPressed = g.xPressed = g.yPressed = false;
    g.startPressed = g.backPressed = false;
    g.dpadUPressed = g.dpadDPressed = g.dpadLPressed = g.dpadRPressed = false;
    g.lbPressed = g.rbPressed = g.ltPressed = g.rtPressed = false;
    // non azzerare prev* — evita edge spurii al riconnetto; reset soft:
    g.prevA = g.prevB = g.prevX = g.prevY = false;
    g.prevStart = g.prevBack = false;
    g.prevDU = g.prevDD = g.prevDL = g.prevDR = false;
    g.prevLb = g.prevRb = false;
    g.prevLt = g.prevRt = 0.f;
}

inline float gpAxisOr(const float* axes, int count, int i, float def = 0.f) {
    return (axes && i >= 0 && i < count) ? axes[i] : def;
}

inline bool gpBtnOr(const unsigned char* btns, int count, int i) {
    return btns && i >= 0 && i < count && btns[i] == GLFW_PRESS;
}

/** Fallback raw: DualSense HID o generico Xbox-like. */
inline bool gpPollRawJoystick(int jid, GamepadState& g) {
    if (!glfwJoystickPresent(jid)) return false;
    int axisCount = 0, btnCount = 0, hatCount = 0;
    const float* axes = glfwGetJoystickAxes(jid, &axisCount);
    const unsigned char* btns = glfwGetJoystickButtons(jid, &btnCount);
    const unsigned char* hats = glfwGetJoystickHats(jid, &hatCount);
    if (!axes || axisCount < 2) return false;

    const char* name = glfwGetJoystickName(jid);
    const char* guid = glfwGetJoystickGUID(jid);
    g.kind = gpDetectKind(name, guid);
    g.jid = jid;
    g.xinputIndex = -1;
    g.connected = true;
    if (name) {
        std::strncpy(g.name, name, sizeof(g.name) - 1);
        g.name[sizeof(g.name) - 1] = 0;
    } else {
        std::snprintf(g.name, sizeof(g.name), "USB Controller");
    }

    if (g.kind == GamepadKind::DualSense) {
        // DualSense DirectInput tipico: LX LY RX L2 R2 RY
        g.lx = gpDeadzone(gpAxisOr(axes, axisCount, 0));
        g.ly = gpDeadzone(-gpAxisOr(axes, axisCount, 1));
        g.rx = gpDeadzone(gpAxisOr(axes, axisCount, 2), 0.16f);
        float l2 = gpAxisOr(axes, axisCount, 3, -1.f);
        float r2 = gpAxisOr(axes, axisCount, 4, -1.f);
        g.ry = gpDeadzone(-gpAxisOr(axes, axisCount, (axisCount > 5) ? 5 : 3), 0.16f);
        g.lt = (l2 + 1.f) * 0.5f;
        g.rt = (r2 + 1.f) * 0.5f;
        // Bottoni Sony: Square Cross Circle Triangle
        g.x = gpBtnOr(btns, btnCount, 0); // Square → X
        g.a = gpBtnOr(btns, btnCount, 1); // Cross → A
        g.b = gpBtnOr(btns, btnCount, 2); // Circle → B
        g.y = gpBtnOr(btns, btnCount, 3); // Triangle → Y
        g.lb = gpBtnOr(btns, btnCount, 4);
        g.rb = gpBtnOr(btns, btnCount, 5);
        g.back = gpBtnOr(btns, btnCount, 8);  // Create/Share
        g.start = gpBtnOr(btns, btnCount, 9); // Options
        g.l3 = gpBtnOr(btns, btnCount, 10);
        g.r3 = gpBtnOr(btns, btnCount, 11);
    } else {
        // Xbox-like DirectInput
        g.lx = gpDeadzone(gpAxisOr(axes, axisCount, 0));
        g.ly = gpDeadzone(-gpAxisOr(axes, axisCount, 1));
        if (axisCount >= 6) {
            g.rx = gpDeadzone(gpAxisOr(axes, axisCount, 3), 0.16f);
            g.ry = gpDeadzone(-gpAxisOr(axes, axisCount, 4), 0.16f);
            float t = gpAxisOr(axes, axisCount, 2); // spesso LT/RT combinati
            g.lt = std::fabs(t) > 0.05f && t < 0.f ? -t : 0.f;
            g.rt = std::fabs(t) > 0.05f && t > 0.f ? t : ((axisCount > 5) ? (gpAxisOr(axes, axisCount, 5) + 1.f) * 0.5f : 0.f);
            if (axisCount > 5) {
                float rtA = gpAxisOr(axes, axisCount, 5);
                if (rtA >= -1.f && rtA <= 1.f) g.rt = (rtA + 1.f) * 0.5f;
                float ltA = gpAxisOr(axes, axisCount, 2);
                if (ltA >= -1.f && ltA <= 1.f) g.lt = (ltA + 1.f) * 0.5f;
            }
        } else {
            g.rx = gpDeadzone(gpAxisOr(axes, axisCount, 2), 0.16f);
            g.ry = gpDeadzone(-gpAxisOr(axes, axisCount, 3), 0.16f);
        }
        g.a = gpBtnOr(btns, btnCount, 0);
        g.b = gpBtnOr(btns, btnCount, 1);
        g.x = gpBtnOr(btns, btnCount, 2);
        g.y = gpBtnOr(btns, btnCount, 3);
        g.lb = gpBtnOr(btns, btnCount, 4);
        g.rb = gpBtnOr(btns, btnCount, 5);
        g.back = gpBtnOr(btns, btnCount, 6);
        g.start = gpBtnOr(btns, btnCount, 7);
        g.l3 = gpBtnOr(btns, btnCount, 8);
        g.r3 = gpBtnOr(btns, btnCount, 9);
    }

    if (g.lt < 0.08f) g.lt = 0.f;
    if (g.rt < 0.08f) g.rt = 0.f;

    if (hats && hatCount > 0) {
        unsigned char h = hats[0];
        g.dpadU = (h & GLFW_HAT_UP) != 0;
        g.dpadD = (h & GLFW_HAT_DOWN) != 0;
        g.dpadL = (h & GLFW_HAT_LEFT) != 0;
        g.dpadR = (h & GLFW_HAT_RIGHT) != 0;
    }

    gpFinishEdges(g);
    return true;
}

inline bool gpPollGlfwGamepad(int jid, GamepadState& g) {
    gpEnsureMapped(jid);
    if (!glfwJoystickIsGamepad(jid)) return false;
    GLFWgamepadstate st{};
    if (glfwGetGamepadState(jid, &st) != GLFW_TRUE) return false;

    const char* name = glfwGetGamepadName(jid);
    if (!name) name = glfwGetJoystickName(jid);
    const char* guid = glfwGetJoystickGUID(jid);

    g.connected = true;
    g.jid = jid;
    g.xinputIndex = -1;
    g.kind = gpDetectKind(name, guid);
    if (name) {
        std::strncpy(g.name, name, sizeof(g.name) - 1);
        g.name[sizeof(g.name) - 1] = 0;
    }

    g.lx = gpDeadzone(st.axes[GLFW_GAMEPAD_AXIS_LEFT_X]);
    g.ly = gpDeadzone(-st.axes[GLFW_GAMEPAD_AXIS_LEFT_Y]);
    g.rx = gpDeadzone(st.axes[GLFW_GAMEPAD_AXIS_RIGHT_X], 0.16f);
    g.ry = gpDeadzone(-st.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y], 0.16f);
    g.lt = (st.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] + 1.f) * 0.5f;
    g.rt = (st.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] + 1.f) * 0.5f;
    if (g.lt < 0.08f) g.lt = 0.f;
    if (g.rt < 0.08f) g.rt = 0.f;

    g.a = st.buttons[GLFW_GAMEPAD_BUTTON_A] == GLFW_PRESS;
    g.b = st.buttons[GLFW_GAMEPAD_BUTTON_B] == GLFW_PRESS;
    g.x = st.buttons[GLFW_GAMEPAD_BUTTON_X] == GLFW_PRESS;
    g.y = st.buttons[GLFW_GAMEPAD_BUTTON_Y] == GLFW_PRESS;
    g.lb = st.buttons[GLFW_GAMEPAD_BUTTON_LEFT_BUMPER] == GLFW_PRESS;
    g.rb = st.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER] == GLFW_PRESS;
    g.back = st.buttons[GLFW_GAMEPAD_BUTTON_BACK] == GLFW_PRESS;
    g.start = st.buttons[GLFW_GAMEPAD_BUTTON_START] == GLFW_PRESS;
    g.dpadU = st.buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP] == GLFW_PRESS;
    g.dpadD = st.buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN] == GLFW_PRESS;
    g.dpadL = st.buttons[GLFW_GAMEPAD_BUTTON_DPAD_LEFT] == GLFW_PRESS;
    g.dpadR = st.buttons[GLFW_GAMEPAD_BUTTON_DPAD_RIGHT] == GLFW_PRESS;
    g.l3 = st.buttons[GLFW_GAMEPAD_BUTTON_LEFT_THUMB] == GLFW_PRESS;
    g.r3 = st.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_THUMB] == GLFW_PRESS;

    gpFinishEdges(g);
    return true;
}

#ifdef _WIN32
inline float gpXInputStick(SHORT v) {
    const float dead = 7849.f; // ~XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE
    float f = (float)v;
    if (std::fabs(f) < dead) return 0.f;
    float s = (f > 0.f) ? 1.f : -1.f;
    return s * (std::fabs(f) - dead) / (32767.f - dead);
}

inline bool gpPollXInput(GamepadState& g) {
    for (DWORD i = 0; i < XUSER_MAX_COUNT; ++i) {
        XINPUT_STATE xs{};
        if (XInputGetState(i, &xs) != ERROR_SUCCESS) continue;
        const auto& p = xs.Gamepad;

        g.connected = true;
        g.jid = -1;
        g.xinputIndex = (int)i;
        g.kind = GamepadKind::Xbox;
        std::snprintf(g.name, sizeof(g.name), "Xbox Controller (XInput %u)", (unsigned)i);

        g.lx = gpXInputStick(p.sThumbLX);
        g.ly = gpXInputStick(p.sThumbLY);
        g.rx = gpXInputStick(p.sThumbRX);
        g.ry = gpXInputStick(p.sThumbRY);
        g.lt = p.bLeftTrigger / 255.f;
        g.rt = p.bRightTrigger / 255.f;
        if (g.lt < 0.08f) g.lt = 0.f;
        if (g.rt < 0.08f) g.rt = 0.f;

        g.a = (p.wButtons & XINPUT_GAMEPAD_A) != 0;
        g.b = (p.wButtons & XINPUT_GAMEPAD_B) != 0;
        g.x = (p.wButtons & XINPUT_GAMEPAD_X) != 0;
        g.y = (p.wButtons & XINPUT_GAMEPAD_Y) != 0;
        g.lb = (p.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0;
        g.rb = (p.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0;
        g.back = (p.wButtons & XINPUT_GAMEPAD_BACK) != 0;
        g.start = (p.wButtons & XINPUT_GAMEPAD_START) != 0;
        g.dpadU = (p.wButtons & XINPUT_GAMEPAD_DPAD_UP) != 0;
        g.dpadD = (p.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) != 0;
        g.dpadL = (p.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) != 0;
        g.dpadR = (p.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0;
        g.l3 = (p.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) != 0;
        g.r3 = (p.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) != 0;

        gpFinishEdges(g);
        return true;
    }
    return false;
}
#endif

inline void gpJoystickCallback(int jid, int event) {
    if (event == GLFW_CONNECTED) {
        const char* name = glfwGetJoystickName(jid);
        const char* guid = glfwGetJoystickGUID(jid);
        std::printf("[Pad] Connesso jid=%d name=%s GUID=%s gamepad=%d\n",
                    jid, name ? name : "?", guid ? guid : "?",
                    glfwJoystickIsGamepad(jid));
        gpLoadBuiltinMappings();
        gpEnsureMapped(jid);
        std::printf("[Pad] Dopo mapping: gamepad=%d name=%s\n",
                    glfwJoystickIsGamepad(jid),
                    glfwGetGamepadName(jid) ? glfwGetGamepadName(jid) : "?");
    } else if (event == GLFW_DISCONNECTED) {
        std::printf("[Pad] Sconnesso jid=%d\n", jid);
    }
}

/** Chiama dopo glfwInit(). */
inline void initGamepad() {
    gpLoadBuiltinMappings();
    glfwSetJoystickCallback(gpJoystickCallback);
    for (int jid = GLFW_JOYSTICK_1; jid <= GLFW_JOYSTICK_LAST; ++jid) {
        if (!glfwJoystickPresent(jid)) continue;
        gpEnsureMapped(jid);
        const char* name = glfwGetJoystickName(jid);
        const char* guid = glfwGetJoystickGUID(jid);
        std::printf("[Pad] Presente jid=%d name=%s GUID=%s isGamepad=%d\n",
                    jid, name ? name : "?", guid ? guid : "?",
                    glfwJoystickIsGamepad(jid));
    }
#ifdef _WIN32
    for (DWORD i = 0; i < XUSER_MAX_COUNT; ++i) {
        XINPUT_STATE xs{};
        if (XInputGetState(i, &xs) == ERROR_SUCCESS)
            std::printf("[Pad] XInput slot %u attivo\n", (unsigned)i);
    }
#endif
}

/** Poll ogni frame: prima USB GLFW (DualSense/PS5), poi XInput (Xbox). */
inline void pollGamepad() {
    auto& g = gamepad();
    static bool wasConnected = false;
    static char lastName[96] = {};

    // 1) DualSense / generici USB via GLFW (prima di XInput: evita pad fantasma XInput)
    for (int jid = GLFW_JOYSTICK_1; jid <= GLFW_JOYSTICK_LAST; ++jid) {
        if (!glfwJoystickPresent(jid)) continue;
        if (gpPollGlfwGamepad(jid, g)) {
            if (!wasConnected || std::strcmp(lastName, g.name) != 0) {
                std::printf("[Pad] Attivo: %s (GLFW)\n", g.name);
                std::strncpy(lastName, g.name, sizeof(lastName) - 1);
            }
            wasConnected = true;
            return;
        }
        if (gpPollRawJoystick(jid, g)) {
            if (!wasConnected || std::strcmp(lastName, g.name) != 0) {
                std::printf("[Pad] Attivo raw: %s lx=%.2f ly=%.2f\n", g.name, g.lx, g.ly);
                std::strncpy(lastName, g.name, sizeof(lastName) - 1);
            }
            wasConnected = true;
            return;
        }
    }

#ifdef _WIN32
    // 2) Xbox via XInput
    if (gpPollXInput(g)) {
        if (!wasConnected || std::strcmp(lastName, g.name) != 0) {
            std::printf("[Pad] Attivo: %s\n", g.name);
            std::strncpy(lastName, g.name, sizeof(lastName) - 1);
        }
        wasConnected = true;
        return;
    }
#endif

    if (wasConnected) {
        std::printf("[Pad] Nessun controller\n");
        lastName[0] = 0;
    }
    wasConnected = false;
    gpClear(g);
}
