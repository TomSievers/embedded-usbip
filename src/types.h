#pragma once

#include <stddef.h>

typedef void* (*alloc_fn)(size_t size);
typedef void (*free_fn)(void* obj);