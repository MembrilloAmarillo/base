/**
* Copyright (c) 20225 Sascha Paez
*
* @author Sascha Paez
* @version 0.4
*
* This library is free software; you can redistribute it and/or modify it
* under the terms of the MIT license. See `microui.c` for details.
*/

/**
 * 2025/10/02. Added linked listing for box creation, in debug mode, ~1000 objects in 3ms, in release ~1.7 ms.
               Still have to be more efficient.
 * 2025/10/01. Added xxhash for faster hashing, and parent-dependent hashing.
               Also improved better scrolling and greater quantity of objects per frame.
 * 2025/09/24. Fixed drag, movement and resize. Also ordered windows by depth index.
 * 2025/09/23. Added Clipboard pasting.
 * 2025/09/23. Fixed Input declaration taking garbage values creating buggy responses
 * 2025/09/22. Better handling for input text when submiting on return
 * 2025/09/22. Input Text fixed.
 * 2025/09/22. Drag and resize added on Windows. Improvements still to be made.
 * 2025/09/19. First somewhat working version. Added tree node implementation
 */

#ifndef _SP_UI_H_
#define _SP_UI_H_

#include "draw.h"

#define HexToRGBA(val) ((rgba){.r = (val & 0xff000000) >> 24, .g = (val & 0x00ff0000) >> 16, .b = (val & 0x0000ff00) >> 8, .a = val & 0x000000ff})
#define HexToU8_Vec4(val) {(val & 0xff000000) >> 24, (val & 0x00ff0000) >> 16, (val & 0x0000ff00) >> 8, val & 0x000000ff}

#define RgbaToNorm(val) (ui_color){(f32)val.r / 255.f, (f32)val.g / 255.f, (f32)val.b / 255.f, (f32)val.a / 255.f}

#define RgbaNew(r, g, b, a) (rgba){r, g, b, a}

#define MAX_STACK_SIZE  64
#define MAX_LAYOUT_SIZE 256

typedef enum ui_lay_opt ui_lay_opt;
enum ui_lay_opt {
    UI_AlignRight  = (1 << 0),
    UI_AlignCenter = (1 << 1),
    UI_AlignTop    = (1 << 2),
    UI_AlignBottom = (1 << 3),
    UI_Frame       = (1 << 4),
    UI_Resize      = (1 << 5),
    UI_Interact    = (1 << 6),
    UI_Drag        = (1 << 7),
    UI_Select      = (1 << 8),
    UI_NoTitle     = (1 << 9),
    UI_AlignVertical = (1 << 10),
    UI_DrawText    = (1 << 11),
    UI_SetPosPersistent = (1 << 12 ),
    UI_DrawShadow       = (1 << 13)
};

typedef enum object_type object_type;
enum object_type {
    UI_Rect,
    UI_LabelType,
    UI_Text,
    UI_Icon,
    UI_List,
    UI_Tree,
    UI_CheckBox,
    UI_InputText,
    UI_ImageType,
    UI_Window,
    UI_ButtonType,
    UI_Panel,
    UI_ScrollbarType,
    UI_ScrollbarTypeButton
};

typedef struct ui_color ui_color;
struct ui_color {
    f32 r;
    f32 g;
    f32 b;
    f32 a;
};

typedef struct object_theme object_theme;
struct object_theme {
    ui_color   Border;
    ui_color   Background;
    ui_color   Foreground;
    f32        Radius;
    f32        BorderThickness;
    FontCache* Font;
};


typedef struct ui_theme ui_theme;
struct ui_theme {
    object_theme Window;
    object_theme Button;
    object_theme Panel;
    object_theme Input;
    object_theme Label;
    object_theme Scrollbar;
};

typedef struct ui_layout ui_layout;
struct ui_layout {
    i32        N_Rows;
    i32        N_Columns;
    i32        *RowSizes;
    i32        *ColumnSizes;
    i32        CurrentRow;
    i32        CurrentColumn;
    rect_2d    Size;
    vec2       ContentSize;
    vec2       BoxSize;
    ui_lay_opt Option;
    u8         AxisDirection;
    vec2       Padding;
};

typedef struct ui_object ui_object;
struct ui_object {
    u64 HashId;
    object_type Type;
    union {
        struct {
            rect_2d   Rect;
            U8_String Text;
            vec2      Pos;
            vec2      Size;
            i32       TextCursorIdx;
        };
        struct {
            vec2 IconPos;
            vec2 IconSize;
        };
    };

    vec2 ContentSize;

    ui_lay_opt Option;

    object_theme Theme;

    u64 DepthIdx;

    ui_input LastInputSet;
    vec2     LastDelta;
    float    ScrollRatio;

    ui_object* Parent;
    ui_object* Left;
    ui_object* Right;
    ui_object* FirstSon;
    ui_object* Last;
};

typedef struct ui_window ui_window;
struct ui_window {
    rect_2d        WindowRect;
    vec2           ContentSize;
    ui_object      Objects;
};

typedef struct ui_win_stack ui_win_stack;
struct ui_win_stack {
    u32 N;
    u32 Current;
    ui_window* Items[MAX_STACK_SIZE];
};

typedef struct ui_theme_stack ui_theme_stack;
struct ui_theme_stack {
    u32 N;
    u32 Current;
    object_theme Items[MAX_STACK_SIZE];
};

typedef struct ui_layout_stack ui_layout_stack;
struct ui_layout_stack {
    u32 N;
    u32 Current;
    ui_layout Items[MAX_LAYOUT_SIZE];
};

typedef struct ui_context ui_context;
struct ui_context {
    hash_table      TableObject;
    ui_win_stack    Windows;
    ui_layout_stack Layouts;
    ui_object       FreeList;
    ui_theme_stack  Themes;

    ui_object* CurrentParent;
    ui_object* FocusObject;

    ui_theme DefaultTheme;

    ui_input KeyPressed;
    ui_input KeyDown;
	
	u32 MaxDepth;

    vec2 TextCursorPos;
    vec2 CursorClick;
    vec2 CursorPos;
    vec2 LastCursorPos;
    vec2 CursorDelta;
    ui_input CursorAction;

    U8_String TextInput;

    ui_input LastInput;

    Stack_Allocator* Allocator;
    Stack_Allocator* TempAllocator;

    UI_Graphics* Gfx;
};

ui_object UI_NULL_OBJECT = {
    .Parent   = &UI_NULL_OBJECT,
    .Left     = &UI_NULL_OBJECT,
    .Right    = &UI_NULL_OBJECT,
    .FirstSon = &UI_NULL_OBJECT,
    .Last     = &UI_NULL_OBJECT,
    .HashId   = 0,
    .DepthIdx = 0,
    .Rect     = {{0, 0}, {0, 0}}
};

internal void UI_Init(ui_context* Context, UI_Graphics* Gfx, Stack_Allocator* Allocator, Stack_Allocator* TempAllocator);

internal void UI_Begin(ui_context* Context);

internal void UI_End(ui_context* Context);

