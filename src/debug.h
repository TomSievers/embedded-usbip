#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#ifdef DEBUG
#define DEBUG_PRINT(fmt, ...) printf(fmt, __VA_ARGS__)
#else
#define DEBUG_PRINT(fmt, ...)
#endif

#ifdef __cplusplus
}   
#endif