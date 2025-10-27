#include "HashTable.h"
// --------------------------------------------------------------- //

static U64 UCF_Strlen( const char* str ) {
    U64 i = 0;

    if( str == NULL ) { return i; }

    while( str[i] != '\0' ) { i += 1; };

    return i;
}

// --------------------------------------------------------------- //

static u32 UCF_Streq( const char* a, const char* b ) {
    char* a1 = a, *b1 = b;

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
    char* a1 = a, *b1 = b;

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

void HashTableInit( hash_table *Table, Stack_Allocator* Allocator, u64 Size, U64 (*HashFunction)(const U8* key, U64 length, U64 seed) ) {
    memset( Table, 0, sizeof(hash_table) );
    Table->Allocator = Allocator;
    Table->Entries = stack_push(Allocator, entry, Size);
    Table->Allocated = Size;
    if( HashFunction == NULL ) {
        Table->HashFunction  = JenkinsHashFunction;
    } else {
        Table->HashFunction = HashFunction;
    }

    memset( Table->Entries, 0, Table->Allocated );
}

// --------------------------------------------------------------- //

entry* HashTableAdd( hash_table *Table, char* Id, void* Value, U64 parent ) {
    U64 HashId = Table->HashFunction( (const U8*)Id, UCF_Strlen( Id ), parent );

    U64 EntryIdx = HashId % Table->Allocated;

    entry* Entry = stack_push(Table->Allocator, entry, 1);
    *Entry = (entry){
        .HashId = HashId,
        .Id     = Id,
        .Value  = Value
    };
    DLIST_INIT(Entry);

    if( Table->Entries[EntryIdx].HashId == 0 ) {
        DLIST_INIT(&Table->Entries[EntryIdx]);
        Table->Entries[EntryIdx].HashId = HashId;
        Table->Entries[EntryIdx].Id     = Id;
    }

    DLIST_INSERT_AS_LAST(&Table->Entries[EntryIdx], Entry);

    return Entry;
}

// --------------------------------------------------------------- //

void* HashTableSet( hash_table *Table, char* Id, void* Value, U64 parent ) {
    U64 HashId = Table->HashFunction( (const U8*)Id, UCF_Strlen( Id ), parent );

    U64 EntryIdx = HashId % Table->Allocated;

    void* value = Table->Entries[EntryIdx].Value;

    if( Table->Entries[EntryIdx].HashId == 0 ) {
        return NULL;
    } else {
        entry* it = Table->Entries[EntryIdx].Next;
        for( ; it != NULL; it = it->Next ) {
            if( it->Next == &Table->Entries[EntryIdx] ) {
                entry* Entry = HashTableAdd(Table, Id, Value, parent);
                value = Entry->Value;
            }
        }
    }

    return value;
}

// --------------------------------------------------------------- //

bool HashTableContains( hash_table *Table, char* Id, U64 parent ) {
    U64 HashId = Table->HashFunction( (const U8*)Id, UCF_Strlen( Id ), parent );

    U64 EntryIdx = HashId % Table->Allocated;

    if( Table->Entries[EntryIdx].HashId == 0 ) {
        return false;
    } else {
        entry* it = Table->Entries[EntryIdx].Next;
        for( ; it != &Table->Entries[EntryIdx]; it = it->Next ) {
            if( it->HashId == HashId ) {
                return true;
            }
        }
    }

    return false;
}

// --------------------------------------------------------------- //

entry* HashTableFindPointer( hash_table *Table, char* Id, U64 parent ) {
    U64 HashId = Table->HashFunction( (const U8*)Id, UCF_Strlen( Id ), parent );

    U64 EntryIdx = HashId % Table->Allocated;

	if( Table->Entries[EntryIdx].HashId == 0 ) {
        return NULL;
    } else {
        entry* it = Table->Entries[EntryIdx].Next;
        for( ; it != &Table->Entries[EntryIdx]; it = it->Next ) {
            if( it->HashId == HashId ) {
                return it;
            }
        }
		return NULL;
    }
}

// --------------------------------------------------------------- //

void* HashTableGet( hash_table *Table, u64 Id, U64 parent ) {
    U64 EntryIdx = Id % Table->Allocated;

    if( Table->Entries[EntryIdx].HashId == 0 ) {
        return NULL;
    } else {
        entry* it = Table->Entries[EntryIdx].Next;
        for( ; it != &Table->Entries[EntryIdx]; it = it->Next ) {
            if( it->HashId == Id ) {
                return it->Value;
            }
        }
		return NULL;
    }
}