internal void UI_WindowBegin(ui_context* Context, rect_2d Rect, const char* Title, ui_lay_opt Options);

internal void UI_WindowEnd(ui_context* Context);

internal ui_input UI_LastEvent(ui_context* Context);

internal ui_object* UI_BuildObjectWithParent(
                                             ui_context* Context,
                                             u8* Key,
                                             u8* Text,
                                             rect_2d Rect,
                                             ui_lay_opt Options,
                                             ui_object* Parent
                                             );

#define UI_BuildObject(ctx, key, txt, rect, opts) UI_BuildObjectWithParent(ctx, key, txt, rect, opts, ctx->CurrentParent);

internal ui_input UI_ConsumeEvents(ui_context* Context, ui_object* Object);
// Widgets
internal ui_input UI_Button(ui_context* Context, const char* text);
internal ui_input UI_Label(ui_context* Context, const char* text);
internal ui_input UI_LabelWithKey(ui_context* Context, const char* key, const char* text);
internal ui_input UI_TextBox(ui_context* Context, const char* text);
internal ui_input UI_BeginTreeNode(ui_context* Context, const char* text);
internal ui_input UI_EndTreeNode(ui_context* Context);

internal void UI_SetNextParent(ui_context* Context, ui_object* Object);
internal void UI_PopLastParent(ui_context* Context);
internal void UI_PushNextLayout(ui_context* Context, rect_2d Rect, ui_lay_opt Options);
internal void UI_PushNextLayoutRow(ui_context* Context, int N_Rows, const int* Rows );
internal void UI_PushNextLayoutColumn(ui_context* Context, int N_Columns, const int* Columns );
internal void UI_PushNextLayoutBoxSize(ui_context* Context, vec2 BoxSize);
internal void UI_PushNextLayoutPadding(ui_context* Context, vec2 Padding);
internal void UI_PushNextLayoutOption(ui_context* Context, ui_lay_opt Options);

internal U8_String*  UI_GetTextFromBox(ui_context* Context, const char* Key);

internal void UI_SetNextTheme(ui_context* Context, object_theme Theme);
internal void UI_PushNextFont(ui_context* Context, FontCache* Font);
internal void UI_PopTheme(ui_context* Context);

internal void UI_BeginColumn(ui_context* Context);
internal void UI_EndColumn(ui_context* Context);

internal void UI_BeginScrollbarViewEx(ui_context* Context, vec2 MaxSize);
internal void ui_EndScrollbarView(ui_context* Context);

#define UI_BeginScrollbarView(Context) UI_BeginScrollbarViewEx(Context, (vec2){0, 0});

internal bool IsCursorOnRect(ui_context* Context, rect_2d Rect);

internal void UI_SortWindowByDepth(ui_win_stack* window, Stack_Allocator* Allocator );

#endif // _SP_UI_H_

#ifdef SP_UI_IMPL

internal u64
UI_CustomXXHash( const u8* buffer, u64 len, u64 seed ) {
    return XXH3_64bits_withSeed(buffer, len, seed);
}

internal void
UI_Init(ui_context* Context, UI_Graphics* Gfx, Stack_Allocator* Allocator, Stack_Allocator* TempAllocator) {
    Context->Allocator = Allocator;
    Context->TempAllocator = TempAllocator;
    Context->Gfx = Gfx;

    Context->Themes.N  = MAX_STACK_SIZE;
    Context->Layouts.N = MAX_LAYOUT_SIZE;
    Context->Windows.N = MAX_STACK_SIZE;

    Context->TextInput = StringCreate(1024, Allocator);
    Context->FocusObject = &UI_NULL_OBJECT;
    Context->CurrentParent = &UI_NULL_OBJECT;

	Context->MaxDepth = 1;

    HashTableInit(&Context->TableObject, Allocator, 4096, UI_CustomXXHash);
}

internal void
UI_Begin(ui_context* Context) {
    Context->KeyPressed   = 0;
    Context->CursorClick  = (vec2){0, 0};
    Context->CursorAction = Input_None;
    Context->LastInput    = Input_None;

    Context->TextInput.idx = 0;

    StackClear(&Context->Windows);
    StackClear(&Context->Layouts);
    StackClear(&Context->Themes);
}

