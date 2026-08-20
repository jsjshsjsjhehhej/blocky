cmake_minimum_required(VERSION 3.20)
project(Roblox LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# --- Offuscamento a build time (solo distribuzione) ---
# Non tocca il sorgente. Toglie dal binario i nomi di funzioni e variabili:
# oggi Blocky.exe ne espone 64k leggibili con un editor di testo, ed e' da
# li' che si parte per capire e aggirare l'anti-cheat, non dal .cpp.
# Attivo di default; disattivabile con -DBLOCKY_STRIP=OFF per debuggare.
# Strip attivo SEMPRE tranne build Debug esplicita: la tua build tipica ha
# CMAKE_BUILD_TYPE vuoto, quindi legarlo a "Release" non basterebbe mai.
option(BLOCKY_STRIP "Strip dei simboli (tutto tranne Debug)" ON)
if(BLOCKY_STRIP AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_link_options(-s)
    message(STATUS "Blocky: strip simboli ATTIVO (build di distribuzione)")
else()
    message(STATUS "Blocky: strip simboli disattivo (build Debug o -DBLOCKY_STRIP=OFF)")
endif()

# Build di sviluppo = strip disattivo. Lì i log di sicurezza (LOG_SEC) sono
# attivi per debuggare l'anti-cheat; nella build di distribuzione spariscono
# dal binario, cosi' non svelano le difese. Un solo interruttore per entrambi.
if(NOT BLOCKY_STRIP OR CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(BLOCKY_DEV=1)
    message(STATUS "Blocky: build di SVILUPPO (LOG_SEC attivo)")
else()
    message(STATUS "Blocky: build di DISTRIBUZIONE (LOG_SEC rimosso dal binario)")
endif()

# Layer multi-piattaforma (PC / Mobile / iOS / PS5)
include(${CMAKE_SOURCE_DIR}/cmake/Platform.cmake)

include(FetchContent)

# ---- stb (caricamento texture PNG) ----
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(stb)

# ---- GLFW (finestra + input) ----
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# ---- GLM ----
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
)

# ---- GLAD (OpenGL loader) ----
FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG        v2.0.6
    SOURCE_SUBDIR  cmake
)

# ---- Assimp ----
FetchContent_Declare(
    assimp
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG        v5.4.3
)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_FBX_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glfw glm glad assimp)

# ---- Lua 5.4 (scripting) ----
FetchContent_Declare(
    lua
    GIT_REPOSITORY https://github.com/lua/lua.git
    GIT_TAG        v5.4.7
)
FetchContent_GetProperties(lua)
if(NOT lua_POPULATED)
    FetchContent_Populate(lua)
    set(LUA_CORE
        lapi.c lauxlib.c lbaselib.c lcode.c lcorolib.c lctype.c ldblib.c ldebug.c
        ldo.c ldump.c lfunc.c lgc.c linit.c liolib.c llex.c lmathlib.c lmem.c
        loadlib.c lobject.c lopcodes.c loslib.c lparser.c lstate.c lstring.c
        lstrlib.c ltable.c ltablib.c ltm.c lundump.c lutf8lib.c lvm.c lzio.c
    )
    set(LUA_SRCS "")
    foreach(f ${LUA_CORE})
        list(APPEND LUA_SRCS ${lua_SOURCE_DIR}/${f})
    endforeach()
    add_library(lua_static STATIC ${LUA_SRCS})
    target_include_directories(lua_static PUBLIC ${lua_SOURCE_DIR})
endif()

add_library(engine_lib INTERFACE)
target_include_directories(engine_lib INTERFACE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(engine_lib INTERFACE lua_static)

# Libreria piattaforma (header-only + compile defs)
add_library(platform_lib INTERFACE)
target_include_directories(platform_lib INTERFACE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(engine_lib INTERFACE platform_lib)

# ---- Dear ImGui (Studio UI OpenGL) ----
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.6
)
FetchContent_MakeAvailable(imgui)

add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui_lib PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui_lib PUBLIC glfw glad_gl_core)

glad_add_library(glad_gl_core STATIC API gl:core=3.3)

add_library(render_common STATIC
    src/render/stb_image_impl.cpp
)
target_include_directories(render_common PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${stb_SOURCE_DIR}
)

