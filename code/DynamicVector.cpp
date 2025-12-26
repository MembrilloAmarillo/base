template<typename T>
dyn_vector<T> dyn_vector<T>::Init(Arena* MemArena, u64 Size) {
  
  dyn_vector<T> Vec = {};

  Vec.MemArena  = MemArena;
  Vec.Data   = PushArray(MemArena, T, Size);
  Vec.Size   = Size;
  Vec.Offset = 0;

  return Vec;
}

template<typename T>
void dyn_vector<T>::Append(T & value) {
  Data[Len] = value;
  Offset += sizeof(T);
  Len++;
}

template<typename T>
void dyn_vector<T>::Append(T value) {
  Data[Len] = value;
  Offset += sizeof(T);
  Len++;
}

template<typename T>
void dyn_vector<T>::PushFirst(T & value) {
  memcpy((u8*)Data + sizeof(T), (u8*)Data, Offset - sizeof(Offset));
  *Data = value;
  Len++;
}

template<typename T>
void dyn_vector<T>::Pop() {
  Offset -= sizeof(T);
  Len -= 1;
}

template<typename T>
void dyn_vector<T>::Delete(u64 idx) {
  u64 IdxOffset = idx * sizeof(T);

  memmove((u8*)Data + IdxOffset, (u8*)Data + IdxOffset + sizeof(T), Len - IdxOffset - 1);
}

template<typename T>
void dyn_vector<T>::Resize(u64 NewSize) {
  assert(false && "[dyn_vector] Resize Error, still needs to be implemented\n");
  Size = NewSize;
}