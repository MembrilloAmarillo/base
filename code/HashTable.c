// --------------------------------------------------------------- //

static U64 UCF_Strlen( char* str ) {
    U64 i = 0;

    if( str == NULL ) { return i; }

    while( str[i] != '\0' ) { i += 1; };

    return i;
}

// --------------------------------------------------------------- //

static U32 JenkinsHashFunction( const U8* key, U64 length ) {
    U64 i    = 0;
    U32 hash = 0;

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

void HashTableInit( hash_table *Table, void* BackingBuffer, U32 (*HashFunction)(const U8* key, U64 length) ) {
    memset( Table, 0, sizeof(hash_table) );
    Table->BackingBuffer = BackingBuffer;
    if( HashFunction == NULL ) {
        Table->HashFunction  = JenkinsHashFunction;
    } else {
        Table->HashFunction = HashFunction;
    }

    memset( Table->Entries, 0, sizeof(&Table->Entries[0]) );
}

// --------------------------------------------------------------- //

entry* HashTableAdd( hash_table *Table, char* Id, void* Value ) {
    U32 HashId = Table->HashFunction( (const U8*)Id, UCF_Strlen( Id ) );

    U32 EntryIdx = HashId % 256;

    entry Entry = {
        .HashId = HashId,
        .Id     = Id,
        .Value  = Value
    };

    if( Table->Entries[EntryIdx].HashId == 0 ) {
        Table->Entries[EntryIdx] = Entry;
    }

    return &Table->Entries[EntryIdx];
}

// --------------------------------------------------------------- //

void* HashTableSet( hash_table *Table, char* Id, void* Value ) {
    U32 HashId = Table->HashFunction( (const U8*)Id, UCF_Strlen( Id ) );

    U32 EntryIdx = HashId % 256;

    if( Table->Entries[EntryIdx].HashId == 0 ) {
        return NULL;
    } else {
        Table->Entries[EntryIdx].Value = Value;
    }

    return &Table->Entries[EntryIdx];
}

// --------------------------------------------------------------- //

bool HashTableContains( hash_table *Table, char* Id ) {
    U32 HashId = Table->HashFunction( (const U8*)Id, UCF_Strlen( Id ) );

    U32 EntryIdx = HashId % 256;

    if( Table->Entries[EntryIdx].HashId == 0 ) {
        return false;
    } else {
        return true;
    }
}

// --------------------------------------------------------------- //

entry* HashTableFindPointer( hash_table *Table, char* Id ) {
    U32 HashId = Table->HashFunction( (const U8*)Id, UCF_Strlen( Id ) );

    U32 EntryIdx = HashId % 256;

    if( Table->Entries[EntryIdx].HashId == 0 ) {
        return NULL;
    } else {
        return &Table->Entries[EntryIdx];
    }
}