# ---- Shader SPIR-V (Vulkan PC) ----
set(SHADER_DIR ${CMAKE_SOURCE_DIR}/shaders)
set(SPV_DIR ${CMAKE_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${SPV_DIR})

find_program(GLSLC glslc HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin")
set(VK_SHADER_SPVS "")
if (GLSLC)
    set(VK_SHADER_SOURCES part.vert part_stud.frag)
    foreach(name ${VK_SHADER_SOURCES})
        set(src ${SHADER_DIR}/${name})
        set(out ${SPV_DIR}/${name}.spv)
        add_custom_command(
            OUTPUT ${out}
            COMMAND ${GLSLC} ${src} -o ${out}
            DEPENDS ${src}
            COMMENT "Compila SPIR-V ${name}"
        )
        list(APPEND VK_SHADER_SPVS ${out})
    endforeach()
    add_custom_target(vk_shaders DEPENDS ${VK_SHADER_SPVS})
endif()

# ---- Vulkan SDK (PC) — opzionale ----
set(HAS_VULKAN OFF)
if (DEFINED ENV{VULKAN_SDK})
    set(_VK_SDK "$ENV{VULKAN_SDK}")
    if (EXISTS "${_VK_SDK}/Include/vulkan/vulkan.h")
        set(Vulkan_INCLUDE_DIRS "${_VK_SDK}/Include")
    elseif (EXISTS "${_VK_SDK}/include/vulkan/vulkan.h")
        set(Vulkan_INCLUDE_DIRS "${_VK_SDK}/include")
    endif()
    if (WIN32 AND EXISTS "${_VK_SDK}/Lib/vulkan-1.lib")
        set(Vulkan_LIBRARIES "${_VK_SDK}/Lib/vulkan-1.lib")
    endif()
    if (Vulkan_INCLUDE_DIRS AND Vulkan_LIBRARIES)
        set(HAS_VULKAN ON)
    endif()
endif()

function(copy_assets target)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_SOURCE_DIR}/assets
                $<TARGET_FILE_DIR:${target}>/assets
        COMMENT "Copia asset per ${target}"
    )
    if (VK_SHADER_SPVS)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${target}>/shaders
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${VK_SHADER_SPVS}
                    $<TARGET_FILE_DIR:${target}>/shaders
            COMMENT "Copia shader SPIR-V per ${target}"
        )
    endif()
endfunction()

if (WIN32)
    enable_language(RC)
    # windres risolve "app.ico" rispetto alla cartella del .rc
    set(ROBLOX_WIN_ICON ${CMAKE_SOURCE_DIR}/assets/icon/app.rc)
    set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} --include-dir=${CMAKE_SOURCE_DIR}/assets/icon")
endif()

function(add_roblox_icon target)
    if (WIN32)
        target_sources(${target} PRIVATE ${ROBLOX_WIN_ICON})
        # Forza ricompilazione risorse se cambia l'ICO
        set_source_files_properties(${ROBLOX_WIN_ICON} PROPERTIES
            OBJECT_DEPENDS "${CMAKE_SOURCE_DIR}/assets/icon/app.ico")
    endif()
endfunction()

# ---- Blocky Game Engine — editor / Studio (OpenGL + ImGui) ----
add_executable(RobloxStudio src/main.cpp)
target_include_directories(RobloxStudio PRIVATE ${CMAKE_SOURCE_DIR}/src ${stb_SOURCE_DIR})
target_compile_definitions(RobloxStudio PRIVATE BLOCKY_GAME_ENGINE=1)
target_link_libraries(RobloxStudio PRIVATE glfw glm::glm glad_gl_core assimp::assimp imgui_lib render_common engine_lib)
set_target_properties(RobloxStudio PROPERTIES OUTPUT_NAME "BlockyGameEngine")
if (WIN32)
    # GUI app: niente finestra terminale nera in background
    set_target_properties(RobloxStudio PROPERTIES WIN32_EXECUTABLE TRUE)
    target_link_libraries(RobloxStudio PRIVATE opengl32 comdlg32 shell32 winmm xinput ws2_32 dbghelp psapi)
    target_link_options(RobloxStudio PRIVATE -mwindows -static -static-libgcc -static-libstdc++)
endif()
copy_assets(RobloxStudio)

# ---- Blocky Studio — editor place (eseguibile separato) ----
# Target distinto da RobloxStudio: condivide gli header di src/engine ma ha il
# proprio main, così l'editor evolve senza toccare BlockyGameEngine.
add_executable(BlockyStudio src/studio/studio_main.cpp)
target_include_directories(BlockyStudio PRIVATE ${CMAKE_SOURCE_DIR}/src ${stb_SOURCE_DIR})
# assimp serve perché LuaRuntime.h include R6WalkAnim.h (import clip da Blender).
target_link_libraries(BlockyStudio PRIVATE glfw glm::glm glad_gl_core assimp::assimp imgui_lib render_common engine_lib)
set_target_properties(BlockyStudio PROPERTIES OUTPUT_NAME "BlockyStudio")
if (WIN32)
    set_target_properties(BlockyStudio PROPERTIES WIN32_EXECUTABLE TRUE)
    target_link_libraries(BlockyStudio PRIVATE opengl32 comdlg32 shell32 winmm ws2_32 dbghelp)
    target_link_options(BlockyStudio PRIVATE -mwindows -static -static-libgcc -static-libstdc++)
endif()
copy_assets(BlockyStudio)
add_roblox_icon(RobloxStudio)

