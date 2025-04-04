#include "memory_win_impl.h"

// Initialization of memory ---------------------------------------------------
// TODO: CommitSize is not supported for now, just allocating as is
void memory_impl::Init( u64 CommitSize, u64 ReserveSize ) {
	Data = VirtualAlloc(0, ReserveSize, MEM_RESERVE, PAGE_NOACCESS);
        
	this->CommitSize  = 0;
	this->ReserveSize = ReserveSize;
}

// Free memory ----------------------------------------------------------------
//
void memory_impl::Release() {
    if( Data ) {
        VirtualFree(Data, 0, MEM_RELEASE);
    }
}

// Commit memory --------------------------------------------------------------
//
void* memory_impl::Commit( u64 size ) {
    if( Data ) {
        VirtualAlloc((u8*)Data + CommitSize, size, MEM_COMMIT, PAGE_READWRITE);
        u64 PreNewCommitSize = CommitSize;
        CommitSize += size;
        
        return (u8*)Data + PreNewCommitSize;
    }
    return NULL;
}

// Decommit memory ------------------------------------------------------------
//
void memory_impl::Decommit( u64 size ) {
    if( Data ) {
        
        if( CommitSize < size ) { 
            size = CommitSize;
            CommitSize = 0; 
        }
        else {
            CommitSize -= size; 
        }
        
        VirtualFree((u8*)Data + CommitSize, size, MEM_DECOMMIT);
    }
}

