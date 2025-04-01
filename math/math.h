/* date = April 1st 2025 2:58 pm */

#ifndef MATH_H
#define MATH_H

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

#endif //MATH_H