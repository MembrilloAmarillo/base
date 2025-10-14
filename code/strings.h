#ifndef _STRINGS_H_
#define _STRINGS_H_

typedef struct U8_String U8_String;
struct U8_String {
    u8* data;
    i64 len;
    i64 idx;
};

U8_String StringCreate(u64 len, Stack_Allocator* a);
U8_String StringNew(const char* str, u64 len, Stack_Allocator* a);
void StringDestroy(U8_String* s);

void StringAppend(U8_String* Dst, const char* Str);
void StringAppendStr(U8_String* Dst, U8_String* Str);

void StringInsert(U8_String* Dst, u32 Idx, const char* Str);
void StringInsertStr(U8_String* Dst, u32 Idx, U8_String* Str);

void StringCpy(U8_String* Dst, char* Src);
void StringCpyStr(U8_String* Dst, U8_String* Src);

void StringErase(U8_String* Str, u32 Idx);
void StringEraseUntil(U8_String* Str, u32 Idx);

void StringPop(U8_String* Str);

U8_String SplitFirst(U8_String* Str, char val);

u64 GetCountOfChar(U8_String* Str, char val);

i32 GetFirstOcurrence(U8_String* Str, char val);
i32 GetLastOcurrence(U8_String* Str, char val);

u64* GetAllOcurrences(U8_String* Str, char val);

void SplitMultiple(U8_String* Dst, u64 Size, const U8_String* Src, char val);

#endif

#ifdef STRINGS_IMPL

U8_String StringNew( const char* str, u64 len, Stack_Allocator* a ) {
    U8_String s = {
        .data = stack_push(a, u8, len),
        .len  = len,
        .idx  = len
    };

    memcpy(s.data, str, len);

    return s;
}

U8_String StringCreate( u64 len, Stack_Allocator* a ) {
    U8_String s = {
        .data = stack_push(a, u8, len),
        .len  = len,
        .idx  = 0
    };

    return s;
}

U8_String
SplitFirst( U8_String* Str, char val ) {
    for( u32 it = 0; it < Str->len; it += 1 ) {
        if( Str->data[it] == val ) {
            U8_String ret = {
                .data = Str->data,
                .len  = it,
                .idx  = it
            };
            return ret;
        }
    }
    return *Str;
}

u64
GetCountOfChar(U8_String* Str, char val) {
    u64 cnt = 0;
    for( u32 it = 0; it < Str->len; it += 1 ) {
        if( Str->data[it] == val ) {
            cnt += 1;
        }
    }
    return cnt;
}

i32 GetFirstOcurrence(U8_String* Str, char val) {
    for( i32 i = 0; i < Str->idx; i += 1 ) {
        if( Str->data[i] == val ) {
            return i;
        }
    }
    return -1;
}

i32 GetLastOcurrence(U8_String* Str, char val) {
    for( i32 i = Str->idx - 1; i >= 0; i -= 1 ) {
        if( Str->data[i] == val ) {
            return i;
        }
    }

    return -1;
}

void
SplitMultiple(U8_String* Dst, u64 Size, const U8_String* Src, char val) {
    u64 Idx = 0;
    u64 LastValFound = 0;
    for( u32 it = 0; it < Src->len && Idx < Size; it += 1 ) {
        if( Src->data[it] == val ) {
            Dst[Idx] = (U8_String){
                .data = Src->data + LastValFound,
                .len  = it - LastValFound,
                .idx  = it - LastValFound
            };
            LastValFound = it + 1;
            Idx += 1;
        }
    }
    if( LastValFound < Src->len ) {
        Dst[Idx] = (U8_String){
            .data = Src->data + LastValFound,
            .len  = Src->len - LastValFound,
            .idx  = Src->len - LastValFound
        };
    }
}

void 
StringInsert(U8_String* Dst, u32 Idx, const char* Str) {
    u64 len = UCF_Strlen(Str);
    if( Dst->idx + len < Dst->len && Dst->idx > Idx ) {
        memcpy(Dst->data + Idx + len, Dst->data + Idx, Dst->idx - Idx);
        memcpy(Dst->data + Idx, Str, len);
        Dst->idx += len;
    } else if ( Dst->idx + len < Dst->len && Dst->idx == 0 ) {
        memcpy(Dst->data, Str, len);
        Dst->idx += len;
    } else if (Dst->idx + len < Dst->len && Dst->idx == Idx) {
        StringAppend(Dst, Str);
    }
}

void 
StringInsertStr(U8_String* Dst, u32 Idx, U8_String* Str) {
    if( Dst->idx + Str->idx < Dst->len && Dst->idx > Idx && Dst->idx > 0 ) {
        memcpy(Dst->data + Idx + Str->idx, Dst->data + Idx, Dst->idx - Idx);
        memcpy(Dst->data + Idx, Str->data, Str->idx);
        Dst->idx += Str->idx;
    } else if ( Dst->idx + Str->idx < Dst->len && Dst->idx == 0 ) {
        memcpy(Dst->data, Str->data, Str->idx);
        Dst->idx += Str->idx;
    } else if (Dst->idx + Str->idx < Dst->len && Dst->idx == Idx) {
        StringAppendStr(Dst, Str);
    }
}

void StringErase(U8_String* Str, u32 Idx) {
    if( Idx == -1 ) { return; }
    if( Idx < Str->idx ) {
        memcpy(Str->data + Idx, Str->data + Idx + 1, Str->idx - Idx);
        Str->idx = Max(0, Str->idx - 1);
    } else if( Idx == Str->idx ) {
        Str->idx = Max(0, Str->idx - 1);
    }
}

void StringEraseUntil(U8_String* Str, u32 Idx) {
    if( Idx < Str->idx ) {
        Str->idx = Idx;
    }
}

void StringPop(U8_String* Str) {
    StringErase(Str, Str->idx);
}

void
StringCpy(U8_String* Dst, char* Src) {
    u64 len = UCF_Strlen(Src);
    if(Dst->len >= len) {
        memcpy(Dst->data, Src, len);
        Dst->idx = len;
    }
}

void
StringCpyStr(U8_String* Dst, U8_String* Src) {
    if(Dst->len >= Src->idx) {
        memcpy(Dst->data, Src->data, Src->idx);
        Dst->idx = Src->idx;
    }
}

void StringAppend(U8_String* Dst, const char* Str) {
    u64 len = UCF_Strlen(Str);
    if( Dst->idx + len >= Dst->len ) {
        // to big of a string, skip
    } else {
        memcpy(Dst->data + Dst->idx, Str, len);
        Dst->idx += len;
    }
}

void StringAppendStr(U8_String* Dst, U8_String* Str) {
    if( Dst->idx + Str->idx >= Dst->len ) {
        // to big of a string, skip
    } else {
        memcpy(Dst->data + Dst->idx, Str->data, Str->idx);
        Dst->idx += Str->idx;
    }
}

void StringDestroy(U8_String* s) {

}

#endif
