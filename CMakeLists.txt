cmake_minimum_required(VERSION 3.20)
project(Blocky LANGUAGES C CXX)

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
    add_compile_options(-O2)
    message(STATUS "Blocky: strip simboli + ottimizzazione -O2 ATTIVI (build di distribuzione)")
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

# --- Firma codice (facoltativa) ---
# Vuota di default: finche' non c'e' un certificato vero, questa e' un no-op
# silenzioso, nessun comportamento diverso da oggi. Quando arriva un .pfx,
# basta passare -DBLOCKY_SIGN_CERT=C:/path/cert.pfx (e la password nella
# variabile d'ambiente BLOCKY_SIGN_PASSWORD, mai nella cache di CMake in
# chiaro) perche' ogni exe/dll di distribuzione venga firmato da solo.
set(BLOCKY_SIGN_CERT "" CACHE FILEPATH "Path a un certificato .pfx per firmare l'output (vuoto = non firmare)")

function(sign_if_configured target)
    if (WIN32 AND BLOCKY_SIGN_CERT)
        find_program(BLOCKY_SIGNTOOL_EXE signtool)
        if (BLOCKY_SIGNTOOL_EXE)
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${BLOCKY_SIGNTOOL_EXE} sign /f "${BLOCKY_SIGN_CERT}" /p "$ENV{BLOCKY_SIGN_PASSWORD}"
                        /fd sha256 /tr http://timestamp.digicert.com /td sha256
                        "$<TARGET_FILE:${target}>"
                COMMENT "Firma codice: ${target}"
                VERBATIM)
        else()
            message(WARNING "BLOCKY_SIGN_CERT impostato ma signtool.exe non e' nel PATH — ${target} non firmato")
        endif()
    endif()
endfunction()

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

