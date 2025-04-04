#include "math.h"

u64 Min( u64 a, u64 b ) {
    if( a < b ) { return a; }

    return b;
}

u64 Max( u64 a, u64 b ) {
    if( a > b ) { return a; }

    return b;
}

u32 Min( u32 a, u32 b ) {
    if( a < b ) { return a; }

    return b;
}

u32 Max( u32 a, u32 b ) {
    if( a > b ) { return a; }

    return b;
}

f32 Min( f32 a, f32 b ) {
    if( a < b ) { return a; }

    return b;
}

f32 Max( f32 a, f32 b ) {
    if( a > b ) { return a; }

    return b;
}

f64 Min( f64 a, f64 b ) {
    if( a < b ) { return a; }

    return b;
}

f64 Max( f64 a, f64 b ) {
    if( a > b ) { return a; }

    return b;
}

f32 Clamp(f32 x, f32 min, f32 max) {
	if (x < min) {
		return min;
	}
	if (x > max) {
		return max;
	}
	return x;
}

f64 Clamp(f64 x, f64 min, f64 max) {
	if (x < min) {
		return min;
	}
	if (x > max) {
		return max;
	}
	return x;
}

u32 Clamp(u32 x, u32 min, u32 max) {
	if (x < min) {
		return min;
	}
	if (x > max) {
		return max;
	}
	return x;
}

u64 Clamp(u64 x, u64 min, u64 max) {
	if (x < min) {
		return min;
	}
	if (x > max) {
		return max;
	}
	return x;
}