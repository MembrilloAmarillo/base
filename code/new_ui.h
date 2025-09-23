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
 * @todo Implement Window API for window creation and Event handling
 * @todo Abstract X11 handling to better support OS-specific API
 */

/**
 * 2025/09/23. Added Clipboard pasting.
 * 2025/09/23. Fixed Input declaration taking garbage values creating buggy responses
 * 2025/09/22. Better handling for input text when submiting on return
 * 2025/09/22. Input Text fixed.
 * 2025/09/22. Drag and resize added on Windows. Improvements still to be made.
 * 2025/09/19. First somewhat working version. Added tree node implementation
 */

#ifndef _SP_UI_H_
#define _SP_UI_H_

#include "types.h"
#include "strings.h"
#include "vector.h"
#include "allocator.h"
#include "HashTable.h"

#include "vk_render.h"
#include "load_font.h"

#include "ui_render.h"


#define HexToRGBA(val) ((rgba){.r = (val & 0xff000000) >> 24, .g = (val & 0x00ff0000) >> 16, .b = (val & 0x0000ff00) >> 8, .a = val & 0x000000ff})
#define HexToU8_Vec4(val) {(val & 0xff000000) >> 24, (val & 0x00ff0000) >> 16, (val & 0x0000ff00) >> 8, val & 0x000000ff}

#define RgbaToNorm(val) (ui_color){(f32)val.r / 255.f, (f32)val.g / 255.f, (f32)val.b / 255.f, (f32)val.a / 255.f}

#define RgbaNew(r, g, b, a) (rgba){r, g, b, a}

#define MAX_STACK_SIZE  64
#define MAX_LAYOUT_SIZE 256

typedef enum ui_input ui_input;
enum ui_input {
    NoInput           = (1 << 0),
    RightClickPress   = (1 << 1),
    RightClickRelease = (1 << 2),
    LeftClickPress    = (1 << 3),
    LeftClickRelease  = (1 << 4),
    DoubleLeftClick   = (1 << 5),
    CursorHover       = (1 << 6),
    Backspace         = (1 << 7),
    Ctrl              = (1 << 8),
    Shift             = (1 << 9),
    Alt               = (1 << 10),
    Return            = (1 << 11),
    MiddleMouse       = (1 << 12),
    MiddleMouseUp     = (1 << 13),
    MiddleMouseDown   = (1 << 14),
    F1                = (1 << 15),
    F2                = (1 << 16),
    F3                = (1 << 17),
    F4                = (1 << 18),
    F5                = (1 << 19),
    Left              = (1 << 20),
    Right             = (1 << 21),
    Up                = (1 << 22),
    Down              = (1 << 23),
    ESC               = (1 << 24),
    StopUI            = (1 << 25),
    ActiveObject      = (1 << 26)
};

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
    UI_NoTitle     = (1 << 9)
};

typedef struct rect_2d rect_2d;
struct rect_2d {
    vec2 Pos;
    vec2 Size;
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
    UI_Panel
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
    ui_color Border;
    ui_color PanelBackground;
    ui_color WindowBackground;
    ui_color WindowForeground;
    ui_color LabelForeground;
    ui_color ButtonForeground;
    ui_color ButtonBackground;
    ui_color ButtonHoverBackground;
    ui_color ButtonPressBackground;
    ui_color TextInputForeground;
    ui_color TextInputBackground;
    ui_color TextInputCursor;
    f32      WindowRadius;
    f32      ButtonRadius;
    f32      InputTextRadius;
    FontCache* Font;
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
    ui_lay_opt Option;
    object_theme* Theme;

    ui_input LastInputSet;

    ui_object* Parent;
    ui_object* Left;
    ui_object* Right;
    ui_object* FirstSon;
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
    ui_window Items[MAX_STACK_SIZE];
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

    object_theme DefaultTheme;

