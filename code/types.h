#ifndef _TYPES_H_
#define _TYPES_H_

#include <stdint.h>

#define u8  uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u64 uint64_t
#define i8  int8_t
#define i16 int16_t
#define i32 int32_t
#define i64 int64_t

#define f32 float
#define F64 double

#define U64_MAX ((u64)(1ULL << 63) - 1)

#if !defined(__cplusplus)
#if (defined(_MSC_VER) && _MSC_VER < 1800) || (!defined(_MSC_VER) && !defined(__STDC_VERSION__))
#ifndef true
#define true  (0 == 0)
#endif
#ifndef false
#define false (0 != 0)
#endif
typedef unsigned char bool;
#else
#include <stdbool.h>
#endif
#endif

// --- Add your chosen fix here ---
#if defined(_MSC_VER) && !defined(__cplusplus)
    typedef long double max_align_t;
#endif


#define internal      static
#define global        static
#define local_persist static

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

typedef union vec3 vec3;
union vec3 {
	struct {
		f32 x;
		f32 y;
		f32 z;
	};
	struct {
		f32 r;
		f32 g;
		f32 b;
	};

	f32 v[3];
};

typedef union rgba rgba;
union rgba {
	struct {
		u8 r;
		u8 g;
		u8 b;
		u8 a;
	};

	u8 v[4];
};

typedef union vec2 vec2;
union vec2 {
	struct {
		f32 x;
		f32 y;
	};
	struct {
		f32 r;
		f32 g;
	};

	f32 v[2];
};

internal inline vec2 Vec2Zero() {
	vec2 x = {0, 0};
	return x;
}

internal inline vec2 Vec2New(f32 x, f32 y) {
	vec2 vx = {x, y};
	return vx;
}

internal inline vec2 Vec2Add(vec2 v1, vec2 v2) {
	vec2 x = { v1.x + v2.x, v1.y + v2.y };
	return x;
}

internal inline vec2 Vec2Sub(vec2 v1, vec2 v2) {
	vec2 x = { v1.x - v2.x, v1.y - v2.y };
	return x;
}

internal inline vec2 Vec2Mul(vec2 v1, vec2 v2) {
	vec2 x = { v1.x * v2.x, v1.y * v2.y };
	return x;
}

internal inline vec2 Vec2ScalarMul(f32 scalar, vec2 v1) {
	vec2 x = { scalar * v1.x, scalar * v1.y };
	return x;
}

typedef union vec4 vec4;
union vec4 {
	struct {
		f32 x;
		f32 y;
		f32 z;
		f32 w;
	};
	struct {
		f32 r;
		f32 g;
		f32 b;
		f32 a;
	};
	struct {
		vec2 xy;
		vec2 zw;
	};

	f32 v[4];
};

internal inline vec4 Vec4New(f32 x, f32 y, f32 z, f32 w) {
	vec4 v = { x, y, z, w };
	return v;
}



typedef struct DLL DLL;
struct DLL {
	DLL* Root;
	DLL* Prev;
	DLL* Next;
	void* val;
};

#define StackPush(Stack, Val)            \
if((Stack)->Current < (Stack)->N - 1) {  \
(Stack)->Items[(Stack)->Current] = Val;  \
(Stack)->Current += 1;                   \
}

#define StackPop(Stack)    \
if((Stack)->Current > 0) {  \
(Stack)->Current -= 1;     \
}

#define StackClear(Stack) (Stack)->Current = 0

#define StackIsEmpty(Stack) ((Stack)->Current == 0)

#define StackGetFront(Stack) (Stack)->Items[(Stack)->Current - 1]

#define DLIST_INSERT(Sentinel, Element)     \
(Element)->Next = (Sentinel)->Next;         \
(Element)->Prev = (Sentinel);               \
(Element)->Next->Prev = (Element);          \
(Element)->Prev->Next = (Element);

#define DLIST_REMOVE(Element)                        \
if((Element)->Next)                                  \
{                                                    \
(Element)->Next->Prev = (Element)->Prev;         \
(Element)->Prev->Next = (Element)->Next;         \
(Element)->Next = (Element)->Prev = 0;           \
}

#define DLIST_INSERT_AS_LAST(Sentinel, Element) \
(Element)->Next = (Sentinel);                   \
(Element)->Prev = (Sentinel)->Prev;             \
(Element)->Next->Prev = (Element);              \
(Element)->Prev->Next = (Element);

#define DLIST_INIT(Sentinel)   \
(Sentinel)->Next = (Sentinel); \
(Sentinel)->Prev = (Sentinel)

#define DLIST_IS_EMPTY(Sentinel) \
((Sentinel)->Next == (Sentinel))

// se example on ui library to now what I mean
// Is just a replacement where you can set the null object
// to NULL or maybe another default fallback structure
//
#define TreeInit(Root, NULL_OBJ)                                \
	(Root)->Parent = (Root);                                    \
	(Root)->Left = (Root)->Right = (Root)->FirstSon = NULL_OBJ; \
	(Root)->Last = NULL_OBJ;

#define TreeClear(Root, NULL_OBJ) \
	TreeInit(Root, NULL_OBJ);

#define TreePushSon(Node, Son, NULL_OBJ)             \
	if( (Node)->FirstSon == NULL_OBJ ) {             \
		(Node)->FirstSon = Son;                      \
		(Son)->Left = (Son)->Right = NULL_OBJ;       \
		(Son)->Parent = Node;                        \
		(Node)->Last  = Son;                         \
	} else {                                         \
		(Node)->Last->Right = Son;                   \
		(Son)->Left = (Node)->Last;                  \
		(Son)->Parent = Node;                        \
		(Son)->Right = (Son)->Last = NULL_OBJ;       \
		(Node)->Last  = Son;                         \
	}                                                \

#define TreePop(Node, NULL_OBJ)                       \
	if( (Node)->Parent == NULL ) {                    \
		TreeClear(Node, NULL_OBJ)                     \
	} else {                                          \
		if( Node == (Node)->Parent->FirstSon ) {      \
			(Node)->Parent->FirstSon = (Node)->Right; \
		} else {                                      \
			(Node)->Left->Right = (Node)->Right;      \
		}                                             \
	}                                                 \

#endif
