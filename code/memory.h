/* date = February 1st 2024 9:00 pm */
/*
Really easy and naive memory arena, inspired by Ryan Fleury Memory Managment post.

It commit chunks of n * 4KiB, that is the size of a page in Windows. This way we ensure
that we are easing the work for the os in terms of memory managment.
*/

#ifndef MEMORY_H
#define MEMORY_H

#define fn_internal static
#define U8       uint8_t
#define U64      uint64_t
#define U32      uint32_t

#ifndef _WIN32
#define _GNU_SOURCE

#include <unistd.h>
#include <sys/resource.h>
#include <sched.h>
#include <sys/utsname.h>

#include <sys/mman.h>
#endif

#ifdef __cplusplus

#include <cstddef>

#define _Alignof alignof

#else
#include <stddef.h>
#endif

#define kibibyte( size ) (size << 10)
#define mebibyte( size ) (size << 20)
#define gigabyte( size ) (((U64)size) << 30)

#define PAGE_SIZE        kibibyte( 4 )
#define DEFAULT_RESERVE  gigabyte( 8 )
#define DEFAULT_COMMIT   PAGE_SIZE
#define DEFAULT_DECOMMIT PAGE_SIZE
#define ARENA_DECOMMIT_THRESHOLD mebibyte(64)

#define Max( a, b ) a > b ? a : b
#define Min( a, b ) a < b ? a : b

typedef struct free_block free_block;
struct free_block {
  void* Data;
  u64   Size;

  free_block* Next;
  free_block* Prev;
} free_block;

typedef struct Arena Arena;
struct Arena
{
    U64 size;
    U64 pos;
    U64 commit_pos;
};

typedef struct Temp Temp;
struct Temp
{
 Arena *arena;
 U64 pos;
};

fn_internal void*  ArenaImplReserve ( U64 size );
fn_internal void   ArenaImplCommit  ( void* ptr, U64 size );
fn_internal void   ArenaImplDecommit( void* ptr, U64 size );
fn_internal void   ArenaImplRelease ( void* ptr, U64 size );

fn_internal void ArenaImplCommitWithFlags(void* ptr, U64 size, U64 MemFlags, U64 PageFlags);

fn_internal Arena* ArenaAlloc( U64 size );
fn_internal Arena* ArenaAllocDefault( void );
fn_internal void*  ArenaPush( Arena* arena, U64 size );
fn_internal void   ArenaPop ( Arena* arena, U64 erase  );

fn_internal void* ArenaPushWithFlags(Arena* arena, U64 size, U64 MemFlags, U64 PageFlags);

fn_internal void   ArenaRelease( Arena* arena );
fn_internal void   ArenaClear  ( Arena* arena );

#define PushArray( arena, type, count ) (type*)ArenaPush( arena, sizeof( type ) * count )
#define PopArray( arena, type, count )  ArenaPop( arena, sizeof( type ) * count )

#endif //MEMORY_H


#ifdef MEMORY_IMPL

fn_internal void*
ArenaImplReserve( U64 size )
{
    U64 gb_snapped_size = size;
    gb_snapped_size += gigabyte(1) - 1;
    gb_snapped_size -= gb_snapped_size%gigabyte(1);

#ifdef _WIN32
    void *ptr = VirtualAlloc( 0, gb_snapped_size, MEM_RESERVE, PAGE_NOACCESS );
#else
    void *ptr = mmap( NULL, gb_snapped_size, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0 );
#endif

    return ptr;
}

fn_internal void
ArenaImplCommit( void* ptr, U64 size )
{
#ifdef _WIN32
    SYSTEM_INFO info;
    GetSystemInfo( &info );
    U64 PageSize = info.dwPageSize;

    U64 page_snapped_size = size;
    page_snapped_size += PageSize - 1;
    page_snapped_size -= page_snapped_size % PageSize;

    VirtualAlloc( ptr, page_snapped_size, MEM_COMMIT, PAGE_READWRITE );
#else
    U32 PageSize = sysconf(_SC_PAGE_SIZE);
    U64 page_snapped_size = size;
    page_snapped_size += PageSize - 1;
    page_snapped_size -= page_snapped_size % PageSize;
    mprotect( ptr, page_snapped_size, PROT_READ | PROT_WRITE);
#endif
}

fn_internal void
ArenaImplCommitWithFlags(void* ptr, U64 size, U64 MemFlags, U64 PageFlags) {
#ifdef _WIN32
  SYSTEM_INFO info;
  GetSystemInfo( &info );
  U64 PageSize = info.dwPageSize;

  U64 page_snapped_size = size;
  page_snapped_size += PageSize - 1;
  page_snapped_size -= page_snapped_size % PageSize;

  VirtualAlloc( ptr, page_snapped_size, MemFlags, PageFlags );
#else
  U32 PageSize = sysconf(_SC_PAGE_SIZE);
  U64 page_snapped_size = size;
  page_snapped_size += PageSize - 1;
  page_snapped_size -= page_snapped_size % PageSize;
  mprotect( ptr, page_snapped_size, MemFlags);

#endif
}

fn_internal void
ArenaImplDecommit( void* ptr, U64 size )
{
#ifdef _WIN32
    VirtualFree( ptr, size, MEM_DECOMMIT );
#else
    madvise(ptr, size, MADV_DONTNEED);
    mprotect(ptr, size, PROT_NONE);
#endif
}