internal void
UI_End(ui_context* UI_Context) {
    UI_Graphics* gfx = UI_Context->Gfx;
    F64 window_width  = gfx->base->Swapchain.Extent.width;
    F64 window_height = gfx->base->Swapchain.Extent.height;

    Stack_Allocator* TempAllocator = UI_Context->TempAllocator;
    Stack_Allocator* Allocator     = UI_Context->Allocator;
    // Stack to iterate over the tree object
    //
    typedef struct {
        u32 N;
        u32 Current;
        ui_object** Items;
    } ObjectStack;

    ObjectStack Stack = {
        .N = 24 << 10,
        .Current = 0
    };
    Stack.Items = stack_push(TempAllocator, ui_object*, 24 << 10);

    void* V_Buffer = stack_push(TempAllocator, v_2d, kibibyte(256));
    gfx->ui_rects  = VectorNew(V_Buffer, 0, kibibyte(256), v_2d);
    void* I_Buffer = stack_push(TempAllocator, u32, kibibyte(256));
    gfx->ui_indxs  = VectorNew(I_Buffer, 0, kibibyte(256), u32);


    UI_SortWindowByDepth(&UI_Context->Windows, UI_Context->TempAllocator);

    i32 N_Windows = UI_Context->Windows.Current;
    for( i32 i = 0 ; i < N_Windows; i += 1) {
        ui_window* win = StackGetFront(&UI_Context->Windows);
        StackPop(&UI_Context->Windows);
        Stack.Current = 0;

        // For each window, iterate over the tree of objects generated
        //
        StackPush(&Stack, win->Objects.FirstSon);
        for( ; Stack.Current > 0 ; )
        {
            ui_object* Object = StackGetFront(&Stack);
            StackPop(&Stack);
            for( ui_object* Child = Object->FirstSon; Child != &UI_NULL_OBJECT && Child != NULL; Child = Child->Right ) {
                StackPush(&Stack, Child);
            }
            u32 BaseVertexIdx = gfx->ui_rects.len;
            u32 idx[6] = {
                BaseVertexIdx + 0, BaseVertexIdx + 1, BaseVertexIdx + 3, BaseVertexIdx + 3, BaseVertexIdx + 2, BaseVertexIdx + 0
            };
            vec2 Pos  = Object->Rect.Pos;
            vec2 Size = Object->Rect.Size;

            ui_color PanelBg = Object->Theme.Background;
            ui_color Color   = Object->Theme.Foreground;
            f32 Radius       = Object->Theme.Radius;

            if (Object->Parent->Type == UI_ScrollbarType && Object->Type != UI_ScrollbarTypeButton) {
                ui_object* Thumb = Object->Parent->Last;

                if( Object->Rect.Pos.y < Object->Parent->Rect.Pos.y ) {
                    continue;
                }
                if( Object->Rect.Pos.y > Object->Parent->Rect.Pos.y + Object->Parent->Rect.Size.y ) {
                    continue;
                } else if( Object->Rect.Pos.y + Object->Rect.Size.y > Object->Parent->Rect.Pos.y + Object->Parent->Rect.Size.y ) {
                    continue;
                }
                //Object->Rect.Pos = Vec2Sub(Object->Rect.Pos, Vec2ScalarMul(Thumb->ScrollRatio, Thumb->LastDelta));
                //Object->Pos = Vec2Sub(Object->Pos, Vec2ScalarMul(Thumb->ScrollRatio, Thumb->LastDelta));

            }

            // This do not need to render a rect
            if( Object->Type != UI_LabelType &&
               Object->Type != UI_Icon      &&
               Object->Type != UI_ImageType )
            {
                vec2 BorderWidth = {Object->Theme.BorderThickness, Object->Theme.BorderThickness};

                // Now, create the vertex data with the correct size
                v_2d v1 = {
                    .LeftCorner   = Pos,
                    .Size         = Size, // <-- Pass the correct width and height
                    .UV           = { -2, -2 },
                    .UVSize       = { 0, 0 },
                    .Color        = {(f32)PanelBg.r, (f32)PanelBg.g, (f32)PanelBg.b, (f32)PanelBg.a},
                    .CornerRadius = Radius,
                    .Border       = 0
                };

                if( gfx->ui_rects.len > gfx->ui_rects.capacity - 3 || gfx->ui_indxs.len > gfx->ui_indxs.capacity - 3 ) {
                    stack_free(TempAllocator, I_Buffer);
                    stack_free(TempAllocator, V_Buffer);
                    V_Buffer = stack_push(TempAllocator, v_2d, gfx->ui_rects.capacity * 2);
                    I_Buffer = stack_push(TempAllocator, u32, gfx->ui_indxs.capacity  * 2);
                    VectorResize(&gfx->ui_rects, V_Buffer, gfx->ui_rects.capacity     * 2);
                    VectorResize(&gfx->ui_indxs, I_Buffer, gfx->ui_indxs.capacity     * 2);
                }

                if( Object->Type == UI_Window && Object->Option & UI_DrawShadow ) {
                    vec2 ShadowSize = {10, 10};
                    v_2d v2 = {
                        .LeftCorner   = Vec2Add(Pos, ShadowSize),
                        .Size         = Size, // <-- Pass the correct width and height
                        .UV           = { -2, -2 },
                        .UVSize       = { 0, 0 },
                        .Color        = {(f32)PanelBg.r * 0.2, (f32)PanelBg.g * 0.2, (f32)PanelBg.b * 0.2, (f32)PanelBg.a * 0.5},
                        .CornerRadius = Radius
                    };
                    VectorAppend(&gfx->ui_rects, &v2);
                }

                VectorAppend(&gfx->ui_rects, &v1);

                if( Object->Theme.BorderThickness > 0 ) {
                    v_2d v2 = {
                        .LeftCorner   = Pos,
                        .Size         = Size,
                        .UV           = { -2, -2 },
                        .UVSize       = { 0, 0 },
                        .Color        = {(f32)Object->Theme.Border.r, (f32)Object->Theme.Border.g, (f32)Object->Theme.Border.b, (f32)Object->Theme.Border.a},
                        .CornerRadius = Radius,
                        .Border       = Object->Theme.BorderThickness
                    };
                    VectorAppend(&gfx->ui_rects, &v2);
                }
                for(u32 i = 0 ; i < 6; i += 1 ) {
                    VectorAppend(&gfx->ui_indxs, &idx[i]);
                }
            }

            if( Object->Text.data != NULL && Object->Text.idx > 0 ) {
                f32 txt_offset = 0;
                // Use unsigned char pointer to avoid signedness issues
                u8* p = Object->Text.data;
                float pen_x = (float)Object->Pos.x;
                float pen_y = (float)Object->Pos.y; // baseline or top depending on your coordinate convention
                for ( u32 _it = 0; *p && _it < Object->Text.idx && pen_x; ++p, ++_it) {
                    // Skip UTF-8 continuation bytes (0b10xxxxxx)
                    if ( (*p & 0xC0) == 0x80 ) continue;

                    unsigned char ch = *p;
                    //if (ch < 32 || ch > 126) {
                    //    // handle fallback or skip
                    //    continue;
                    //}
                    int gi = (int)ch - 32;
                    FontCache* UI_Font = Object->Theme.Font;
                    f_Glyph g = UI_Font->glyph[gi];
                    vec2 BitmapOffset = UI_Font->BitmapOffset;


                    // UVs: add half-texel offset to avoid bleeding when sampling with linear filter
                    float _u0 = (g.x + BitmapOffset.x);
                    float _v0 = (g.y + BitmapOffset.y);
                    float _u1 = (g.width);
                    float _v1 = (g.height);

                    float x0 = pen_x + g.x_off;
                    float y0 = pen_y + g.y_off + UI_Font->line_height;

                    vec4 ColorVec = {Color.r, Color.g, Color.b, Color.a};
                    // Create quad vertices in the same winding you use for indices
                    v_2d v1 = {
                        .LeftCorner = { x0, y0 },
                        .Size = {g.width, g.height} ,
                        .UV = { _u0, _v0 },
                        .UVSize = { _u1, _v1 },
                        .Color = ColorVec,
                        .CornerRadius = 0
                    };

                    // Append verts & indices (use your existing capacity logic)
                    u32 base = gfx->ui_rects.len;
                    VectorAppend(&gfx->ui_rects, &v1);

                    u32 idx[6] = { base+0, base+1, base+2, base+2, base+3, base+0 };
                    for (int k=0;k<6;k++) VectorAppend(&gfx->ui_indxs, &idx[k]);

                    // Advance pen by glyph advance (use xadvance from packing)
                    pen_x += g.advance;
                    if( _it < Object->Text.idx - 1 ) {
                       pen_x += F_GetKerningFromCodepoint( UI_Font, (int)gi, (int)*(p + 1) - 32 );
                    }
                }
                if( Object->TextCursorIdx != -1 ) {
                    float Start = Object->Pos.x + F_TextWidth(Object->Theme.Font, Object->Text.data, Object->TextCursorIdx);
                    v_2d v1 = {
                        .LeftCorner = { Start, Object->Pos.y },
                        .Size = {3, Object->Size.y} ,
                        .UV = { -2, -2 },
                        .UVSize = { 0, 0 },
                        .Color = {Color.r, Color.g, Color.b, 1},
                        .CornerRadius = 4,
                        .Border = 0
                    };
                    VectorAppend(&gfx->ui_rects, &v1);
                }
            } else if( Object->Text.idx == 0 && Object->Type == UI_InputText ) {
                float Start = Object->Pos.x + F_TextWidth(Object->Theme.Font, Object->Text.data, Object->TextCursorIdx);
                v_2d v1 = {
                    .LeftCorner = { Start, Object->Rect.Pos.y },
                    .Size = {3, Object->Rect.Size.y} ,
                    .UV = { -2, -2 },
                    .UVSize = { 0, 0 },
                    .Color = {Color.r, Color.g, Color.b, 1},
                    .CornerRadius = 4,
                    .Border = 0
                };
                VectorAppend(&gfx->ui_rects, &v1);
            }
        }
    }
}