# Blocky PC — Vulkan + stud texture sulle superfici
if (HAS_VULKAN)
    add_executable(Roblox src/vk_main.cpp)
    target_include_directories(Roblox PRIVATE ${CMAKE_SOURCE_DIR}/src ${stb_SOURCE_DIR} ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(Roblox PRIVATE glfw glm::glm ${Vulkan_LIBRARIES} render_common)
    set_target_properties(Roblox PROPERTIES OUTPUT_NAME "Blocky")
    if (WIN32)
        set_target_properties(Roblox PROPERTIES WIN32_EXECUTABLE TRUE)
        target_link_options(Roblox PRIVATE -mwindows -static -static-libgcc -static-libstdc++)
    endif()
    if (TARGET vk_shaders)
        add_dependencies(Roblox vk_shaders)
    endif()
    copy_assets(Roblox)
    add_roblox_icon(Roblox)
else()
    message(WARNING "Vulkan non trovato: Blocky.exe = OpenGL Studio (installa Vulkan SDK per backend Vulkan).")
    add_executable(Roblox src/main.cpp)
    target_include_directories(Roblox PRIVATE ${CMAKE_SOURCE_DIR}/src ${stb_SOURCE_DIR})
    target_link_libraries(Roblox PRIVATE glfw glm::glm glad_gl_core assimp::assimp imgui_lib render_common engine_lib)
    set_target_properties(Roblox PROPERTIES OUTPUT_NAME "Blocky")
    if (WIN32)
        # GUI app: niente finestra terminale nera in background
        set_target_properties(Roblox PROPERTIES WIN32_EXECUTABLE TRUE)
        target_link_libraries(Roblox PRIVATE opengl32 comdlg32 shell32 winmm xinput ws2_32 dbghelp psapi)
        target_link_options(Roblox PRIVATE -mwindows -static -static-libgcc -static-libstdc++)
    endif()
    copy_assets(Roblox)
    add_roblox_icon(Roblox)
endif()

# Garden — OpenGL desktop; compilare con -DGARDEN_GLES=ON per mobile OpenGL ES + atmosfera
option(GARDEN_GLES "OpenGL ES 3.2 + atmospheric shading (mobile)" OFF)
add_executable(Garden src/garden/garden_main.cpp)
target_include_directories(Garden PRIVATE ${CMAKE_SOURCE_DIR}/src ${stb_SOURCE_DIR})
target_link_libraries(Garden PRIVATE glfw glm::glm glad_gl_core render_common)
if (GARDEN_GLES)
    target_compile_definitions(Garden PRIVATE GARDEN_GLES)
endif()
if (WIN32)
    target_link_libraries(Garden PRIVATE opengl32)
    target_link_options(Garden PRIVATE -static -static-libgcc -static-libstdc++)
endif()
copy_assets(Garden)
add_roblox_icon(Garden)

add_executable(export_place src/tools/export_place.cpp)
target_include_directories(export_place PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(export_place PRIVATE engine_lib glm::glm)

# Server dedicato multiplayer (console)
add_executable(BlockyServer src/tools/blocky_server.cpp)
target_include_directories(BlockyServer PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(BlockyServer PRIVATE glm::glm)
if (WIN32)
    target_link_libraries(BlockyServer PRIVATE ws2_32 psapi)
    target_link_options(BlockyServer PRIVATE -static -static-libgcc -static-libstdc++)
endif()
set_target_properties(BlockyServer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    OUTPUT_NAME "BlockyServer")

# Controllo asset pre-release: esce != 0 se trova errori, così può fermare
# uno script di packaging prima che il pacchetto venga creato.
add_executable(asset_audit src/tools/asset_audit.cpp)
target_include_directories(asset_audit PRIVATE ${CMAKE_SOURCE_DIR}/src)
if (WIN32)
    # Statico come gli altri target: deve girare su una macchina senza MinGW.
    target_link_options(asset_audit PRIVATE -static -static-libgcc -static-libstdc++)
endif()

# Indice di pubblicazione: mesh/collider/materiali/luci/script/hash in un
# solo game_index.json, cosi' il client scarica e basta.
add_executable(build_game_index src/tools/build_game_index.cpp)
target_include_directories(build_game_index PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(build_game_index PRIVATE glm::glm)
if (WIN32)
    target_link_options(build_game_index PRIVATE -static -static-libgcc -static-libstdc++)
endif()

add_executable(test_lua src/tools/test_lua.cpp)
target_include_directories(test_lua PRIVATE ${CMAKE_SOURCE_DIR}/src)
# LuaRuntime.h non è più headless: tira dentro R6WalkAnim.h (assimp),
# render/Particles.h (glad) e <GLFW/glfw3.h>. Il test non apre finestre,
# ma senza queste dipendenze non compila nemmeno.
target_link_libraries(test_lua PRIVATE engine_lib glm::glm assimp::assimp glad_gl_core glfw)
if(WIN32)
    # SoundService.h → winmm (PlaySound/mciSendString), LanNet.h → ws2_32 (sendto)
    target_link_libraries(test_lua PRIVATE winmm ws2_32)
endif()

add_executable(inspect src/inspect.cpp)
target_link_libraries(inspect PRIVATE assimp::assimp)
