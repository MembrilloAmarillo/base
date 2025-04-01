#include "./memory.h"

void Arena::Init( u64 CommitSize, u64 ReserveSize ) {
    this->memory_impl::Init( CommitSize, ReserveSize ); 
    current_offset = 0;
}

template<typename T> 
T* Arena::Push( u64 Size ) {
    T* data = (T*)this->memory_impl::Commit( sizeof(T) * Size );
    current_offset += sizeof(T) * Size;
    return data;
}

template<typename T> 
void Arena::Pop( u64 Size ) {
    this->memory_impl::Decommit( sizeof(T) * Size );
    if (current_offset > sizeof(T) * Size) {
        current_offset -= sizeof(T) * Size;
    }
    else {
        current_offset = 0;
    }
}

void Arena::FreeAll() {
    this->memory_impl::Release();
}