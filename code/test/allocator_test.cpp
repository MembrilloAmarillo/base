#include <cassert>
#include <chrono>
#include <cstdio>

#include "../memory/memory.h"
#include "../memory/allocator.h"

#define MEMORY_IMPL
#include "../memory/memory.h"
#include "../memory/allocator.cpp"

constexpr U64 num_iterations = 1000000;

int main() {
  Arena arena = {};
  Arena* arena_ptr = &arena;
  arena_ptr = ArenaAllocDefault();

  Allocator allocator = Mem_Allocator::Make(arena_ptr);
  // time the allocation of 10 integers
  auto start = std::chrono::high_resolution_clock::now();

  for (U64 i = 0; i < num_iterations; i++) {
    int* data = (int*)Mem_Allocator::Make<int>(&allocator, 2);

    int* data3 = (int*)Mem_Allocator::Make<int>(&allocator, 200);

    int* data2 = (int*)Mem_Allocator::Make<int>(&allocator, 4);

    Mem_Allocator::Delete(&allocator, data);
    Mem_Allocator::Delete(&allocator, data2);
    Mem_Allocator::Delete(&allocator, data3);
  }
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed = end - start;

  printf("Free List Impl: %llu iterations\nTook %f milliseconds\n\t total allocated: %llu bytes, total freed: %llu bytes\n", num_iterations, elapsed.count() * 1000, allocator.total_allocated, allocator.total_freed);
  // std::cout << "Iteration " << i << " took " << elapsed.count() << " seconds\n";

  start = std::chrono::high_resolution_clock::now();
  for (U64 i = 0; i < num_iterations; i++) {
    int* data  = PushArray(arena_ptr, int, 2);
    int* data3 = PushArray(arena_ptr, int, 200);
    int* data2 = PushArray(arena_ptr, int, 4);

    PopArray(arena_ptr, int, 4);
    PopArray(arena_ptr, int, 200);
    PopArray(arena_ptr, int, 2 + 200 + 4);
  }

  end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed2 = end - start;
  printf("Arena Impl: %llu iterations\nTook %f milliseconds\n\t total commited: %llu bytes, pos: %llu\n", num_iterations, elapsed2.count() * 1000, arena_ptr->commit_pos, arena_ptr->pos);

  return 0;
}