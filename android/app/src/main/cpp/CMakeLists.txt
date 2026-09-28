# Blocky su Android — build del motore VERO come libreria nativa.
#
# Non tocca il CMakeLists.txt principale (che resta la build Windows): qui si
# riusano le STESSE sorgenti delle dipendenze gia' scaricate in build/_deps,
# compilandole per Android. Il motore non e' stato forkato: e' lo stesso
# src/main.cpp, reso portabile con gli shim in glfw_shim/ (GLFW, glad e il
# backend ImGui, che su Android non esistono).

cmake_minimum_required(VERSION 3.22.1)
project("blocky")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Radice del repo: da android/app/src/main/cpp si risale di 5 livelli
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)
set(DEPS "${REPO_ROOT}/build/_deps")

# ---- Dipendenze (stesse sorgenti della build Windows) ------------------
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
add_subdirectory("${DEPS}/assimp-src" "${CMAKE_BINARY_DIR}/assimp" EXCLUDE_FROM_ALL)

set(LUAU_BUILD_CLI OFF CACHE BOOL "" FORCE)
set(LUAU_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
add_subdirectory("${DEPS}/luau-src" "${CMAKE_BINARY_DIR}/luau" EXCLUDE_FROM_ALL)
unset(CMAKE_POLICY_VERSION_MINIMUM)

# Jolt (sotto Blocky Physics): stesse opzioni "leggere" della build Windows.
# Su ARM non servono flag SIMD: NEON e' la base di armeabi-v7a e arm64.
set(TARGET_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(TARGET_HELLO_WORLD OFF CACHE BOOL "" FORCE)
set(TARGET_PERFORMANCE_TEST OFF CACHE BOOL "" FORCE)
set(TARGET_SAMPLES OFF CACHE BOOL "" FORCE)
set(TARGET_VIEWER OFF CACHE BOOL "" FORCE)
set(ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
set(GENERATE_DEBUG_SYMBOLS OFF CACHE BOOL "" FORCE)
set(DEBUG_RENDERER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "" FORCE)
set(PROFILER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "" FORCE)
set(ENABLE_OBJECT_STREAM OFF CACHE BOOL "" FORCE)
set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)
foreach(_jolt_isa USE_SSE4_1 USE_SSE4_2 USE_AVX USE_AVX2 USE_LZCNT USE_TZCNT USE_F16C USE_FMADD)
    set(${_jolt_isa} OFF CACHE BOOL "" FORCE)
endforeach()
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_subdirectory("${DEPS}/joltphysics-src/Build" "${CMAKE_BINARY_DIR}/jolt" EXCLUDE_FROM_ALL)
# Anche nell'APK Debug: senza ottimizzazione Jolt pesava 4,3 MB di codice
# (e' quasi tutto template inline) e girava molte volte piu' lento, falsando
# le misure [perf] sul telefono. Jolt non lo debugghiamo noi: sempre -O2.
target_compile_options(Jolt PRIVATE -O2)

# ImGui: stessi file della build Windows MENO il backend GLFW (qui lo
# sostituisce glfw_shim/backends/imgui_impl_glfw.h, tre funzioni in tutto).
add_library(imgui_android STATIC
    "${DEPS}/imgui-src/imgui.cpp"
    "${DEPS}/imgui-src/imgui_draw.cpp"
    "${DEPS}/imgui-src/imgui_tables.cpp"
    "${DEPS}/imgui-src/imgui_widgets.cpp"
    "${DEPS}/imgui-src/backends/imgui_impl_opengl3.cpp"
    "${DEPS}/imguicolortextedit-src/TextEditor.cpp"
)
target_include_directories(imgui_android PUBLIC
    "${DEPS}/imgui-src"
    "${DEPS}/imgui-src/backends"
    "${DEPS}/imguicolortextedit-src"
)
target_compile_definitions(imgui_android PUBLIC
    IMGUI_DEFINE_MATH_OPERATORS
    IMGUI_IMPL_OPENGL_ES3      # il backend OpenGL3 di ImGui parla GLES
)

# ---- libcontroller_touch: NON costruita, per ora ----------------------
#
# I sorgenti stanno in controller_touch/ ma non entrano nel build. La logica
# "il pad si comporta da dito" esiste gia' nel motore, in
# src/ui/MenuPadCursor.h, e funziona su OGNI piattaforma.
#
# Una seconda copia in una .so a parte non potrebbe nemmeno essere usata da
# src/, che e' condiviso con Windows e non puo' dipendere da una libreria
# solo-Android. Averla collegata creava due sistemi che muovevano lo stesso
# cursore nello stesso frame, e vinceva quello sbagliato: e' cio' che faceva
# sembrare rotto il tasto X.
#
# I file restano perche' il taglio serve davvero a libblockyvr.so, dove i
# controller VR hanno lo stesso problema di puntare dentro un'interfaccia
# nata per le dita — e li' una .so separata e' la forma giusta.
#
#   add_library(controller_touch SHARED controller_touch/controller_touch.cpp)
#   target_include_directories(controller_touch PUBLIC controller_touch)

# ---- Il motore ---------------------------------------------------------
add_library(blocky SHARED
    "${REPO_ROOT}/src/main.cpp"
    "${REPO_ROOT}/src/render/stb_image_impl.cpp"
    android_entry.cpp
    imgui_glfw_bridge.cpp
    asset_unpack.cpp
    android_bridge.cpp
    # Stesso motivo del CMake principale: il client crea archivi zip per
    # spedire i giochi al server, quindi le funzioni zip_* servono linkate.
    "${DEPS}/assimp-src/contrib/zip/src/zip.c"
)

# glfw_shim PRIMO: e' cosi' che <GLFW/glfw3.h> e <glad/gl.h> trovano i finti
# invece dei veri, senza modificare una riga del motore.
target_include_directories(blocky PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/glfw_shim"
    "${REPO_ROOT}/src"
    "${DEPS}/assimp-src/contrib/zip/src"
    "${DEPS}/glm-src"
    "${DEPS}/stb-src"
)

# assimp imposta CMAKE_DEBUG_POSTFIX "d" e con add_subdirectory se lo porta
# dietro anche il nostro target: la libreria diventerebbe libblockyd.so e
# System.loadLibrary("blocky") non la troverebbe.
set_target_properties(blocky PROPERTIES DEBUG_POSTFIX "")

find_package(game-activity REQUIRED CONFIG)

set(CMAKE_SHARED_LINKER_FLAGS
    "${CMAKE_SHARED_LINKER_FLAGS} -u Java_com_google_androidgamesdk_GameActivity_initializeNativeCode")

target_link_libraries(blocky
    imgui_android
    assimp
    Luau.VM
    Luau.Compiler
    Jolt
    game-activity::game-activity_static
    EGL
    GLESv3
    android
    log
    # Riproduzione audio (AndroidAudio.h): OpenSL ES viene dal NDK,
    # non serve una dipendenza Gradle a parte.
    OpenSLES
)
