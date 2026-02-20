#ifndef _DYNAMIC_VECTOR_H_
#define _DYNAMIC_VECTOR_H_

#include <cstring>
#include <cassert>

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
  void Destroy() {
    Mem_Allocator::Delete<T>(Mem, Data);
  }

  T* begin()              noexcept { return Data; }
  T* end()                noexcept { return Data + Size; }
  const T* begin()  const noexcept { return Data; }
  const T* end()    const noexcept { return Data + Size; }
  const T* cbegin() const noexcept { return Data; }
  const T* cend()   const noexcept { return Data + Size; }

  void Append(T & value)           noexcept;
  void AppendByCopy(T value)       noexcept;
  void Insert(T & value, u64 idx)  noexcept;
  void PushFirst(T & value)        noexcept;
  void Pop()                       noexcept;
  void Delete(u64 idx)             noexcept;
  void Resize(u64 new_size)        noexcept;

  T* Memory() { return Data; }

  const u64 Capacity()  const noexcept { return Size; }
  const u64 Length()    const noexcept { return Len; }
  const u64 SizeBytes() const noexcept { return Offset; }
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
