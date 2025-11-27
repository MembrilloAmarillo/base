#ifndef _FILES_H_
#define _FILES_H_

// --- Platform Includes ---
#ifdef __linux__
    #include <sys/mman.h>
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <unistd.h>
#elif _WIN32
    #include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#include "types.h"

// --- Public API ---
typedef enum f_flags {
    RDONLY = (1 << 0),
    WRONLY = (1 << 1),
    APPEND = (1 << 2)
};

typedef struct f_file f_file;
struct f_file {
    f_flags     Flags;
    const char* Path;
    void*       Data;
    u32         Offset;

    // --- Platform-Specific Handle ---
#ifdef __linux__
    i32         Fd; // File descriptor on Linux
#elif _WIN32
    HANDLE      Fd; // Win32 File Handle
#endif
};

internal f_file F_OpenFile(const char* c, f_flags o_flags);
internal void   F_CloseFile(f_file* f);
internal i32    F_FileLength(f_file* f);
internal i32    F_FileRead(f_file* f);
internal void   F_SetFileData(f_file* f, void* data);
internal u32    F_FileOffset(f_file* f);

#endif // _FILES_H_

// --- Implementation ---
#ifdef FILES_IMPL

internal f_file
F_OpenFile(const char* c, f_flags o_flags) {
    f_file f = {};
    f.Path = c;
    f.Flags = o_flags;

#ifdef __linux__
    // --- Linux Implementation (using POSIX) ---
    int flags = 0;
    if (o_flags & RDONLY) flags |= O_RDONLY;
    if (o_flags & WRONLY) flags |= O_WRONLY;
    if (o_flags & APPEND) flags |= O_APPEND;
    if (o_flags & (WRONLY | APPEND)) flags |= O_CREAT;

    f.Fd = open(c, flags, 0644);
    if (f.Fd == -1) {
        // Optionally set an error flag in f_file
    }

#elif _WIN32
    DWORD dwDesiredAccess = 0;
    DWORD dwCreationDisposition = 0;
    SECURITY_ATTRIBUTES security_attributes = {sizeof(security_attributes), 0, 0};

    if (o_flags & RDONLY) {
        dwDesiredAccess |= GENERIC_READ;
    }
    if (o_flags & WRONLY) {
        dwDesiredAccess |= GENERIC_WRITE;
    }
    // Si quieres leer después de abrir en escritura, asegúrate de pedir lectura también:
    if ((o_flags & WRONLY) && !(o_flags & RDONLY)) {
        // si solo WRONLY, pero vas a leer, añade lectura
        dwDesiredAccess |= GENERIC_READ;
    }

    if (o_flags & APPEND) {
        dwCreationDisposition = OPEN_ALWAYS;
    } else if (o_flags & WRONLY) {
        dwCreationDisposition = CREATE_ALWAYS;
    } else {
        dwCreationDisposition = OPEN_EXISTING;
    }

    f.Fd = CreateFileA(
        (LPCSTR)c,
        dwDesiredAccess,
        FILE_SHARE_WRITE | FILE_SHARE_READ,
        &security_attributes,
        dwCreationDisposition,
        FILE_ATTRIBUTE_NORMAL,
        0
    );
    if (f.Fd == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        fprintf(stderr, "[ERROR] CreateFileA failed for path '%s', error = %u\n", c, err);
    }
#endif

    return f;
}

internal void
F_CloseFile(f_file* f) {
    if (!f) return;

#ifdef __linux__
    if (f->Fd != -1) {
        close(f->Fd);
        f->Fd = -1;
    }
#elif _WIN32
    if (f->Fd != NULL && f->Fd != INVALID_HANDLE_VALUE) {
        CloseHandle(f->Fd);
        f->Fd = INVALID_HANDLE_VALUE;
    }
#endif
}

internal i32
F_FileLength(f_file* f) {
    if (!f) return -1;

#ifdef __linux__
    struct stat fd_stat = {0};
    if (fstat(f->Fd, &fd_stat) == 0) {
        return (i32)fd_stat.st_size;
    }
    return -1;

#elif _WIN32
    LARGE_INTEGER size;
    if (GetFileSizeEx(f->Fd, &size)) {
        // LARGE_INTEGER is a 64-bit struct. We return i32, so check for overflow.
        if (size.HighPart == 0) {
            return (i32)size.LowPart;
        }
        // File is larger than 2GB, handle error or return max i32
        return 0x7FFFFFFF; 
    }
    return -1;
#endif
}

internal void
F_SetFileData(f_file* f, void* data) {
    if (f) {
        f->Data = data;
    }
}

internal u32
F_FileOffset(f_file* f) {
    return f ? f->Offset : 0;
}

internal i32
F_FileRead(f_file* f) {
    if (!f || !f->Data) {
        // Error: puntero inválido
        return -1;
    }

    i32 file_length = F_FileLength(f);
    if (file_length <= 0) {
        return -1;
    }

#ifdef _WIN32
    DWORD bytesRead = 0;
    OVERLAPPED overlapped = {0};
    BOOL result = ReadFile(
        f->Fd,
        (u8*)f->Data,
        (DWORD)file_length,
        &bytesRead,
        &overlapped
    );

    if (!result) {
        DWORD err = GetLastError();
        // Opcional: loggear err para saber qué pasó
        printf("ReadFile failed, error = %u\n", err);
        return -1;
    }

    if (bytesRead == 0) {
        // Llegaste al EOF o no había bytes para leer
        return 0;
    }

    return (i32)bytesRead;
#else
    // Linux parte
    i32 r_len = (i32)read(f->Fd, f->Data, (unsigned int)file_length);
    return r_len;
#endif
}

#endif // FILES_IMPL