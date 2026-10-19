#ifndef _DYNAMIC_VECTOR_H_
#define _DYNAMIC_VECTOR_H_

#include <cstring>
#include <cassert>
#include <initializer_list>

#include "../util/types.h"
#include "../memory/memory.h"
#include "../memory/allocator.h"

template <typename T>
struct dyn_vector {

  dyn_vector() {};
  ~dyn_vector() {
    //Mem_Allocator::Delete<T>(Mem, Data);
  };

  static dyn_vector Init(Allocator* Mem, u64 Size);

  dyn_vector& operator=(std::initializer_list<T> values) {
    if (values.size() <= Size) {
        Len = 0;
        for (const T& v : values) {  // Range-based for works
            Data[Len++] = v;
        }
    } else {
        Resize(values.size());
        Len = 0;
        for (const T& v : values) {
            Data[Len++] = v;
        }
    }
    return *this;
  }

  void Destroy() {
    Mem_Allocator::Delete<T>(Mem, Data);
  }

  T* begin()              noexcept { return Data; }
  T* end()                noexcept { return Data + Len; }
  const T* begin()  const noexcept { return Data; }
  const T* end()    const noexcept { return Data + Len; }
  const T* cbegin() const noexcept { return Data; }
  const T* cend()   const noexcept { return Data + Len; }

  void Append(T & value);
  void AppendByCopy(T value);
  void Insert(T & value, u64 idx);
  void PushFirst(T & value);
  void Pop();
  void Delete(u64 idx);
  void Resize(u64 new_size);

  T* Memory() { return Data; }

  u64 Capacity()  const noexcept { return Size; }
  u64 Length()    const noexcept { return Len; }
  u64 SizeBytes() const noexcept { return Offset; }
  const T & At(u64 idx) const noexcept { return Data[idx]; }

  T& operator [](u64 idx) {
    return Data[idx];
  }

  const T& operator [](u64 idx) const {
    return Data[idx];
  }

  Allocator *Mem;
  T     *Data;
  u64    Offset;
  u64    Size;
  u64    Len;

};

#endif // _DYNAMIC_VECTOR_H_
