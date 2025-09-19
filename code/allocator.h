#ifndef _ALLOCATOR_H_
#define _ALLOCATOR_H_

/**
 * @author Ginger Bill
 * @note This code has been presented on https://www.gingerbill.org/article/2019/02/15/memory-allocation-strategies-003/
 * And I have just ported it in here
 * All credits to him
 */

#if !defined(__cplusplus)
	#if (defined(_MSC_VER) && _MSC_VER < 1800) || (!defined(_MSC_VER) && !defined(__STDC_VERSION__))
		#ifndef true
		#define true  (0 == 0)
		#endif
		#ifndef false
		#define false (0 != 0)
		#endif
		typedef unsigned char bool;
	#else
		#include <stdbool.h>
	#endif
#endif

#include <stdlib.h>
#include <assert.h>

#ifndef DEFAULT_ALIGNMENT
#define DEFAULT_ALIGNMENT 8
#endif

typedef enum Allocator_Mode Allocator_Mode;
enum Allocator_Mode {
    Alloc,
	Free,
	Free_All,
	Resize,
	Query_Features,
	Query_Info,
	Alloc_Non_Zeroed,
	Resize_Non_Zeroed,
};

typedef struct Allocator Allocator;
struct Allocator {
    void* (*Allocator_Proc)(void* data, Allocator_Mode mode, i64 size, i64 alignment, void* old_memory, i64 old_size);
    void* data;
};

typedef struct Buddy_Block Buddy_Block;
struct Buddy_Block { // Allocation header (metadata)
    size_t size;
    bool   is_free;
};

typedef struct Buddy_Allocator Buddy_Allocator;
struct Buddy_Allocator {
    Buddy_Block *head; // same pointer as the backing memory data
    Buddy_Block *tail; // sentinel pointer representing the memory boundary
    size_t alignment;
};

bool is_power_of_two(uintptr_t x);

uintptr_t align_forward(uintptr_t ptr, size_t align);

uintptr_t align_forward_uintptr(uintptr_t ptr, uintptr_t align);

size_t align_forward_size(size_t ptr, size_t align);

Buddy_Block *buddy_block_next(Buddy_Block *block);

Buddy_Block *buddy_block_split(Buddy_Block *block, size_t size);

Buddy_Block *buddy_block_find_best(Buddy_Block *head, Buddy_Block *tail, size_t size);

void buddy_allocator_init(Buddy_Allocator *b, void *data, size_t size, size_t alignment);

size_t buddy_block_size_required(Buddy_Allocator *b, size_t size);

void buddy_block_coalescence(Buddy_Block *head, Buddy_Block *tail);

void *buddy_allocator_alloc(Buddy_Allocator *b, size_t size);

void buddy_allocator_free(Buddy_Allocator *b, void *data);


typedef struct Stack_Allocator Stack_Allocator;
struct Stack_Allocator {
    void* data;
    i64   prev_offset;
    i64   curr_offset;
    i64   peak_used;
    i64   buf_len;
};

typedef struct Stack_Allocation_Header Stack_Allocation_Header;
struct Stack_Allocation_Header {
    i64 prev_offset;
    i64 padding;
};

void stack_init(Stack_Allocator* s, void* data, i64 len);

void* stack_alloc(Stack_Allocator* s, i64 size, i64 alignment);

void* stack_alloc_non_zeroed(Stack_Allocator* s, i64 size, i64 alignment);

void* stack_free(Stack_Allocator* s, void* old_memory);

void  stack_free_all(Stack_Allocator* s);

void* stack_resize(Stack_Allocator* s, void* old_memory, i64 old_size, i64 size, i64 alignment);

void* stack_resize_non_zeroed(Stack_Allocator* s, void* old_memory, i64 old_size, i64 size, i64 alignment);

void* *stack_allocator_proc(void* data, Allocator_Mode mode, i64 size, i64 alignment, void* old_memory, i64 old_size);

#define stack_push(s, type, size) (type*)stack_alloc(s, sizeof(type) * size, DEFAULT_ALIGNMENT)


#endif // _ALLOCATOR_H_

#ifdef ALLOCATOR_IMPL

