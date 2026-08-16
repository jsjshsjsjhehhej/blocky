# Blocky — selezione piattaforma (includi da CMakeLists.txt)
# Uso: cmake -DROBLOX_TARGET=PC|MOBILE|IOS|PS5|AUTO ..

set(ROBLOX_TARGET "AUTO" CACHE STRING "Target: AUTO, PC, MOBILE, IOS, PS5")
set_property(CACHE ROBLOX_TARGET PROPERTY STRINGS AUTO PC MOBILE IOS PS5)

option(ROBLOX_PREFER_VULKAN "Su PC preferisci Vulkan se disponibile" OFF)
option(ROBLOX_OPTIMIZE_SIZE "Ottimizza per size (mobile)" OFF)

# Normalizza
string(TOUPPER "${ROBLOX_TARGET}" _RT)

if (_RT STREQUAL "PC")
    add_compile_definitions(ROBLOX_TARGET_PC=1)
    message(STATUS "Blocky target: PC")
elseif (_RT STREQUAL "MOBILE" OR _RT STREQUAL "ANDROID")
    add_compile_definitions(ROBLOX_TARGET_MOBILE=1 ROBLOX_TARGET_ANDROID=1)
    set(ROBLOX_OPTIMIZE_SIZE ON CACHE BOOL "" FORCE)
    message(STATUS "Blocky target: Mobile (Android)")
elseif (_RT STREQUAL "IOS")
    add_compile_definitions(ROBLOX_TARGET_IOS=1 ROBLOX_TARGET_MOBILE=1)
    set(ROBLOX_OPTIMIZE_SIZE ON CACHE BOOL "" FORCE)
    message(STATUS "Blocky target: iOS (Metal stub — serve Xcode)")
elseif (_RT STREQUAL "PS5")
    add_compile_definitions(ROBLOX_TARGET_PS5=1)
    message(STATUS "Blocky target: PS5 (richiede Sony Prospero SDK)")
else()
    add_compile_definitions(ROBLOX_TARGET_AUTO=1)
    message(STATUS "Blocky target: AUTO (rileva a compile-time)")
endif()

if (ROBLOX_PREFER_VULKAN)
    add_compile_definitions(ROBLOX_PREFER_VULKAN=1)
endif()

# Flag ottimizzazione per-piattaforma
if (ROBLOX_OPTIMIZE_SIZE)
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options($<$<CONFIG:Release>:-Os> $<$<CONFIG:Release>:-ffunction-sections>)
        add_link_options($<$<CONFIG:Release>:-Wl,--gc-sections>)
    endif()
else()
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options($<$<CONFIG:Release>:-O3> $<$<CONFIG:Release>:-DNDEBUG>)
    endif()
endif()

# MSVC
if (MSVC)
    add_compile_options(/MP $<$<CONFIG:Release>:/O2>)
endif()
