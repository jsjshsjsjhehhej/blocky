#pragma once

#include <GLFW/glfw3.h>
#include <cstdio>
#include <string>
#include <vector>

#include "stb_image.h"

#include "platform/Platform.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#ifndef GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#endif
#include <GLFW/glfw3native.h>
#endif

// Quinta copia di "dov'e' l'eseguibile", e l'ultima dentro libblocky. Il ramo
// non-Windows rispondeva "." — cioe' la cartella di lavoro di turno, che non
// e' dove stanno le icone. plat::selfDir() lo sa su tutte le piattaforme.
inline std::string iconBaseDir() { return plat::selfDir(); }

#ifdef _WIN32
inline bool fileExistsA(const std::string& path) {
    DWORD a = GetFileAttributesA(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

inline HICON loadIconFile(const std::string& path, int cx, int cy) {
    if (!fileExistsA(path)) return nullptr;
    int n = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    if (n <= 0) return nullptr;
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, w.data(), n);
    return (HICON)LoadImageW(nullptr, w.c_str(), IMAGE_ICON, cx, cy,
                             LR_LOADFROMFILE | LR_DEFAULTCOLOR);
}

inline void applyNativeExeIcon(HWND hwnd) {
    if (!hwnd) return;
    HINSTANCE inst = GetModuleHandleW(nullptr);
    const int cx = GetSystemMetrics(SM_CXICON);
    const int cy = GetSystemMetrics(SM_CYICON);
    const int cxs = GetSystemMetrics(SM_CXSMICON);
    const int cys = GetSystemMetrics(SM_CYSMICON);

    HICON big = nullptr;
    HICON small = nullptr;

    for (int id : {1, 101}) {
        if (!big) {
            big = (HICON)LoadImageW(inst, MAKEINTRESOURCEW(id), IMAGE_ICON, cx, cy, LR_DEFAULTCOLOR);
            if (!big) big = LoadIconW(inst, MAKEINTRESOURCEW(id));
        }
        if (!small) {
            small = (HICON)LoadImageW(inst, MAKEINTRESOURCEW(id), IMAGE_ICON, cxs, cys, LR_DEFAULTCOLOR);
        }
        if (big && small) break;
    }

    std::string base = iconBaseDir();
    const std::string icoPaths[] = {
        base + "\\assets\\icon\\app.ico",
        base + "\\app.ico",
        "assets\\icon\\app.ico",
    };
    for (const auto& p : icoPaths) {
        if (!big) big = loadIconFile(p, cx, cy);
        if (!small) small = loadIconFile(p, cxs, cys);
        if (big && small) break;
    }

    if (!big) {
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        big = (HICON)ExtractIconW(inst, path, 0);
    }
    if (!small) small = big;

    if (big) {
        SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)big);
        SetClassLongPtrW(hwnd, GCLP_HICON, (LONG_PTR)big);
    }
    if (small) {
        SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)small);
        SetClassLongPtrW(hwnd, GCLP_HICONSM, (LONG_PTR)small);
    }
}
#endif

inline void setWindowIconB(GLFWwindow* win) {
    if (!win) return;

    std::string base = iconBaseDir();
    std::vector<std::string> paths = {
        base + "/assets/icon/b_icon.png",
        base + "\\assets\\icon\\b_icon.png",
        "assets/icon/b_icon.png",
    };

    bool loadedPng = false;
    for (const auto& path : paths) {
        stbi_set_flip_vertically_on_load(false);
        int w = 0, h = 0, ch = 0;
        unsigned char* px = stbi_load(path.c_str(), &w, &h, &ch, 4);
        if (!px || w <= 0 || h <= 0) {
            if (px) stbi_image_free(px);
            continue;
        }
        GLFWimage img{ w, h, px };
        glfwSetWindowIcon(win, 1, &img);
        stbi_image_free(px);
        loadedPng = true;
        break;
    }

#ifdef _WIN32
    applyNativeExeIcon(glfwGetWin32Window(win));
#endif

    if (!loadedPng)
        std::fprintf(stderr, "Avviso: icona Blocky non trovata (assets/icon/b_icon.png)\n");
}

inline void setWindowIconR(GLFWwindow* win) { setWindowIconB(win); }