    ui_input KeyPressed;
    ui_input KeyDown;

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
    .FirstSon = &UI_NULL_OBJECT
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

#define UI_BuildObject(ctx, key, txt, rect, opts) UI_BuildObjectWithParent(ctx, key, txt, rect, opts, NULL);

internal ui_input UI_ConsumeEvents(ui_context* Context, ui_object* Object);
// Widgets
internal ui_input UI_Button(ui_context* Context, const char* text);
internal ui_input UI_Label(ui_context* Context, const char* text);
internal ui_input UI_LabelWithKey(ui_context* Context, const char* key, const char* text);
internal ui_input UI_TextBox(ui_context* Context, const char* text);
internal ui_input UI_BeginTreeNode(ui_context* Context, const char* text);
internal ui_input UI_EndTreeNode(ui_context* Context);

internal void UI_SetNextParent(ui_context* Context, ui_object* Object);
internal void UI_PushNextLayout(ui_context* Context, rect_2d Rect, ui_lay_opt Options);
internal void UI_PushNextLayoutRow(ui_context* Context, int N_Rows, const int* Rows );
internal void UI_PushNextLayoutColumn(ui_context* Context, int N_Columns, const int* Columns );
internal void UI_PushNextLayoutBoxSize(ui_context* Context, vec2 BoxSize);

internal U8_String*  UI_GetTextFromBox(ui_context* Context, const char* Key);

internal void UI_SetNextTheme(ui_context* Context, object_theme Theme);
internal void UI_PushNextFont(ui_context* Context, FontCache* Font);
internal void UI_PopTheme(ui_context* Context);

internal void UI_BeginColumn(ui_context* Context);
internal void UI_EndColumn(ui_context* Context);

internal void UI_BeginScrollbarView(ui_context* Context);
internal void ui_EndScrollbarView(ui_context* Context);

internal bool IsCursorOnRect(ui_context* Context, rect_2d Rect);

#endif // _SP_UI_H_

#ifdef SP_UI_IMPL

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

    HashTableInit(&Context->TableObject, stack_push(Allocator, entry, 256), NULL);
}

