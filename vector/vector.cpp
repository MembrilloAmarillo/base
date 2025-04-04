#include "vector.h"


// ------------------------------------------------------------------

template <typename T>
vector<T>::vector( Arena* pArenaData = nullptr, u32 Size = 0  ) {
	if (pArenaData == nullptr) {
		ArenaData = (Arena*)malloc(sizeof(Arena));
		ArenaData->Init(0, Size);
	}
	else {
		ArenaData = pArenaData;
	}

	this->Size = Size;

	Data = ArenaData->Push<T>(Size);
	N_Elements = 0;
	IdxStart = 0;
	IdxTail  = 0;
}

// ------------------------------------------------------------------

template<typename T>
void vector<T>::PushBack(T value) {
	if (N_Elements == Size) {
		fprintf(stderr, "[ERROR] Pushing in vector with full capacity\n");
		exit(1);
	}
	Data[IdxTail] = value;
	IdxTail = IdxTail % Size;
	N_Elements += 1;
}

// ------------------------------------------------------------------

template<typename T>
void vector<T>::PushFront(T value) {
	if (N_Elements == Size - 1) {
		fprintf(stderr, "[ERROR] Pushing in vector with full capacity\n");
		exit(1);
	}
	if (IdxStart == 0) {
		IdxStart = N_Elements + 1;
	}
	else {
		IdxStart -= 1;
	}
	Data[IdxStart] = value;
	N_Elements += 1;
}

// ------------------------------------------------------------------

template<typename T>
void vector<T>::PopBack() {
	if (N_Elements == 0) {
		fprintf(stderr, "[ERROR] Pop in empty vector\n");
		exit(1);
	}
	if (IdxStart == IdxTail && IdxStart != 0) {
		IdxStart = IdxTail = 0;
	} else if (IdxTail != 0 ) {
		IdxTail = (IdxTail - 1) % Size;
	}
	else if (IdxTail == 0) {
		IdxTail = N_Elements;
	}
	N_Elements -= 1;
}

// ------------------------------------------------------------------

template<typename T>
void vector<T>::PopFront() {
	if (N_Elements == 0) {
		fprintf(stderr, "[ERROR] Pop in empty vector\n");
		exit(1);
	}
	if (IdxStart == IdxTail && IdxStart != 0) {
		IdxStart = IdxTail = 0;
	} else {
		IdxStart = (IdxStart + 1) % Size;
	}
	N_Elements -= 1;
}

// ------------------------------------------------------------------

// TODO: I have to know how to erase this 
//
template<typename T>
vector<T>::~vector() {
}