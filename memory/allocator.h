#ifndef _CUSTOM_ALLOCATOR_H_
#define _CUSTOM_ALLOCATOR_H_

#ifndef DEFAULT_ALIGNMENT
#define DEFAULT_ALIGNMENT (2*sizeof(void *))
#endif

#include <assert.h>

#include "../types/types.h"

typedef struct Arena Arena;
struct Arena {
    U8* buffer;
    U64 len;
    U64 prev_offset;
    U64 curr_offset;
};

typedef struct Temp_Arena_Memory Temp_Arena_Memory;
struct Temp_Arena_Memory {
	Arena *arena;
	size_t prev_offset;
	size_t curr_offset;
};

Temp_Arena_Memory TempArenaMemoryBegin(Arena *a);
void TempArenaMemoryEnd(Temp_Arena_Memory temp);

uintptr_t AlignForward(uintptr_t ptr, size_t align);
bool IsPowTwo(uintptr_t x);

Arena ArenaInit( Arena *a, void *backing_buffer, size_t backing_buffer_length );

void* ArenaAllocAlign( Arena* ar,  U64 size, U64 align );

void *ArenaResizeAlign(Arena *a, void *old_memory, size_t old_size, size_t new_size);

void *ArenaResize(Arena *a, void *old_memory, size_t old_size, size_t new_size);

void  ArenaFree(Arena *a, void *ptr);

void ArenaFreeAll(Arena *a);

#define ArenaAlloc(arena, size) ArenaAllocAlign(arena, size, DEFAULT_ALIGNMENT)

#endif // ALLOCATOR_IMPL

uintptr_t AlignForward(uintptr_t ptr, size_t align) {
	uintptr_t p, a, modulo;

	assert(IsPowTwo(align));

	p = ptr;
	a = (uintptr_t)align;
	// Same as (p % a) but faster as 'a' is a power of two
	modulo = p & (a-1);

	if (modulo != 0) {
		// If 'p' address is not aligned, push the address to the
		// next value which is aligned
		p += a - modulo;
	}

	return p;
}

bool IsPowTwo(uintptr_t x) {
    return (x & (x-1)) == 0;
}

Temp_Arena_Memory TempArenaMemoryBegin(Arena *a) {
	Temp_Arena_Memory temp;
	temp.arena = a;
	temp.prev_offset = a->prev_offset;
	temp.curr_offset = a->curr_offset;
	return temp;
}

void TempArenaMemoryEnd(Temp_Arena_Memory temp) {
	temp.arena->prev_offset = temp.prev_offset;
	temp.arena->curr_offset = temp.curr_offset;
}

Arena ArenaInit( Arena *a, void *backing_buffer, size_t backing_buffer_length ) {
    a->buffer      = (unsigned char *)backing_buffer;
	a->len         = backing_buffer_length;
	a->curr_offset = 0;
	a->prev_offset = 0;
}

void* ArenaAllocAlign( Arena* ar,  U64 size, U64 align ) {
    // Align 'curr_offset' forward to the specified alignment
	uintptr_t curr_ptr = (uintptr_t)a->buf + (uintptr_t)a->curr_offset;
	uintptr_t offset = align_forward(curr_ptr, align);
	offset -= (uintptr_t)a->buf; // Change to relative offset

	// Check to see if the backing memory has space left
	if (offset+size <= a->buf_len) {
		void *ptr = &a->buf[offset];
		a->prev_offset = offset;
		a->curr_offset = offset+size;

		// Zero new memory by default
		memset(ptr, 0, size);
		return ptr;
	}
	// Return NULL if the arena is out of memory (or handle differently)
	return NULL;
}

void *ArenaResizeAlign(Arena *a, void *old_memory, size_t old_size, size_t new_size, size_t align) {
	unsigned char *old_mem = (unsigned char *)old_memory;

	assert(is_power_of_two(align));

	if (old_mem == NULL || old_size == 0) {
		return arena_alloc_align(a, new_size, align);
	} else if (a->bufffer <= old_mem && old_mem < a->buffer+a->len) {
		if (a->buffer+a->prev_offset == old_mem) {
			a->curr_offset = a->prev_offset + new_size;
			if (new_size > old_size) {
				// Zero the new memory by default
				memset(&a->buffer[a->curr_offset], 0, new_size-old_size);
			}
			return old_memory;
		} else {
			void *new_memory = ArenaAllocAlign(a, new_size, align);
			size_t copy_size = old_size < new_size ? old_size : new_size;
			// Copy across old memory to the new memory
			memmove(new_memory, old_memory, copy_size);
			return new_memory;
		}

	} else {
		assert(0 && "Memory is out of bounds of the buffer in this arena");
		return NULL;
	}
}

void *ArenaResize(Arena *a, void *old_memory, size_t old_size, size_t new_size) {
	return arena_resize_align(a, old_memory, old_size, new_size, DEFAULT_ALIGNMENT);
}

void ArenaFree(Arena *a, void *ptr) {
	// Do nothing
}

void ArenaFreeAll(Arena *a) {
	a->curr_offset = 0;
	a->prev_offset = 0;
}

#ifdef ALLOCATOR_IMPL
#endif // ALLOCATOR_IMPL