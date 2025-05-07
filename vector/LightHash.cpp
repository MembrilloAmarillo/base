#include "LightHash.h"

// ------------------------------------------------------------------

global 
U64 NextPowerOfTwo(U64 x)
{
    assert(x != 0);
    U64 p = 1;
	while (x > p)
	{
		p += p;
	}
    return p;
}

// ------------------------------------------------------------------

template <typename key_type, typename value_type>
void Table<key_type, value_type>::Init(Arena* arena, U64 size)
{
	AllocArena = arena; 

	Entries.Init(AllocArena, Size);
}

// ------------------------------------------------------------------

template<typename key_type, typename value_type>
void Table<key_type, value_type>::Resize(U64 Size)
{
	Entries.Resize(U64 Size);
}

// ------------------------------------------------------------------

template<typename key_type, typename value_type>
void Table<key_type, value_type>::Deinit()
{
	Entries.Clear();
	Reset();
}

// ------------------------------------------------------------------

template<typename key_type, typename value_type>
void Table<key_type, value_type>::Reset()
{
	Count       = 0;
	Allocated   = 0;
	SlotsFilled = 0;

	Entries.Clear();
}

// ------------------------------------------------------------------

template<typename key_type, typename value_type>
void Table<key_type, value_type>::EnsureSpace( U64 Items )
{
	if ((SlotsFilled + Items) * 100 >= Allocated * LoadFactorPercent)
	{
		Expand();
	}
}

// ------------------------------------------------------------------

template<typename key_type, typename value_type>
void Table<key_type, value_type>::Expand()
{
	vector<Entry> OldEntries = Entries;
	U64 NewAllocated = 0;

	if ((Count * 2 + 1) * 100 < Allocated * LoadFactorPercent) {
		NewAllocated = Allocated;
	}
	else {
		NewAllocated = Allocated * 2;
	}

	if (NewAllocated < MinSize)
	{
		NewAllocated = MinSize;
	}

	Resize(NewAllocated);

	Count = 0;
	SlotsFilled = 0;

	for (U64 idx = 0; idx < OldEntries.GetLength(); idx += 1)
	{
		Entry _Entry = OldEntries.At(idx);
		if (Entry.Hash >= FirstValidHash) {
			Add(_Entry.Key, _Entry.Value);
		}
	}
}

// ------------------------------------------------------------------
