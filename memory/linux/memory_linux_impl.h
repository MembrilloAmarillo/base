/* date = April 1st 2025 3:14 pm */

#ifndef MEMORY_LINUX_IMPL_H
#define MEMORY_LINUX_IMPL_H

#include <sys/mman.h>

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
    
    void Init( u64 CommitSize, u64 ReserveSize );
    void Release();
    void* Commit( u64 size );
    void Decommit( u64 size );
    
	u64 GetCommitSize()   { return CommitSize; }
	u64 GetReservedSize() { return ReserveSize; }

    private:
    void* Data;
    u64 CommitSize;
    u64 ReserveSize;
};


#endif //MEMORY_LINUX_IMPL_H