internal void
TopDownMerge(ui_window** WorkBuffer, u32 Begin, u32 Middle, u32 End, ui_window** SrcWindows) {
    u32 i = Begin, j = Middle;
    for (u32 k = Begin; k < End; k++) {
        if (i < Middle && (j >= End || SrcWindows[i]->Objects.FirstSon->DepthIdx >= SrcWindows[j]->Objects.FirstSon->DepthIdx)) {
            WorkBuffer[k] = SrcWindows[i];
            i++;
        } else {
            WorkBuffer[k] = SrcWindows[j];
            j++;
        }
    }
}

internal void
TopDownSplitMerge(ui_window** WorkBuffer, u32 Begin, u32 End, ui_window** SrcWindows) {
	if (End - Begin <= 1) {
		return;
	}

	u32 Middle = (End + Begin) / 2;
	TopDownSplitMerge(SrcWindows, Begin, Middle, WorkBuffer);
	TopDownSplitMerge(SrcWindows, Middle, End, WorkBuffer);
	TopDownMerge(SrcWindows, Begin, Middle, End, WorkBuffer);
}

internal void
TopDownMergeSort(ui_window** WorkBuffer, ui_window** SrcWindows, u32 N) {
	
	TopDownSplitMerge(WorkBuffer, 0, N, SrcWindows);
}

internal void
UI_SortWindowByDepth(ui_win_stack* window, Stack_Allocator* Alloc ) {
	
	ui_window** WorkBuffer = stack_push(Alloc, ui_window**, window->Current);
	for (u32 i = 0; i < window->Current; i += 1) {
		WorkBuffer[i] = (window->Items[i]);
	}
	TopDownMergeSort(WorkBuffer, window->Items, window->Current);
}

internal void
UI_WindowBegin(ui_context* Context, rect_2d Rect, const char* Title, ui_lay_opt Options) {
    ui_window* win = stack_push(Context->TempAllocator, ui_window, 1);

    win->WindowRect = Rect;
    win->ContentSize = (vec2){0, 0};

    TreeInit(&win->Objects, &UI_NULL_OBJECT);

    StackPush(&Context->Windows, win);

    TreeInit(&StackGetFront(&Context->Windows)->Objects, &UI_NULL_OBJECT);

    ui_window* NewWin = StackGetFront(&Context->Windows);

    ui_object* Object = &UI_NULL_OBJECT;

    ui_lay_opt WOpt = UI_DrawText;

    if( Options & UI_NoTitle ) {
        WOpt ^= UI_DrawText;
    }

    UI_SetNextTheme( Context, Context->DefaultTheme.Window );
    Object = UI_BuildObjectWithParent(Context, Title, Title, Rect, WOpt | Options, &NewWin->Objects);
    Object->Type = UI_Window;
    // Make it the focus object if clicking on title bar or
    // on the resize box on the bottom-left part of the window
    //
    if( UI_ConsumeEvents(Context, Object) & Input_LeftClickPress ) {
        rect_2d TitleRect = (rect_2d){Object->Rect.Pos, {Object->Rect.Size.x, Object->Size.y}};
        vec2 CornerSize = (vec2){20, 20};
        rect_2d ResizeRect = (rect_2d){
            Vec2Add(Object->Rect.Pos, Vec2Sub(Object->Rect.Size, CornerSize)),
            {20, 20}
        };
        if(IsCursorOnRect(Context, TitleRect) && Context->FocusObject == &UI_NULL_OBJECT && (Options & UI_Select) ) {
			if (Object->DepthIdx < Context->MaxDepth) {
				Object->DepthIdx = Context->MaxDepth + 1;
			}
            Context->FocusObject = Object;
        } else if( Object->Option & UI_Resize  && Context->FocusObject == &UI_NULL_OBJECT  && (Options & UI_Select) )  {
            if( IsCursorOnRect(Context, ResizeRect) ) {
                if (Object->DepthIdx <= Context->MaxDepth) {
					//Object->DepthIdx += 1;
				}
                Context->FocusObject = Object;
            }
        }

		Context->MaxDepth = Max(Context->MaxDepth, Object->DepthIdx);
    }

    if( !( UI_ConsumeEvents(Context, Object) & Input_LeftClickRelease) && Object == Context->FocusObject ) {
        vec2 CornerSize = (vec2){20, 20};
        rect_2d ResizeRect = (rect_2d){
            Vec2Add(Object->Rect.Pos, Vec2Sub(Object->Rect.Size, CornerSize)),
            {20, 20}
        };
        if( Object->Option & UI_Resize )  {
            if( IsCursorOnRect(Context, ResizeRect) ) {
                Object->Rect.Size = Vec2Add(Object->Rect.Size, Context->CursorDelta);
            }
        }
        if( Object->Option & UI_Drag && !IsCursorOnRect(Context, ResizeRect) ) {
            Object->Rect.Pos = Vec2Add(Object->Rect.Pos, Context->CursorDelta);
        }
    } 
	if( UI_ConsumeEvents(Context, Object) & Input_LeftClickRelease ) {
        rect_2d TitleRect = (rect_2d){Object->Rect.Pos, {Object->Rect.Size.x, Object->Size.y}};
        vec2 CornerSize = (vec2){20, 20};
        rect_2d ResizeRect = (rect_2d){
            Vec2Add(Object->Rect.Pos, Vec2Sub(Object->Rect.Size, CornerSize)),
            {20, 20}
        };
		if (IsCursorOnRect(Context, TitleRect)) {
			Context->FocusObject = &UI_NULL_OBJECT;
		} else if (IsCursorOnRect(Context, ResizeRect)) {
			Context->FocusObject = &UI_NULL_OBJECT;
		}
    }

    UI_PushNextLayout(Context, Object->Rect, 0);
    ui_layout* Layout = &StackGetFront(&Context->Layouts);
    Layout->BoxSize = (vec2){Object->Rect.Size.x, F_TextHeight(Object->Theme.Font)};
    Layout->ContentSize.v[Layout->AxisDirection] += Object->Size.v[Layout->AxisDirection];
    Layout->Padding = (vec2){5, 0};
    Context->CurrentParent = Object;

    NewWin->WindowRect = Object->Rect;
}

internal void
UI_WindowEnd(ui_context* Context) {
    if( !StackIsEmpty(&Context->Themes) ) {
        StackPop(&Context->Themes);
    }
    if( !StackIsEmpty(&Context->Layouts) ) {
        StackPop(&Context->Layouts);
    }
}

