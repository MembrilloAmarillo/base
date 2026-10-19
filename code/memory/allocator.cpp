#define MEMORY_IMPL
#include "allocator.h"

namespace Mem_Allocator {
  fn_internal Allocator Make(Arena* arena) {

    Allocator allocator = {
        .arena = arena,
        .first = nullptr,
        .last = nullptr,
        .current = nullptr,
        .free_blocks = static_cast<FreeBlock*>(ArenaPush(arena, sizeof(FreeBlock)))
    };

    DLIST_INIT(allocator.free_blocks);

    #ifndef NDEBUG
    allocator.total_allocated = 0;
    allocator.total_freed = 0;
    #endif
    return allocator;
  }

  fn_internal void Init(Allocator* allocator) {
    MemoryBlock* data = static_cast<MemoryBlock*>(ArenaPush(allocator->arena, sizeof(MemoryBlock)));
    allocator->current = allocator->first = allocator->last = data;
  }

  template <typename T> fn_internal T* Make(Allocator* allocator, u64 capacity) {
    if (allocator->current == nullptr) {
        Init(allocator);
    }
    if( allocator->free_blocks ) {
        for( auto block = allocator->free_blocks->Next; block != allocator->free_blocks && block; block = block->Next ) {
            if (block->Size >= sizeof(T) * capacity) {
                void* result = block->Data;
                // block->Data = (char*)block->Data + sizeof(T) * capacity;
                // block->Size -= sizeof(T) * capacity;
                block->Prev->Next = block->Next;
                block->Next->Prev = block->Prev;
                #ifndef NDEBUG
                allocator->total_freed -= block->Size;
                #endif
                return reinterpret_cast<T*>(result);
            }
        }
    }
    u64 size = sizeof(T) * capacity;
    MemoryBlock* block = static_cast<MemoryBlock*>(ArenaPush(allocator->arena, sizeof(MemoryBlock) + size));
    block->data = (char*)block + sizeof(MemoryBlock);
    block->size = size;
    block->offset_from_start = (char*)block->data - (char*)allocator->arena;
    if (allocator->last) {
        allocator->last->next = block;
        block->previous = allocator->last;
    }
    allocator->last = block;
    if (allocator->current == nullptr) {
        allocator->current = block;
    }

    #ifndef NDEBUG
    allocator->total_allocated += block->size;
    #endif
    return reinterpret_cast<T*>(block->data);
  }

  template <typename T> fn_internal T* Make(Allocator* allocator, u64 len, u64 capacity) {
    if (allocator->current == nullptr) {
        Init(allocator);
    }
    if( allocator->free_blocks ) {
        for( auto block = allocator->free_blocks->Next; block != allocator->free_blocks && block; block = block->Next ) {
            if (block->Size >= sizeof(T) * capacity) {
                void* result = block->Data;
                // block->Data = (char*)block->Data + sizeof(T) * capacity;
                // block->Size -= sizeof(T) * capacity;
                block->Prev->Next = block->Next;
                block->Next->Prev = block->Prev;
                #ifndef NDEBUG
                allocator->total_freed -= block->Size;
                #endif
                return reinterpret_cast<T*>(result);
            }
        }
    }
    u64 size = sizeof(T) * capacity;
    MemoryBlock* block = static_cast<MemoryBlock*>(ArenaPush(allocator->arena, sizeof(MemoryBlock) + size));
    block->data = (char*)block + sizeof(MemoryBlock);
    block->size = size;
    block->offset_from_start = (char*)block->data - (char*)allocator->arena;
    if (allocator->last) {
        allocator->last->next = block;
        block->previous = allocator->last;
    }
    allocator->last = block;
    if (allocator->current == nullptr) {
        allocator->current = block;
    }
    #ifndef NDEBUG
    allocator->total_allocated += block->size;
    #endif
    return reinterpret_cast<T*>(block->data);
  }

  template <typename T> fn_internal void Delete(Allocator* allocator, T* ptr) {
    for( auto block = allocator->first->next; block != allocator->first && block; block = block->next ) {
        if (reinterpret_cast<uintptr_t>(block->data) == reinterpret_cast<uintptr_t>(ptr)) {
            FreeBlock* free_block = static_cast<FreeBlock*>(ArenaPush(allocator->arena, sizeof(FreeBlock)));
            *free_block = FreeBlock{block->data, block->size, nullptr, nullptr};
            DLIST_INSERT_AS_LAST(allocator->free_blocks, free_block);

            #ifndef NDEBUG
            allocator->total_freed += block->size;
            #endif
            break;
        }
    }
  }

  fn_internal void Clear(Allocator* allocator) {
    allocator->current = allocator->first;
    allocator->free_blocks = nullptr;
  }
}
