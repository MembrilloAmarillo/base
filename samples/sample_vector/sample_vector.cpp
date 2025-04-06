#include <stdio.h>
#include <stdlib.h>

#ifdef __linux__
#include "../../memory/linux/memory_linux_impl.cpp"
#elif _WIN32
#include "../../memory/win/memory_win_impl.cpp"
#endif

#include "../../memory/memory.cpp"
#include "../../vector/vector.cpp"

int main() {
	
	Arena* MainArena = (Arena*)malloc(sizeof(Arena));
	MainArena->Init(0, 256 << 20);

	vector<U32> MyVector;
	MyVector.Init(MainArena, 256000);

	for (U32 i = 0; i < 255999; i += 1) 
	{
		MyVector.PushBack(i);
	}

	fprintf(stdout, "Length  : %d\n", MyVector.GetLength());
	fprintf(stdout, "Capacity: %d\n", MyVector.GetCapacity());

	for (U32 i = 0; i < 255998; i += 1) 
	{
		MyVector.PopBack();
	}

	fprintf(stdout, "Length  : %d\n", MyVector.GetLength());
	fprintf(stdout, "Capacity: %d\n", MyVector.GetCapacity());

	for (U32 i = 0; i < 255998; i += 1) 
	{
		MyVector.PushFront(i);
	}

	fprintf(stdout, "Length  : %d\n", MyVector.GetLength());
	fprintf(stdout, "Capacity: %d\n", MyVector.GetCapacity());


	return 0;
}