internal U8_String*
UI_GetTextFromBox(ui_context* Context, const char* Key) {
    ui_object* parent = Context->CurrentParent;
    if( HashTableContains(&Context->TableObject, Key, parent->HashId) ) {
        entry* StoredWindowEntry = HashTableFindPointer(&Context->TableObject, Key, parent->HashId);
        for( entry* it = StoredWindowEntry->Next; it != StoredWindowEntry; it = it->Next ) {
            ui_object* Value = (ui_object*)it->Value;
            if( Value == NULL ) {
            } else {
                if( Value->HashId == UI_CustomXXHash(Key, UCF_Strlen(Key), parent->HashId) ) {
                    return &Value->Text;
                }
            }
        }
    }
    return NULL;
}

internal ui_object*
UI_BuildObjectWithParent(ui_context* Context, u8* Key, u8* Text, rect_2d Rect, ui_lay_opt Options, ui_object* Parent )
{
    ui_object* Object = &UI_NULL_OBJECT;

    ui_object* IdParent = Context->CurrentParent;
    if( HashTableContains(&Context->TableObject, Key, Parent->HashId) ) {
        entry* StoredWindowEntry = HashTableFindPointer(&Context->TableObject, Key, Parent->HashId);
        for( entry* it = StoredWindowEntry->Next; it != StoredWindowEntry; it = it->Next ) {
            ui_object* Value = (ui_object*)it->Value;
            if( Value == NULL ) {
            } else {
                if( Value->HashId == UI_CustomXXHash(Key, UCF_Strlen(Key), Parent->HashId) ) {
                    Object = Value;
                    break;
                }
            }
        }

    } else {
        Object = stack_push(Context->Allocator, ui_object, 1);
        memset(Object, 0, sizeof(ui_object));
        entry* val = HashTableAdd(&Context->TableObject, Key, Object, Parent->HashId);
        Object->HashId = val->HashId;
        Object->TextCursorIdx = -1;
        Object->Rect   = Rect;
        Object->Option = Options;
    }
    // As these are dragable, we do not want the to have the fixed rect size
    //
    if( !(Options & UI_SetPosPersistent) ) {
        Object->Rect   = Rect;
        Object->Option = Options;
    }

    if( !StackIsEmpty(&Context->Themes) ) {
        Object->Theme = StackGetFront(&Context->Themes);
    } else {
        Object->Theme = Context->DefaultTheme.Label;
    }

    if( !StackIsEmpty(&Context->Layouts) ) {
        ui_layout* Layout    = &StackGetFront(&Context->Layouts);

        Object->Rect.Pos = Vec2Add(Object->Rect.Pos, Layout->Padding);
        vec2 RightPadding = Vec2ScalarMul(2, Layout->Padding);
        if( Layout->N_Columns == 0 && Layout->N_Rows == 0 ) {
        } else if( Layout->N_Columns > 0 && Layout->AxisDirection == 0 && Layout->CurrentColumn < Layout->N_Columns) {
            Object->Rect.Size.x = Layout->ColumnSizes[Layout->CurrentColumn];
            Layout->CurrentColumn += 1;
        } else if( Layout->N_Rows > 0 && Layout->AxisDirection == 1 && Layout->CurrentRow < Layout->N_Rows) {
            Object->Rect.Size.y = Layout->RowSizes[Layout->CurrentRow];
            Layout->CurrentRow += 1;
        }

        Object->Rect.Size = Vec2Sub(Object->Rect.Size, RightPadding);
        Layout->ContentSize.v[Layout->AxisDirection] += Object->Rect.Size.v[Layout->AxisDirection];
        
        Parent->ContentSize.v[Layout->AxisDirection] += Object->Rect.Size.v[Layout->AxisDirection];
    }

    if( Options & UI_DrawText ) {
        u32 Len = UCF_Strlen(Text);
        if( Object->Text.data == NULL ) {
            Object->Text = StringNew(Text, Len, Context->Allocator);
        } else if( Object->Type != UI_InputText ) {
            // If it is Input text we do not want to copy again the title text, as
            // it already stores input information from the user
            StringCpy(&Object->Text, Text);
        }
        if( Object->Type == UI_InputText && Object == Context->FocusObject ) {
            Object->TextCursorIdx = Max(0, Object->TextCursorIdx);
            if( Context->TextInput.len > Object->Text.len ) {
                Object->Text = StringCreate(Context->TextInput.len, Context->Allocator);
            }
            if( Context->TextInput.idx > 0 && !(Context->LastInput & Input_Backspace) && !(Context->LastInput & DeleteWord) ) {
                //StringAppndStr(&Object->Text, &Context->TextInput);
                //Object->Text.data[Object->Text.idx] = '\0';
                StringInsertStr(&Object->Text, Object->TextCursorIdx, &Context->TextInput);
                Object->TextCursorIdx += Context->TextInput.idx;
            }
            if( Context->LastInput & DeleteWord ) {
                i32 LastSlash      = GetLastOcurrence(&Object->Text, '/'); 
                i32 LastUnderScore = GetLastOcurrence(&Object->Text, '_');
                i32 LastSpace      = GetLastOcurrence(&Object->Text, ' ');
                // This is done so that it does not remove the actual character
                // hello/asdf --> hello/
                // instead of
                // hello/asdf --> hello
                //
                if( LastSlash == -1 ) {
                    LastSlash += 1;
                }
                if( LastUnderScore == -1 ) {
                    LastUnderScore += 1;
                }
                if( LastSpace == -1 ) {
                    LastSpace += 1;
                }

                if( LastSlash > LastUnderScore ) {
                    if( LastSlash < LastSpace && LastSpace > 0 ) {
                        Object->TextCursorIdx = LastSpace;
                        StringEraseUntil(&Object->Text, LastSpace);
                        StringEraseUntil(&Context->TextInput, LastSpace);
                    } else if( LastSlash > 0) {
                        Object->TextCursorIdx = LastSlash;
                        StringEraseUntil(&Object->Text, LastSlash);
                        StringEraseUntil(&Context->TextInput, LastSlash);
                    }
                } else if( LastUnderScore > 0 ) {
                    if( LastUnderScore < LastSpace ) {
                        Object->TextCursorIdx = LastSpace;
                        StringEraseUntil(&Object->Text, LastSpace);
                        StringEraseUntil(&Context->TextInput, LastSpace);
                    } else {
                        Object->TextCursorIdx = LastUnderScore;
                        StringEraseUntil(&Object->Text, LastUnderScore);
                        StringEraseUntil(&Context->TextInput, LastUnderScore);
                    }
                } else if( LastSpace > 0 ) {
                    Object->TextCursorIdx = LastSpace;
                    StringEraseUntil(&Object->Text, LastSpace);
                    StringEraseUntil(&Context->TextInput, LastSpace);
                } else {
                    StringEraseUntil(&Object->Text, 0);
                    StringEraseUntil(&Context->TextInput, 0);
                }
                
            } else if( Context->LastInput & Input_Backspace ) {
                //Object->Text.idx = Object->Text.idx > 0 ? Object->Text.idx - 1 : 0;
                StringErase(&Object->Text, Object->TextCursorIdx);
            }
        }

        Object->Pos  = Object->Rect.Pos;
        Object->Size = (vec2){
            (f32)F_TextWidth( Object->Theme.Font, Object->Text.data, Object->Text.idx ),
            (f32)F_TextHeight(Object->Theme.Font)
        };

        if( Options & UI_AlignVertical ) {
            Object->Pos.y += Object->Rect.Size.y / 2 - Object->Size.y / 2;
        }
        if( Options & UI_AlignCenter ) {
            Object->Pos.x = (Object->Pos.x + (Object->Rect.Size.x / 2.0)) - (Object->Size.x / 2.0);
        }
        if( Options & UI_AlignBottom ) {
            Object->Pos = Rect.Pos;
            Object->Pos.y += Object->Rect.Size.y - 2*Object->Size.y;
        }
        if( Options & UI_AlignRight ) {
            Object->Pos.x += (Object->Rect.Size.x - Object->Size.x);
        }
    }

    // Handle the scroll offset given in the last frame
    //
    if( Parent->Type == UI_ScrollbarType && Object->Type != UI_ScrollbarTypeButton ) {
        Object->Rect.Pos.y -= Parent->ScrollRatio * Parent->LastDelta.y;
        Object->Pos.y      -= Parent->ScrollRatio * Parent->LastDelta.y;
    }

    TreeInit(Object, &UI_NULL_OBJECT);
    TreePushSon(Parent, Object, &UI_NULL_OBJECT);

    Object->ContentSize = Vec2Zero();
    
    return Object;
}

