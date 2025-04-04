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
    
    u8* a = MainArena->Push<u8>( 256000 );
    printf("New Push: %lld\n", MainArena->current_offset);
    u8*  b = MainArena->Push<u8> ( 2048 );
    printf("New Push: %lld\n", MainArena->current_offset);
    MainArena->Pop<u8>( 2048 );
    printf("New Push: %lld\n", MainArena->current_offset);
    MainArena->Pop<u8>( 256001 );
    printf("New Push: %lld\n", MainArena->current_offset);
    b = MainArena->Push<u8> ( 2048 );
    printf("New Push: %lld\n", MainArena->current_offset);
    
    return 0;
}
