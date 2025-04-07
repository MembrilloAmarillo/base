#ifndef _LIGHT_HASH_H_
#define _LIGHT_HASH_H_

#include <functional>
#include <cassert>

#include "../types/types.h"
#include "../memory/memory.h"
#include "vector.h"

global U64 NextPowerOfTwo(U64 x);
//#define COUNT_COLLISIONS

template <typename key_type, value_type>
struct Table {

	U64 Count       { 0 };
	U64 Allocated   { 0 };
	U64 SlotsFilled { 0 };
	U64 MinSize     { 32 };

	U64 LoadFactorPercent { 70 };

	Arena* AllocArena    { nullptr };

	struct Entry {
		U64        Hash;
		key_type   Key;
		value_type Value;
	};

	template <typename T>
	struct ValueSuccess {
		T Value;
		bool Success;
	};

	vector<Entry> Entries;
	bool CustomFunction;

	std::function<U64> HashFunction;

	void Init(Arena* arena, U64 Size);
	void Resize(U64 Size);
	void Deinit();
	void Reset();
	void EnsureSpace(U64 Items);
	void Expand();

	value_type*  Add(key_type Key, value_type Value);
	value_type*  Set(key_type Key, value_type Value);
	bool         Contains(key_type Key);
	value_type*  FindPointer(key_type Key);

	ValueSuccess<value_type>  Find(key_type);
	ValueSuccess<value_type>  Remove(key_type);
	ValueSuccess<value_type*> FindOrAdd(key_type Key, value_type Type);

};

#endif // _LIGHT_HASH_H_