internal bool
IsCursorOnRect(ui_context* Context, rect_2d Rect) {
    vec2 Cursor = Context->CursorPos;
    if( Cursor.x >= Rect.Pos.x && Cursor.x <= Rect.Pos.x + Rect.Size.x ) {
        if( Cursor.y >= Rect.Pos.y && Cursor.y <= Rect.Pos.y + Rect.Size.y ) {
            return true;
        }
    }
    return false;
}

internal bool
UI_CheckBoxInsideScrollView(ui_context* Context, ui_object* Object) {
    if(Object->Parent != &UI_NULL_OBJECT && Object->Parent->Type == UI_ScrollbarType) {
        if( Object->Type == UI_ScrollbarTypeButton ) {
            return true;
        }
        rect_2d pr = Object->Parent->Rect;
        rect_2d cr = Object->Rect;

        if( cr.Pos.y >= pr.Pos.y ) {
            if( cr.Pos.y + cr.Size.y < pr.Pos.y + pr.Size.y ) {
                return true;
            }
        }

        return false;
    }

    return true;
}

internal ui_input
UI_ConsumeEvents(ui_context* Context, ui_object* Object) {
    if( (Object->Option & UI_Select) && IsCursorOnRect(Context, Object->Rect) ) {
        if( UI_CheckBoxInsideScrollView(Context, Object)) {
            return Context->CursorAction | Input_CursorHover;
        }
    }

    return Input_None;
}

internal ui_input
UI_Button(ui_context* Context, const char* Title) {

    ui_window* Win = &StackGetFront(&Context->Windows);

    ui_object* Parent = Context->CurrentParent;

    ui_layout Layout = StackGetFront(&Context->Layouts);
    rect_2d Rect     = Layout.Size;
    Rect.Pos         = Vec2Add(Rect.Pos, Layout.ContentSize);
    Rect.Size        = Layout.BoxSize;

    ui_lay_opt Options = UI_DrawText | UI_AlignVertical | UI_AlignCenter | UI_Interact | UI_Select;

    UI_SetNextTheme( Context, Context->DefaultTheme.Button );
    ui_object* Button = UI_BuildObjectWithParent(Context, Title, Title, Rect, Options, Parent);

    ui_input Input = UI_ConsumeEvents(Context, Button);
    Button->LastInputSet = Input;
    Button->Type = UI_ButtonType;

    if( Input & Input_LeftClickPress ) {
        Context->FocusObject = Button;
    } else if ( Input & Input_LeftClickRelease ) {
        Context->FocusObject = &UI_NULL_OBJECT;
    }

    if( Button->LastInputSet & Input_CursorHover ) {
        Button->Theme = Context->DefaultTheme.Panel;
    }

    return Input;
}

internal ui_input
UI_Label(ui_context* Context, const char* text) {
    ui_window* Win = &StackGetFront(&Context->Windows);

    ui_object* Parent = Context->CurrentParent;

    ui_layout Layout = StackGetFront(&Context->Layouts);
    rect_2d Rect     = Layout.Size;
    Rect.Pos         = Vec2Add(Rect.Pos, Layout.ContentSize);
    Rect.Size        = Layout.BoxSize;

    ui_lay_opt Options = UI_DrawText | UI_AlignVertical | UI_Interact | Layout.Option;
    UI_SetNextTheme( Context, Context->DefaultTheme.Label );
    ui_object* Label = UI_BuildObjectWithParent(Context, text, text, Rect, Options, Parent);

    ui_input Input = UI_ConsumeEvents(Context, Label);
    Label->LastInputSet = Input;
    Label->Type = UI_LabelType;

    return Input;
}

internal ui_input
UI_LabelWithKey(ui_context* Context, const char* Key, const char* text) {
    ui_window* Win = &StackGetFront(&Context->Windows);

    ui_object* Parent = Context->CurrentParent;

    ui_layout Layout = StackGetFront(&Context->Layouts);
    rect_2d Rect     = Layout.Size;
    Rect.Pos         = Vec2Add(Rect.Pos, Layout.ContentSize);
    Rect.Size        = Layout.BoxSize;

    ui_lay_opt Options = UI_DrawText | UI_AlignVertical | UI_Interact;
    UI_SetNextTheme( Context, Context->DefaultTheme.Input );
    ui_object* Label = UI_BuildObjectWithParent(Context, Key, text, Rect, Options, Parent);

    ui_input Input = UI_ConsumeEvents(Context, Label);
    Label->LastInputSet = Input;
    Label->Type = UI_LabelType;

    return Input;
}

internal ui_input
UI_TextBox(ui_context* Context, const char* text) {
    ui_window* Win = &StackGetFront(&Context->Windows);

    ui_object* Parent = Context->CurrentParent;

    ui_layout Layout = StackGetFront(&Context->Layouts);
    rect_2d Rect     = Layout.Size;
    Rect.Pos         = Vec2Add(Rect.Pos, Layout.ContentSize);
    Rect.Size        = Layout.BoxSize;

    ui_lay_opt Options = UI_DrawText | UI_AlignVertical | UI_Interact | UI_Select;
    UI_SetNextTheme( Context, Context->DefaultTheme.Input );
    ui_object* TextBox = UI_BuildObjectWithParent(Context, text, text, Rect, Options, Parent);

    ui_input Input = UI_ConsumeEvents(Context, TextBox);
    TextBox->LastInputSet = Input;
    TextBox->Type = UI_InputText;

	if( Input & Input_LeftClickPress ) {
		Context->FocusObject = TextBox;
	}

	if( TextBox == Context->FocusObject ) {
		TextBox->LastInputSet |= Context->LastInput;
	}


    if( TextBox->LastInputSet & Input_CursorHover ) {
        TextBox->Theme.Background.a *= 0.8;
    }

    return TextBox->LastInputSet;
}


