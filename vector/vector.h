#ifndef _VECTOR_H_ 
#define _VECTOR_H_ 

#include <cassert>

#include "../memory/memory.h" 
#include "../types/types.h"

template <typename T>
class vector {
public:
	vector();
	vector( Arena* ArenaData, U32 Size );
	~vector();

	void Init(Arena* ArenaData, U32 Size);
	void PushBack( T value );
	void PushFront( T value );

	void PopBack();
	void PopFront();

	[[nodiscard]] T& At(U32 idx) { if (idx < N_Elements) { return Data[idx]; } else { assert(idx < N_Elements); } }
	[[nodiscard]] T* GetData() { return Data; };

	[[nodiscard]] U32 GetLength()   const { return N_Elements; }
	[[nodiscard]] U32 GetCapacity() const { return Size; }

	void BufferSetLength(U32 len) { N_Elements = len; }

	inline void Clear() { N_Elements = 0; }

private:
	Arena* ArenaData;
	T*  Data;
	U32 IdxStart;
	U32 IdxTail;
	U32 N_Elements;
	U32 Size;
};

#endif 