#include "HashTable.h"
#include <string.h>
// --------------------------------------------------------------- //

static U64 UCF_Strlen( const char* str ) {
    U64 i = 0;

    if( str == NULL ) { return i; }

    while( str[i] != '\0' ) { i += 1; };

    return i;
}

// --------------------------------------------------------------- //

static u32 UCF_Streq( const char* a, const char* b ) {
    const char* a1 = a, *b1 = b;

    while( (*a1 && *b1) && (*a1 == *b1) ) {
        a1++;
        b1++;
    }

    // if there is still more to read from one of the
    // strings return 0 as its not equal
    //
    if( *a1 || *b1 ) {
        return 0;
    }

    return 1;
}

// --------------------------------------------------------------- //

static u32 UCF_Streqn( const char* a, const char* b, u32 n ) {
    const char* a1 = a, *b1 = b;

    u32 i = 0;
    while( (*a1 && *b1) && (*a1 == *b1) && i < n ) {
        a1++;
        b1++;
        i++;
    }

    // if i != n means that a1 == b1 was not succesfull for all n iterations
    //
    if( i != n ) {
        return 0;
    }

    return 1;
}

// --------------------------------------------------------------- //

static U64 JenkinsHashFunction( const U8* key, U64 length, U64 seed ) {
    U64 i    = 0;
    U64 hash = 0;

    for( ; i != length; ) {
        hash += key[i];
        i += 1;
        hash += hash << 10;
        hash ^= hash >> 6;
    }

    hash += hash << 3;
    hash ^= hash >> 11;
    hash += hash << 15;

    return hash;
}

// --------------------------------------------------------------- //

void HashTableInit( hash_table *Table, Allocator* Alloc, u64 Size, U64 (*HashFunction)(const U8* key, U64 length, U64 seed) ) {
    memset( Table, 0, sizeof(hash_table) );
    Table->Alloc = Alloc;
    if (Size == 0) {
        Size = 1;
    }
    Table->Entries = Mem_Allocator::Make<entry>(Alloc, Size);
    Table->Allocated = Size;
    if( HashFunction == NULL ) {
        Table->HashFunction  = JenkinsHashFunction;
    } else {
        Table->HashFunction = HashFunction;
    }

    memset( Table->Entries, 0, Table->Allocated * sizeof(entry) );
    for (u64 i = 0; i < Table->Allocated; ++i) {
        DLIST_INIT(&Table->Entries[i]);
    }
}

// --------------------------------------------------------------- //

entry* HashTableAdd( hash_table *Table, const char* Id, void* Value, U64 parent ) {
    U64 HashId = Table->HashFunction( (const U8*)Id, UCF_Strlen( Id ), parent );

    U64 EntryIdx = HashId % Table->Allocated;

    entry* Entry = Mem_Allocator::Make<entry>(Table->Alloc, 1);
    *Entry = {};
    Entry->HashId = HashId;
    Entry->Id     = Id;
    Entry->Value  = Value;

    DLIST_INSERT_AS_LAST(&Table->Entries[EntryIdx], Entry);
    Table->Count += 1;

    return Entry;
}

// --------------------------------------------------------------- //

void* HashTableSet( hash_table *Table, const char* Id, void* Value, U64 parent ) {
    U64 HashId = Table->HashFunction( (const U8*)Id, UCF_Strlen( Id ), parent );

    U64 EntryIdx = HashId % Table->Allocated;
    entry* head = &Table->Entries[EntryIdx];
    for (entry* it = head->Next; it != head; it = it->Next) {
        if (it->HashId == HashId && UCF_Streq(it->Id, Id)) {
            void* old_value = it->Value;
            it->Value = Value;
            return old_value;
        }
    }
    HashTableAdd(Table, Id, Value, parent);
    return NULL;
}

// --------------------------------------------------------------- //

bool HashTableContains( hash_table *Table, const char* Id, U64 parent ) {
    U64 HashId = Table->HashFunction( (const U8*)Id, UCF_Strlen( Id ), parent );

    U64 EntryIdx = HashId % Table->Allocated;
    entry* head = &Table->Entries[EntryIdx];
    for( entry* it = head->Next; it != head; it = it->Next ) {
        if( it->HashId == HashId && UCF_Streq(it->Id, Id) ) {
            return true;
        }
    }
    return false;
}

// --------------------------------------------------------------- //

entry* HashTableFindPointer( hash_table *Table, const char* Id, U64 parent ) {
    U64 HashId = Table->HashFunction( (const U8*)Id, UCF_Strlen( Id ), parent );

    U64 EntryIdx = HashId % Table->Allocated;
    entry* head = &Table->Entries[EntryIdx];
    for( entry* it = head->Next; it != head; it = it->Next ) {
        if( it->HashId == HashId && UCF_Streq(it->Id, Id) ) {
            return it;
        }
    }
    return NULL;
}

// --------------------------------------------------------------- //

void* HashTableGet( hash_table *Table, u64 Id, U64 parent ) {
    (void)parent;
    U64 EntryIdx = Id % Table->Allocated;
    entry* head = &Table->Entries[EntryIdx];
    for( entry* it = head->Next; it != head; it = it->Next ) {
        if( it->HashId == Id ) {
            return it->Value;
        }
    }
    return NULL;
}