internal ui_input
UI_BeginTreeNode(ui_context* Context, const char* text) {
    ui_window* Win = &StackGetFront(&Context->Windows);

    ui_object* Parent = Context->CurrentParent;

    ui_layout Layout = StackGetFront(&Context->Layouts);
    rect_2d Rect     = Layout.Size;
    Rect.Pos         = Vec2Add(Rect.Pos, Layout.ContentSize);
    Rect.Size        = Layout.BoxSize;

    ui_lay_opt Options = UI_DrawText | UI_AlignVertical | UI_AlignCenter | UI_Interact | UI_Select;
    UI_SetNextTheme( Context, Context->DefaultTheme.Panel );
    ui_object* TreeNode = UI_BuildObjectWithParent(Context, text, text, Rect, Options, Parent);

    ui_input Input = UI_ConsumeEvents(Context, TreeNode);
    ui_input IsActive = TreeNode->LastInputSet & ActiveObject;
    TreeNode->LastInputSet = Input | IsActive;
    TreeNode->Type = UI_Tree;

    if( Input & Input_LeftClickPress && Context->FocusObject != TreeNode && !(TreeNode->LastInputSet & ActiveObject) ) {
        Context->FocusObject = TreeNode;
        TreeNode->LastInputSet |= ActiveObject;
    } else if( Context->FocusObject == TreeNode ) {
        if (Input & Input_LeftClickPress) {
            TreeNode->LastInputSet ^= ActiveObject;
        }
    } else if( Input & Input_LeftClickPress && TreeNode->LastInputSet & ActiveObject ) {
        TreeNode->LastInputSet ^= ActiveObject;
	} else if (Input & Input_LeftClickRelease) {
		Context->FocusObject = &UI_NULL_OBJECT;
	}

    Context->CurrentParent = TreeNode;

    if( TreeNode->LastInputSet & Input_CursorHover ) {
        TreeNode->Theme.Background.a *= 0.8;
    }

    return TreeNode->LastInputSet;
}

internal ui_input
UI_EndTreeNode(ui_context* Context) {
    Context->CurrentParent = Context->CurrentParent->Parent;
}

internal void
UI_BeginScrollbarViewEx(ui_context* Context, vec2 Size) {

    char name[] = "ScrollBarView";

    ui_layout *Layout = &StackGetFront(&Context->Layouts);

    ui_object* Parent = Context->CurrentParent;

    rect_2d Rect = Layout->Size;

    vec2 ContentSize = Layout->ContentSize;

    vec2 Padding = Layout->Padding;
    Layout->Padding = (vec2){0, 0};

    Rect.Pos.x = Rect.Pos.x + Rect.Size.x - 15;
    Rect.Size.x = 15;
    Rect.Pos.y += ContentSize.y;
    if( Size.x == 0 || Size.y == 0 ) {
        Rect.Size.y = Rect.Size.y - ContentSize.y;
    } else {
        Rect.Size.y = Size.y;
    }

    UI_SetNextTheme( Context, Context->DefaultTheme.Panel );
    ui_object* ScrollObj = UI_BuildObjectWithParent(Context, name, NULL, Rect, 0, Parent);

    ScrollObj->Type = UI_ScrollbarType;

    Layout->ContentSize = ContentSize;

    Context->CurrentParent = ScrollObj;

    Layout->Padding = Padding;
}

internal void
UI_EndScrollbarView(ui_context* Context) {
    ui_object* Scrollbar   = Context->CurrentParent;

    float ContentHeight = Scrollbar->ContentSize.y;

    float ViewportContent = Scrollbar->Rect.Size.y;

    rect_2d ScrollableSize = Scrollbar->Rect;

    ContentHeight = Max(1.0f, ContentHeight);

    ScrollableSize.Size.y = ContentHeight / ViewportContent;

    f32 ScrollRatio = 1;
    ScrollRatio = ContentHeight / ViewportContent;

    ScrollableSize.Size.y = Max(ScrollableSize.Size.y, 20.0f);

    float ThumbMovementRange = ViewportContent - ScrollableSize.Size.y;
    float ScrollRange        = ContentHeight - ViewportContent;
    float ScrollT = ThumbMovementRange / ScrollRange; // 0..1

    char buf[] = "EndOfScroll";

    assert(!StackIsEmpty(&Context->Layouts));

    ui_layout* Layout = &StackGetFront(&Context->Layouts);

    vec2 Padding = Layout->Padding;
    Layout->Padding = (vec2){0, 0};

    UI_SetNextTheme( Context, Context->DefaultTheme.Button );
    ui_object* Scrollable = UI_BuildObjectWithParent(Context, buf, NULL, ScrollableSize, UI_Select | UI_Drag, Scrollbar);
    Scrollable->Type = UI_ScrollbarTypeButton;
    Scrollable->Theme = Context->DefaultTheme.Scrollbar;
    Layout->Padding = Padding;

    Layout->ContentSize.y -= Max(0, ContentHeight - ViewportContent + Scrollable->Rect.Size.y);

    vec2 VerticalDelta = Vec2Zero();
    if( Context->FocusObject == Scrollable ) {
        VerticalDelta = (vec2){0, Context->CursorDelta.y};
    }

    Scrollable->LastDelta = Vec2Add(Scrollable->LastDelta, VerticalDelta);
    Scrollable->ScrollRatio = ScrollRatio;
    Scrollable->Rect.Pos.y += Scrollable->LastDelta.y * ScrollRatio;
    Scrollable->Rect.Pos.y = Max(Scrollbar->Rect.Pos.y, Scrollable->Rect.Pos.y);
    Scrollable->Rect.Pos.y = Min(Scrollable->Rect.Pos.y, Scrollbar->Rect.Pos.y + ViewportContent);

    if(Scrollable->Rect.Pos.y == Scrollbar->Rect.Pos.y) {
        Scrollable->LastDelta = Vec2Zero();
    } else if( Scrollable->Rect.Pos.y == Scrollbar->Rect.Pos.y + Scrollbar->Rect.Size.y ) {
        Scrollable->LastDelta = Vec2Sub(Scrollable->LastDelta, VerticalDelta);
    }

    // Set the scrollbar begin base LastDelta to the offset created by the 
    // scrollbar button, this is done to offset the childs when drawed again 
    // on the next frame
    //
    Scrollbar->LastDelta.y = Scrollable->ScrollRatio * Scrollable->LastDelta.y;
    Scrollbar->ScrollRatio = Scrollable->ScrollRatio;
    //for (ui_object* Child = Scrollbar->FirstSon; Child != &UI_NULL_OBJECT; Child = Child->Right) {
    //    Child->Rect.Pos.y -= Scrollable->ScrollRatio * Scrollable->LastDelta.y;
    //    Child->Pos.y      -= Scrollable->ScrollRatio * Scrollable->LastDelta.y;
    //}

    if( UI_ConsumeEvents(Context, Scrollable) & Input_LeftClickPress ) {
        Context->FocusObject = Scrollable;
    }

    if( UI_ConsumeEvents(Context, Scrollable) & Input_LeftClickRelease && Context->FocusObject == Scrollable ) {
        Context->FocusObject = &UI_NULL_OBJECT;
    } else if( Context->FocusObject == Scrollable && Context->CursorAction & Input_LeftClickRelease ) {
        Context->FocusObject = &UI_NULL_OBJECT;
    }
    Context->CurrentParent = Context->CurrentParent->Parent;
}

