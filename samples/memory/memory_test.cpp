#include <stdio.h>
#include <stdlib.h>

#ifdef __linux__
#include "../../memory/linux/memory_linux_impl.cpp"
#elif _WIN32
#include "../../memory/win/memory_win_impl.cpp"
#endif

#include "../../memory/memory.cpp"


int main() {
    Arena* MainArena = (Arena*)malloc(sizeof(Arena));
    MainArena->Init(0, 2 << 30 );
    
    U8* a = MainArena->Push<U8>( 256000 );
    printf("New Push: %lld\n", MainArena->current_offset);
    U8*  b = MainArena->Push<U8> ( 2048 );
    printf("New Push: %lld\n", MainArena->current_offset);
    MainArena->Pop<U8>( 2048 );
    printf("New Push: %lld\n", MainArena->current_offset);
    MainArena->Pop<U8>( 256001 );
    printf("New Push: %lld\n", MainArena->current_offset);
    b = MainArena->Push<U8> ( 2048 );
    printf("New Push: %lld\n", MainArena->current_offset);
    
    return 0;
}
