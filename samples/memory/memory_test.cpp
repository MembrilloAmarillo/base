#include <stdio.h>

#include "../../memory/win/memory_win_impl.cpp"
#include "../../memory/memory.cpp"

int main() {
    Arena* MainArena = (Arena*)malloc(sizeof(Arena));
    MainArena->Init(0, 2 << 30 );
    
    u64* a = MainArena->Push<u64>( 1 << 30 );
    printf("New Push: %lld\n", MainArena->current_offset);
    u8*  b = MainArena->Push<u8> ( 1 << 20 );
    printf("New Push: %lld\n", MainArena->current_offset);
    MainArena->Pop<u8>( 1 << 20 );
    printf("New Push: %lld\n", MainArena->current_offset);
    MainArena->Pop<u8>( 2 << 30 );
    printf("New Push: %lld\n", MainArena->current_offset);
    b = MainArena->Push<u8> ( 1 << 20 );
    printf("New Push: %lld\n", MainArena->current_offset);
    
    return 0;
}