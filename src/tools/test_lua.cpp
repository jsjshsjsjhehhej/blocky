// Test rapido runtime Lua senza OpenGL
#include "engine/DataModel.h"
#include "engine/LuaRuntime.h"
#include "engine/GameHub.h"

#include <cstdio>
#include <string>

int main() {
    DataModel dm;
    dm.loadBaseplatePlace();
    LuaRuntime lua;
    if (!lua.init(&dm)) {
        std::fprintf(stderr, "FAIL init: %s\n", lua.lastError.c_str());
        return 1;
    }

    const char* scripts[] = {
        "scripts/baseplate.lua",
        "scripts/garden.lua",
        "scripts/obby.lua",
    };
    for (const char* rel : scripts) {
        dm.clear();
        dm.loadBaseplatePlace();
        lua.init(&dm);
        std::string path = resolveScriptPath(rel);
        std::fprintf(stdout, "RUN %s\n", path.c_str());
        if (!lua.runFile(path)) {
            std::fprintf(stderr, "FAIL %s: %s\n", rel, lua.lastError.c_str());
            return 2;
        }
        std::fprintf(stdout, "OK %s parts=%zu\n", rel, dm.parts.size());
        lua.fireHeartbeat(0.016f);
    }
    std::fprintf(stdout, "ALL LUA TESTS PASSED\n");
    return 0;
}
