#pragma once

#include <windows.h>
#include <stdio.h>

#define LOG(fmt, ...) \
    do { \
        wchar_t buffer[512]; \
        swprintf_s(buffer, sizeof(buffer)/sizeof(wchar_t), fmt, __VA_ARGS__); \
        OutputDebugStringW(buffer); \
    } while (0)