#include <cassert>
#include <chrono>
#include <cstdio>

#include "../memory/memory.h"
#include "../memory/allocator.h"
#include "../vector/DynamicVector.h"

#define MEMORY_IMPL
#include "../memory/memory.h"
#include "../memory/allocator.cpp"
#include "../vector/DynamicVector.cpp"

int main() { 
    Arena arena = {}; 
    Arena* arena_ptr = &arena; 
    arena_ptr = ArenaAllocDefault(); 
    Allocator allocator = Mem_Allocator::Make(arena_ptr); 
    dyn_vector<int> vec = vec.Init(&allocator, 2000); 
    for (int i = 0; i < 2000; i++) { 
        vec.AppendByCopy(i); 
        assert(vec.At(i) == i); 
    } 
    for (int i = 0; i < 2000; i++) { 
        vec.Delete(0); 
        assert(vec.Length() == 1999 - i); 
        for (int j = 0; j < vec.Length(); j++) { 
            assert(vec.At(j) == j + i + 1); 
        } 
    } 

    vec.Resize(20000); 
    for (int i = 0; i < 20000; i++) { 
        vec.AppendByCopy(i); 
        assert(vec.At(i) == i); 
    }

    dyn_vector<int> vec2 = vec.Init(&allocator, 2000); 
    for (int i = 0; i < 2000; i++) { 
        vec2.AppendByCopy(i); 
        assert(vec2.At(i) == i); 
    }

    vec.Destroy(); vec2.Destroy();

    printf("Total free bytes: %llu, total allocated bytes: %llu\n", allocator.total_freed, allocator.total_allocated);
    printf("Arena commited size: %llu bytes\n", arena_ptr->commit_pos);
    return 0; 
}