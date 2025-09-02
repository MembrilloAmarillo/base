#ifndef _STRINGS_H_
#define _STRINGS_H_

#include "types.h"
#include "memory.h"

typedef struct U8_String U8_String;
struct U8_String {
    u8* data;
    u64 len;
};

U8_String StringNew(const char* str, u64 len, Arena* a);
void StringDestroy(U8_String* s);

#endif

#ifdef STRINGS_IMPL

U8_String StringNew( const char* str, u64 len, Arena* a ) {
    U8_String s = {
        .data = PushArray(a, u8, len),
        .len  = len
    };

    memcpy(s.data, str, len);

    return s;
}

void StringDestroy(U8_String* s) {

}

#endif
