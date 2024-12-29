#pragma once

#ifdef __ANDROID__
#include <android/log.h>
#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "SDL", __VA_ARGS__)
#else
#include <SDL3/SDL_log.h>
#define LOG(...) SDL_Log(__VA_ARGS__)
#endif
