#ifndef _D_DRAW_H_
#define _D_DRAW_H_

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

#define MinVec2D_Size (1 << 20)
#define MinVec3D_Size (1 << 20)

typedef struct rect_2d rect_2d;
struct rect_2d {
    vec2 Pos;
    vec2 Size;
};

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
    void*            BackBuffer2D;
    void*            BackBuffer3D;
    draw_bucket_2d   Instance2D;
    draw_bucket_3d   Instance3D;
    vector           Current2DBuffer;
    vector           Current3DBuffer;
};

internal draw_bucket_instance D_DrawInit(Stack_Allocator* Alloc);

internal void D_DrawDestroy( draw_bucket_instance* Instance );

internal void D_BeginDraw2D( draw_bucket_instance* Instance );

internal void D_EndDraw2D( draw_bucket_instance* Instance );

internal void D_DrawRect2D( draw_bucket_instance* Instance, rect_2d rect, f32 CornerRadius, f32 BorderSize, vec4 RgbaColor );

internal void D_DrawIcon(draw_bucket_instance* Instance, rect_2d rect, rect_2d IconUvPos);

internal void D_DrawText2D( draw_bucket_instance* Instance, rect_2d rect, U8_String* Text, FontCache* FC, vec4 Color );

internal rect_2d NewRect2D(f32 x, f32 y, f32 width, f32 height);

#endif

#ifdef DRAW_IMPL

internal draw_bucket_instance
D_DrawInit(Stack_Allocator* Alloc) {
    draw_bucket_instance D_Instance;
    D_Instance.Allocator = Alloc;
    D_Instance.BackBuffer2D = stack_push(Alloc, v_2d, MinVec2D_Size);
    //D_Instance.BackBuffer3D = stack_push(Alloc, v_3d, MinVec3D_Size);

    DLIST_INIT(&D_Instance.Instance2D);
    DLIST_INIT(&D_Instance.Instance3D);

	return D_Instance;
}

internal void
D_DrawDestroy( draw_bucket_instance* D_Instance ) {
    stack_free_all(D_Instance->Allocator);
}

internal void
D_BeginDraw2D( draw_bucket_instance* D_Instance ) {
    //D_Instance->BackBuffer2D = stack_push(D_Instance->Allocator, draw_bucket_2d, MinVec2D_Size);
    D_Instance->Current2DBuffer = VectorNew(D_Instance->BackBuffer2D, 0, MinVec2D_Size, v_2d);
}

internal void
D_EndDraw2D( draw_bucket_instance* D_Instance ) {
    //draw_bucket_2d* Bucket = stack_push(D_Instance->Allocator, draw_bucket_2d, 1);
    //DLIST_INIT(Bucket);
    //DLIST_INSERT(&D_Instance->Instance2D, Bucket);
}

internal void
D_DrawRect2D(draw_bucket_instance* Instance, rect_2d rect, f32 CornerRadius, f32 BorderSize, vec4 RgbaColor) {
	v_2d Vertex = {};
	Vertex.LeftCorner = rect.Pos;
	Vertex.Size = rect.Size;
	Vertex.UV = { -2, -2 };
	Vertex.Color = RgbaColor;
	Vertex.CornerRadius = CornerRadius;
	Vertex.Border = BorderSize;
	Vertex.IconUvSize = Vec2New(0, 0);

	VectorAppend(&Instance->Current2DBuffer, &Vertex);
}

internal void
D_DrawIcon(draw_bucket_instance* Instance, rect_2d rect, rect_2d IconUvPos) {
	v_2d Vertex = {};
	Vertex.LeftCorner = rect.Pos;
	Vertex.Size = rect.Size;
	Vertex.UV = Vec2Zero();
	Vertex.CornerRadius = 0;
	Vertex.Border = 0;
	Vertex.IconUv = IconUvPos.Pos;
	Vertex.IconUvSize = IconUvPos.Size;

	VectorAppend(&Instance->Current2DBuffer, &Vertex);
}

internal void
D_DrawText2D(draw_bucket_instance* Instance, rect_2d rect, U8_String* Text, FontCache* FC, vec4 Color ) {

	vec2 Pos = rect.Pos;
	if( Text->data != NULL && Text->idx > 0 ) {
		f32 txt_offset = 0;
		// Use unsigned char pointer to avoid signedness issues
		u8* p = Text->data;
		float pen_x = (float)Pos.x;
		float pen_y = (float)Pos.y; // baseline or top depending on your coordinate convention
		for ( u32 _it = 0; *p && _it < Text->idx && pen_x; ++p, ++_it) {
			// Skip UTF-8 continuation bytes (0b10xxxxxx)
			if ( (*p & 0xC0) == 0x80 ) continue;

			unsigned char ch = *p;

			int gi             = (int)ch - 32;
			FontCache* UI_Font = FC;
			f_Glyph g          = UI_Font->glyph[gi];
			vec2 BitmapOffset  = UI_Font->BitmapOffset;


			// UVs: add half-texel offset to avoid bleeding when sampling with linear filter
			float _u0 = (g.x + BitmapOffset.x);
			float _v0 = (g.y + BitmapOffset.y);
			float _u1 = (g.width);
			float _v1 = (g.height);

			float x0 = pen_x + g.x_off;
			float y0 = pen_y + g.y_off + UI_Font->line_height;

			vec4 ColorVec = {Color.r, Color.g, Color.b, Color.a};
		// Create quad vertices in the same winding you use for indices
		v_2d v1 = {};
		v1.LeftCorner = { x0, y0 };
		v1.Size = {g.width, g.height};
		v1.UV = { _u0, _v0 };
		v1.UVSize = { _u1, _v1 };
		v1.Color = ColorVec;
		v1.CornerRadius = 0;
		v1.IconUvSize = Vec2Zero();			
		VectorAppend(&Instance->Current2DBuffer, &v1);

			// Advance pen by glyph advance (use xadvance from packing)
			pen_x += g.advance;
			if( _it < Text->idx - 1 ) {
				pen_x += F_GetKerningFromCodepoint( UI_Font, (int)gi, (int)*(p + 1) - 32 );
			}
		}
	}
}

internal rect_2d NewRect2D(f32 x, f32 y, f32 width, f32 height) {
	rect_2d Rect;
	Rect.Pos.x = x;
	Rect.Pos.y = y;
	Rect.Size.x = width;
	Rect.Size.y = height;

	return Rect;
}

#endif
