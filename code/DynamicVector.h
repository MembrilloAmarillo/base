#ifndef _DYNAMIC_VECTOR_H_ 
#define _DYNAMIC_VECTOR_H_ 

#include "types.h"
#include "memory.h"


template <typename T>
struct dyn_vector {

  dyn_vector()  = delete;
  ~dyn_vector() {};

  global dyn_vector Init(Arena* MemArena, u64 Size);

  void Append(T & value);
  void Append(T value);
  void Insert(T & value, u64 idx);
  void PushFirst(T & value);
  void Pop();
  void Delete(u64 idx);
  void Resize(u64 new_size);

  const u64 Length() const { return Len; }
  const u64 SizeBytes() const { return Offset; }

  const T & At(u64 idx) const { return Data[idx]; }

  Arena *MemArena;
  T     *Data;
  u64    Offset;
  u64    Size;
  u64    Len;

};


#endif // _DYNAMIC_VECTOR_H_ 