// Prova isolata di Luau, separata da LuaRuntime.h: compila ed esegue uno
// script minimo per verificare che l'integrazione (fetch CMake + link)
// funzioni davvero, prima di toccare il motore di scripting vero.
#include <lua.h>
#include <lualib.h>
#include <Luau/Compiler.h>

#include <cstdio>
#include <string>

int main() {
    lua_State* L = luaL_newstate();
    if (!L) {
        std::fprintf(stderr, "luaL_newstate fallita\n");
        return 1;
    }
    luaL_openlibs(L);

    const std::string source =
        "local x = 10\n"
        "local y = 20\n"
        "print('ciao da Luau, x+y =', x + y)\n"
        "return x + y\n";

    const std::string bytecode = Luau::compile(source);

    if (luau_load(L, "=test_luau", bytecode.data(), bytecode.size(), 0) != 0) {
        std::fprintf(stderr, "luau_load fallito: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }

    if (lua_pcall(L, 0, LUA_MULTRET, 0) != 0) {
        std::fprintf(stderr, "esecuzione fallita: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }

    if (lua_isnumber(L, -1))
        std::printf("risultato numerico: %g\n", lua_tonumber(L, -1));

    std::printf("Luau smoke test: OK\n");
    lua_close(L);
    return 0;
}
