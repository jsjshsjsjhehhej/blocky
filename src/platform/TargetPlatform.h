#pragma once


#include <cstdint>
#include <string>

enum class TargetPlatform : uint8_t {
    PC = 0,
    MobileAndroid,
    MobileIOS,
    PlayStation5,
    Unknown
};

enum class GfxApi : uint8_t {
    OpenGL33 = 0,
    Vulkan,
    OpenGLES,
    Metal,
    AgcPs5,
    Auto
};

inline const char* targetPlatformName(TargetPlatform p) {
    switch (p) {
    case TargetPlatform::PC:            return "PC";
    case TargetPlatform::MobileAndroid: return "Android";
    case TargetPlatform::MobileIOS:     return "iOS";
    case TargetPlatform::PlayStation5:  return "PlayStation 5";
    default:                            return "Unknown";
    }
}

inline const char* gfxApiName(GfxApi a) {
    switch (a) {
    case GfxApi::OpenGL33:  return "OpenGL 3.3";
    case GfxApi::Vulkan:    return "Vulkan";
    case GfxApi::OpenGLES:  return "OpenGL ES";
    case GfxApi::Metal:     return "Metal";
    case GfxApi::AgcPs5:    return "PS5 AGC";
    default:                return "Auto";
    }
}

inline TargetPlatform detectTargetPlatform() {
#if defined(ROBLOX_TARGET_PS5)
    return TargetPlatform::PlayStation5;
#elif defined(ROBLOX_TARGET_IOS)
    return TargetPlatform::MobileIOS;
#elif defined(ROBLOX_TARGET_MOBILE) || defined(ROBLOX_TARGET_ANDROID)
    return TargetPlatform::MobileAndroid;
#elif defined(ROBLOX_TARGET_PC)
    return TargetPlatform::PC;
#elif defined(__APPLE__) && defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
    return TargetPlatform::MobileIOS;
#elif defined(__ANDROID__)
    return TargetPlatform::MobileAndroid;
#elif defined(_WIN32) || defined(__linux__) || (defined(__APPLE__) && defined(__MACH__))
    return TargetPlatform::PC;
#else
    return TargetPlatform::PC;
#endif
}

inline GfxApi recommendedGfxApi(TargetPlatform p) {
    switch (p) {
    case TargetPlatform::PC:
#if defined(ROBLOX_PREFER_VULKAN)
        return GfxApi::Vulkan;
#else
        return GfxApi::OpenGL33;
#endif
    case TargetPlatform::MobileAndroid: return GfxApi::OpenGLES;
    case TargetPlatform::MobileIOS:     return GfxApi::Metal;
    case TargetPlatform::PlayStation5:  return GfxApi::AgcPs5;
    default:                            return GfxApi::OpenGL33;
    }
}
