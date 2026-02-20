#ifndef _ALLOCATOR_H_
#define _ALLOCATOR_H_

#include "../util/types.h"
#include "memory.h"

/**
 * @author: MembrilloAmarillo
 * @version: 0.1
 * @description: A simple allocator that composes of an internal arena to push the memory and then
 * manages the memory blocks and uses a free list to keep track of the freed blocks. The allocator does not support coalescing of free blocks, so it may lead to fragmentation over time. However, it is designed to be simple and fast for use in scenarios where memory allocation and deallocation patterns are predictable and do not lead to excessive fragmentation. The allocator also keeps track of the total allocated and freed memory for debugging purposes, which can help identify memory leaks or excessive fragmentation.
 */

/**
 * @todo Highly inneficient as it has to traverse the whole list until it finds a fitted
 * memory block. It can be further improved.
 */
typedef struct FreeBlock FreeBlock;
struct FreeBlock {
  void* Data;
  u64   Size;

  FreeBlock* Next;
  FreeBlock* Prev;
};

struct MemoryBlock {
    void* data;
    u64 size;
    u64 offset_from_start;
    MemoryBlock* next;
    MemoryBlock* previous;

    MemoryBlock(void* data, u64 size, u64 offset_from_start) : data(data), size(size), offset_from_start(offset_from_start) {}
};

typedef struct Allocator Allocator;
struct Allocator {
  Arena* arena;
  MemoryBlock* first;
  MemoryBlock* last;
  MemoryBlock* current;

  FreeBlock* free_blocks;

  #ifndef NDEBUG
  u64 total_allocated;
  u64 total_freed;
  #endif
};

namespace Mem_Allocator {
  fn_internal Allocator Make(Arena* arena);
  fn_internal void Init(Allocator* allocator);
  template <typename T> fn_internal T* Make(Allocator* allocator, u64 capacity);
  template <typename T> fn_internal T* Make(Allocator* allocator, u64 len, u64 capacity);
  template <typename T> fn_internal void Delete(Allocator* allocator, T* ptr);
  fn_internal void Clear(Allocator* allocator);
};

#endif // _ALLOCATOR_H_