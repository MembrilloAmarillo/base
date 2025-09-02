#ifndef _FILES_H_
#define _FILES_H_

#ifdef __linux__
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#elif _WIN32
#include <windows.h>
#endif

#include <stdio.h>

#include "types.h"

typedef enum f_flags f_flags;
enum f_flags {
    RDONLY,
    WRONLY,
    APPEND
};

typedef struct f_file f_file;
struct f_file {
    f_flags     Flags;
    i32         Fd;
    const char* Path;
    void*       Data;
    u32         Offset;
};

internal f_file OpenFile(const char* c, i32 o_flags);
internal void   CloseFile(f_file* f);
internal i32    FileLength(f_file* f);
internal i32    FileRead(f_file* f);
internal void   SetFileData(f_file* f, void* data);
internal u32    FileOffset(f_file* f);

#endif // _FILES_H_

#ifdef FILES_IMPL

internal f_file
OpenFile( const char* c, i32 o_flags ) {
    f_file f; 

    i32 flags = 0;
    if( o_flags & RDONLY ) {
        flags |= O_RDONLY;
    } 
    if( o_flags & WRONLY ) {
        flags |= O_WRONLY;
    } 
    if( o_flags & APPEND ) {
        flags |= O_APPEND;
    }
    if( o_flags & (WRONLY | APPEND)) {
        flags |= O_CREAT;
    }
    f.Fd = open(c, flags, flags, 0755);
    f.Path = c;

    return f;
}

internal void   
CloseFile(f_file* f) {
    if( f->Fd != -1 ) {
        close(f->Fd);
    }
}

internal i32    
FileLength(f_file* f) {
    /**
     * struct stat {
     *    dev_t     st_dev;     ID of device containing file
     *    ino_t     st_ino;     inode number 
     *    mode_t    st_mode;    protection 
     *    nlink_t   st_nlink;   number of hard links 
     *    uid_t     st_uid;     user ID of owner 
     *    gid_t     st_gid;     group ID of owner 
     *    dev_t     st_rdev;    device ID (if special file) 
     *    off_t     st_size;    total size, in bytes 
     *    blksize_t st_blksize; blocksize for file system I/O 
     *    blkcnt_t  st_blocks;  number of 512B blocks allocate
     *    time_t    st_atime;   time of last access 
     *    time_t    st_mtime;   time of last modification 
     *    time_t    st_ctime;   time of last status change 
     * };
     */
    struct stat fd_stat = {0};
    i32 fstat_res = fstat(f->Fd, &fd_stat);
    
    return fd_stat.st_size;
}

internal void   
SetFileData(f_file* f, void* data) {
    f->Data = data;
}

internal u32    
FileOffset(f_file* f) {
    return f->Offset;
}


internal i32
FileRead(f_file* f) {
    i32 file_length = FileLength(f);
    i32 r_len = read(f->Fd, f->Data, file_length);

    return r_len;
}

#endif