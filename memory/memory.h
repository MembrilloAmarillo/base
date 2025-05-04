#ifndef MEMORY_H
#define MEMORY_H

#ifdef __linux__
#include "./linux/memory_linux_impl.h"
#else
#ifdef _WIN32
#include "./win/memory_win_impl.h"
#endif
#endif

#include <cassert>

class Arena : public memory_impl {
public:
    Arena()        = delete;
    Arena(Arena&)  = delete;
    Arena(Arena&&) = delete;
    ~Arena()       = delete;

    void Init(U64 CommitSize, U64 ReserveSize) {
        this->memory_impl::Init(CommitSize, ReserveSize);
        current_offset = 0;
    }

    template<typename T>
    T* Push(U64 count) {
        size_t total = sizeof(T) * count;
#ifndef NDEBUG
        assert(current_offset + total < GetReservedSize());
#endif
        T* ptr = (T*)this->memory_impl::Commit(total);
        if (!ptr) return nullptr;
        current_offset += total;
        return ptr;
    }

    template<typename T>
    void Pop(U64 count) {
        size_t total = sizeof(T) * count;
        this->memory_impl::Decommit(total);
        if (current_offset > total)
            current_offset -= total;
        else
            current_offset = 0;
    }

    void FreeAll() {
        this->memory_impl::Release();
        current_offset = 0;
    }

    U64 current_offset = 0;
};

#endif //MEMORY_H
