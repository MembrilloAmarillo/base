#include "./memory_linux_impl.h"

static size_t page_align(size_t size) {
    size_t page_size = sysconf(_SC_PAGESIZE);
    return ((size + page_size - 1) / page_size) * page_size;
}

void memory_impl::Init(U64 /*CommitSize*/, U64 ReserveSize) {
    this->ReserveSize = page_align(ReserveSize);
    Data = mmap(0, ReserveSize, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (Data == MAP_FAILED) {
        this->Data = nullptr;
        this->ReserveSize = 0;
        this->CommitSize = 0;
        // Optionally: handle error
    } else {
        this->CommitSize = 0;
        this->ReserveSize = ReserveSize;
    }
}

void memory_impl::Release() {
    if (Data) {
        munmap(Data, ReserveSize);
        Data = nullptr;
        CommitSize = 0;
        ReserveSize = 0;
    }
}

void* memory_impl::Commit(U64 size) {
    if (!Data) return nullptr;
    size = page_align(size);
    if (CommitSize + size > ReserveSize) return nullptr;
    mprotect((char*)Data + CommitSize, size, PROT_READ | PROT_WRITE);
    U64 pre_commit = CommitSize;
    CommitSize += size;
    return (char*)Data + pre_commit;
}

void memory_impl::Decommit(U64 size) {
    if (!Data) return;
    size = page_align(size);
    if (CommitSize < size) {
        size = CommitSize;
        CommitSize = 0;
    } else {
        CommitSize -= size;
    }
    madvise((char*)Data + CommitSize, size, MADV_DONTNEED);
    mprotect((char*)Data + CommitSize, size, PROT_NONE);
}
