/* date = April 1st 2025 2:58 pm */

#ifndef MATH_H
#define MATH_H

#include <xmmintrin.h>
#include <immintrin.h>

#include "../types/types.h"

#define POW2(a) ((a) * (a))

#define POW3(a) ((a) * (a) * (a))

struct Vec2 {
	union {
		struct {
			F32 x;
			F32 y;
		};
		struct {
			F32 vec[2];
		};
	};
};

struct Vec3 {
    union {
        struct {
            F32 x;
            F32 y;
            F32 z;
        };
        struct {
            F32 r;
            F32 g;
            F32 b;
        };
		struct {
			F32 vec[3];
        };
    };
};

struct Vec4 {
    union {
        struct {
            F32 x;
            F32 y;
            F32 z;
			F32 w;
        };
        struct {
            F32 r;
            F32 g;
            F32 b;
			F32 a;
        };
		struct {
			F32 vec[4];
        };
    };
};

struct m4x4 {
	// [row][column]
	//
	F32 V[4][4];
};

struct f32_4v_simd {
	union {
		__m128 V;
		F32    F[4];
		U32    U[4];
	};
};


// Casey style simd matrix
//
struct m4x4_simd {
	union
    {
        struct
        {
            union
            {
                f32_4v_simd x;
                f32_4v_simd r;
            };
            
            union
            {
                f32_4v_simd y;
                f32_4v_simd g;
            };
            
            union
            {
                f32_4v_simd z;
                f32_4v_simd b;
            };
            
            union
            {
                f32_4v_simd w;
                f32_4v_simd a;
            };
        };
        
        f32_4v_simd E[4];
    };
};

U64 Min(U64 a, U64 b);

U64 Max(U64 a, U64 b);

U32 Min(U32 a, U32 b);

U32 Max(U32 a, U32 b);

F32 Min(F32 a, F32 b);

F32 Max(F32 a, F32 b);

F64 Min(F64 a, F64 b);

F64 Max(F64 a, F64 b);

F32 Clamp(F32 x, F32 min, F32 max);

#endif //MATH_H
