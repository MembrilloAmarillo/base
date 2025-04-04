/* date = April 1st 2025 2:58 pm */

#ifndef MATH_H
#define MATH_H

#include "../types/types.h"

struct Vec2 {
	union {
		struct {
			f32 x;
			f32 y;
		};
		struct {
			f32 vec[3];
		};
	};
};

struct Vec3 {
    union {
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
		struct {
			f32 vec[3];
        };
    };
};

struct Vec4 {
    union {
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
			f32 vec[4];
        };
    };
};

u64 Min(u64 a, u64 b);

u64 Max(u64 a, u64 b);

u32 Min(u32 a, u32 b);

u32 Max(u32 a, u32 b);

f32 Min(f32 a, f32 b);

f32 Max(f32 a, f32 b);

f64 Min(f64 a, f64 b);

f64 Max(f64 a, f64 b);

f32 Clamp(f32 x, f32 min, f32 max);

#endif //MATH_H
