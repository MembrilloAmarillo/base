#ifndef _DRAW_H_
#define _DRAW_H_

#include <stdio.h>

#include "types.h"
#include "vector.h"
#include "queue.h"
#include "memory.h"
#include "allocator.h"
#include "strings.h"

#include "load_font.h"
#include "window_creation.h"
#include "vk_render.h"

#define MinVec2D_Size (8 << 10)
#define MinVec3D_Size (1 << 20)

typedef struct draw_bucket_2d draw_bucket_2d;
struct draw_bucket_2d {
    vector BucketData;
    draw_bucket_2d* Next;
    draw_bucket_2d* Prev;
};

typedef struct draw_bucket_3d draw_bucket_3d;
struct draw_bucket_3d {
    vector BucketData;
    draw_bucket_3d* Next;
    draw_bucket_3d* Prev;
};

typedef struct draw_bucket_instance draw_bucket_instance;
struct draw_bucket_instance {
    Stack_Allocator* Allocator;
    void*            2D_BackBuffer;
    void*            3D_BackBuffer;
    draw_bucket_2d   Instance2D;
    draw_bucket_3d   Instance3D;
    vector           Current2DBuffer;
    vector           Current3DBuffer;
};

internal draw_bucket_instance D_DrawInit(Stack_Allocator* Alloc);

internal void D_DrawDestroy( draw_bucket_instance* Instance );

internal void D_BeginDraw2D( draw_bucket_instance* Instance );

internal void D_EndDraw2D( draw_bucket_instance* Instance );

internal void D_RenderDraw( draw_bucket_instance* Instance, vulkan_base* Renderer, Stack_Allocator* Allocator );

internal void D_DrawRect2D( draw_bucket_instance* Instance );

internal void D_DrawText2D( draw_bucket_instance* Instance, vec4 rect, U8_String* Text, FontCache* FC );

#endif 

#ifdef DRAW_IMPL

internal draw_bucket_instance 
D_DrawInit(Stack_Allocator* Alloc) {
    draw_bucket_instance Instance;
    Instance.Allocator = Alloc;
    //Instance.2D_BackBuffer = stack_push(Alloc, v_2d, MinVec2D_Size);
    //Instance.3D_BackBuffer = stack_push(Alloc, v_3d, MinVec3D_Size);
        
    DLIST_INIT(&Instance.Instance2D);
    DLIST_INIT(&Instance.Instance3D);
}

internal void 
D_DrawDestroy( draw_bucket_instance* Instance ) {
    stack_free_all(Instance->Allocator);
}

internal void 
D_BeginDraw2D( draw_bucket_instance* Instance ) {
    Instance->2D_BackBuffer = stack_push(Instance->Allocator, draw_bucket_2d, MinVec2D_Size);
    Instance->Current2DBuffer = VectorNew(Instance->2D_BackBuffer, 0, MinVec2D_Size, v_2d);
}

internal void 
D_EndDraw2D( draw_bucket_instance* Instance ) {
    draw_bucket_2d* Bucket = stack_push(Instance->Allocator, draw_bucket_2d, 1);
    DLIST_INIT(Bucket);
    DLIST_INSERT(&Instance->Instance2D, Bucket);
}

internal void 
D_RenderDraw( draw_bucket_instance* Instance, vulkan_base* Renderer, Stack_Allocator* Allocator ) {

}

#endif 
