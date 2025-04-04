#ifndef _VECTOR_H_ 
#define _VECTOR_H_ 

#include "../memory/memory.h" 
#include "../types/types.h"

template <typename T>
class vector {
public:
	vector( Arena* ArenaData, u32 Size );
	~vector();

	void PushBack( T value );
	void PushFront( T value );

	void PopBack();
	void PopFront();

	u32 GetLength() { return N_Elements; }
	u32 GetCapacity() { return Size; }

private:
	Arena* ArenaData;
	T*  Data;
	u32 IdxStart;
	u32 IdxTail;
	u32 N_Elements;
	u32 Size;
};

#endif 