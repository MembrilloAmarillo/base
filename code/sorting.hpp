#ifndef __SORTING_HPP__

#include "types.h"

template <typename T>
fn_internal void TopDownSplitMerge(T*BackBuffer, T* Buffer, i64 N);

#endif // __SORTING_HPP__

#ifdef SORT_IMPL

template<typename T>
fn_internal void TopDownMerge(T* BackBuffer, T* Buffer, i64 Begin, i64 Middle, i64 End) {
	i64 i = Begin, j = Middle;
    for (i64 k = Begin; k < End; k++) {
        if (i < Middle && (j >= End || Buffer[i] >= Buffer[j])) {
            BackBuffer[k] = Buffer[i];
            i++;
        } else {
            BackBuffer[k] = Buffer[j];
            j++;
        }
    }
}

template<typename T>
fn_internal void TopDownSplitMergeEx(T* BackBuffer, T* Buffer, i64 Begin, i64 End) {
	if (End - Begin <= 1) {
		return;
	}

	u32 Middle = (End + Begin) / 2;
	TopDownSplitMergeEx(Buffer, BackBuffer, Begin, Middle);
	TopDownSplitMergeEx(Buffer, BackBuffer, Middle, End);
	TopDownMerge(Buffer, BackBuffer, Begin, Middle, End);
}

template<typename T>
fn_internal void TopDownSplitMerge(T* BackBuffer, T* Buffer, i64 N) {
	TopDownSplitMergeEx(BackBuffer, Buffer, 0, N);
}

#endif