uintptr_t align_forward_uintptr(uintptr_t ptr, uintptr_t align) {
	uintptr_t a, p, modulo;

	assert(is_power_of_two(align));

	a = align;
	p = ptr;
	modulo = p & (a-1);
	if (modulo != 0) {
		p += a - modulo;
	}
	return p;
}

size_t align_forward_size(size_t ptr, size_t align) {
	size_t a, p, modulo;

	assert(is_power_of_two((uintptr_t)align));

	a = align;
	p = ptr;
	modulo = p & (a-1);
	if (modulo != 0) {
		p += a - modulo;
	}
	return p;
}


bool is_power_of_two(uintptr_t x) {
	return (x & (x-1)) == 0;
}

uintptr_t align_forward(uintptr_t ptr, size_t align) {
	uintptr_t p, a, modulo;

	assert(is_power_of_two(align));

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

Buddy_Block *buddy_block_next(Buddy_Block *block) {
    return (Buddy_Block *)((char *)block + block->size);
}

Buddy_Block *buddy_block_split(Buddy_Block *block, size_t size) {
    if (block != NULL && size != 0) {
        // Recursive split
        while (size < block->size) {
            size_t sz = block->size >> 1;
            block->size = sz;
            block = buddy_block_next(block);
            block->size = sz;
            block->is_free = true;
        }

        if (size <= block->size) {
            return block;
        }
    }

    // Block cannot fit the requested allocation size
    return NULL;
}

Buddy_Block *buddy_block_find_best(Buddy_Block *head, Buddy_Block *tail, size_t size) {
    // Assumes size != 0

    Buddy_Block *best_block = NULL;
    Buddy_Block *block = head;                    // Left Buddy
    Buddy_Block *buddy = buddy_block_next(block); // Right Buddy

    // The entire memory section between head and tail is free,
    // just call 'buddy_block_split' to get the allocation
    if (buddy == tail && block->is_free) {
        return buddy_block_split(block, size);
    }

    // Find the block which is the 'best_block' to requested allocation sized
    while (block < tail && buddy < tail) { // make sure the buddies are within the range
        // If both buddies are free, coalesce them together
        // NOTE: this is an optimization to reduce fragmentation
        //       this could be completely ignored
        if (block->is_free && buddy->is_free && block->size == buddy->size) {
            block->size <<= 1;
            if (size <= block->size && (best_block == NULL || block->size <= best_block->size)) {
                best_block = block;
            }

            block = buddy_block_next(buddy);
            if (block < tail) {
                // Delay the buddy block for the next iteration
                buddy = buddy_block_next(block);
            }
            continue;
        }


        if (block->is_free && size <= block->size &&
            (best_block == NULL || block->size <= best_block->size)) {
            best_block = block;
        }

        if (buddy->is_free && size <= buddy->size &&
            (best_block == NULL || buddy->size < best_block->size)) {
            // If each buddy are the same size, then it makes more sense
            // to pick the buddy as it "bounces around" less
            best_block = buddy;
        }

        if (block->size <= buddy->size) {
            block = buddy_block_next(buddy);
            if (block < tail) {
                // Delay the buddy block for the next iteration
                buddy = buddy_block_next(block);
            }
        } else {
            // Buddy was split into smaller blocks
            block = buddy;
            buddy = buddy_block_next(buddy);
        }
    }

    if (best_block != NULL) {
        // This will handle the case if the 'best_block' is also the perfect fit
        return buddy_block_split(best_block, size);
    }

    // Maybe out of memory
    return NULL;
}

void buddy_allocator_init(Buddy_Allocator *b, void *data, size_t size, size_t alignment) {
    assert(data != NULL);
    assert(is_power_of_two(size) && "size is not a power-of-two");
    assert(is_power_of_two(alignment) && "alignment is not a power-of-two");

    // The minimum alignment depends on the size of the `Buddy_Block` header
    assert(is_power_of_two(sizeof(Buddy_Block)));
    if (alignment < sizeof(Buddy_Block)) {
        alignment = sizeof(Buddy_Block);
    }
    assert(((uintptr_t)data % alignment == 0) && "data is not aligned to minimum alignment");

    b->head          = (Buddy_Block *)data;
    b->head->size    = size;
    b->head->is_free = true;

    // The tail here is a sentinel value and not a true block
    b->tail = buddy_block_next(b->head);

    b->alignment = alignment;
}

size_t buddy_block_size_required(Buddy_Allocator *b, size_t size) {
    size_t actual_size = b->alignment;

    size += sizeof(Buddy_Block);
    size = align_forward_size(size, b->alignment);

    while (size > actual_size) {
        actual_size <<= 1;
    }

    return actual_size;
}

void buddy_block_coalescence(Buddy_Block *head, Buddy_Block *tail) {
    for (;;) {
        // Keep looping until there are no more buddies to coalesce

        Buddy_Block *block = head;
        Buddy_Block *buddy = buddy_block_next(block);

        bool no_coalescence = true;
        while (block < tail && buddy < tail) { // make sure the buddies are within the range
            if (block->is_free && buddy->is_free && block->size == buddy->size) {
                // Coalesce buddies into one
                block->size <<= 1;
                block = buddy_block_next(block);
                if (block < tail) {
                    buddy = buddy_block_next(block);
                    no_coalescence = false;
                }
            } else if (block->size < buddy->size) {
                // The buddy block is split into smaller blocks
                block = buddy;
                buddy = buddy_block_next(buddy);
            } else {
                block = buddy_block_next(buddy);
                if (block < tail) {
                    // Leave the buddy block for the next iteration
                    buddy = buddy_block_next(block);
                }
            }
        }

        if (no_coalescence) {
            return;
        }
    }
}

void *buddy_allocator_alloc(Buddy_Allocator *b, size_t size) {
    if (size != 0) {
        size_t actual_size = buddy_block_size_required(b, size);

        Buddy_Block *found = buddy_block_find_best(b->head, b->tail, actual_size);
        if (found == NULL) {
            // Try to coalesce all the free buddy blocks and then search again
            buddy_block_coalescence(b->head, b->tail);
            found = buddy_block_find_best(b->head, b->tail, actual_size);
        }

        if (found != NULL) {
            found->is_free = false;
            return (void *)((char *)found + b->alignment);
        }

        // Out of memory (possibly due to too much internal fragmentation)
    }

    return NULL;
}

void buddy_allocator_free(Buddy_Allocator *b, void *data) {
    if (data != NULL) {
        Buddy_Block *block;

        assert(b->head <= data);
        assert(data < b->tail);

        block = (Buddy_Block *)((char *)data - b->alignment);
        block->is_free = true;

        // NOTE: Coalescence could be done now but it is optional
        // buddy_block_coalescence(b->head, b->tail);
    }
}

void
stack_init(Stack_Allocator* s, void* data, i64 len) {
    s->data        = data;
	s->prev_offset = 0;
	s->curr_offset = 0;
	s->peak_used   = 0;
    s->buf_len     = len;
}

void*
stack_alloc(Stack_Allocator* s, i64 size, i64 alignment) {
    return stack_alloc_non_zeroed(s, size, alignment);
}

size_t
calc_padding_with_header(uintptr_t ptr, uintptr_t alignment, size_t header_size) {
	uintptr_t p, a, modulo, padding, needed_space;

	assert(is_power_of_two(alignment));

	p = ptr;
	a = alignment;
	modulo = p & (a-1); // (p % a) as it assumes alignment is a power of two

	padding = 0;
	needed_space = 0;

	if (modulo != 0) { // Same logic as 'align_forward'
		padding = a - modulo;
	}

	needed_space = (uintptr_t)header_size;

	if (padding < needed_space) {
		needed_space -= padding;

		if ((needed_space & (a-1)) != 0) {
			padding += a * (1+(needed_space/a));
		} else {
			padding += a * (needed_space/a);
		}
	}

	return (size_t)padding;
}

void*
stack_alloc_non_zeroed(Stack_Allocator* s, i64 size, i64 alignment) {
    if( s->data == NULL ) {
        assert(s->data != NULL && "Allocation on an uninitialized Stack allocator.");
    }

    void* curr_addr = s->data + s->curr_offset;
    i64 padding = calc_padding_with_header((uintptr_t)curr_addr, alignment, sizeof(Stack_Allocation_Header));

    if( s->curr_offset + padding + size > s->buf_len ) {
        return NULL;
    }

    i64 old_offset = s->prev_offset;
    s->prev_offset = s->curr_offset;
    s->curr_offset += padding;
    void* next_addr = curr_addr + padding;

    Stack_Allocation_Header* header = next_addr - sizeof(Stack_Allocation_Header);
    header->padding = padding;
    header->prev_offset = old_offset;
    s->curr_offset += size;
    s->peak_used = Max(s->peak_used, s->curr_offset);

    return memset((void *)next_addr, 0, size);
}

void*
stack_free(Stack_Allocator* s, void* old_memory) {
    if( s->data == NULL ) {
        assert(s->data != NULL && "Allocation on an uninitialized Stack allocator.");
    }
    if( old_memory == NULL ) {
        return NULL;
    }

    uintptr_t start = (uintptr_t)s->data;
    uintptr_t end   = start + (uintptr_t)s->buf_len;
    uintptr_t curr_addr = (uintptr_t)old_memory;
    if( !(start <= curr_addr && curr_addr < end) ) {
        assert(false &&"Out of bounds memory address passed to Stack allocator. (free)");
    }
    if( curr_addr >= start + (uintptr_t)s->curr_offset ) {
		return NULL;
	}

    Stack_Allocation_Header* header = (Stack_Allocation_Header*)(curr_addr - sizeof(Stack_Allocation_Header));
    i64 old_offset = (i64)( curr_addr - (uintptr_t)header->padding - (uintptr_t)s->data );
    if( old_offset != s->prev_offset ) {
        assert(false && "Out of order stack free");
    }

    s->prev_offset = header->prev_offset;
    s->curr_offset = old_offset;

    return NULL;
}

void
stack_free_all(Stack_Allocator* s) {
    s->prev_offset = 0;
    s->curr_offset = 0;
}

void*
stack_resize(Stack_Allocator* s, void* old_memory, i64 old_size, i64 size, i64 alignment) {
    return stack_resize_non_zeroed(s, old_memory, old_size, size, alignment);
}

void*
stack_resize_non_zeroed(Stack_Allocator* s, void* old_memory, i64 old_size, i64 size, i64 alignment) {
    if( s->data == NULL ) {
        assert(s->data != NULL && "Allocation on an uninitialized Stack allocator.");
    }

    if( old_memory == NULL ) {
        return stack_alloc_non_zeroed(s, size, alignment);
    }

    if( size == 0 ) {
        return stack_free(s, old_memory);
    }
    uintptr_t start = (uintptr_t)s->data;
    uintptr_t end   = start + (uintptr_t)s->buf_len;
    uintptr_t curr_addr = (uintptr_t)old_memory;
    if( !(start <= curr_addr && curr_addr < end) ) {
        assert(false &&"Out of bounds memory address passed to Stack allocator. (free)");
    }
    if( curr_addr >= start + (uintptr_t)s->curr_offset ) {
		return NULL;
	}
    if( (uintptr_t)old_memory & (uintptr_t)(alignment - 1) != 0 ) {
        void* data = stack_alloc_non_zeroed(s, size, alignment);

        if(data != NULL) {
            return data;
        }
    }
    if( old_size == size ) {
        return memset(old_memory, 0, size);
    }

    Stack_Allocation_Header* header = (Stack_Allocation_Header*)(curr_addr - sizeof(Stack_Allocation_Header));
    i64 old_offset = (i64)( curr_addr - (uintptr_t)header->padding - (uintptr_t)s->data );
    if( old_offset != s->prev_offset ) {
        void* data = stack_alloc_non_zeroed(s, size, alignment);
        if( data != NULL ) {
            memcpy(data, memset(old_memory, 0, old_size), old_size);
        }
    }

    uintptr_t old_memory_size = (uintptr_t)s->curr_offset - (curr_addr - start);
    assert(old_memory_size == (uintptr_t)old_size);
    uintptr_t diff = size - (uintptr_t)old_size;
    s->curr_offset += diff;
    if( diff > 0 ) {
        memset((void*)curr_addr + diff, 0, diff);
    }

    return memset(old_memory, 0, size);
}

#endif