# ---- Jolt Physics (Fase 1: solo un target di prova isolato, vedi sotto) ----
#
# Scelta fra Jolt e Bullet3, il motivo e' concreto: Jolt e' C++17 moderno,
# licenza MIT, pensato per i giochi fin dall'inizio (lo usa Horizon Forbidden
# West), API piccola e pulita, multithread di serie. Bullet3 e' piu' diffuso
# ma e' C-style, licenza zlib, e porta dentro un'enormita' di demo/tool che
# non servirebbero a Blocky — esattamente il tipo di zavorra che gia' oggi
# evitiamo con Assimp (importer disattivati uno per uno).
#
# Il motore la usa attraverso src/engine/BlockyPhysics.h (LOD e budget per
# device deboli); nessun altro file include Jolt direttamente.
FetchContent_Declare(
    joltphysics
    GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
    GIT_TAG        v5.2.0
    SOURCE_SUBDIR  Build
)
set(TARGET_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(TARGET_HELLO_WORLD OFF CACHE BOOL "" FORCE)
set(TARGET_PERFORMANCE_TEST OFF CACHE BOOL "" FORCE)
set(TARGET_SAMPLES OFF CACHE BOOL "" FORCE)
set(TARGET_VIEWER OFF CACHE BOOL "" FORCE)
set(TARGET_EXPLICIT_TEMPLATE_INSTANTIATION OFF CACHE BOOL "" FORCE)
set(ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
# Debug renderer, profiler e ObjectStream servono a chi sviluppa Jolt, non a
# chi gioca: tolti, la libreria pesa meno (conta sugli APK e sui PC vecchi).
set(DEBUG_RENDERER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "" FORCE)
set(PROFILER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "" FORCE)
set(ENABLE_OBJECT_STREAM OFF CACHE BOOL "" FORCE)
set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)
# Di default Jolt compila per AVX2/FMA: su una CPU senza (PC vecchi, molti
# portatili economici) libblocky.dll crasherebbe all'avvio con "istruzione
# illegale". Con pochi corpi attivi il guadagno SIMD e' trascurabile, quindi
# si resta sulla base SSE2 che ogni CPU x86-64 ha.
foreach(_jolt_isa USE_SSE4_1 USE_SSE4_2 USE_AVX USE_AVX2 USE_LZCNT USE_TZCNT USE_F16C USE_FMADD)
    set(${_jolt_isa} OFF CACHE BOOL "" FORCE)
endforeach()

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

# ---- Luau (scripting) ----
# Stessa licenza MIT di Lua standard (nessun problema di copyright) — e' il
# linguaggio che Roblox stesso ha reso open source nel 2021. Sostituisce la
# Lua 5.4 "di riferimento" usata prima: l'API C per tabelle/funzioni/
# riferimenti e' la stessa, cambia solo come si compila ed esegue il
# sorgente (Luau::compile + luau_load invece di luaL_dostring).
FetchContent_Declare(
    luau
    GIT_REPOSITORY https://github.com/luau-lang/luau.git
    GIT_TAG        0.663
)
set(LUAU_BUILD_CLI OFF CACHE BOOL "" FORCE)
set(LUAU_BUILD_TESTS OFF CACHE BOOL "" FORCE)
# La CMakeLists.txt di Luau dichiara un cmake_minimum_required troppo
# vecchio per CMake recenti (rifiutano di procedere senza questo). Non e'
# una nostra scelta di versione, solo un permesso di compatibilita' per
# una dipendenza esterna non aggiornata.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
FetchContent_MakeAvailable(luau)
unset(CMAKE_POLICY_VERSION_MINIMUM)

add_library(engine_lib INTERFACE)
target_include_directories(engine_lib INTERFACE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(engine_lib INTERFACE Luau.VM Luau.Compiler)

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

# ---- ImGuiColorTextEdit (evidenziazione sintattica per l'editor di script Lua) ----
FetchContent_Declare(
    imguicolortextedit
    GIT_REPOSITORY https://github.com/BalazsJako/ImGuiColorTextEdit.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(imguicolortextedit)

add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    ${imguicolortextedit_SOURCE_DIR}/TextEditor.cpp
)
target_include_directories(imgui_lib PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${imguicolortextedit_SOURCE_DIR}
)
target_compile_definitions(imgui_lib PUBLIC IMGUI_DEFINE_MATH_OPERATORS)
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

# Tool privati/da sviluppo (server dedicato, strumenti da riga di comando):
# copiati anche in Documents\blocky servers dell'utente a ogni build, cosi'
# gli script .bat che gia' vivono li' (BLOCKY-Server-dedicato.bat) li trovano
# senza dover puntare a build\bin del repository. Copia, non sposta: restano
# anche in build\bin, che continua a funzionare come sempre.
if (WIN32)
    set(BLOCKY_PERSONAL_TOOLS_DIR "$ENV{USERPROFILE}/Documents/blocky servers")
endif()

# Copia version.txt (radice del progetto) accanto al lanciatore: e' il
# numero che AutoUpdater::readLocalVersion() confronta con l'ultima release
# GitHub. Senza questo file l'updater assume "0.0.0" — funziona comunque
# (qualunque release vera risulta "piu' nuova"), ma un numero vero evita che
# il primissimo avvio dopo l'installazione riscarichi una dll che magari e'
# gia' quella giusta.
function(copy_version_file target)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/version.txt"
                "$<TARGET_FILE_DIR:${target}>/version.txt"
        COMMENT "Copia version.txt per ${target}"
    )
endfunction()

function(copy_to_personal_tools target)
    if (WIN32 AND BLOCKY_PERSONAL_TOOLS_DIR)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${BLOCKY_PERSONAL_TOOLS_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:${target}>"
                    "${BLOCKY_PERSONAL_TOOLS_DIR}/"
            COMMENT "Copia ${target} in Documents/blocky servers"
        )
    endif()
endfunction()

function(copy_assets target)
    # copy_directory da sola aggiunge/sovrascrive ma non cancella mai: un
    # file tolto da assets/ resta per sempre come copia zombie in bin/assets,
    # invisibile finche' qualcuno non lo trova per caso (e' successo — asset
    # scoperti stasera, spariti dal sorgente da giorni, ancora nella build).
    # rm -rf prima della copia rende bin/assets uno specchio vero, non un
    # accumulo: costa una copia in piu' a ogni build, ~2 MB, non si sente.
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E rm -rf
                $<TARGET_FILE_DIR:${target}>/assets
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

# ---- libblocky.dll — l'intero engine client (hub/player, Studio classico
# dentro main.cpp, editor place separato) in un solo binario condiviso. ----
#
# Prima erano tre eseguibili, ognuno con la propria copia compilata di
# main.cpp o studio_main.cpp: RobloxStudio (BLOCKY_GAME_ENGINE=1), Roblox
# senza quella define, BlockyStudio col proprio main separato. Ora è un
# corpo solo — GLFW, ImGui, assimp, glad vivono tutti qui — e i tre .exe
# sotto sono lanciatori minimi che caricano questa dll a runtime e chiamano
# l'export giusto (vedi src/engine/BlockyApi.h). Nessuna classe attraversa
# il confine della dll, solo le funzioni C dichiarate lì.
FetchContent_MakeAvailable(joltphysics)
# Jolt compila con -Werror, e questo GCC da un falso positivo di
# -Warray-bounds su un accesso legittimo (Vector<3>::operator[] con indice
# runtime). Si toglie -Werror solo per il target Jolt.
target_compile_options(Jolt PRIVATE -Wno-error)

add_library(blocky SHARED
    src/main.cpp
    src/studio/studio_main.cpp
    src/engine/BlockyApi.cpp
    # zip.c (kuba--/zip, gia' dentro i sorgenti di assimp): il client ora non
    # si limita a ESTRARRE archivi, ne crea uno per spedire al server un gioco
    # appena pubblicato — quindi le funzioni zip_* vanno linkate davvero, non
    # basta piu' il solo header.
    ${assimp_SOURCE_DIR}/contrib/zip/src/zip.c)
target_include_directories(blocky PRIVATE ${CMAKE_SOURCE_DIR}/src ${stb_SOURCE_DIR}
    ${assimp_SOURCE_DIR}/contrib/zip/src)
target_link_libraries(blocky PRIVATE glfw glm::glm glad_gl_core assimp::assimp imgui_lib render_common engine_lib Jolt)
# Header precompilato: glad/GLFW/ImGui/glm/STL pesanti e stabili, analizzati
# una volta sola invece che in ognuno dei tre file di questo target — accelera
# soprattutto le build pulite e dopo un git pull/checkout.
target_precompile_headers(blocky PRIVATE ${CMAKE_SOURCE_DIR}/src/pch.h)
# zip.c e' C, non C++: l'header precompilato (pieno di ImGui, glm, template)
# non puo' essere applicato a lui, il compilatore C non saprebbe leggerlo.
set_source_files_properties(${assimp_SOURCE_DIR}/contrib/zip/src/zip.c
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
set_target_properties(blocky PROPERTIES
    PREFIX "lib"
    OUTPUT_NAME "blocky")   # esplicito: alcuni generatori (MSVC) non prefissano "lib" da soli
if (WIN32)
    # xinput9_1_0 invece di xinput (= XInput1_3.dll): quella versione richiede
    # il runtime DirectX di giugno 2010 installato a parte, che su un PC
    # "normale" spesso non c'e' — LoadLibrary di libblocky.dll falliva per
    # questa dipendenza indiretta mancante, con Windows che segnalava
    # (fuorviante) "libblocky.dll non trovato". XInput9_1_0.dll invece e'
    # incluso in ogni Windows da Vista in poi, nessun redistributable
    # richiesto — usiamo solo XInputGetState/XInputSetState, presenti in
    # entrambe le versioni.
    target_link_libraries(blocky PRIVATE opengl32 comdlg32 shell32 winmm xinput9_1_0 ws2_32 dbghelp psapi)
    target_link_options(blocky PRIVATE -static -static-libgcc -static-libstdc++)
endif()
# --- Offuscamento pesante (solo il target blocky, solo distribuzione) ---
# -flto fonde main.cpp/studio_main.cpp/BlockyApi.cpp in un unico blob al
# link: sparisce il confine "una funzione = un blocco di codice" che rende
# leggibile un disassemblato, e il compilatore inlinea/elimina codice tra
# file diversi (impossibile con la compilazione normale, un file alla
# volta). -fno-rtti toglie le stringhe con i nomi delle classi (nessun
# dynamic_cast/typeid nel codice, quindi sicuro). -O3 e' -O2 ma piu'
# aggressivo sull'inlining. Solo su blocky (dove vive la logica vera) per
# non appesantire ogni rebuild di assimp/glfw/imgui con LTO.
if(BLOCKY_STRIP AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(blocky PRIVATE -O3 -fno-rtti -flto)
    target_link_options(blocky PRIVATE -flto)
    message(STATUS "Blocky: LTO + -fno-rtti ATTIVI su blocky (offuscamento pesante)")
endif()
copy_assets(blocky)   # una volta sola: i tre lanciatori finiscono nella stessa build/bin/
sign_if_configured(blocky)

# libblocky.dll.sha256 accanto alla dll a ogni build: sono i due file da
# allegare a una release GitHub perche' AutoUpdater.h (src/tools/AutoUpdater.h)
# li scarichi e verifichi prima di sostituire la dll installata. "cmake -E
# sha256sum" stampa "HASH  nomefile" (come sha256sum vero) — stesso formato
# che AutoUpdater si aspetta di leggere (prende solo la prima parola).
add_custom_command(TARGET blocky POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E sha256sum "$<TARGET_FILE:blocky>" > "$<TARGET_FILE:blocky>.sha256"
    COMMENT "Genero libblocky.dll.sha256 per la release"
    VERBATIM
)

# ---- Blocky.exe — lanciatore hub/player ----
add_executable(BlockyLauncher src/tools/blocky_launcher.cpp)
add_dependencies(BlockyLauncher blocky)
set_target_properties(BlockyLauncher PROPERTIES OUTPUT_NAME "Blocky")
if (WIN32)
    set_target_properties(BlockyLauncher PROPERTIES WIN32_EXECUTABLE TRUE)
    # Stack di sistema del thread principale piu' grande del default MinGW
    # (2MB): il motore chiama nel loop per-frame catene di funzioni molto
    # profonde (ImGui, Studio, scripting Luau con GC ricorsivo) e con il
    # default puo' finire in STACK_OVERFLOW su percorsi pesanti come il
    # Test in Studio. 16MB da margine ampio senza costo reale (e' solo
    # spazio di indirizzi riservato, non memoria committata subito).
    target_link_options(BlockyLauncher PRIVATE -static -static-libgcc -static-libstdc++ -Wl,--stack,16777216)
    target_link_libraries(BlockyLauncher PRIVATE wininet advapi32 comctl32)
endif()
add_roblox_icon(BlockyLauncher)
copy_version_file(BlockyLauncher)
sign_if_configured(BlockyLauncher)

# ---- BlockyGameEngine.exe — lanciatore Studio classico (dentro main.cpp) ----
add_executable(BlockyStudioLauncher src/tools/blocky_studio_launcher.cpp)
add_dependencies(BlockyStudioLauncher blocky)
set_target_properties(BlockyStudioLauncher PROPERTIES OUTPUT_NAME "BlockyGameEngine")
if (WIN32)
    set_target_properties(BlockyStudioLauncher PROPERTIES WIN32_EXECUTABLE TRUE)
    # Vedi commento sullo stesso flag su BlockyLauncher: qui e' dove il
    # Test in Studio (F5) andava in STACK_OVERFLOW con il default 2MB.
    target_link_options(BlockyStudioLauncher PRIVATE -static -static-libgcc -static-libstdc++ -Wl,--stack,16777216)
    target_link_libraries(BlockyStudioLauncher PRIVATE wininet advapi32 comctl32)
endif()
add_roblox_icon(BlockyStudioLauncher)
copy_version_file(BlockyStudioLauncher)
sign_if_configured(BlockyStudioLauncher)

# ---- BlockyStudio.exe — lanciatore editor place separato ----
# Stessa icona di default assente che aveva il vecchio target BlockyStudio:
# comportamento preesistente, non un'omissione di questa modifica.
add_executable(BlockyEditorLauncher src/tools/blocky_editor_launcher.cpp)
add_dependencies(BlockyEditorLauncher blocky)
set_target_properties(BlockyEditorLauncher PROPERTIES OUTPUT_NAME "BlockyStudio")
if (WIN32)
    set_target_properties(BlockyEditorLauncher PROPERTIES WIN32_EXECUTABLE TRUE)
    # Vedi commento sullo stesso flag su BlockyLauncher.
    target_link_options(BlockyEditorLauncher PRIVATE -static -static-libgcc -static-libstdc++ -Wl,--stack,16777216)
    target_link_libraries(BlockyEditorLauncher PRIVATE wininet advapi32 comctl32)
endif()
copy_version_file(BlockyEditorLauncher)
sign_if_configured(BlockyEditorLauncher)

# Renderer Vulkan alternativo (vk_main.cpp) — sperimentale, non condivide
# codice con main.cpp/libblocky.dll. Prima produceva anche lui "Blocky.exe"
# (era l'uno o l'altro, mai insieme: Vulkan se disponibile, altrimenti la
# copia OpenGL di main.cpp). Ora "Blocky.exe" lo produce sempre BlockyLauncher
# via libblocky.dll, quindi questo target avrebbe lo stesso nome di output:
# rinominato per evitare la collisione. Nessuna Vulkan SDK in questo ambiente
# stasera, quindi il ramo qui sotto non è mai stato ricompilato né verificato
# in questa sessione — solo reso coerente col resto.
if (HAS_VULKAN)
    add_executable(Roblox src/vk_main.cpp)
    target_include_directories(Roblox PRIVATE ${CMAKE_SOURCE_DIR}/src ${stb_SOURCE_DIR} ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(Roblox PRIVATE glfw glm::glm ${Vulkan_LIBRARIES} render_common)
    set_target_properties(Roblox PROPERTIES OUTPUT_NAME "BlockyVulkan")
    if (WIN32)
        set_target_properties(Roblox PROPERTIES WIN32_EXECUTABLE TRUE)
        target_link_options(Roblox PRIVATE -mwindows -static -static-libgcc -static-libstdc++)
    endif()
    if (TARGET vk_shaders)
        add_dependencies(Roblox vk_shaders)
    endif()
    copy_assets(Roblox)
    add_roblox_icon(Roblox)
endif()

# Prova DirectX 11 (dx_main.cpp) — stesso spirito del target Vulkan sopra:
# "Hello Triangle" per dimostrare che il toolchain (MinGW ha gia'
# d3d11.h/d3dcompiler.h, nessun SDK esterno da installare) funziona
# davvero, non il motore vero. Sempre disponibile su Windows: DirectX 11
# fa parte di Windows stesso, non serve rilevarlo come per Vulkan.
if (WIN32)
    add_executable(BlockyDx11 src/dx_main.cpp)
    target_link_libraries(BlockyDx11 PRIVATE glfw d3d11 dxgi d3dcompiler)
    set_target_properties(BlockyDx11 PROPERTIES WIN32_EXECUTABLE TRUE)
    target_link_options(BlockyDx11 PRIVATE -static -static-libgcc -static-libstdc++)
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
copy_to_personal_tools(export_place)

# Server dedicato multiplayer (console)
add_executable(BlockyServer src/blocky_servers/blocky_server.cpp)
target_include_directories(BlockyServer PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(BlockyServer PRIVATE glm::glm)
if (WIN32)
    target_link_libraries(BlockyServer PRIVATE ws2_32 psapi)
    target_link_options(BlockyServer PRIVATE -static -static-libgcc -static-libstdc++)
endif()
set_target_properties(BlockyServer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    OUTPUT_NAME "BlockyServer")
copy_to_personal_tools(BlockyServer)

# Lo stesso server dedicato, ma come servizio Windows vero — l'equivalente
# Blocky del vecchio RCCService di Roblox: parte da solo con Windows, gira
# senza console, e la Service Control Manager lo fa ripartire se muore.
# Solo Windows: la Service Control Manager non esiste altrove, e su Android
# l'equivalente e' un Foreground Service (vedi android/), non un eseguibile.
if (WIN32)
    add_executable(BlockyRccService src/blocky_servers/blocky_rcc_service.cpp)
    target_include_directories(BlockyRccService PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(BlockyRccService PRIVATE glm::glm ws2_32 advapi32 psapi)
    target_link_options(BlockyRccService PRIVATE -static -static-libgcc -static-libstdc++)
    set_target_properties(BlockyRccService PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        OUTPUT_NAME "BlockyRccService")
    copy_to_personal_tools(BlockyRccService)
endif()

# Relay/matchmaking "blk://" per multiplayer via internet (da deployare su una
# VPS con IP vero, MAI sulla rete di un giocatore — vedi PoolConfig.h).
add_executable(BlockyPool src/blocky_servers/blocky_pool.cpp)
target_include_directories(BlockyPool PRIVATE ${CMAKE_SOURCE_DIR}/src)
if (WIN32)
    target_link_libraries(BlockyPool PRIVATE ws2_32)
    target_link_options(BlockyPool PRIVATE -static -static-libgcc -static-libstdc++)
endif()
set_target_properties(BlockyPool PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    OUTPUT_NAME "BlockyPool")
copy_to_personal_tools(BlockyPool)

# Test isolato del relay "blk://" — vedi il commento in cima a test_pool.cpp.
# Stesse dipendenze minime di BlockyServer (solo LanNet.h, non LuaRuntime.h):
# niente assimp/glad/glfw.
add_executable(test_pool src/tools/test_pool.cpp)
target_include_directories(test_pool PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_pool PRIVATE glm::glm)
if (WIN32)
    target_link_libraries(test_pool PRIVATE ws2_32)
endif()

add_executable(test_physics src/tools/test_physics.cpp)
target_link_libraries(test_physics PRIVATE Jolt)
target_include_directories(test_physics PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_physics PRIVATE glm::glm)

# Controllo asset pre-release: esce != 0 se trova errori, così può fermare
# uno script di packaging prima che il pacchetto venga creato.
add_executable(asset_audit src/tools/asset_audit.cpp)
target_include_directories(asset_audit PRIVATE ${CMAKE_SOURCE_DIR}/src)
if (WIN32)
    # Statico come gli altri target: deve girare su una macchina senza MinGW.
    target_link_options(asset_audit PRIVATE -static -static-libgcc -static-libstdc++)
endif()
copy_to_personal_tools(asset_audit)

# Indice di pubblicazione: mesh/collider/materiali/luci/script/hash in un
# solo game_index.json, cosi' il client scarica e basta.
add_executable(build_game_index src/tools/build_game_index.cpp)
target_include_directories(build_game_index PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${assimp_SOURCE_DIR}/contrib/zip/src)   # GamePublish.h include ZipArchive.h → zip.h;
                                            # nessuna funzione zip_* è chiamata da qui, quindi
                                            # serve solo l'header, non zip.c da linkare.
target_link_libraries(build_game_index PRIVATE glm::glm)
if (WIN32)
    target_link_options(build_game_index PRIVATE -static -static-libgcc -static-libstdc++)
endif()
copy_to_personal_tools(build_game_index)

# Server di hosting: riceve un .zip pubblicato, lo valida con la stessa
# pipeline di build_game_index e lo serve via HTTP. zip.c e' la libreria
# kuba--/zip gia' scaricata dentro il sorgente di assimp (contrib/zip),
# pinnata alla stessa v5.4.3 di ASSIMP_GIT_TAG sopra — non una dipendenza
# nuova, solo codice che era gia' sul disco e non veniva compilato.
add_executable(blocky_hostd
    src/blocky_servers/blocky_hostd.cpp
    ${assimp_SOURCE_DIR}/contrib/zip/src/zip.c)
target_include_directories(blocky_hostd PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${assimp_SOURCE_DIR}/contrib/zip/src)
target_link_libraries(blocky_hostd PRIVATE glm::glm)
if (WIN32)
    target_link_libraries(blocky_hostd PRIVATE ws2_32)
    target_link_options(blocky_hostd PRIVATE -static -static-libgcc -static-libstdc++)
endif()
set_target_properties(blocky_hostd PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    OUTPUT_NAME "blocky_hostd")
copy_to_personal_tools(blocky_hostd)

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
copy_to_personal_tools(test_lua)

# DataStore: verifica che i dati sopravvivano a un riavvio (due runtime
# separati) e che le chiavi malevole non scrivano fuori cartella.
add_executable(test_datastore src/tools/test_datastore.cpp)
target_include_directories(test_datastore PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_datastore PRIVATE engine_lib glm::glm assimp::assimp glad_gl_core glfw)
if(WIN32)
    target_link_libraries(test_datastore PRIVATE winmm ws2_32)
endif()


# Animatore a keyframe del personaggio: verifica interpolazione, sostituzione
# delle chiavi e giro completo su disco.

# Elenco dei server da riga di comando: prova la scansione senza interfaccia.
add_executable(scan_servers src/tools/scan_servers.cpp)
target_include_directories(scan_servers PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(scan_servers PRIVATE glm::glm)
if(WIN32)
    target_link_libraries(scan_servers PRIVATE ws2_32)
endif()
set_target_properties(scan_servers PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_executable(test_riganim src/tools/test_riganim.cpp)
target_include_directories(test_riganim PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_riganim PRIVATE engine_lib glm::glm assimp::assimp glad_gl_core glfw)
if(WIN32)
    target_link_libraries(test_riganim PRIVATE winmm ws2_32)
endif()

add_executable(test_luau src/tools/test_luau.cpp)
target_link_libraries(test_luau PRIVATE Luau.VM Luau.Compiler)

add_executable(test_antilag src/tools/test_antilag.cpp)
target_include_directories(test_antilag PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Riproduzione isolata di un blocco (non crash) segnalato dall'utente con lo
# script "Atmosfera dark horror" in stile Roblox. Stessa ricetta di
# dipendenze di test_lua qui sopra.
add_executable(test_hang_repro src/tools/test_hang_repro.cpp)
target_include_directories(test_hang_repro PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_hang_repro PRIVATE engine_lib glm::glm assimp::assimp glad_gl_core glfw)
if(WIN32)
    target_link_libraries(test_hang_repro PRIVATE winmm ws2_32)
endif()

# Prova isolata del contenimento crash di Test/Play (TestSessionGuard.h):
# scatena crash nativi veri (null deref, div/0, stack overflow) e verifica
# che il processo li intercetti e continui a girare, invece di terminare.
add_executable(test_crash_guard src/tools/test_crash_guard.cpp)
target_include_directories(test_crash_guard PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_executable(inspect src/inspect.cpp)
target_link_libraries(inspect PRIVATE assimp::assimp)
copy_to_personal_tools(inspect)
