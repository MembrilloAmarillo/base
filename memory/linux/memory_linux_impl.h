/* date = April 1st 2025 3:14 pm */

#ifndef MEMORY_LINUX_IMPL_H
#define MEMORY_LINUX_IMPL_H

#include <sys/mman.h>

#include <unistd.h>

#include "../../types/types.h"


// class that implements memory operations over the OS
// we do not want constructors of any kind
//
class memory_impl {
    public:
    
    memory_impl()                  = delete;
    memory_impl( memory_impl &m )  = delete;
    memory_impl( memory_impl &&m ) = delete;
    ~memory_impl()                 = delete;
    
    void Init( U64 CommitSize, U64 ReserveSize );
    void Release();
    void* Commit( U64 size );
    void Decommit( U64 size );
    
	U64 GetCommitSize()   { return CommitSize; }
	U64 GetReservedSize() { return ReserveSize; }

    protected:
    void* Data;
    U64 CommitSize;
    U64 ReserveSize;
};


#endif //MEMORY_LINUX_IMPL_H