internal void
UI_SetNextParent(ui_context* Context, ui_object* Object) {
    Context->CurrentParent = Object;
}

internal void
UI_PopLastParent(ui_context* Context) {
    Context->CurrentParent = Context->CurrentParent->Parent;
}

internal void
UI_PushNextLayout(ui_context* Context, rect_2d Rect, ui_lay_opt Options) {
    StackPush(&Context->Layouts, (ui_layout){});
    ui_layout* Layout = &StackGetFront(&Context->Layouts);

    Layout->Size          = Rect;
    Layout->ContentSize   = Vec2Zero();
    Layout->Option        = Options;
    Layout->AxisDirection = 1; // y axis (goes vertically)
    Layout->BoxSize       = Vec2Zero();
}


internal void
UI_PushNextLayoutRow(ui_context* Context, int N_Rows, const int* Rows ) {

    assert(!StackIsEmpty(&Context->Layouts));

    ui_layout* Layout = &StackGetFront(&Context->Layouts);

    Layout->N_Rows   = N_Rows;
    Layout->RowSizes = Rows;
}

internal void
UI_PushNextLayoutColumn(ui_context* Context, int N_Columns, const int* Columns ) {
    assert(!StackIsEmpty(&Context->Layouts));

    ui_layout* Layout = &StackGetFront(&Context->Layouts);

    Layout->N_Columns   = N_Columns;
    Layout->ColumnSizes = Columns;
}

internal void
UI_PushNextLayoutPadding(ui_context* Context, vec2 Padding) {
    assert(!StackIsEmpty(&Context->Layouts));

    ui_layout* Layout = &StackGetFront(&Context->Layouts);
    Layout->Padding    = Padding;
}

internal void
UI_PushNextLayoutOption(ui_context* Context, ui_lay_opt Options) {
    assert(!StackIsEmpty(&Context->Layouts));

    ui_layout* Layout = &StackGetFront(&Context->Layouts);
    Layout->Option    = Options;
}

internal void
UI_PushNextLayoutDisableOption(ui_context* Context, ui_lay_opt Options) {
    assert(!StackIsEmpty(&Context->Layouts));

    ui_layout* Layout = &StackGetFront(&Context->Layouts);
    Layout->Option    ^= Options;
}


internal void
UI_SetNextTheme(ui_context* Context, object_theme Theme) {
    StackPush(&Context->Themes, Theme);
}

internal void
UI_PushNextFont(ui_context* Context, FontCache* Font) {
    object_theme* Theme = &StackGetFront(&Context->Themes);
    Theme->Font = Font;
}

internal void
UI_PopTheme(ui_context* Context) {
    StackPop(&Context->Themes);
}

internal void
UI_BeginColumn(ui_context* Context) {
    assert(!StackIsEmpty(&Context->Layouts));

    ui_layout* Layout = &StackGetFront(&Context->Layouts);
    Layout->AxisDirection = 0;
    Layout->CurrentColumn = 0;
}

internal void
UI_EndColumn(ui_context* Context) {
    assert(!StackIsEmpty(&Context->Layouts));
    ui_layout* Layout = &StackGetFront(&Context->Layouts);
    Layout->AxisDirection = 1;
    Layout->ContentSize.x = 0;
    Layout->ContentSize.y += Layout->BoxSize.y;
}


internal ui_input
UI_LastEvent(ui_context* Context) {
    ui_input Input = Input_None;
    UI_Graphics* gfx = Context->Gfx;
    u32 window_width  = gfx->base->Swapchain.Extent.width;
    u32 window_height = gfx->base->Swapchain.Extent.height;
    Input = GetNextEvent(&gfx->base->Window);

    {
        vec2 Mouse = GetMousePosition(&gfx->base->Window);
        vec2 LastCursor = Context->CursorPos;
        Context->CursorPos = Mouse;

        while( Context->LastCursorPos.x == Mouse.x && Context->LastCursorPos.y == Mouse.y && (Input & Input_None) ) {
            Mouse = GetMousePosition(&gfx->base->Window);
            Input = GetNextEvent(&gfx->base->Window);
        }

        Context->LastCursorPos = LastCursor;
        Context->CursorDelta = Vec2Sub(Context->CursorPos, LastCursor);
    }

    if( Input & FrameBufferResized ) {
        gfx->base->FramebufferResized = true;
    }
    if( Input & ClipboardPaste ) {
        StringAppendStr(&Context->TextInput, &gfx->base->Window.ClipboardContent);
    }
    if( Input & Input_MiddleMouseUp ) {
        Context->CursorDelta.y += 6;
    }
    if( Input & Input_MiddleMouseDown ) {
        Context->CursorDelta.y -= 6;
    }
    if( Input & Input_LeftClickPress ) {
        Context->CursorClick   = Context->LastCursorPos;
        Context->CursorAction |= Input_LeftClickPress;
    }
    if( Input & Input_LeftClickRelease ) {
        Context->CursorClick   = Context->LastCursorPos;
        Context->CursorAction |= Input_LeftClickRelease;
	}
    if( Input & Input_Left ) {
        Context->FocusObject->TextCursorIdx = Max(0, Context->FocusObject->TextCursorIdx - 1);
    }
    if( Input & Input_Right ) {
        Context->FocusObject->TextCursorIdx = Min(Context->FocusObject->Text.idx, Context->FocusObject->TextCursorIdx + 1);
    }
    if( Input & DeleteWord ) {

    } else if( Input & Input_Backspace ) {
        Context->FocusObject->TextCursorIdx = Max(0, Context->FocusObject->TextCursorIdx - 1 );
        Context->TextInput.idx = Context->TextInput.idx > 0 ? Context->TextInput.idx - 1 : 0;
    }
    if( Input & ClipboardPaste ) {
        StringAppend(&Context->TextInput, GetClipboard(&gfx->base->Window));
    }
    if( Input & Input_KeyChar ) {
        Context->KeyPressed |= gfx->base->Window.KeyPressed;
        Context->KeyDown    |= gfx->base->Window.KeyPressed;
        char buffer[2] = {0};
        buffer[0] = gfx->base->Window.KeyPressed;
        StringAppend(&Context->TextInput, buffer);
    }

    if( Input & Input_F1 ) {
        gfx->UI_EnableDebug = !gfx->UI_EnableDebug;
    }

    Context->LastInput = Input;
    return Input;
}

#endif
