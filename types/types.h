/* date = April 1st 2025 3:35 pm */

#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

#if defined(_WIN64) || defined(_WIN32)
#include <Windows.h>
#endif

#define local_persist static
#define global        static

#define U8  char
#define U16 uint16_t
#define U32 uint32_t
#define U64 uint64_t

#define I16 int16_t
#define I32 int32_t
#define I64 int64_t

#define F32 float
#define F64 double

global U32 IsPow2(U32 Value)
{
    U32 Result = ((Value & ~(Value - 1)) == Value);
    return(Result);
}

global U32 GetThreadID(void)
{
    U32 ThreadID;
	#if defined(__APPLE__) && defined(__x86_64__)
    asm("mov %%gs:0x00,%0" : "=r"(ThreadID));
	#elif defined(__i386__)
    asm("mov %%gs:0x08,%0" : "=r"(ThreadID));
	#elif defined(__x86_64__)
    asm("mov %%fs:0x10,%0" : "=r"(ThreadID));
	#elif defined(_WIN64) || defined(_WIN32)
	ThreadID = GetCurrentThreadId();
	#else
	#error Unsupported architecture
	#endif
    
    return ThreadID;
}

#endif //TYPES_H
