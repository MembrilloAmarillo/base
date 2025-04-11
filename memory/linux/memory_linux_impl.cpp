#include "./memory_linux_impl.h"

// Initialization of memory ---------------------------------------------------
// TODO: CommitSize is not supported for now, just allocating as is
void memory_impl::Init( U64 CommitSize, U64 ReserveSize ) {
	Data = mmap(0, ReserveSize, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);

	this->CommitSize  = 0;
	this->ReserveSize = ReserveSize;
}

// Free memory ----------------------------------------------------------------
//
void memory_impl::Release() {
    if( Data ) {
        //VirtualFree(Data, 0, MEM_RELEASE);
        munmap(Data, ReserveSize);
    }
}

// Commit memory --------------------------------------------------------------
//
void* memory_impl::Commit( U64 size ) {
    if( Data ) {
        //VirtualAlloc(Data + CommitSize, size, MEM_COMMIT, PAGE_READWRITE);
        mprotect((U8*)Data + CommitSize, size, PROT_READ|PROT_WRITE);
        U64 PreNewCommitSize = CommitSize;
        CommitSize += size;

        return (U8*)Data + PreNewCommitSize;
    }
    return NULL;
}

// Decommit memory ------------------------------------------------------------
//
void memory_impl::Decommit( U64 size ) {
    if( Data ) {

        if( CommitSize < size ) {
            size = CommitSize;
            CommitSize = 0;
        }
        else {
            CommitSize -= size;
        }

        //VirtualFree(Data + CommitSize, size, MEM_DECOMMIT);
        madvise((U8*)Data + CommitSize, size, MADV_DONTNEED);
        mprotect((U8*)Data + CommitSize, size, PROT_NONE);
    }
}