internal void
UI_Begin(ui_context* Context) {
    Context->KeyPressed   = 0;
    Context->CursorClick  = (vec2){0, 0};
    Context->CursorAction = NoInput;
    Context->LastInput    = NoInput;

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

    void* V_Buffer = stack_push(TempAllocator, v_2d, kibibyte(256));
    gfx->ui_rects  = VectorNew(V_Buffer, 0, kibibyte(256), v_2d);
    void* I_Buffer = stack_push(TempAllocator, u32, kibibyte(256));
    gfx->ui_indxs  = VectorNew(I_Buffer, 0, kibibyte(256), u32);

    for( ui_window* win = &StackGetFront(&UI_Context->Windows); !StackIsEmpty(&UI_Context->Windows); win = &StackGetFront(&UI_Context->Windows) ) {
        StackPop(&UI_Context->Windows);
        for(
            ui_object* Object = win->Objects.FirstSon;
            Object != &UI_NULL_OBJECT && Object != NULL;
            Object = Object->Right
            )
        {
            u32 BaseVertexIdx = gfx->ui_rects.len;
            u32 idx[6] = {
                BaseVertexIdx + 0, BaseVertexIdx + 1, BaseVertexIdx + 3, BaseVertexIdx + 3, BaseVertexIdx + 2, BaseVertexIdx + 0
            };
            vec2 Pos  = Object->Rect.Pos;
            vec2 Size = Object->Rect.Size;

            ui_color PanelBg = Object->Theme->WindowBackground;
            ui_color Color   = Object->Theme->WindowForeground;
            f32 Radius       = Object->Theme->WindowRadius;
            if(Object->Type == UI_ButtonType) {
                PanelBg = Object->Theme->ButtonBackground;
                Color   = Object->Theme->ButtonForeground;
                Radius  = Object->Theme->ButtonRadius;
            }
            if( Object->LastInputSet & CursorHover ) {
                PanelBg = Object->Theme->ButtonHoverBackground;
            } else if( Object->LastInputSet & LeftClickPress ) {
                PanelBg = Object->Theme->ButtonPressBackground;
            }

            // This do not need to render a rect
            if( Object->Type != UI_LabelType &&
               Object->Type != UI_Text      &&
               Object->Type != UI_Icon      &&
               Object->Type != UI_ImageType )
            {

                // Now, create the vertex data with the correct size
                v_2d v1 = {
                    .LeftCorner   = Pos,
                    .Size         = Size, // <-- Pass the correct width and height
                    .UV           = { -2, -2 },
                    .UVSize       = { 0, 0 },
                    .Color        = {(f32)PanelBg.r, (f32)PanelBg.g, (f32)PanelBg.b, (f32)PanelBg.a},
                    .CornerRadius = Radius
                };

                if( gfx->ui_rects.len > gfx->ui_rects.capacity - 3 || gfx->ui_indxs.len > gfx->ui_indxs.capacity - 3 ) {
                    stack_free(TempAllocator, I_Buffer);
                    stack_free(TempAllocator, V_Buffer);
                    V_Buffer = stack_push(TempAllocator, v_2d, gfx->ui_rects.capacity * 2);
                    I_Buffer = stack_push(TempAllocator, u32, gfx->ui_indxs.capacity * 2);
                    VectorResize(&gfx->ui_rects, V_Buffer, gfx->ui_rects.capacity * 2);
                    VectorResize(&gfx->ui_indxs, I_Buffer, gfx->ui_indxs.capacity * 2);
                }


                if( Object->Type == UI_Window ) {
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
                for ( u32 _it = 0; *p && _it < Object->Text.idx; ++p, ++_it) {
                    // Skip UTF-8 continuation bytes (0b10xxxxxx)
                    if ( (*p & 0xC0) == 0x80 ) continue;

                    unsigned char ch = *p;
                    //if (ch < 32 || ch > 126) {
                    //    // handle fallback or skip
                    //    continue;
                    //}
                    int gi = (int)ch - 32;
                    FontCache* UI_Font = Object->Theme->Font;
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
                }
                if( Object->TextCursorIdx != -1 ) {
                    float Start = Object->Pos.x + F_TextWidth(Object->Theme->Font, Object->Text.data, Object->TextCursorIdx);
                    v_2d v1 = {
                        .LeftCorner = { Start, Object->Pos.y },
                        .Size = {4, Object->Size.y} ,
                        .UV = { -2, -2 },
                        .UVSize = { 0, 0 },
                        .Color = {Color.r, Color.g, Color.b, 1},
                        .CornerRadius = 6
                    };
                    VectorAppend(&gfx->ui_rects, &v1);
                }
            }
        }
    }
}

internal void
UI_WindowBegin(ui_context* Context, rect_2d Rect, const char* Title, ui_lay_opt Options) {
    ui_window win = {
        .WindowRect = Rect,
        .ContentSize = (vec2){0, 0}
    };
    TreeInit(&win.Objects, &UI_NULL_OBJECT);

    ui_object* Object = &UI_NULL_OBJECT;

    if( HashTableContains(&Context->TableObject, Title) ) {
        entry* StoredWindowEntry = HashTableFindPointer(&Context->TableObject, Title);
        Object = (ui_object*)StoredWindowEntry->Value;
    } else {
        Object = stack_push(Context->Allocator, ui_object, 1);
        HashTableAdd(&Context->TableObject, Title, Object);
        Object->Rect = win.WindowRect;
        Object->TextCursorIdx = -1;
    }

    // Make it the focus object if clicking on title bar or
    // on the resize box on the bottom-left part of the window
    //
    if( UI_ConsumeEvents(Context, Object) & LeftClickPress ) {
        rect_2d TitleRect = (rect_2d){Object->Rect.Pos, {Object->Rect.Size.x, Object->Size.y}};
        vec2 CornerSize = (vec2){20, 20};
        rect_2d ResizeRect = (rect_2d){
            Vec2Add(Object->Rect.Pos, Vec2Sub(Object->Rect.Size, CornerSize)),
            {20, 20}
        };
        if(IsCursorOnRect(Context, TitleRect)) {
            Context->FocusObject = Object;
        } else if( Object->Option & UI_Resize )  {
            if( IsCursorOnRect(Context, ResizeRect) ) {
                Context->FocusObject = Object;
            }
        }
    }

    if( !( UI_ConsumeEvents(Context, Object) & LeftClickRelease) && Object == Context->FocusObject ) {
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
    } else if( UI_ConsumeEvents(Context, Object) & LeftClickRelease && Object == Context->FocusObject ) {
        Context->FocusObject = &UI_NULL_OBJECT;
    }

    win.WindowRect = Object->Rect;
    if( !StackIsEmpty(&Context->Themes) ) {
        Object->Theme = &StackGetFront(&Context->Themes);
    } else {
        Object->Theme = &Context->DefaultTheme;
    }

    if( !(Options & UI_NoTitle) ) {
        u32 TitleLen = UCF_Strlen(Title);
        if( Object->Text.data == NULL ) {
            Object->Text = StringNew(Title, TitleLen, Context->Allocator);
        }
        Object->Pos  = win.WindowRect.Pos;
        Object->Size = (vec2){(f32)F_TextWidth( Object->Theme->Font, Title, TitleLen ), (f32)F_TextHeight(Object->Theme->Font)};
        Object->Pos.x = (Object->Pos.x + (Object->Rect.Size.x / 2.0)) - (Object->Size.x / 2.0);
    }
    Object->Type = UI_Window;
    Object->Option = Options;
    TreeClear(Object, &UI_NULL_OBJECT);
    StackPush(&Context->Windows, win);

    TreeInit(&StackGetFront(&Context->Windows).Objects, &UI_NULL_OBJECT);
    TreePushSon(&StackGetFront(&Context->Windows).Objects, Object, &UI_NULL_OBJECT);

    UI_PushNextLayout(Context, Object->Rect, Options);
    ui_layout* Layout = &StackGetFront(&Context->Layouts);
    Layout->BoxSize = (vec2){Object->Rect.Size.x, Object->Size.y};
    Layout->ContentSize.v[Layout->AxisDirection] += Object->Size.v[Layout->AxisDirection];

    Context->CurrentParent = &StackGetFront(&Context->Windows).Objects;
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
    if( HashTableContains(&Context->TableObject, Key) ) {
        entry* StoredWindowEntry = HashTableFindPointer(&Context->TableObject, Key);
        ui_object* Object = (ui_object*)StoredWindowEntry->Value;
        return &Object->Text;
    }
    return NULL;
}

internal ui_object*
UI_BuildObjectWithParent(ui_context* Context, u8* Key, u8* Text, rect_2d Rect, ui_lay_opt Options, ui_object* Parent )
{
    ui_object* Object = &UI_NULL_OBJECT;
    // @TODO For now it does not compute the hash with the parent
    //
    if( HashTableContains(&Context->TableObject, Key) ) {
        entry* StoredWindowEntry = HashTableFindPointer(&Context->TableObject, Key);
        Object = (ui_object*)StoredWindowEntry->Value;
    } else {
        Object = stack_push(Context->Allocator, ui_object, 1);
        HashTableAdd(&Context->TableObject, Key, Object);
        Object->TextCursorIdx = -1;
    }

    Object->Rect   = Rect;
    Object->Option = Options;

    if( !StackIsEmpty(&Context->Themes) ) {
        Object->Theme = &StackGetFront(&Context->Themes);
    } else {
        Object->Theme = &Context->DefaultTheme;
    }

    if( !StackIsEmpty(&Context->Layouts) ) {
        ui_layout* Layout    = &StackGetFront(&Context->Layouts);
        if( Layout->N_Rows > 0 ) {
            Object->Rect.Pos.y += Layout->ColumnSizes[Layout->CurrentColumn];
        }
        if( Layout->N_Columns > 0 ) {
            Object->Rect.Pos.x += Layout->RowSizes[Layout->CurrentRow];
        }

        Layout->ContentSize.v[Layout->AxisDirection] += Object->Rect.Size.v[Layout->AxisDirection];
    }

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
        if( Context->TextInput.idx > 0 && !(Context->LastInput & Backspace) ) {
            //StringAppndStr(&Object->Text, &Context->TextInput);
            //Object->Text.data[Object->Text.idx] = '\0';
            StringInsertStr(&Object->Text, Object->TextCursorIdx, &Context->TextInput);
            Object->TextCursorIdx += Context->TextInput.idx;
        }
        if( Context->LastInput & Backspace ) {
            //Object->Text.idx = Object->Text.idx > 0 ? Object->Text.idx - 1 : 0;
            StringErase(&Object->Text, Object->TextCursorIdx);
        }
    }

    Object->Pos  = Rect.Pos;
    Object->Size = (vec2){
        (f32)F_TextWidth( Object->Theme->Font, Object->Text.data, Object->Text.idx ),
        (f32)F_TextHeight(Object->Theme->Font)
    };

    Object->Pos.y += Rect.Size.y / 2 - Object->Size.y / 2;

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

    TreeInit(Object, &UI_NULL_OBJECT);
    TreePushSon(Parent, Object, &UI_NULL_OBJECT);

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

internal ui_input
UI_ConsumeEvents(ui_context* Context, ui_object* Object) {
    if( (Object->Option & UI_Select) && IsCursorOnRect(Context, Object->Rect) ) {
        return Context->CursorAction | CursorHover;
    }

    return NoInput;
}

internal ui_input
UI_Button(ui_context* Context, const char* Title) {

    ui_window* Win = &StackGetFront(&Context->Windows);

    ui_object* Parent = Context->CurrentParent;

    ui_layout Layout = StackGetFront(&Context->Layouts);
    rect_2d Rect     = Layout.Size;
    Rect.Pos         = Vec2Add(Rect.Pos, Layout.ContentSize);
    Rect.Size.v[Layout.AxisDirection] = Layout.BoxSize.v[Layout.AxisDirection];

    ui_lay_opt Options = UI_AlignCenter | UI_Interact | UI_Select | Layout.Option;
    ui_object* Button = UI_BuildObjectWithParent(Context, Title, Title, Rect, Options, Parent);

    ui_input Input = UI_ConsumeEvents(Context, Button);
    Button->LastInputSet = Input;
    Button->Type = UI_ButtonType;

    return Input;
}

internal ui_input
UI_Label(ui_context* Context, const char* text) {
    ui_window* Win = &StackGetFront(&Context->Windows);

    ui_object* Parent = Context->CurrentParent;

    ui_layout Layout = StackGetFront(&Context->Layouts);
    rect_2d Rect     = Layout.Size;
    Rect.Pos         = Vec2Add(Rect.Pos, Layout.ContentSize);
    Rect.Size.v[Layout.AxisDirection] = Layout.BoxSize.v[Layout.AxisDirection];

    ui_lay_opt Options = UI_Interact | UI_Select | Layout.Option;
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
    Rect.Size.v[Layout.AxisDirection] = Layout.BoxSize.v[Layout.AxisDirection];

    ui_lay_opt Options = UI_Interact | UI_Select | Layout.Option;
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
    Rect.Size.v[Layout.AxisDirection] = Layout.BoxSize.v[Layout.AxisDirection];

    ui_lay_opt Options = UI_AlignCenter | UI_Interact | UI_Select | Layout.Option;
    ui_object* TextBox = UI_BuildObjectWithParent(Context, text, text, Rect, Options, Parent);

    ui_input Input = UI_ConsumeEvents(Context, TextBox);
    TextBox->LastInputSet = Input;
    TextBox->Type = UI_InputText;

    if( Input & LeftClickPress ) {
        Context->FocusObject = TextBox;
    }

    if( TextBox == Context->FocusObject ) {
        TextBox->LastInputSet |= Context->LastInput;
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
    Rect.Size.v[Layout.AxisDirection] = Layout.BoxSize.v[Layout.AxisDirection];

    ui_lay_opt Options = UI_AlignCenter | UI_Interact | UI_Select | Layout.Option;
    ui_object* TreeNode = UI_BuildObjectWithParent(Context, text, text, Rect, Options, Parent);

    ui_input Input = UI_ConsumeEvents(Context, TreeNode);
    ui_input IsActive = TreeNode->LastInputSet & ActiveObject;
    TreeNode->LastInputSet = Input | IsActive;
    TreeNode->Type = UI_Tree;

    if( Input & LeftClickPress && Context->FocusObject != TreeNode) {
        Context->FocusObject = TreeNode;
        TreeNode->LastInputSet |= ActiveObject;
    } else if( Context->FocusObject == TreeNode ) {
        if (Input & LeftClickPress) {
            TreeNode->LastInputSet ^= ActiveObject;
        }
    }

    return TreeNode->LastInputSet;
}

internal ui_input
UI_EndTreeNode(ui_context* Context) {
    Context->CurrentParent = Context->CurrentParent->Parent;
}

internal vec2
UI_GetMousePosition(UI_Graphics* gfx) {
    Window window_returned;
    int root_x, root_y;
    int win_x, win_y;
    u32 mask_return;
    XQueryPointer(
                  gfx->base->Window.Dpy,
                  gfx->base->Window.Win,
                  &window_returned,
                  &window_returned,
                  &root_x, &root_y,
                  &win_x, &win_y,
                  &mask_return
                  );

    vec2 MousePosition = {.x = (f32)win_x, .y = (f32)win_y};

    return MousePosition;
}

internal void UI_SetNextParent(ui_context* Context, ui_object* Object) {
    Context->CurrentParent = Object;
}

internal void UI_PushNextLayout(ui_context* Context, rect_2d Rect, ui_lay_opt Options) {
    StackPush(&Context->Layouts, (ui_layout){});
    ui_layout* Layout = &StackGetFront(&Context->Layouts);

    Layout->Size          = Rect;
    Layout->ContentSize   = Vec2Zero();
    Layout->Option        = Options;
    Layout->AxisDirection = 1; // y axis (goes vertically)
    Layout->BoxSize       = Vec2Zero();
}


internal void UI_PushNextLayoutRow(ui_context* Context, int N_Rows, const int* Rows ) {

    assert(!StackIsEmpty(&Context->Layouts));

    ui_layout* Layout = &StackGetFront(&Context->Layouts);

    Layout->N_Rows   = N_Rows;
    Layout->RowSizes = Rows;
}

internal void UI_PushNextLayoutColumn(ui_context* Context, int N_Columns, const int* Columns ) {
    assert(!StackIsEmpty(&Context->Layouts));

    ui_layout* Layout = &StackGetFront(&Context->Layouts);

    Layout->N_Columns   = N_Columns;
    Layout->ColumnSizes = Columns;
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


internal ui_input
UI_LastEvent(ui_context* Context) {
    ui_input Input = NoInput;
    UI_Graphics* gfx = Context->Gfx;
    XEvent ev = {0};
    u32 window_width  = gfx->base->Swapchain.Extent.width;
    u32 window_height = gfx->base->Swapchain.Extent.height;
    {
        vec2 Mouse = UI_GetMousePosition(gfx);
        vec2 LastCursor = Context->CursorPos;
        Context->CursorPos = Mouse;

        while( Context->LastCursorPos.x == Mouse.x && Context->LastCursorPos.y == Mouse.y && !XPending(gfx->base->Window.Dpy)) {
            Mouse = UI_GetMousePosition(gfx);
        }

        Context->LastCursorPos = LastCursor;
        Context->CursorDelta = Vec2Sub(Context->CursorPos, LastCursor);
    }

    while (XPending(gfx->base->Window.Dpy)) {
        XNextEvent(gfx->base->Window.Dpy, &ev);

        if( ev.type == FocusOut ) {
            while( XNextEvent( gfx->base->Window.Dpy, &ev) && ev.type != FocusIn ) {
                if( ev.type == FocusIn ) {
                    break;
                }
            }
        }

        typedef struct {
            KeySym x_key;
            ui_input input;
        } KeyMap;

        int mouse_button_pressed = 0;
        static KeyMap keys_to_check[] = {
            { XK_BackSpace, Backspace},
            { XK_Return,    Return},
            { XK_Shift_L,   Shift},
            { XK_Shift_R,   Shift},
            { XK_Control_L, Ctrl},
            { XK_Control_R, Ctrl},
            { XK_Meta_L,    Alt },
            { XK_Meta_R,    Alt },
            { XK_F1,        F1 },
            { XK_F2,        F2 },
            { XK_F3,        F3 },
            { XK_F4,        F4 },
            { XK_F5,        F5 },
            { XK_Left,      Left},
            { XK_Right,     Right},
            { XK_Up,        Up},
            { XK_Down,      Down},
            { XK_Escape,    ESC},
        };

        switch (ev.type) {
            // --- Window Events ---
            case ConfigureNotify: {
                XConfigureEvent* ce = (XConfigureEvent*)&ev;
                if (ce->width != window_width || ce->height != window_height) {
                    gfx->base->FramebufferResized = true;
                }
                break;
            }

            // --- Clipboard ---
            case SelectionNotify: {
                if (ev.xselection.property == None) {
                    printf("Failed to get clipboard data. The owner did not respond or format is not supported.\n");
                } else {
                    unsigned char* data = NULL;
                    Atom actual_type;
                    int actual_format;
                    unsigned long nitems, bytes_after;

                    XGetWindowProperty(gfx->base->Window.Dpy,
                                       ev.xselection.requestor, // Our window
                                       ev.xselection.property,  // The property we asked for
                                       0, 1024, False, AnyPropertyType,
                                       &actual_type, &actual_format, &nitems, &bytes_after, &data);

                    if (data && nitems > 0) {
                        printf("Pasted content: %s\n", data);
                        StringAppend(&Context->TextInput, data);
                        XFree(data);
                    } else {
                        printf("Clipboard is empty or data could not be read.\n");
                    }
                    // Clean up the property
                    XDeleteProperty(gfx->base->Window.Dpy, ev.xselection.requestor, ev.xselection.property);
                }
            }break;

            // --- Mouse Events ---
            case MotionNotify: {
                XMotionEvent* me = (XMotionEvent*)&ev;
                // Optional: handle mouse movement
                break;
            }

            case ButtonPress: {
                XButtonEvent* be = (XButtonEvent*)&ev;
                Context->CursorClick  = (vec2){ev.xbutton.x, ev.xbutton.y};
                Context->CursorAction |= LeftClickPress;
                Context->LastInput = LeftClickPress;
                return LeftClickPress;
                break;
            }

            case ButtonRelease: {
                XButtonEvent* be = (XButtonEvent*)&ev;
                Context->CursorClick  = (vec2){ev.xbutton.x, ev.xbutton.y};
                Context->CursorAction |= LeftClickRelease;
                Context->LastInput = LeftClickPress;
                return LeftClickRelease;
                break;
            }

            case KeyPress: {
                char buffer[256] = {0};
                KeySym keysym_from_utf8 = NoSymbol;
                int num_bytes = Xutf8LookupString(gfx->base->Window.xic, &ev.xkey, buffer, sizeof(buffer), &keysym_from_utf8, NULL);

                int key = 0;
                for (size_t i = 0; i < ArrayCount(keys_to_check); ++i) {
                    if (keysym_from_utf8 == keys_to_check[i].x_key) {
                        key = keys_to_check[i].input;
                        Input |= key;
                    }
                }
                if (key) {
                    Context->KeyPressed |= key;
                    Context->KeyDown    |= key;
                }

                if (keysym_from_utf8 == XK_Escape) {
                    Input = StopUI;
                    Context->LastInput = Input;
                    return Input; // This will now correctly exit the loop
                }

                if ( (ev.xkey.state & ControlMask) && (keysym_from_utf8 == XK_v || keysym_from_utf8 == XK_V)) {
                    printf("Ctrl+V detected! Requesting clipboard content...\n");
                    XConvertSelection(
                                      gfx->base->Window.Dpy,
                                      gfx->base->Window.Clipboard.Clipboard,
                                      gfx->base->Window.Clipboard.Utf8,
                                      gfx->base->Window.Clipboard.Paste,
                                      gfx->base->Window.Win,
                                      CurrentTime
                                      );
                    XFlush(gfx->base->Window.Dpy); // Ensure the request is sent immediately
                    break;
                }

                if (num_bytes > 0 && !key) {
                    //buffer[num_bytes] = '\0';
                    StringAppend(&Context->TextInput, buffer);
                }

                switch( keysym_from_utf8 ) {
                    case XK_F1: {
                        gfx->UI_EnableDebug = !gfx->UI_EnableDebug;
                        Input |= F1;
                    }break;
                    case XK_F2: {
                        Input |= F2;
                    }break;
                    case XK_F3: {
                        Input |= F3;
                    }break;
                    case XK_F4: {
                        Input |= F4;
                    }break;
                    case XK_F5: {
                        Input |= F5;
                    }break;
                    case XK_Left: {
                        Input |= Left;
                        Context->FocusObject->TextCursorIdx = Max(0, Context->FocusObject->TextCursorIdx - 1);
                    }break;
                    case XK_Right: {
                        Input |= Right;
                        Context->FocusObject->TextCursorIdx = Min(Context->FocusObject->Text.idx, Context->FocusObject->TextCursorIdx + 1);
                    }break;
                    case XK_Up: {
                        Input |= Up;
                    }break;
                    case XK_Down: {
                        Input |= Down;
                    }break;
                    case XK_Return: {
                        Input |= Return;
                    } break;
                    case XK_Control_L:
                    case XK_Control_R: {
                        Input |= Ctrl;
                    } break;
                    case XK_Shift_L:
                    case XK_Shift_R: {
                        Input |= Shift;
                    } break;
                    case XK_Meta_L:
                    case XK_Meta_R: {
                        Input |= Alt;
                    } break;
                    case XK_BackSpace: {
                        Input |= Backspace;
                        Context->FocusObject->TextCursorIdx = Max(0, Context->FocusObject->TextCursorIdx - 1 );
                        Context->TextInput.idx = Context->TextInput.idx > 0 ? Context->TextInput.idx - 1 : 0;
                    } break;
                    default: {}break;
                }

                break;
            }

            case KeyRelease: {
                KeySym keysym = XkbKeycodeToKeysym(gfx->base->Window.Dpy, ev.xkey.keycode, 0, 0);
                for (size_t i = 0; i < sizeof(keys_to_check)/sizeof(keys_to_check[0]); ++i) {
                    if (keysym == keys_to_check[i].x_key) {
                        Context->KeyDown &= ~keysym;
                        break;
                    }
                }
                break;
            }

            // --- Quit on window close ---
            case ClientMessage: {
                //if ((Atom)ev.xclient.data.l[0] == wm_delete_window) {
                //    //running = false;
                //}
                break;
            }

            default:
            break;
        }
    }

    Context->LastInput = Input;
    return Input;
}

#endif
