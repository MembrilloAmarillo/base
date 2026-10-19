template<typename T>
dyn_vector<T> dyn_vector<T>::Init(Allocator* Mem, u64 Size) {

  dyn_vector<T> Vec{};

  Vec.Mem  = Mem;
  if (Size == 0) {
    Size = 1;
  }
  Vec.Data   = Mem_Allocator::Make<T>(Mem, Size);
  Vec.Size   = Size;
  Vec.Offset = 0;
  Vec.Len = 0;

  return Vec;
}

template<typename T>
void dyn_vector<T>::Append(T & value) {
  if (Len >= Size) {
    u64 new_resize = (Size == 0) ? 1 : 2 * Size;
    Resize(new_resize);
  }
  Data[Len] = value;
  Offset += sizeof(T);
  Len++;
}

template<typename T>
void dyn_vector<T>::AppendByCopy(T value) {
  if (Len >= Size) {
    u64 new_resize = (Size == 0) ? 1 : 2 * Size;
    Resize(new_resize);
  }
  Data[Len] = value;
  Offset += sizeof(T);
  Len++;
}

template<typename T>
void dyn_vector<T>::PushFirst(T & value) {
  if (Len >= Size) {
    u64 new_resize = (Size == 0) ? 1 : 2 * Size;
    Resize(new_resize);
  }
  /* shift existing elements one slot to the right */
  if (Len > 0) {
    memmove(
      reinterpret_cast<char*>(Data) + sizeof(T),
      reinterpret_cast<char*>(Data),
      Len * sizeof(T)
    );
  }
  Data[0] = value;
  Offset += sizeof(T);
  Len++;
}

template<typename T>
void dyn_vector<T>::Pop() {
  assert(Len > 0 && "[dyn_vector] Pop Error, vector is empty");
  if (Len == 0) {
    return;
  }
  Offset -= sizeof(T);
  Len -= 1;
}

template<typename T>
void dyn_vector<T>::Delete(u64 idx) {
  assert(idx < Len && "[dyn_vector] Delete Error, index out of bounds");
  if (idx >= Len) {
    return;
  }
  u64 IdxOffset = idx * sizeof(T);

  memmove(
    reinterpret_cast<U8*>(Data) + IdxOffset,
    reinterpret_cast<U8*>(Data) + IdxOffset + sizeof(T),
    (Len - idx - 1) * sizeof(T)
  );
  Len -= 1; Offset -= sizeof(T);
}

template<typename T>
void dyn_vector<T>::Resize(u64 NewSize) {
  assert( NewSize > Size  && "[dyn_vector] Resize Error, New Size has to be bigger than old size\n");
  T* old_data = Data;
  u64 old_len = Len;
  Data = Mem_Allocator::Make<T>(Mem, NewSize);
  if (old_data && old_len > 0) {
    memcpy(static_cast<void*>(Data), static_cast<void*>(old_data), Offset);
  }
  Mem_Allocator::Delete<T>(Mem, old_data);
  Size = NewSize;
}
