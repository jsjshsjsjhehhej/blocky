# Incorpora i file Luau del motore (src/engine/lua/*.luau) nel programma:
# genera <OUT_DIR>/blocky_lua_embedded.h con una stringa per file.
# Lo usano sia la build Windows (CMakeLists.txt) sia quella Android.
# Modificare un .luau rifa' la configurazione (CMAKE_CONFIGURE_DEPENDS); in
# sviluppo pero' non serve ricompilare: il motore li legge dal disco
# (src/engine/LuaPrelude.h, devDir).

function(blocky_embed_lua LUA_DIR OUT_DIR)
    file(GLOB_RECURSE LUA_FILES CONFIGURE_DEPENDS "${LUA_DIR}/*.luau")
    list(SORT LUA_FILES)
    set(body "")
    foreach(f ${LUA_FILES})
        file(RELATIVE_PATH rel "${LUA_DIR}" "${f}")
        file(READ "${f}" code)
        string(FIND "${code}" ")BLKLUA\"" bad)
        if(NOT bad EQUAL -1)
            message(FATAL_ERROR "${rel}: contiene )BLKLUA\" (rompe la stringa C++)")
        endif()
        string(APPEND body "        {\"${rel}\", R\"BLKLUA(${code})BLKLUA\"},\n")
    endforeach()
    set(content "// Generato da cmake/BlockyEmbedLua.cmake: non modificare (cambia src/engine/lua/).
#pragma once
namespace blk {
namespace luaembedded {
struct File { const char* name; const char* code; };
inline const File* files(int& n) {
    static const File f[] = {
${body}        {nullptr, nullptr}
    };
    n = (int)(sizeof(f) / sizeof(f[0])) - 1;
    return f;
}
} // namespace luaembedded
} // namespace blk
")
    file(MAKE_DIRECTORY "${OUT_DIR}")
    file(WRITE "${OUT_DIR}/blocky_lua_embedded.h.tmp" "${content}")
    # solo se cambia: non ricompila tutto per niente
    configure_file("${OUT_DIR}/blocky_lua_embedded.h.tmp" "${OUT_DIR}/blocky_lua_embedded.h" COPYONLY)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${LUA_FILES})
endfunction()
