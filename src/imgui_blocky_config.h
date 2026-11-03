#pragma once

/**
 * Force-include su imgui_lib: se un IM_ASSERT scatta, salva
 * espressione/file/riga in %APPDATA%/Blocky/crashes/last_assert.txt.
 */

#ifdef __cplusplus
#include <cassert>
#include <cstdio>
#include <cstdlib>
#else
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#endif

#ifdef __cplusplus
inline void blockyImGuiAssertFail(const char* expr, const char* file, int line) {
    char outPath[512] = "last_assert.txt";
#ifdef _WIN32
    if (const char* appdata = std::getenv("APPDATA")) {
        if (appdata[0]) {
            std::snprintf(outPath, sizeof(outPath),
                          "%s\\Blocky\\crashes\\last_assert.txt", appdata);
        }
    }
#endif
    if (FILE* f = std::fopen(outPath, "w")) {
        std::fprintf(f, "IM_ASSERT failed\nExpression: %s\nFile: %s\nLine: %d\n",
                     expr ? expr : "?", file ? file : "?", line);
        std::fclose(f);
    }
    std::fprintf(stderr, "\nIM_ASSERT failed: %s\n  at %s:%d\n",
                 expr ? expr : "?", file ? file : "?", line);
    std::fflush(stderr);
    std::abort();
}
#else
static void blockyImGuiAssertFail(const char* expr, const char* file, int line) {
    fprintf(stderr, "\nIM_ASSERT failed: %s\n  at %s:%d\n",
            expr ? expr : "?", file ? file : "?", line);
    fflush(stderr);
    abort();
}
#endif

#ifndef IM_ASSERT
#define IM_ASSERT(_EXPR) \
    do { if (!!(_EXPR)) { } else { blockyImGuiAssertFail(#_EXPR, __FILE__, __LINE__); } } while (0)
#endif
