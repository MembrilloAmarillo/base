#ifndef _STRINGS_H_
#define _STRINGS_H_

#include "types.h"
#include "allocator.h"

typedef struct U8_String U8_String;
struct U8_String {
    u8* data;
    i64 len;
    i64 idx;
};

U8_String StringCreate(i64 len, Stack_Allocator* a);
U8_String StringNew(const char* str, i64 len, Stack_Allocator* a);
void StringDestroy(U8_String* s);

void StringAppend(U8_String* Dst, const char* Str);
void StringAppendStr(U8_String* Dst, U8_String* Str);

void StringInsert(U8_String* Dst, i64 Idx, const char* Str);
void StringInsertStr(U8_String* Dst, i64 Idx, U8_String* Str);

void StringCpy(U8_String* Dst, const char* Src);
void StringCpyStr(U8_String* Dst, U8_String* Src);

void StringErase(U8_String* Str, i64 Idx);
void StringEraseUntil(U8_String* Str, i64 Idx);

void StringPop(U8_String* Str);

U8_String SplitFirst(U8_String* Str, char val);

i64 GetCountOfChar(U8_String* Str, char val);

i64 GetFirstOcurrence(U8_String* Str, char val);
i64 GetLastOcurrence(U8_String* Str, char val);

i64* GetAllOcurrences(U8_String* Str, char val);

void SplitMultiple(U8_String* Dst, i64 Size, const U8_String* Src, char val);

fn_internal i64 StringGetLen(U8_String* Str) { return Str->idx; }
fn_internal i64 StringGetCapacity(U8_String* Str) { return Str->len; }

fn_internal i64 CustomStrlen(const char* str);
fn_internal i64 CustomStreq(const char* a, const char* b);
fn_internal u32 CustomStreqn(const char* a, const char* b, u32 n);

#endif

#ifdef STRINGS_IMPL

U8_String StringNew( const char* str, i64 len, Stack_Allocator* a ) {
	U8_String s = {};
	s.data = stack_push(a, u8, len);
	s.len = len;
	s.idx = len;

    memcpy(s.data, str, len);

    return s;
}

U8_String StringCreate( i64 len, Stack_Allocator* a ) {
    U8_String s = {};
	s.data = stack_push(a, u8, len);
	s.len = len;
	s.idx = 0;

    return s;
}

U8_String
SplitFirst( U8_String* Str, char val ) {
    for( i64 it = 0; it < Str->len; it += 1 ) {
        if( Str->data[it] == val ) {
			U8_String ret = {};
			ret.data = Str->data;
			ret.len  = it;
			ret.idx  = it;
            
            return ret;
        }
    }
    return *Str;
}

i64
GetCountOfChar(U8_String* Str, char val) {
    i64 cnt = 0;
    for( i64 it = 0; it < Str->len; it += 1 ) {
        if( Str->data[it] == val ) {
            cnt += 1;
        }
    }
    return cnt;
}

i64 GetFirstOcurrence(U8_String* Str, char val) {
    for( i64 i = 0; i < Str->idx; i += 1 ) {
        if( Str->data[i] == val ) {
            return i;
        }
    }
    return -1;
}

i64 GetLastOcurrence(U8_String* Str, char val) {
    for( i64 i = Str->idx - 1; i >= 0; i -= 1 ) {
        if( Str->data[i] == val ) {
            return i;
        }
    }

    return -1;
}

void
SplitMultiple(U8_String* Dst, i64 Size, const U8_String* Src, char val) {
    i64 Idx = 0;
    i64 LastValFound = 0;
    for( i64 it = 0; it < Src->len && Idx < Size; it += 1 ) {
        if( Src->data[it] == val ) {
			U8_String str = {};
			str.data = Src->data + LastValFound;
			str.len  = it - LastValFound;
            str.idx  = it - LastValFound;
            
			Dst[Idx] = str;
            LastValFound = it + 1;
            Idx += 1;
        }
    }
    if( LastValFound < Src->len ) {
		U8_String str = {};
		str.data = Src->data + LastValFound;
		str.len = Src->len - LastValFound;
		str.idx = Src->len - LastValFound;
		Dst[Idx] = str;
    }
}

void 
StringInsert(U8_String* Dst, i64 Idx, const char* Str) {
    i64 len = UCF_Strlen(Str);
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
StringInsertStr(U8_String* Dst, i64 Idx, U8_String* Str) {
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

void StringErase(U8_String* Str, i64 Idx) {
    if( Idx == -1 ) { return; }
    if( Idx < Str->idx ) {
        memcpy(Str->data + Idx, Str->data + Idx + 1, Str->idx - Idx);
        Str->idx = Max(0, Str->idx - 1);
    } else if( Idx == Str->idx ) {
        Str->idx = Max(0, Str->idx - 1);
    }
}

void StringEraseUntil(U8_String* Str, i64 Idx) {
    if( Idx < Str->idx ) {
        Str->idx = Idx;
    }
}

void StringPop(U8_String* Str) {
    StringErase(Str, Str->idx);
}

void
StringCpy(U8_String* Dst, const char* Src) {
    i64 len = UCF_Strlen(Src);
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
    i64 len = UCF_Strlen(Str);
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

fn_internal i64 CustomStrlen(const char* str) {
	U64 i = 0;

    if( str == NULL ) { return i; }

    while( str[i] != '\0' ) { i += 1; };

    return i;
}

fn_internal i64 CustomStreq( const char* a, const char* b ) {
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

fn_internal u32 CustomStreqn( const char* a, const char* b, u32 n ) {
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

#endif