fn_internal void
ArenaImplRelease( void* ptr, U64 size )
{
#ifdef _WIN32
    VirtualFree( ptr, 0, MEM_RELEASE );
#else
    munmap( ptr, size );
#endif
}

fn_internal Arena*
ArenaAlloc( U64 size )
{
    void* block = ArenaImplReserve( size );
    ArenaImplCommit( block, DEFAULT_COMMIT );
    Arena* arena      = (Arena*)block;
    arena->pos        = sizeof( Arena );
    arena->commit_pos = DEFAULT_COMMIT;
    arena->size       = size;

    return arena;
}

fn_internal Arena*
ArenaAllocDefault( void )
{
    void* block = ArenaImplReserve( DEFAULT_RESERVE );
    ArenaImplCommit( block, DEFAULT_COMMIT );
    Arena* arena      = (Arena*)block;
    arena->pos        = sizeof( Arena );
    arena->commit_pos = DEFAULT_COMMIT;
    arena->size       = DEFAULT_RESERVE;

    return arena;
}

fn_internal void*
ArenaPush( Arena* arena, U64 size )
{
    uint8_t *base = (uint8_t *)arena;

    /* align pos up to max_align_t boundary */
    U64 aligned_pos = (arena->pos + _Alignof(max_align_t) - 1) & ~(_Alignof(max_align_t) - 1);

    if (aligned_pos + size > arena->size)
        return NULL;                    // out of memory

    void *result     = base + aligned_pos;
    arena->pos       = aligned_pos + size;

    /* commit pages as you already do … */
    if (arena->commit_pos < arena->pos) {
        U64 to_commit = arena->pos - arena->commit_pos;
        to_commit    += DEFAULT_COMMIT - 1;
        to_commit    -= to_commit % DEFAULT_COMMIT;
        ArenaImplCommit(base + arena->commit_pos, to_commit);
        arena->commit_pos += to_commit;
    }

    return result;
}

fn_internal void*
ArenaPushWithFlags( Arena* arena, U64 size, U64 MemFlags, U64 PageFlags )
{
  uint8_t *base = (uint8_t *)arena;

  /* align pos up to max_align_t boundary */
  U64 aligned_pos = (arena->pos + _Alignof(max_align_t) - 1) & ~(_Alignof(max_align_t) - 1);

  if (aligned_pos + size > arena->size)
    return NULL;                    // out of memory

  void *result     = base + aligned_pos;
  arena->pos       = aligned_pos + size;

  /* commit pages as you already do … */
  if (arena->commit_pos < arena->pos) {
    U64 to_commit = arena->pos - arena->commit_pos;
    to_commit    += DEFAULT_COMMIT - 1;
    to_commit    -= to_commit % DEFAULT_COMMIT;
    ArenaImplCommitWithFlags(base + arena->commit_pos, to_commit, MemFlags, PageFlags);
    arena->commit_pos += to_commit;
  }

  return result;
}

fn_internal void
ArenaPopTo(Arena *arena, U64 pos)
{
 U64 min_pos = sizeof(Arena);
 U64 new_pos = Max(min_pos, pos);
 arena->pos = new_pos;
 U64 pos_aligned_to_commit_chunks = arena->pos + DEFAULT_COMMIT-1;
 pos_aligned_to_commit_chunks -= pos_aligned_to_commit_chunks%DEFAULT_COMMIT;
 if(pos_aligned_to_commit_chunks + ARENA_DECOMMIT_THRESHOLD <= arena->commit_pos)
 {
  U8 *base = (U8 *)arena;
  U64 size_to_decommit = arena->commit_pos-pos_aligned_to_commit_chunks;
  ArenaImplDecommit(base + pos_aligned_to_commit_chunks, size_to_decommit);
  arena->commit_pos -= size_to_decommit;
 }
}


fn_internal void
ArenaPop( Arena* arena, U64 erase  )
{
    /*
    U64 min_pos = sizeof( Arena );
    U64 to_pop  = Min( erase, arena->pos );
    U64 new_pos = arena->pos - to_pop;
    new_pos     = Max( min_pos, erase );

    arena->pos -= new_pos;
    */

    arena->pos -= erase;

    U64 decommit_file_size = erase;
    decommit_file_size += DEFAULT_DECOMMIT;
    decommit_file_size  -= decommit_file_size % DEFAULT_DECOMMIT;
    decommit_file_size  -= DEFAULT_DECOMMIT;
    // Now we have that we are going to decommit the proportional part
    // in page sizes. However, if the erasing size given by the user
    // is not n * DEFAULT_COMMIT, we will not be able to decommit everything,
    // because if we do it, we will be erasing other data that we still want
    // to use in that page
    //
    arena->commit_pos -= decommit_file_size;
    // NOTE: Maybe we want to put a threshold different to 4 Kib when we want
    // to decommit. I have to research.
    ArenaImplDecommit( (char*)arena + arena->commit_pos, decommit_file_size );
}

fn_internal void
ArenaRelease( Arena* arena )
{
    ArenaImplRelease( arena, arena->size );
}

fn_internal void
ArenaClear( Arena* arena )
{
    ArenaPop( arena, arena->pos );
}


fn_internal Temp
TempBegin(Arena *arena)
{
 Temp temp  = {0};
 temp.arena = arena;
 temp.pos   = arena->pos;
 return temp;
}

fn_internal void
TempEnd(Temp temp)
{
 ArenaPopTo(temp.arena, temp.pos);
}

#endif
