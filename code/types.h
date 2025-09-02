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

#define internal static

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

typedef union vec4 vec4;
union vec4 {
	struct {
		f32 x; 
		f32 y; 
		f32 z;
		f32 m;
	};
	struct {
		f32 r;
		f32 g;
		f32 b;
		f32 a;
	};

	f32 v[4];
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

#endif
