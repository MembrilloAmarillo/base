#include "math.h"

U64 Min( U64 a, U64 b ) {
    if( a < b ) { return a; }

    return b;
}

U64 Max( U64 a, U64 b ) {
    if( a > b ) { return a; }

    return b;
}

U32 Min( U32 a, U32 b ) {
    if( a < b ) { return a; }

    return b;
}

U32 Max( U32 a, U32 b ) {
    if( a > b ) { return a; }

    return b;
}

F32 Min( F32 a, F32 b ) {
    if( a < b ) { return a; }

    return b;
}

F32 Max( F32 a, F32 b ) {
    if( a > b ) { return a; }

    return b;
}

F64 Min( F64 a, F64 b ) {
    if( a < b ) { return a; }

    return b;
}

F64 Max( F64 a, F64 b ) {
    if( a > b ) { return a; }

    return b;
}

F32 Clamp(F32 x, F32 min, F32 max) {
	if (x < min) {
		return min;
	}
	if (x > max) {
		return max;
	}
	return x;
}

F64 Clamp(F64 x, F64 min, F64 max) {
	if (x < min) {
		return min;
	}
	if (x > max) {
		return max;
	}
	return x;
}

U32 Clamp(U32 x, U32 min, U32 max) {
	if (x < min) {
		return min;
	}
	if (x > max) {
		return max;
	}
	return x;
}

U64 Clamp(U64 x, U64 min, U64 max) {
	if (x < min) {
		return min;
	}
	if (x > max) {
		return max;
	}
	return x;
}