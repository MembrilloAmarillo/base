#ifndef _VECTOR_H_
#define _VECTOR_H_

#include <cassert>

#include <cstdio>

#include "../memory/memory.h"
#include "../types/types.h"

template <typename T>
class vector {
public:
	vector();
	vector( Arena* ArenaData, U32 Size );
	~vector();

	void Init(Arena* ArenaData, U32 Size);
	void Resize(U64 Size);

	void PushBack( T value );
	void PushFront( T value );
	void PopBack();
	void PopFront();

	[[nodiscard]] T& At(U32 idx) { if (idx <= N_Elements && idx < Size) { if (idx == N_Elements) N_Elements++; IdxTail = (IdxTail + 1) % Size; return Data[idx]; } else { fprintf(stderr, "Idx: %d\n", idx); assert(idx <= N_Elements); } }
	[[nodiscard]] T* GetData() { return Data; };

	[[nodiscard]] U32 GetLength()   const { return N_Elements; }
	[[nodiscard]] U32 GetCapacity() const { return Size; }

	void SetLength(U32 len) { N_Elements = len; }

	inline void Clear() { N_Elements = 0; IdxStart = IdxTail = 0; }

private:
	Arena* ArenaData;
	T*  Data;
	U32 IdxStart;
	U32 IdxTail;
	U32 N_Elements;
	U32 Size;
};

#endif
