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
    
    void Init( u64 CommitSize, u64 ReserveSize );
    template<typename T> 
        T* Push( u64 Size );
    template<typename T>
        void Pop( u64 size);
    void FreeAll();
    
    u64 current_offset;
};

#endif //MEMORY_H