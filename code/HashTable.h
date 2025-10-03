#ifndef _HASH_TABLE_H_
#define _HASH_TABLE_H_

#include "types.h"
#include "allocator.h"

#include "third-party/xxhash.h"

typedef struct entry entry;
struct entry {
    entry* Root;
    entry* Prev;
    entry* Next;

    U64 HashId;
    char* Id;
    void* Value;
};

typedef struct hash_table hash_table;
struct hash_table {
    U64 Count;
    U64 Allocated;
    U64 SlotsFilled;
    U64 MinSize;

    U64 LoadFactorPercent;

    Stack_Allocator* Allocator;

    // TODO(s.p): Maybe do it dynamic
    //
    entry* Entries;

    bool CustomFunction;

    U64 (*HashFunction)(const U8* key, U64 length, U64 seed);
};

/**
 * @brief Inits the hash table values
 * @param Table         hash_table pointer to the struct
 * @param BackingBuffer pointer to a buffer used for backing memory
 * @param HashFunction  pointer to a custom hash function
 */
void  HashTableInit( hash_table *Table, Stack_Allocator* BackingBuffer, u64 BufSize, U64 (*HashFunction)(const U8* key, U64 length, U64 seed) );
/**
 * @brief Adds a pointer to a value into the table
 * @param Table hash_table pointer to the struct
 * @param Id    char* with the Id relative to the value being added
 * @param Value void* pointer to the value
 * @return entry* to the entry added, if already existed, pointer to that entry
 */
entry* HashTableAdd( hash_table *Table, char* Id, void* Value, U64 parent );
/**
 * @brief Sets a pointer to a value into an already existing entry
 * @param Table hash_table pointer to the struct
 * @param Id    char* with the Id
 * @param Value void* pointer to the value
 */
void* HashTableSet( hash_table *Table, char* Id, void* Value, U64 parent );

/**
 * @brief Indicates if an entry already exists
 * @param Table     hash_table pointer to the struct
 * @param Id        char* with the Id
 * @return bool true if exists, if not, false
 */
bool HashTableContains( hash_table *Table, char* Id, U64 parent );

/**
 * @brief Returns the value of an entry, if exists
 * @param Table     hash_table pointer to the struct
 * @param Id        char* with the Id
 * @return entry*    Non-null if exists, if not, NULL
 */
entry* HashTableFindPointer( hash_table *Table, char* Id, U64 parent );

// --------------------------------------------------------------- //

static U64 UCF_Strlen( char* str );

static u32 UCF_Streq( char* a, char* b );

static u32 UCF_Streqn( char* a, char* b, u32 n );

#endif
