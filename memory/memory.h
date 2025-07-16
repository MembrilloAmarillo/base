#ifndef MEMORY_H
#define MEMORY_H

#ifdef __linux__
#include "./linux/memory_linux_impl.h"
#else
#ifdef _WIN32
#include "./win/memory_win_impl.h"
#endif
#endif

#include "allocator.h"
#include "buddy_allocator.h"
#include "pool_allocator.h"

#endif //MEMORY_H
