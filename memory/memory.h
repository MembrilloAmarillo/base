/* date = April 1st 2025 2:58 pm */

#ifndef MEMORY_H
#define MEMORY_H

#ifdef __linux__
#include "./linux/memory_linux_impl.h"
#else
#ifdef _WIN32
#include "./win/memory_win_impl.h"
#endif
#endif

class Arena : memory_impl {
    public:
    Arena()            = delete;
    Arena( Arena  &m ) = delete;
    Arena( Arena &&m ) = delete;
    ~Arena()           = delete;
    
    void Init( U64 CommitSize, U64 ReserveSize );
    template<typename T> 
        T* Push( U64 Size );
    template<typename T>
        void Pop( U64 size);
    void FreeAll();
    
    U64 current_offset;
};

#endif //MEMORY_H