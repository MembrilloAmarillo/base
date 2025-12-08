<<<<<<< HEAD
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

// --- Spall Profiler Compatibility ---
// These are used for profiling but not available in this build
// Define empty stubs to avoid compilation errors
#ifndef SPALL_PROFILING_ENABLED
#define spall_ctx nullptr
#define spall_buffer nullptr
#define get_time_in_nanos() 0
#define spall_buffer_begin(ctx, buf, name, namelen, timestamp) ((void)0)
#define spall_buffer_end(ctx, buf, timestamp) ((void)0)
#endif

#define HexToRGBA(val) rgba{(uint8_t)((val & 0xff000000) >> 24), (uint8_t)((val & 0x00ff0000) >> 16), (uint8_t)((val & 0x0000ff00) >> 8), (uint8_t)(val & 0x000000ff)}
#define HexToU8_Vec4(val) {(val & 0xff000000) >> 24, (val & 0x00ff0000) >> 16, (val & 0x0000ff00) >> 8, val & 0x000000ff}

#define RgbaToNorm(val) ui_color{(f32)val.r / 255.f, (f32)val.g / 255.f, (f32)val.b / 255.f, (f32)val.a / 255.f}
#define Vec4ToRGBA(val) rgba{val.r, val.g, val.b, val.a}
#define RgbaNew(r, g, b, a) rgba{r, g, b, a}

#define MAX_STACK_SIZE  64
#define MAX_LAYOUT_SIZE 256

typedef enum ui_lay_opt {
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
    UI_DrawShadow       = (1 << 13),
	UI_DrawRect         = (1 << 14),
	UI_DrawBorder       = (1 << 15),
	UI_FitRectToText    = (1 << 16),
	UI_DrawIcon         = (1 << 17),
	UI_HintSize         = (1 << 18)
};

typedef enum object_type {
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

typedef enum icon_type {
    Icon_None,
	Icon_File,
	Icon_Folder,
	Icon_Download,
	Icon_Send,
	Icon_ArrowDown,
	Icon_Wave,
	Icon_Details
};

typedef vec4 ui_color;

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

	ui_color OnDefaultBackground;
	ui_color OnDefaultForeground;
	ui_color OnDefaultBorder;
	ui_color OnHoverBackground;
	ui_color OnHoverForeground;
	ui_color OnHoverBorder;
	ui_color OnSuccessBackground; // Very faint green tint
	ui_color OnSuccessForeground; // Vibrant Terminal Green
	ui_color OnSuccessBorder;
	ui_color OnErrorBackground; // Faint red tint
	ui_color OnErrorForeground; // High-Vis Red
	ui_color OnErrorBorder;
	ui_color OnWarning;
	ui_color BorderPrimary;
	ui_color BorderMedium;
	ui_color BorderHover;
};

typedef struct ui_layout ui_layout;
struct ui_layout {
    i32        N_Rows;
    i32        N_Columns;
    const f32  *RowSizes;
    const f32  *ColumnSizes;
    i32        CurrentRow;
    i32        CurrentColumn;
    rect_2d    Size;
    vec2       ContentSize;
    vec2       BoxSize;
    ui_lay_opt Option;
    u8         AxisDirection;
    vec2       Padding;
	vec2       Spacer;
	icon_type  NextIconType;
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
			i32       TextStartIdx;
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

ui_object UI_NULL_OBJECT;

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

	ui_object RootObject;
    ui_object* CurrentParent;
    ui_object* FocusObject;

    ui_theme DefaultTheme;

	bool IsOnDrag;
	bool IsOnResize;
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

	vec4 IconsUvCoords[Icon_Details + 1];
};

fn_internal void UI_Init(ui_context* Context, Stack_Allocator* Allocator, Stack_Allocator* TempAllocator);

fn_internal void UI_Begin(ui_context* Context);

fn_internal void UI_End( ui_context* Context, draw_bucket_instance* DrawInstance );

fn_internal void UI_WindowBegin(ui_context* Context, rect_2d Rect, const char* Title, ui_lay_opt Options);

fn_internal void UI_WindowEnd(ui_context* Context);

fn_internal ui_input UI_LastEvent(ui_context* Context, api_window* Window);

fn_internal void UI_DrawRect2D(ui_context* Context, rect_2d Rect, vec4 color, vec4 color_border, f32 border, f32 radius);
fn_internal void UI_DrawText2D(ui_context* Context, rect_2d Rect, U8_String* String, FontCache* Font, vec4 color);

fn_internal ui_object* UI_BuildObjectWithParent(
	ui_context* Context,
	const u8* Key,
	const u8* Text,
	rect_2d Rect,
	ui_lay_opt Options,
	ui_object* Parent
);

#define UI_BuildObject(ctx, key, txt, rect, opts) UI_BuildObjectWithParent(ctx, key, txt, rect, opts, ctx->CurrentParent);

fn_internal ui_input UI_ConsumeEvents(ui_context* Context, ui_object* Object);
// Widgets
fn_internal ui_input UI_Button(ui_context* Context, const char* text);
fn_internal ui_input UI_Label(ui_context* Context, const char* text);
fn_internal ui_input UI_LabelWithKey(ui_context* Context, const char* key, const char* text);
fn_internal ui_input UI_TextBox(ui_context* Context, const char* text);
fn_internal ui_input UI_BeginTreeNode(ui_context* Context, const char* text);
fn_internal void     UI_EndTreeNode(ui_context* Context);

fn_internal void UI_PushNextLayoutIcon(ui_context* Context, icon_type Type);
fn_internal void UI_PopNextIcon(ui_context* Context);

fn_internal ui_input UI_SetIcon(ui_context* Context, const char* Name, icon_type Type);

fn_internal void UI_SetNextParent(ui_context* Context, ui_object* Object);
fn_internal void UI_PopLastParent(ui_context* Context);
fn_internal void UI_PushNextLayout(ui_context* Context, rect_2d Rect, ui_lay_opt Options);
fn_internal void UI_PushNextLayoutRow(ui_context* Context, int N_Rows, const f32* Rows );
fn_internal void UI_PushNextLayoutColumn(ui_context* Context, int N_Columns, const f32* Columns );
fn_internal void UI_PushNextLayoutBoxSize(ui_context* Context, vec2 BoxSize);
fn_internal void UI_PushNextLayoutPadding(ui_context* Context, vec2 Padding);
fn_internal void UI_PushNextLayoutOption(ui_context* Context, ui_lay_opt Options);
fn_internal void UI_PushLayoutSpacer(ui_context* Context, vec2 Spacer);

fn_internal void UI_Spacer(ui_context* Context, vec2 Spacer);

fn_internal U8_String*  UI_GetTextFromBox(ui_context* Context, const char* Key);

fn_internal vec2 UI_UpdateObjectSize(ui_context* Context, ui_object* Object);

fn_internal void UI_SetNextTheme(ui_context* Context, object_theme Theme);
fn_internal void UI_PushNextFont(ui_context* Context, FontCache* Font);
fn_internal void UI_PopTheme(ui_context* Context);

fn_internal void UI_BeginColumn(ui_context* Context);
fn_internal void UI_SetNextCol(ui_context* Context);
fn_internal void UI_EndColumn(ui_context* Context);

fn_internal void UI_Column(ui_context* Context);
fn_internal void UI_Row(ui_context* Context);
fn_internal void UI_NextRow(ui_context* Context);

fn_internal void UI_BeginRow(ui_context* Context);
fn_internal void UI_EndRow(ui_context* Context);

fn_internal void UI_Divisor(ui_context* Context, f32 Width);

fn_internal void UI_BeginScrollbarViewEx(ui_context* Context, vec2 MaxSize);
fn_internal void UI_EndScrollbarView(ui_context* Context);

#define UI_BeginScrollbarView(Context) UI_BeginScrollbarViewEx(Context, (vec2){0, 0})

fn_internal bool IsCursorOnRect(ui_context* Context, rect_2d Rect);

fn_internal void UI_SortWindowByDepth(ui_win_stack* window, Stack_Allocator* Allocator );

#endif // _SP_UI_H_

#ifdef SP_UI_IMPL

fn_internal u64
	UI_CustomXXHash( const u8* buffer, u64 len, u64 seed ) {
    return XXH3_64bits_withSeed(buffer, len, seed);
}

fn_internal void
	UI_Init(ui_context* Context, Stack_Allocator* Allocator, Stack_Allocator* TempAllocator) {
    spall_buffer_begin(&spall_ctx, &spall_buffer, 
					   __FUNCTION__,             // name of your function
					   sizeof(__FUNCTION__) - 1, // name len minus the null terminator
					   get_time_in_nanos()      // timestamp in nanoseconds -- start of your timing block
					   );
	Context->Allocator = Allocator;
    Context->TempAllocator = TempAllocator;

	UI_NULL_OBJECT.HashId   = 0;
	UI_NULL_OBJECT.DepthIdx = 0;
	UI_NULL_OBJECT.Parent   = &UI_NULL_OBJECT;
	UI_NULL_OBJECT.Left     = &UI_NULL_OBJECT;
	UI_NULL_OBJECT.Right    = &UI_NULL_OBJECT;
	UI_NULL_OBJECT.Last     = &UI_NULL_OBJECT;
	UI_NULL_OBJECT.FirstSon = &UI_NULL_OBJECT;

    Context->Themes.N  = MAX_STACK_SIZE;
    Context->Layouts.N = MAX_LAYOUT_SIZE;
    Context->Windows.N = MAX_STACK_SIZE;

    Context->TextInput = StringCreate(2048, Allocator);
    Context->FocusObject = &UI_NULL_OBJECT;
    Context->CurrentParent = &UI_NULL_OBJECT;

	Context->MaxDepth = 1;
	Context->IsOnDrag = false;
	Context->IsOnResize = false;

    HashTableInit(&Context->TableObject, Allocator, 4096, UI_CustomXXHash);
	
	spall_buffer_end(&spall_ctx, &spall_buffer, get_time_in_nanos() // timestamp in nanoseconds -- end of your timing block
					 );
}

fn_internal void
	UI_Begin(ui_context* Context) {
    Context->KeyPressed   = 0;
    Context->CursorClick = vec2{0, 0};
    Context->CursorAction = Input_None;
    Context->LastInput    = Input_None;

    Context->TextInput.idx = 0;

	TreeInit(&Context->RootObject, &UI_NULL_OBJECT);

    StackClear(&Context->Windows);
    StackClear(&Context->Layouts);
    StackClear(&Context->Themes);
}

fn_internal void UI_End(ui_context* UI_Context, draw_bucket_instance* DrawInstance) {
	spall_buffer_begin(&spall_ctx, &spall_buffer, 
					   __FUNCTION__,             // name of your function
					   sizeof(__FUNCTION__) - 1, // name len minus the null terminator
					   get_time_in_nanos()      // timestamp in nanoseconds -- start of your timing block
					   );
    Stack_Allocator* TempAllocator = UI_Context->TempAllocator;
    Stack_Allocator* Allocator     = UI_Context->Allocator;
    // Stack to iterate over the tree object
    //
    typedef struct {
        u32 N;
        u32 Current;
        ui_object** Items;
    } ObjectStack;

    ObjectStack Stack = {};
    Stack.N = 8 << 20;
    Stack.Current = 0;
    Stack.Items = stack_push(TempAllocator, ui_object*, 8 << 20);

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
            for( ui_object* Child = Object->Last; Child != &UI_NULL_OBJECT && Child != NULL; Child = Child->Left ) {
                StackPush(&Stack, Child);
            }

            vec2 Pos  = Object->Rect.Pos;
            vec2 Size = Object->Rect.Size;

            ui_color PanelBg = Object->Theme.Background;
            ui_color Color   = Object->Theme.Foreground;
            f32 Radius       = Object->Theme.Radius;

			if (Object->Type == UI_ScrollbarTypeButton) {
				PanelBg = Object->Theme.Foreground;
			}

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
            if( Object->Option & UI_DrawRect )
            {
				vec4 BgColor = {PanelBg.r, PanelBg.g, PanelBg.b, PanelBg.a};
				D_DrawRect2D(
					DrawInstance,
					Object->Rect,
					Radius,
					0,
					BgColor
				);

                if( Object->Type == UI_Window && Object->Option & UI_DrawShadow ) {
                    vec2 ShadowSize = {10, 10};
					D_DrawRect2D(
						DrawInstance,
						(rect_2d){Vec2Add(Pos, ShadowSize), Size},
						Radius,
						0,
						Vec4New(
							static_cast<f32>(PanelBg.r * 0.2), 
							static_cast<f32>(PanelBg.g * 0.2), 
							static_cast<f32>(PanelBg.b * 0.2), 
							static_cast<f32>(PanelBg.a * 0.5)
						)
					);
                }

                if( Object->Theme.BorderThickness > 0 && Object->Option & UI_DrawBorder ) {
					vec4 BdColor = {Object->Theme.Border.r, Object->Theme.Border.g, Object->Theme.Border.b, Object->Theme.Border.a};
					D_DrawRect2D(
						DrawInstance,
						Object->Rect,
						Radius,
						Object->Theme.BorderThickness,
						BdColor
					);
                }
            }

            if( Object->Text.data != NULL && Object->Text.idx > 0 && Object->Option & UI_DrawText) {

				U8_String S = StringNew(
					(const char*)(Object->Text.data + Object->TextStartIdx),
					Object->Text.idx - Object->TextStartIdx,
					UI_Context->TempAllocator
				);

				D_DrawText2D(
					DrawInstance,
					NewRect2D(Object->Pos.x, Object->Pos.y, Object->Size.x, Object->Size.y),
					&S,
					Object->Theme.Font,
					Vec4New(Color.r, Color.g, Color.b, Color.a)
				);

                if( Object->TextCursorIdx >= 0 && Object->TextStartIdx >= 0 && Object->Type == UI_InputText) {
                    float Start = Object->Pos.x + F_TextWidth(Object->Theme.Font, (const char*)Object->Text.data, Object->TextCursorIdx - Object->TextStartIdx);
					float CursorSize = F_TextWidth(Object->Theme.Font, "H", 1);
					D_DrawRect2D(
						DrawInstance,
						(rect_2d) { {Start, Object->Pos.y + 2}, {CursorSize, Object->Size.y - 4} },
						4,
						0,
						Vec4New(Color.r, Color.g, Color.b, Color.a * 0.5)
					);
                }
            } else if( Object->Text.idx == 0 && Object->Type == UI_InputText && Object->Option & UI_DrawText ) {
                float Start = Object->Pos.x + F_TextWidth(Object->Theme.Font, (const char*)Object->Text.data, Object->TextCursorIdx);
				float CursorSize = F_TextWidth(Object->Theme.Font, "H", 1);
				D_DrawRect2D(
					DrawInstance,
					(rect_2d) { {Start, Object->Pos.y + 2}, {CursorSize, Object->Size.y - 4} },
					4,
					0,
					Vec4New(Color.r, Color.g, Color.b, Color.a * 0.5)
				);
            }

			if (Object->Option & UI_DrawIcon) {
			     if( Object->Type == Icon_None ) {
			         continue;
			     }

				vec4 uv = UI_Context->IconsUvCoords[Object->Type - 1];
				f32 IconHeight = Object->Rect.Size.y;
				//printf("%f %f\n", Object->Rect.Size.x, Object->Rect.Size.y);
				D_DrawIcon(
					DrawInstance,
					NewRect2D(Object->Rect.Pos.x + 5, Object->Rect.Pos.y + 5, IconHeight - 10, IconHeight - 10),
					NewRect2D(uv.x, uv.y, uv.z, uv.w)
				);
			}
        }
    }
	spall_buffer_end(&spall_ctx, &spall_buffer, get_time_in_nanos() // timestamp in nanoseconds -- end of your timing block
					 );
}

fn_internal void
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

fn_internal void
	TopDownSplitMerge(ui_window** WorkBuffer, u32 Begin, u32 End, ui_window** SrcWindows) {
	if (End - Begin <= 1) {
		return;
	}

	u32 Middle = (End + Begin) / 2;
	TopDownSplitMerge(SrcWindows, Begin, Middle, WorkBuffer);
	TopDownSplitMerge(SrcWindows, Middle, End, WorkBuffer);
	TopDownMerge(SrcWindows, Begin, Middle, End, WorkBuffer);
}

fn_internal void
	TopDownMergeSort(ui_window** WorkBuffer, ui_window** SrcWindows, u32 N) {

	TopDownSplitMerge(WorkBuffer, 0, N, SrcWindows);
}

fn_internal void
	UI_SortWindowByDepth(ui_win_stack* window, Stack_Allocator* Alloc ) {

	ui_window** WorkBuffer = stack_push(Alloc, ui_window*, window->Current);
	for (u32 i = 0; i < window->Current; i += 1) {
		WorkBuffer[i] = (window->Items[i]);
	}
	TopDownMergeSort(WorkBuffer, window->Items, window->Current);
}

fn_internal void UI_WindowBegin(ui_context* Context, rect_2d Rect, const char* Title, ui_lay_opt Options) {
    ui_window* win = stack_push(Context->TempAllocator, ui_window, 1);

    win->WindowRect = Rect;
    win->ContentSize = vec2{0, 0};

    TreeInit(&win->Objects, &UI_NULL_OBJECT);

    StackPush(&Context->Windows, win);

    TreeInit(&StackGetFront(&Context->Windows)->Objects, &UI_NULL_OBJECT);

    ui_window* NewWin = StackGetFront(&Context->Windows);

    ui_object* Object = &UI_NULL_OBJECT;

    ui_lay_opt WOpt = (ui_lay_opt)(UI_DrawText | UI_Interact);

    if( Options & UI_NoTitle ) {
        WOpt = (ui_lay_opt)(WOpt ^ UI_DrawText);
    }
	UI_SetNextParent(Context, &Context->RootObject);
    UI_SetNextTheme( Context, Context->DefaultTheme.Window );
    Object = UI_BuildObjectWithParent(Context, (const u8*)Title, (const u8*)Title, Rect, static_cast<ui_lay_opt>(WOpt | Options), &NewWin->Objects);
    Object->Type = UI_Window;
    // Make it the focus object if clicking on title bar or
    // on the resize box on the bottom-left part of the window
    //
    if( UI_ConsumeEvents(Context, Object) & Input_LeftClickPress ) {
        rect_2d TitleRect = rect_2d{Object->Rect.Pos, vec2{Object->Rect.Size.x, Object->Size.y}};
        vec2 CornerSize = vec2{20, 20};
        rect_2d ResizeRect = rect_2d{
            Vec2Add(Object->Rect.Pos, Vec2Sub(Object->Rect.Size, CornerSize)),
            vec2{20, 20}
        };
        if(IsCursorOnRect(Context, TitleRect) && (Options & UI_Select) ) {
			if (Object->DepthIdx < Context->MaxDepth) {
				Object->DepthIdx = Context->MaxDepth + 1;
			}
            Context->FocusObject = Object;
        } else if( Object->Option & UI_Resize && (Options & UI_Select) )  {
            if( IsCursorOnRect(Context, ResizeRect) ) {
                if (Object->DepthIdx <= Context->MaxDepth) {
					//Object->DepthIdx += 1;
				}
                Context->FocusObject = Object;
            }
        }

		Context->MaxDepth = Max(Context->MaxDepth, Object->DepthIdx);
    }

	if (UI_ConsumeEvents(Context, Object) & Input_LeftClickPress) {
		Context->FocusObject = Object;
	} else if (Context->LastInput & Input_LeftClickRelease && Object == Context->FocusObject) {
		Context->FocusObject = &UI_NULL_OBJECT;
	} else if (Object == Context->FocusObject) {
		vec2 CornerSize = (vec2){20, 20};
        rect_2d ResizeRect = (rect_2d){
            Vec2Add(Object->Rect.Pos, Vec2Sub(Object->Rect.Size, CornerSize)),
            CornerSize
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

    UI_PushNextLayout(Context, Object->Rect, static_cast<ui_lay_opt>(0));
    ui_layout* Layout = &StackGetFront(&Context->Layouts);
    Layout->BoxSize = (vec2){Object->Rect.Size.x, F_TextHeight(Object->Theme.Font)};
    Layout->ContentSize.v[Layout->AxisDirection] += Object->Size.v[Layout->AxisDirection];
    Layout->Padding = (vec2){0, 0};
    Context->CurrentParent = Object;

    NewWin->WindowRect = Object->Rect;
}

fn_internal void UI_WindowEnd(ui_context* Context) {
    if( !StackIsEmpty(&Context->Themes) ) {
        StackPop(&Context->Themes);
    }
    if( !StackIsEmpty(&Context->Layouts) ) {
        StackPop(&Context->Layouts);
    }
}

fn_internal U8_String* UI_GetTextFromBox(ui_context* Context, const char* Key) {
    spall_buffer_begin(&spall_ctx, &spall_buffer, 
					  __FUNCTION__,             // name of your function
					  sizeof(__FUNCTION__) - 1, // name len minus the null terminator
					  get_time_in_nanos()      // timestamp in nanoseconds -- start of your timing block
					  );
	ui_object* parent = Context->CurrentParent;
    if( HashTableContains(&Context->TableObject, Key, parent->HashId) ) {
        entry* StoredWindowEntry = HashTableFindPointer(&Context->TableObject, Key, parent->HashId);
        ui_object* Value = (ui_object*)StoredWindowEntry->Value;
        return &Value->Text;
    }
	spall_buffer_end(&spall_ctx, &spall_buffer, get_time_in_nanos() // timestamp in nanoseconds -- end of your timing block
					 );
    return NULL;
}

fn_internal void UI_DrawRect2D(ui_context* Context, rect_2d Rect, vec4 color, vec4 color_border, f32 border, f32 radius) {
	spall_buffer_begin(&spall_ctx, &spall_buffer, 
					   __FUNCTION__,             // name of your function
					   sizeof(__FUNCTION__) - 1, // name len minus the null terminator
					   get_time_in_nanos()      // timestamp in nanoseconds -- start of your timing block
					   );
	ui_object* Parent = Context->CurrentParent;

	ui_object* Object = stack_push(Context->TempAllocator, ui_object, 1);
	memset(Object, 0, sizeof(ui_object));
	Object->Rect = Rect;
	Object->Theme.BorderThickness = border;
	Object->Theme.Radius = radius;
	Object->Theme.Background = color;
	Object->Theme.Border = color_border;

	Object->Option = static_cast<ui_lay_opt>(UI_DrawRect | UI_DrawBorder);

	TreeInit(Object, &UI_NULL_OBJECT);
    TreePushSon(Parent, Object, &UI_NULL_OBJECT);
	spall_buffer_end(&spall_ctx, &spall_buffer, get_time_in_nanos() // timestamp in nanoseconds -- end of your timing block
					 );
}

fn_internal void UI_DrawText2D(ui_context* Context, rect_2d Rect, U8_String* String, FontCache* Font, vec4 color) {
	spall_buffer_begin(&spall_ctx, &spall_buffer, 
					   __FUNCTION__,             // name of your function
					   sizeof(__FUNCTION__) - 1, // name len minus the null terminator
					   get_time_in_nanos()      // timestamp in nanoseconds -- start of your timing block
					   );
	ui_object* Parent = Context->CurrentParent;

	ui_object* Object = stack_push(Context->TempAllocator, ui_object, 1);
	memset(Object, 0, sizeof(ui_object));
	Object->Pos = Rect.Pos;
	Object->Size = Vec2New(F_TextWidth(Font, (const char*)String->data, String->idx), F_TextHeight(Font));
	Object->Theme.Foreground = color;
	Object->Text = StringCreate(String->len, Context->TempAllocator);
	Object->Theme.Font = Font;
	StringCpyStr(&Object->Text, String);

	Object->Option = UI_DrawText;

	TreeInit(Object, &UI_NULL_OBJECT);
	TreePushSon(Parent, Object, &UI_NULL_OBJECT);
	spall_buffer_end(&spall_ctx, &spall_buffer, get_time_in_nanos() // timestamp in nanoseconds -- end of your timing block
					 );
}

fn_internal ui_object* UI_BuildObjectWithParent(ui_context* Context, const u8* Key, const u8* Text, rect_2d Rect, ui_lay_opt Options, ui_object* Parent )
{
	spall_buffer_begin(&spall_ctx, &spall_buffer, 
					   __FUNCTION__,             // name of your function
					   sizeof(__FUNCTION__) - 1, // name len minus the null terminator
					   get_time_in_nanos()      // timestamp in nanoseconds -- start of your timing block
					   );
    ui_object* Object = &UI_NULL_OBJECT;

    ui_object* IdParent = Context->CurrentParent;
	entry* StoredEntry = HashTableFindPointer(&Context->TableObject, (const char*)Key, Parent->HashId);

    if( StoredEntry != NULL ) {
        
		Object = (ui_object*)StoredEntry->Value;
    } else {
        Object = stack_push(Context->Allocator, ui_object, 1);
        memset(Object, 0, sizeof(ui_object));
        StoredEntry = HashTableAdd(&Context->TableObject, (const char*)Key, Object, Parent->HashId);
        Object->HashId = StoredEntry->HashId;
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

	if (!StackIsEmpty(&Context->Layouts)) {
		ui_layout* Layout = &StackGetFront(&Context->Layouts);
		if (Options & UI_HintSize) {
			Object->Rect.Size = Rect.Size;
		} else {
			Object->Rect.Size = Layout->BoxSize;
		}
		Object->Rect.Pos.v[Layout->AxisDirection] = Layout->Size.Pos.v[Layout->AxisDirection] + Layout->ContentSize.v[Layout->AxisDirection];
		vec2 RightPadding = Vec2ScalarMul(2, Layout->Padding);

        if( Layout->N_Columns == 0 && Layout->N_Rows == 0 ) {
        }
		if( Layout->N_Columns > 0 && Layout->AxisDirection == 0 && Layout->CurrentColumn < Layout->N_Columns) {
            Object->Rect.Size.x = Layout->ColumnSizes[Layout->CurrentColumn];
            Layout->CurrentColumn += 1;
        }
		if( Layout->N_Rows > 0 && Layout->AxisDirection == 1 && Layout->CurrentRow < Layout->N_Rows) {
            Object->Rect.Size.y = Layout->RowSizes[Layout->CurrentRow];
            Layout->CurrentRow += 1;
        }

        Object->Rect.Pos  = Vec2Add(Object->Rect.Pos, Layout->Padding);
		if (!(Options & UI_HintSize)) {
			Object->Rect.Size = Vec2Sub(Object->Rect.Size, RightPadding);
		}
		Object->Rect.Size = Vec2Add(Object->Rect.Size, Layout->Spacer);

		if( Layout->NextIconType != Icon_None ) {
			Object->Option = static_cast<ui_lay_opt>(Object->Option | UI_DrawIcon);
			Object->Type = static_cast<object_type>(Layout->NextIconType);
		  //Object->Pos += Object->Rect.Size.y;
		  Object->Size.x += Object->Rect.Size.y;
		}

        Layout->ContentSize.v[Layout->AxisDirection] += Object->Rect.Size.v[Layout->AxisDirection] + Layout->Padding.v[Layout->AxisDirection];
		Parent->ContentSize.v[Layout->AxisDirection] += Object->Rect.Size.v[Layout->AxisDirection] + Layout->Padding.v[Layout->AxisDirection];

		//Layout->ContentSize.v[Layout->AxisDirection] += Layout->Spacer.v[Layout->AxisDirection];
	}   //Parent->ContentSize.v[Layout->AxisDirection] += Layout->Spacer.v[Layout->AxisDirection];

    if( Options & UI_DrawText ) {
        u32 Len = CustomStrlen((const char*)Text);
        if( Object->Text.data == NULL ) {
            Object->Text = StringNew((const char*)Text, Len, Context->Allocator);
        } else if( Object->Type != UI_InputText ) {
            // If it is Input text we do not want to copy again the title text, as
            // it already stores input information from the user
            StringCpy(&Object->Text, (const char*)Text);
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
                if( LastSlash != -1 ) {
                    LastSlash += 1;
                }
                if( LastUnderScore != -1 ) {
                    LastUnderScore += 1;
                }
                if( LastSpace != -1 ) {
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
					Object->TextCursorIdx = 0;
                }

            } else if( Context->LastInput & Input_Backspace ) {
                //Object->Text.idx = Object->Text.idx > 0 ? Object->Text.idx - 1 : 0;
                StringErase(&Object->Text, Object->TextCursorIdx);
            }
        }

        Object->Pos  = Object->Rect.Pos;
        Object->Size = vec2{
            (f32)F_TextWidth(Object->Theme.Font, (const char*)Object->Text.data, Object->Text.idx),
            (f32)F_TextHeight(Object->Theme.Font)
        };

        if( Object->Option & UI_DrawIcon && !(Object->Option & UI_AlignCenter)) {
            Object->Pos.x += Object->Rect.Size.y + 5;
        }

		if (Object->Size.x > Object->Rect.Size.x) {

			// How much text is outside the render box
			//
			f32 Offset = Object->Size.x - Object->Rect.Size.x;
			// Average size of a character given the size of the string and the number of
			// characters is composed of
			//
			f32 C_Avg  = Object->Size.x / Object->Text.idx;

			u32 N_Chars = (u32) ceilf(Offset / C_Avg);

			if (N_Chars <= Object->Text.idx) {
				// @todo This would affect text boxes
				// Object->Text.idx -= N_Chars;
			}
		}

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
	spall_buffer_end(&spall_ctx, &spall_buffer, get_time_in_nanos() // timestamp in nanoseconds -- end of your timing block
					 );
    return Object;
}

fn_internal bool IsCursorOnRect(ui_context* Context, rect_2d Rect) {
    vec2 Cursor = Context->CursorPos;
    if( Cursor.x >= Rect.Pos.x && Cursor.x <= Rect.Pos.x + Rect.Size.x ) {
        if( Cursor.y >= Rect.Pos.y && Cursor.y <= Rect.Pos.y + Rect.Size.y ) {
            return true;
        }
    }
    return false;
}

fn_internal bool UI_CheckBoxInsideScrollView(ui_context* Context, ui_object* Object) {
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

fn_internal ui_input UI_ConsumeEvents(ui_context* Context, ui_object* Object) {
    if( (Object->Option & UI_Interact) && IsCursorOnRect(Context, Object->Rect) ) {
        if( UI_CheckBoxInsideScrollView(Context, Object)) {
            return Context->CursorAction | Input_CursorHover | Context->LastInput;
        }
    }

    return Input_None;
}

fn_internal ui_input UI_Button(ui_context* Context, const char* Title) {

    ui_object* Parent = Context->CurrentParent;

    ui_layout Layout = StackGetFront(&Context->Layouts);
    rect_2d Rect     = Layout.Size;
    Rect.Pos         = Vec2Add(Rect.Pos, Layout.ContentSize);
    Rect.Size        = Layout.BoxSize;

    ui_lay_opt Options = static_cast<ui_lay_opt>(UI_DrawRect | UI_DrawBorder | UI_DrawText | UI_AlignVertical | UI_AlignCenter | UI_Interact | UI_Select);

    UI_SetNextTheme( Context, Context->DefaultTheme.Button );
    ui_object* Button = UI_BuildObjectWithParent(Context, (const u8*)Title, (const u8*)Title, Rect, Options, Parent);

    ui_input Input = UI_ConsumeEvents(Context, Button);
    Button->LastInputSet = Input;
    //Button->Type = UI_ButtonType;

    if( Input & Input_LeftClickPress ) {
        Context->FocusObject = Button;
    } else if ( Input & Input_LeftClickRelease ) {
        Context->FocusObject = &UI_NULL_OBJECT;
    }

    if( Button->LastInputSet & Input_CursorHover ) {
		//rgba Background = HexToRGBA(0x0088FFFF);
		//Button->Theme.Background = RgbaToNorm(Background);
		Button->Theme.Background = Context->DefaultTheme.OnHoverBackground;
		Button->Theme.Foreground = Context->DefaultTheme.OnHoverForeground;
	} else if( Button == Context->FocusObject ) {
		Button->Theme.Background = Context->DefaultTheme.OnSuccessBackground;
		Button->Theme.Foreground = Context->DefaultTheme.OnSuccessForeground;
	}

    return Input;
}

fn_internal ui_input UI_Label(ui_context* Context, const char* text) {
    ui_object* Parent = Context->CurrentParent;

    ui_layout Layout = StackGetFront(&Context->Layouts);
    rect_2d Rect     = Layout.Size;
    Rect.Pos         = Vec2Add(Rect.Pos, Layout.ContentSize);
    Rect.Size        = Layout.BoxSize;

    ui_lay_opt Options = static_cast<ui_lay_opt>(UI_DrawText | UI_AlignVertical | UI_Interact | UI_Select | Layout.Option);
    UI_SetNextTheme( Context, Context->DefaultTheme.Label );
    ui_object* Label = UI_BuildObjectWithParent(Context, (const u8*)text, (const u8*)text, Rect, Options, Parent);

	StackPop(&Context->Themes);
    ui_input Input = UI_ConsumeEvents(Context, Label);
    Label->LastInputSet = Input;
    //Label->Type = UI_LabelType;

    return Input;
}

fn_internal ui_input UI_LabelWithKey(ui_context* Context, const char* Key, const char* text) {
    ui_object* Parent = Context->CurrentParent;

    ui_layout Layout = StackGetFront(&Context->Layouts);
    rect_2d Rect     = Layout.Size;
    Rect.Pos         = Vec2Add(Rect.Pos, Layout.ContentSize);
    Rect.Size        = Layout.BoxSize;

    ui_lay_opt Options = static_cast<ui_lay_opt>(UI_DrawText | UI_AlignVertical | UI_Interact | UI_Select);
    UI_SetNextTheme( Context, Context->DefaultTheme.Input );
    ui_object* Label = UI_BuildObjectWithParent(Context, (const u8*)Key, (const u8*)text, Rect, Options, Parent);
	StackPop(&Context->Themes);
    ui_input Input = UI_ConsumeEvents(Context, Label);
    Label->LastInputSet = Input;
    //Label->Type = UI_LabelType;

    return Input;
}

fn_internal ui_input UI_TextBox(ui_context* Context, const char* text) {
    ui_object* Parent = Context->CurrentParent;

    ui_layout Layout = StackGetFront(&Context->Layouts);
    rect_2d Rect     = Layout.Size;
    Rect.Pos         = Vec2Add(Rect.Pos, Layout.ContentSize);
    Rect.Size        = Layout.BoxSize;

    ui_lay_opt Options = static_cast<ui_lay_opt>(UI_DrawBorder | UI_DrawRect | UI_DrawText | UI_AlignVertical | UI_Interact | UI_Select);
    UI_SetNextTheme( Context, Context->DefaultTheme.Input );
    ui_object* TextBox = UI_BuildObjectWithParent(Context, (const u8*)text, (const u8*)text, Rect, Options, Parent);
	StackPop(&Context->Themes);
    ui_input Input = UI_ConsumeEvents(Context, TextBox);
    TextBox->LastInputSet = Input;
    TextBox->Type = UI_InputText;

	if (Input & Input_LeftClickPress) {

		Context->FocusObject = TextBox;
	}

	if( TextBox == Context->FocusObject ) {
        TextBox->LastInputSet |= Context->LastInput;
		Input |= Context->LastInput;
    }

	if ( Input & Input_KeyChar || Input & Input_KeyPressed ) {
		if (TextBox->Size.x > TextBox->Rect.Size.x) {
			// How much text is outside the render box
			//
			f32 Offset = TextBox->Size.x - TextBox->Rect.Size.x;
			// Average size of a character given the size of the string and the number of
			// characters is composed of
			//
			f32 C_Avg  = TextBox->Size.x / TextBox->Text.idx;

			u32 N_Chars = (u32) ceilf(Offset / C_Avg);

			if (TextBox->TextCursorIdx >= N_Chars && (Input & Input_Backspace || Input & Input_Left ) ) {
				TextBox->TextStartIdx = Max(0, TextBox->TextStartIdx - 1);
			} else if (TextBox->TextCursorIdx >= N_Chars) {
				TextBox->TextStartIdx = Min(TextBox->TextCursorIdx, TextBox->TextStartIdx + 1);
			}
		} else {
			TextBox->TextStartIdx = 0;
		}
	}

    if( TextBox->LastInputSet & Input_CursorHover ) {
		TextBox->Theme.Background.b += 0.2;
    }

    return TextBox->LastInputSet;
}


fn_internal ui_input UI_BeginTreeNode(ui_context* Context, const char* text) {
    ui_object* Parent = Context->CurrentParent;

    ui_layout Layout = StackGetFront(&Context->Layouts);
    rect_2d Rect     = Layout.Size;
    Rect.Pos         = Vec2Add(Rect.Pos, Layout.ContentSize);
    Rect.Size        = Layout.BoxSize;

    ui_lay_opt Options = static_cast<ui_lay_opt>(UI_DrawBorder | UI_DrawRect | UI_DrawText | UI_AlignVertical | UI_AlignCenter | UI_Interact | UI_Select);
    UI_SetNextTheme( Context, Context->DefaultTheme.Panel );
    ui_object* TreeNode = UI_BuildObjectWithParent(Context, (const u8*)text, (const u8*)text, Rect, Options, Parent);
	StackPop(&Context->Themes);
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
    }

	if ((Input & Input_LeftClickRelease) && Context->FocusObject == TreeNode) {
		Context->FocusObject = &UI_NULL_OBJECT;
	}

	UI_SetNextParent(Context, TreeNode);

    if( TreeNode->LastInputSet & Input_CursorHover ) {
        TreeNode->Theme.Background.b *= 1.8;
    }

    return TreeNode->LastInputSet;
}

fn_internal void UI_EndTreeNode(ui_context* Context) {
	UI_PopLastParent(Context);
}

fn_internal void UI_PushNextLayoutIcon(ui_context* Context, icon_type Type) {
    ui_layout* Layout = &StackGetFront(&Context->Layouts);
    Layout->NextIconType = Type;
}


fn_internal void UI_PopNextIcon(ui_context* Context) {
    ui_layout* Layout = &StackGetFront(&Context->Layouts);
    Layout->NextIconType = Icon_None;
}

fn_internal ui_input UI_SetIcon(ui_context* Context, const char* Name, icon_type Type) {
	ui_object* Parent = Context->CurrentParent;

    ui_layout Layout = StackGetFront(&Context->Layouts);
    rect_2d Rect     = Layout.Size;
    Rect.Pos         = Vec2Add(Rect.Pos, Layout.ContentSize);
    Rect.Size        = Vec2New(Layout.BoxSize.y, Layout.BoxSize.y);

	Layout.AxisDirection = 0;

	char KeyName[126];
	snprintf(KeyName, 126, "%s_%p", Name, Parent->Last);

    vec2 BoxSize = Layout.BoxSize;
    //UI_PushNextLayoutBoxSize(Context, Rect.Size);

	ui_lay_opt Options = static_cast<ui_lay_opt>(UI_DrawIcon | UI_HintSize);
	ui_object* Icon  = UI_BuildObjectWithParent(Context, (const u8*)KeyName, (const u8*)Name, Rect, Options, Parent);
	Icon->Type = static_cast<object_type>(Type);

	//UI_PushNextLayoutBoxSize(Context, BoxSize);

	Layout.AxisDirection = 1;

	switch (Type) {
		case Icon_ArrowDown: {

		} break;
		case Icon_File: {

		} break;
		case Icon_Folder: {

		} break;
		case Icon_Wave: {

		} break;
		case Icon_Download: {

		} break;
		case Icon_Details: {

		} break;
		case Icon_Send:	 {

		} break;
	}
}

fn_internal void UI_BeginScrollbarViewEx(ui_context* Context, vec2 Size) {

	spall_buffer_begin(&spall_ctx, &spall_buffer, 
					   __FUNCTION__,             // name of your function
					   sizeof(__FUNCTION__) - 1, // name len minus the null terminator
					   get_time_in_nanos()      // timestamp in nanoseconds -- start of your timing block
					   );

    char name[] = "ScrollBarView";

    ui_layout *Layout = &StackGetFront(&Context->Layouts);

    ui_object* Parent = Context->CurrentParent;

    rect_2d Rect = Layout->Size;


    Rect.Pos.x = Rect.Pos.x + Rect.Size.x - 16;
	Rect.Pos.y += Layout->ContentSize.y;
	if (Size.x == 0 || Size.y == 0) {
		Rect.Size.x = 8;
		Rect.Size.y -= 2 * Layout->ContentSize.y;
	} else {
		Rect.Size = Size;
	}

	UI_DrawRect2D(Context, Rect, Context->DefaultTheme.Scrollbar.Background, Context->DefaultTheme.Scrollbar.Foreground, 1, 4);
    Parent->Last->Type = UI_ScrollbarType;

    Context->CurrentParent = Parent->Last;
	spall_buffer_end(&spall_ctx, &spall_buffer, get_time_in_nanos() // timestamp in nanoseconds -- end of your timing block
					 );
}

fn_internal void UI_EndScrollbarView(ui_context* Context) {
	spall_buffer_begin(&spall_ctx, &spall_buffer, 
					   __FUNCTION__,             // name of your function
					   sizeof(__FUNCTION__) - 1, // name len minus the null terminator
					   get_time_in_nanos()      // timestamp in nanoseconds -- start of your timing block
					   );
    ui_object* Scrollbar   = Context->CurrentParent;
    float ContentHeight    = Scrollbar->ContentSize.y;
    float ViewportContent  = Scrollbar->Rect.Size.y;
    rect_2d ScrollableSize = Scrollbar->Rect;
    ContentHeight          = Max(1.0f, ContentHeight);
    ScrollableSize.Size.y  = (ViewportContent / ContentHeight);
	f32 ScrollableDistance = ContentHeight - ViewportContent;
	f32 TrackScrollableDistance = ViewportContent - ScrollableSize.Size.y;
	f32 ScrollRatio = 6;
    ScrollableSize.Size.y  = Max(ScrollableSize.Size.y, 20.0f);

    char buf[] = "EndOfScroll";

    assert(!StackIsEmpty(&Context->Layouts));

    ui_layout* Layout = &StackGetFront(&Context->Layouts);

    vec2 Padding = Layout->Padding;
    Layout->Padding = vec2{0, 0};

	vec2 BoxSize = Layout->BoxSize;
	UI_PushNextLayoutBoxSize(Context, ScrollableSize.Size);

    UI_SetNextTheme( Context, Context->DefaultTheme.Scrollbar );
    ui_object* Scrollable = UI_BuildObjectWithParent(Context, (const u8*)buf, NULL, ScrollableSize, static_cast<ui_lay_opt>(UI_DrawRect | UI_Select | UI_Interact), Scrollbar);
    StackPop(&Context->Themes);
	Scrollable->Type = UI_ScrollbarTypeButton;
    Scrollable->Theme = Context->DefaultTheme.Scrollbar;
    Layout->Padding = Padding;

    Layout->ContentSize.y -= Max(0, ContentHeight - ViewportContent + Scrollable->Rect.Size.y);

    vec2 VerticalDelta = Vec2Zero();
    if( Context->FocusObject == Scrollable ) {
        VerticalDelta = vec2{0, Context->CursorDelta.y};
    }

	if (Scrollable->Rect.Pos.y + Scrollable->Rect.Size.y > Scrollbar->Rect.Pos.y + Scrollbar->Rect.Size.y) {
		f32 OldPos = Scrollable->Rect.Pos.y;
		f32 FarOff = (Scrollable->Rect.Pos.y + Scrollable->Rect.Size.y) - (Scrollbar->Rect.Pos.y + Scrollbar->Rect.Size.y);
		Scrollable->Rect.Pos.y -= FarOff;
	} else {
		vec2 LastDelta = Scrollable->LastDelta;
		Scrollable->LastDelta = Vec2Add(Scrollable->LastDelta, VerticalDelta);
		Scrollable->ScrollRatio = ScrollRatio;
		Scrollable->Rect.Pos.y += Scrollable->LastDelta.y * ScrollRatio;
		Scrollable->Rect.Pos.y = Max(Scrollbar->Rect.Pos.y, Scrollable->Rect.Pos.y);
		Scrollable->Rect.Pos.y = Min(Scrollable->Rect.Pos.y, Scrollbar->Rect.Pos.y + ViewportContent);
	}

    if(Scrollable->Rect.Pos.y == Scrollbar->Rect.Pos.y) {
        Scrollable->LastDelta = Vec2Zero();
    } else if( Scrollable->Rect.Pos.y == Scrollbar->Rect.Pos.y + Scrollbar->Rect.Size.y ) {
		f32 OldPos = Scrollable->Rect.Pos.y;
		f32 FarOff = (Scrollable->Rect.Pos.y + Scrollable->Rect.Size.y) - (Scrollbar->Rect.Pos.y + Scrollbar->Rect.Size.y);
		Scrollable->Rect.Pos.y -= FarOff;
		//Scrollable->LastDelta = Vec2Sub(Scrollable->LastDelta, VerticalDelta);
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
	UI_PushNextLayoutBoxSize(Context, BoxSize);

	spall_buffer_end(&spall_ctx, &spall_buffer, get_time_in_nanos());
}

fn_internal void UI_SetNextParent(ui_context* Context, ui_object* Object) {
    Context->CurrentParent = Object;
}

fn_internal void UI_PopLastParent(ui_context* Context) {
    Context->CurrentParent = Context->CurrentParent->Parent;
}

fn_internal void UI_PushNextLayout(ui_context* Context, rect_2d Rect, ui_lay_opt Options) {
    StackPush(&Context->Layouts, (ui_layout){});
    ui_layout* Layout = &StackGetFront(&Context->Layouts);

    Layout->Size          = Rect;
    Layout->ContentSize   = Vec2Zero();
    Layout->Option        = Options;
    Layout->AxisDirection = 1; // y axis (goes vertically)
    Layout->BoxSize       = Vec2Zero();
	Layout->Padding = Vec2Zero();
	Layout->Spacer = Vec2Zero();
}


fn_internal void UI_PushNextLayoutRow(ui_context* Context, int N_Rows, const f32* Rows ) {

    assert(!StackIsEmpty(&Context->Layouts));

    ui_layout* Layout = &StackGetFront(&Context->Layouts);

    Layout->N_Rows   = N_Rows;
    Layout->RowSizes = Rows;
}

fn_internal void UI_PushNextLayoutColumn(ui_context* Context, int N_Columns, const f32* Columns ) {
    assert(!StackIsEmpty(&Context->Layouts));

    ui_layout* Layout = &StackGetFront(&Context->Layouts);

    Layout->N_Columns   = N_Columns;
    Layout->ColumnSizes = Columns;
}

fn_internal void UI_PushNextLayoutBoxSize(ui_context* Context, vec2 BoxSize) {
	assert(!StackIsEmpty(&Context->Layouts));

    ui_layout* Layout = &StackGetFront(&Context->Layouts);

	Layout->BoxSize = BoxSize;
}

fn_internal void
	UI_PushNextLayoutPadding(ui_context* Context, vec2 Padding) {
    assert(!StackIsEmpty(&Context->Layouts));

    ui_layout* Layout = &StackGetFront(&Context->Layouts);
    Layout->Padding    = Padding;
}

fn_internal void
	UI_PushNextLayoutOption(ui_context* Context, ui_lay_opt Options) {
    assert(!StackIsEmpty(&Context->Layouts));

    ui_layout* Layout = &StackGetFront(&Context->Layouts);
    Layout->Option    = Options;
}

fn_internal void UI_PushLayoutSpacer(ui_context* Context, vec2 Spacer) {
	assert(!StackIsEmpty(&Context->Layouts));

    ui_layout* Layout = &StackGetFront(&Context->Layouts);
    Layout->Spacer    = Spacer;
}

fn_internal void UI_Spacer(ui_context* Context, vec2 Spacer) {
	assert(!StackIsEmpty(&Context->Layouts));

    ui_layout* Layout = &StackGetFront(&Context->Layouts);

	Layout->ContentSize = Vec2Add(Layout->ContentSize, Spacer);
	Context->CurrentParent->ContentSize = Vec2Add(Context->CurrentParent->ContentSize, Spacer);
}

fn_internal void
	UI_PushNextLayoutDisableOption(ui_context* Context, ui_lay_opt Options) {
    assert(!StackIsEmpty(&Context->Layouts));

    ui_layout* Layout = &StackGetFront(&Context->Layouts);
    Layout->Option    = static_cast<ui_lay_opt>(Layout->Option ^ Options);
}

fn_internal vec2
	UI_UpdateObjectSize(ui_context* Context, ui_object* Object) {
	if (UI_ConsumeEvents(Context, Object) & Input_LeftClickPress) {
		Context->FocusObject = Object;
	} else if (Context->LastInput & Input_LeftClickRelease && Object == Context->FocusObject) {
		Context->FocusObject = &UI_NULL_OBJECT;
		Context->IsOnDrag = false;
		Context->IsOnResize = false;
	} else if (Object == Context->FocusObject) {
		vec2 CornerSize = (vec2){20, 20};
        rect_2d ResizeRect = (rect_2d){
            Vec2Add(Object->Rect.Pos, Vec2Sub(Object->Rect.Size, CornerSize)),
            CornerSize
        };
        if( Object->Option & UI_Resize && !Context->IsOnDrag)  {
            if( IsCursorOnRect(Context, ResizeRect) || Context->IsOnResize ) {
                Object->Rect.Size = Vec2Add(Object->Rect.Size, Context->CursorDelta);
				Context->IsOnResize = true;
            }
        }
        if( Object->Option & UI_Drag && !Context->IsOnResize ) {
            Object->Rect.Pos = Vec2Add(Object->Rect.Pos, Context->CursorDelta);
			if (IsCursorOnRect(Context, Object->Rect) || Context->IsOnDrag) {
				Context->IsOnDrag = true;
			}
        }

		return Context->CursorDelta;
	}

	return Vec2Zero();
}

fn_internal void
	UI_SetNextTheme(ui_context* Context, object_theme Theme) {
    StackPush(&Context->Themes, Theme);
}

fn_internal void
	UI_PushNextFont(ui_context* Context, FontCache* Font) {
    object_theme* Theme = &StackGetFront(&Context->Themes);
    Theme->Font = Font;
}

fn_internal void
	UI_PopTheme(ui_context* Context) {
    StackPop(&Context->Themes);
}

fn_internal void UI_BeginColumn(ui_context* Context) {
    assert(!StackIsEmpty(&Context->Layouts));

    ui_layout* Layout = &StackGetFront(&Context->Layouts);
    Layout->AxisDirection = 0;
    Layout->CurrentColumn = 0;
}

fn_internal void UI_SetNextCol(ui_context* Context) {
    assert(!StackIsEmpty(&Context->Layouts));

    ui_layout* Layout = &StackGetFront(&Context->Layouts);
	if (Layout->CurrentColumn < Layout->N_Columns) {
		Layout->ContentSize.x += Layout->ColumnSizes[Layout->CurrentColumn] + Layout->Padding.x;
		Context->CurrentParent->ContentSize = Layout->ContentSize;
		Layout->CurrentColumn += 1;
	}
}

fn_internal void UI_EndColumn(ui_context* Context) {
    assert(!StackIsEmpty(&Context->Layouts));
    ui_layout* Layout = &StackGetFront(&Context->Layouts);
    Layout->AxisDirection = 1;
	for (i32 it = 0; it < Layout->N_Columns; it += 1){
		Layout->ContentSize.x -= Layout->ColumnSizes[it] - Layout->Padding.x;
	}
    Layout->ContentSize.y += Layout->BoxSize.y;
}

fn_internal void UI_Column(ui_context* Context) {
    assert(!StackIsEmpty(&Context->Layouts));
    ui_layout* Layout = &StackGetFront(&Context->Layouts);
    Layout->AxisDirection = 0;
}

fn_internal void UI_Row(ui_context* Context) {
    assert(!StackIsEmpty(&Context->Layouts));
    ui_layout* Layout = &StackGetFront(&Context->Layouts);
    Layout->AxisDirection = 1;
}

fn_internal void UI_NextRow(ui_context* Context) {
    assert(!StackIsEmpty(&Context->Layouts));
    ui_layout* Layout = &StackGetFront(&Context->Layouts);
    Layout->AxisDirection = 1;
    Layout->ContentSize.x = 0;
}

fn_internal void UI_BeginRow(ui_context* Context) {
	assert(!StackIsEmpty(&Context->Layouts));

    ui_layout* Layout = &StackGetFront(&Context->Layouts);
    Layout->AxisDirection = 1;
    Layout->CurrentRow = 0;
	Layout->N_Rows = 0;
	if (Layout->CurrentColumn > 0) {
		Layout->ContentSize.x -= Layout->ColumnSizes[Layout->CurrentColumn - 1] - Layout->Padding.x;
		Layout->ContentSize.y += Context->CurrentParent->Last->Rect.Size.y;
		Context->CurrentParent->ContentSize = Layout->ContentSize;
	}
}
fn_internal void UI_EndRow(ui_context* Context)
{
	assert(!StackIsEmpty(&Context->Layouts));
    ui_layout* Layout = &StackGetFront(&Context->Layouts);
    Layout->AxisDirection = 0;
    //Layout->ContentSize.x = 0;
    Layout->ContentSize.y = 0;
}

fn_internal void UI_Divisor(ui_context* Context, f32 Width) {
	assert(!StackIsEmpty(&Context->Layouts));
    ui_layout* Layout = &StackGetFront(&Context->Layouts);

	ui_object* Parent = Context->CurrentParent;
	vec2 ContentSize = Layout->ContentSize;

	rect_2d Rect = NewRect2D(Parent->Rect.Pos.x + ContentSize.x + Layout->Padding.x, Parent->Rect.Pos.y + ContentSize.y, Parent->Rect.Size.x - ContentSize.x - 2 * Layout->Padding.x, Width);

	UI_DrawRect2D(Context, Rect, Context->DefaultTheme.Panel.Background, Parent->Theme.Border, 1, 4);
}

fn_internal ui_input UI_LastEvent(ui_context* Context, api_window* Window) {
    #ifndef NDEBUG 
	spall_buffer_begin(&spall_ctx, &spall_buffer, 
					   __FUNCTION__,             // name of your function
					   sizeof(__FUNCTION__) - 1, // name len minus the null terminator
					   get_time_in_nanos()      // timestamp in nanoseconds -- start of your timing block
					   );
	#endif
	ui_input Input = Input_None;
    u32 window_width  = Window->Width;
    u32 window_height = Window->Height;
    Input = GetNextEvent(Window);

    {
        vec2 Mouse = GetMousePosition(Window);
        vec2 LastCursor = Context->CursorPos;
        Context->CursorPos = Mouse;

        while( Context->LastCursorPos.x == Mouse.x && Context->LastCursorPos.y == Mouse.y && (Input & Input_None) ) {
            Mouse = GetMousePosition(Window);
            Input = GetNextEvent(Window);
        }

        Context->LastCursorPos = LastCursor;
        Context->CursorDelta = Vec2Sub(Context->CursorPos, LastCursor);
    }

    if( Input & FrameBufferResized ) {
        //gfx->base->FramebufferResized = true;
    }
    if( Input & ClipboardPaste ) {
        StringAppend(&Context->TextInput, (const char*)&Window->ClipboardContent);
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
		//Context->FocusObject->TextCursorIdx -= 1;
	}
    if( Input & Input_Right ) {
        Context->FocusObject->TextCursorIdx = Min(Context->FocusObject->Text.idx, Context->FocusObject->TextCursorIdx + 1);
	}
    if( Input & DeleteWord ) {

    } else if( Input & Input_Backspace ) {
        Context->FocusObject->TextCursorIdx = Max(0, Context->FocusObject->TextCursorIdx - 1 );
        Context->TextInput.idx = Max(0, Context->TextInput.idx - 1);
    }
    if( Input & ClipboardPaste ) {
        StringAppend(&Context->TextInput, GetClipboard(Window));
    }
    if( Input & Input_KeyChar ) {
        Context->KeyPressed |= Window->KeyPressed;
        Context->KeyDown    |= Window->KeyPressed;
        char buffer[2] = {0};
        buffer[0] = Window->KeyPressed;
        StringAppend(&Context->TextInput, buffer);
    }

    if( Input & Input_F1 ) {
    }

    Context->LastInput = Input;

	#ifndef NDEBUG 
	spall_buffer_end(&spall_ctx, &spall_buffer, get_time_in_nanos() // timestamp in nanoseconds -- end of your timing block
					 );
	#endif 

    return Input;
}

#endif
=======
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

// --- Spall Profiler Compatibility ---
// These are used for profiling but not available in this build
// Define empty stubs to avoid compilation errors
#ifndef SPALL_PROFILING_ENABLED
#define spall_ctx nullptr
#define spall_buffer nullptr
#define get_time_in_nanos() 0
#define spall_buffer_begin(ctx, buf, name, namelen, timestamp) ((void)0)
#define spall_buffer_end(ctx, buf, timestamp) ((void)0)
#endif

#define HexToRGBA(val) rgba{(uint8_t)((val & 0xff000000) >> 24), (uint8_t)((val & 0x00ff0000) >> 16), (uint8_t)((val & 0x0000ff00) >> 8), (uint8_t)(val & 0x000000ff)}
#define HexToU8_Vec4(val) {(val & 0xff000000) >> 24, (val & 0x00ff0000) >> 16, (val & 0x0000ff00) >> 8, val & 0x000000ff}

#define RgbaToNorm(val) ui_color{(f32)val.r / 255.f, (f32)val.g / 255.f, (f32)val.b / 255.f, (f32)val.a / 255.f}
#define Vec4ToRGBA(val) rgba{val.r, val.g, val.b, val.a}
#define RgbaNew(r, g, b, a) rgba{r, g, b, a}

#define MAX_STACK_SIZE  64
#define MAX_LAYOUT_SIZE 256

typedef enum ui_lay_opt {
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
    UI_DrawShadow       = (1 << 13),
	UI_DrawRect         = (1 << 14),
	UI_DrawBorder       = (1 << 15),
	UI_FitRectToText    = (1 << 16),
	UI_DrawIcon         = (1 << 17),
	UI_HintSize         = (1 << 18)
};

typedef enum object_type {
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

typedef enum icon_type {
    Icon_None,
	Icon_File,
	Icon_Folder,
	Icon_Download,
	Icon_Send,
	Icon_ArrowDown,
	Icon_Wave,
	Icon_Details
};

typedef vec4 ui_color;

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

	ui_color OnDefaultBackground;
	ui_color OnDefaultForeground;
	ui_color OnDefaultBorder;
	ui_color OnHoverBackground;
	ui_color OnHoverForeground;
	ui_color OnHoverBorder;
	ui_color OnSuccessBackground; // Very faint green tint
	ui_color OnSuccessForeground; // Vibrant Terminal Green
	ui_color OnSuccessBorder;
	ui_color OnErrorBackground; // Faint red tint
	ui_color OnErrorForeground; // High-Vis Red
	ui_color OnErrorBorder;
	ui_color OnWarning;
	ui_color BorderPrimary;
	ui_color BorderMedium;
	ui_color BorderHover;
};

typedef struct ui_layout ui_layout;
struct ui_layout {
    i32        N_Rows;
    i32        N_Columns;
    const f32  *RowSizes;
    const f32  *ColumnSizes;
    i32        CurrentRow;
    i32        CurrentColumn;
    rect_2d    Size;
    vec2       ContentSize;
    vec2       BoxSize;
    ui_lay_opt Option;
    u8         AxisDirection;
    vec2       Padding;
	vec2       Spacer;
	icon_type  NextIconType;
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
			i32       TextStartIdx;
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

ui_object UI_NULL_OBJECT;

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

	ui_object RootObject;
    ui_object* CurrentParent;
    ui_object* FocusObject;

    ui_theme DefaultTheme;

	bool IsOnDrag;
	bool IsOnResize;
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

	vec4 IconsUvCoords[Icon_Details + 1];
};

fn_internal void UI_Init(ui_context* Context, Stack_Allocator* Allocator, Stack_Allocator* TempAllocator);

fn_internal void UI_Begin(ui_context* Context);

fn_internal void UI_End( ui_context* Context, draw_bucket_instance* DrawInstance );

fn_internal void UI_WindowBegin(ui_context* Context, rect_2d Rect, const char* Title, ui_lay_opt Options);

fn_internal void UI_WindowEnd(ui_context* Context);

fn_internal ui_input UI_LastEvent(ui_context* Context, api_window* Window);

fn_internal void UI_DrawRect2D(ui_context* Context, rect_2d Rect, vec4 color, vec4 color_border, f32 border, f32 radius);
fn_internal void UI_DrawText2D(ui_context* Context, rect_2d Rect, U8_String* String, FontCache* Font, vec4 color);

fn_internal ui_object* UI_BuildObjectWithParent(
	ui_context* Context,
	const u8* Key,
	const u8* Text,
	rect_2d Rect,
	ui_lay_opt Options,
	ui_object* Parent
);

#define UI_BuildObject(ctx, key, txt, rect, opts) UI_BuildObjectWithParent(ctx, key, txt, rect, opts, ctx->CurrentParent);

fn_internal ui_input UI_ConsumeEvents(ui_context* Context, ui_object* Object);
// Widgets
fn_internal ui_input UI_Button(ui_context* Context, const char* text);
fn_internal ui_input UI_Label(ui_context* Context, const char* text);
fn_internal ui_input UI_LabelWithKey(ui_context* Context, const char* key, const char* text);
fn_internal ui_input UI_TextBox(ui_context* Context, const char* text);
fn_internal ui_input UI_BeginTreeNode(ui_context* Context, const char* text);
fn_internal void     UI_EndTreeNode(ui_context* Context);

fn_internal void UI_PushNextLayoutIcon(ui_context* Context, icon_type Type);
fn_internal void UI_PopNextIcon(ui_context* Context);

fn_internal ui_input UI_SetIcon(ui_context* Context, const char* Name, icon_type Type);

fn_internal void UI_SetNextParent(ui_context* Context, ui_object* Object);
fn_internal void UI_PopLastParent(ui_context* Context);
fn_internal void UI_PushNextLayout(ui_context* Context, rect_2d Rect, ui_lay_opt Options);
fn_internal void UI_PushNextLayoutRow(ui_context* Context, int N_Rows, const f32* Rows );
fn_internal void UI_PushNextLayoutColumn(ui_context* Context, int N_Columns, const f32* Columns );
fn_internal void UI_PushNextLayoutBoxSize(ui_context* Context, vec2 BoxSize);
fn_internal void UI_PushNextLayoutPadding(ui_context* Context, vec2 Padding);
fn_internal void UI_PushNextLayoutOption(ui_context* Context, ui_lay_opt Options);
fn_internal void UI_PushLayoutSpacer(ui_context* Context, vec2 Spacer);

fn_internal void UI_Spacer(ui_context* Context, vec2 Spacer);

fn_internal U8_String*  UI_GetTextFromBox(ui_context* Context, const char* Key);

fn_internal vec2 UI_UpdateObjectSize(ui_context* Context, ui_object* Object);

fn_internal void UI_SetNextTheme(ui_context* Context, object_theme Theme);
fn_internal void UI_PushNextFont(ui_context* Context, FontCache* Font);
fn_internal void UI_PopTheme(ui_context* Context);

fn_internal void UI_BeginColumn(ui_context* Context);
fn_internal void UI_SetNextCol(ui_context* Context);
fn_internal void UI_EndColumn(ui_context* Context);

fn_internal void UI_Column(ui_context* Context);
fn_internal void UI_Row(ui_context* Context);
fn_internal void UI_NextRow(ui_context* Context);

fn_internal void UI_BeginRow(ui_context* Context);
fn_internal void UI_EndRow(ui_context* Context);

fn_internal void UI_Divisor(ui_context* Context, f32 Width);

fn_internal void UI_BeginScrollbarViewEx(ui_context* Context, vec2 MaxSize);
fn_internal void UI_EndScrollbarView(ui_context* Context);

#define UI_BeginScrollbarView(Context) UI_BeginScrollbarViewEx(Context, (vec2){0, 0})

fn_internal bool IsCursorOnRect(ui_context* Context, rect_2d Rect);

fn_internal void UI_SortWindowByDepth(ui_win_stack* window, Stack_Allocator* Allocator );

#endif // _SP_UI_H_

#ifdef SP_UI_IMPL

fn_internal u64
	UI_CustomXXHash( const u8* buffer, u64 len, u64 seed ) {
    return XXH3_64bits_withSeed(buffer, len, seed);
}

fn_internal void
	UI_Init(ui_context* Context, Stack_Allocator* Allocator, Stack_Allocator* TempAllocator) {
    spall_buffer_begin(&spall_ctx, &spall_buffer,
					   __FUNCTION__,             // name of your function
					   sizeof(__FUNCTION__) - 1, // name len minus the null terminator
					   get_time_in_nanos()      // timestamp in nanoseconds -- start of your timing block
					   );
	Context->Allocator = Allocator;
    Context->TempAllocator = TempAllocator;

	UI_NULL_OBJECT.HashId   = 0;
	UI_NULL_OBJECT.DepthIdx = 0;
	UI_NULL_OBJECT.Parent   = &UI_NULL_OBJECT;
	UI_NULL_OBJECT.Left     = &UI_NULL_OBJECT;
	UI_NULL_OBJECT.Right    = &UI_NULL_OBJECT;
	UI_NULL_OBJECT.Last     = &UI_NULL_OBJECT;
	UI_NULL_OBJECT.FirstSon = &UI_NULL_OBJECT;

    Context->Themes.N  = MAX_STACK_SIZE;
    Context->Layouts.N = MAX_LAYOUT_SIZE;
    Context->Windows.N = MAX_STACK_SIZE;

    Context->TextInput = StringCreate(2048, Allocator);
    Context->FocusObject = &UI_NULL_OBJECT;
    Context->CurrentParent = &UI_NULL_OBJECT;

	Context->MaxDepth = 1;
	Context->IsOnDrag = false;
	Context->IsOnResize = false;

    HashTableInit(&Context->TableObject, Allocator, 4096, UI_CustomXXHash);

	spall_buffer_end(&spall_ctx, &spall_buffer, get_time_in_nanos() // timestamp in nanoseconds -- end of your timing block
					 );
}

fn_internal void
	UI_Begin(ui_context* Context) {
    Context->KeyPressed   = 0;
    Context->CursorClick = vec2{0, 0};
    Context->CursorAction = Input_None;
    Context->LastInput    = Input_None;

    Context->TextInput.idx = 0;

	TreeInit(&Context->RootObject, &UI_NULL_OBJECT);

    StackClear(&Context->Windows);
    StackClear(&Context->Layouts);
    StackClear(&Context->Themes);
}

fn_internal void UI_End(ui_context* UI_Context, draw_bucket_instance* DrawInstance) {
	spall_buffer_begin(&spall_ctx, &spall_buffer,
					   __FUNCTION__,             // name of your function
					   sizeof(__FUNCTION__) - 1, // name len minus the null terminator
					   get_time_in_nanos()      // timestamp in nanoseconds -- start of your timing block
					   );
    Stack_Allocator* TempAllocator = UI_Context->TempAllocator;
    Stack_Allocator* Allocator     = UI_Context->Allocator;
    // Stack to iterate over the tree object
    //
    typedef struct {
        u32 N;
        u32 Current;
        ui_object** Items;
    } ObjectStack;

    ObjectStack Stack = {};
    Stack.N = 8 << 20;
    Stack.Current = 0;
    Stack.Items = stack_push(TempAllocator, ui_object*, 8 << 20);

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
            for( ui_object* Child = Object->Last; Child != &UI_NULL_OBJECT && Child != NULL; Child = Child->Left ) {
                StackPush(&Stack, Child);
            }

            vec2 Pos  = Object->Rect.Pos;
            vec2 Size = Object->Rect.Size;

            ui_color PanelBg = Object->Theme.Background;
            ui_color Color   = Object->Theme.Foreground;
            f32 Radius       = Object->Theme.Radius;

			if (Object->Type == UI_ScrollbarTypeButton) {
				PanelBg = Object->Theme.Foreground;
			}

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
            if( Object->Option & UI_DrawRect )
            {
				vec4 BgColor = {PanelBg.r, PanelBg.g, PanelBg.b, PanelBg.a};
				D_DrawRect2D(
					DrawInstance,
					Object->Rect,
					Radius,
					0,
					BgColor
				);

                if( Object->Type == UI_Window && Object->Option & UI_DrawShadow ) {
                    vec2 ShadowSize = {10, 10};
					D_DrawRect2D(
						DrawInstance,
						(rect_2d){Vec2Add(Pos, ShadowSize), Size},
						Radius,
						0,
						Vec4New(
							static_cast<f32>(PanelBg.r * 0.2),
							static_cast<f32>(PanelBg.g * 0.2),
							static_cast<f32>(PanelBg.b * 0.2),
							static_cast<f32>(PanelBg.a * 0.5)
						)
					);
                }

                if( Object->Theme.BorderThickness > 0 && Object->Option & UI_DrawBorder ) {
					vec4 BdColor = {Object->Theme.Border.r, Object->Theme.Border.g, Object->Theme.Border.b, Object->Theme.Border.a};
					D_DrawRect2D(
						DrawInstance,
						Object->Rect,
						Radius,
						Object->Theme.BorderThickness,
						BdColor
					);
                }
            }

            if( Object->Text.data != NULL && Object->Text.idx > 0 && Object->Option & UI_DrawText) {

				U8_String S = StringNew(
					(const char*)(Object->Text.data + Object->TextStartIdx),
					Object->Text.idx - Object->TextStartIdx,
					UI_Context->TempAllocator
				);

				D_DrawText2D(
					DrawInstance,
					NewRect2D(Object->Pos.x, Object->Pos.y, Object->Size.x, Object->Size.y),
					&S,
					Object->Theme.Font,
					Vec4New(Color.r, Color.g, Color.b, Color.a)
				);

                if( Object->TextCursorIdx >= 0 && Object->TextStartIdx >= 0 && Object->Type == UI_InputText) {
                    float Start = Object->Pos.x + F_TextWidth(Object->Theme.Font, "H", 1) * (Object->TextCursorIdx - Object->TextStartIdx);
					float CursorSize = F_TextWidth(Object->Theme.Font, "H", 1);
					D_DrawRect2D(
						DrawInstance,
						(rect_2d) { {Start, Object->Pos.y + 2}, {CursorSize, Object->Size.y - 4} },
						4,
						0,
						Vec4New(Color.r, Color.g, Color.b, Color.a * 0.5)
					);
                }
            } else if( Object->Text.idx == 0 && Object->Type == UI_InputText && Object->Option & UI_DrawText ) {
                float Start = Object->Pos.x + F_TextWidth(Object->Theme.Font, "H", 1 ) * Object->TextCursorIdx;
				float CursorSize = F_TextWidth(Object->Theme.Font, "H", 1);
				D_DrawRect2D(
					DrawInstance,
					(rect_2d) { {Start, Object->Pos.y + 2}, {CursorSize, Object->Size.y - 4} },
					4,
					0,
					Vec4New(Color.r, Color.g, Color.b, Color.a * 0.5)
				);
            }

			if (Object->Option & UI_DrawIcon) {
			     if( Object->Type == Icon_None ) {
			         continue;
			     }

				vec4 uv = UI_Context->IconsUvCoords[Object->Type - 1];
				f32 IconHeight = Object->Rect.Size.y;
				//printf("%f %f\n", Object->Rect.Size.x, Object->Rect.Size.y);
				D_DrawIcon(
					DrawInstance,
					NewRect2D(Object->Rect.Pos.x + 5, Object->Rect.Pos.y + 5, IconHeight - 10, IconHeight - 10),
					NewRect2D(uv.x, uv.y, uv.z, uv.w)
				);
			}
        }
    }
	spall_buffer_end(&spall_ctx, &spall_buffer, get_time_in_nanos() // timestamp in nanoseconds -- end of your timing block
					 );
}

fn_internal void
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

fn_internal void
	TopDownSplitMerge(ui_window** WorkBuffer, u32 Begin, u32 End, ui_window** SrcWindows) {
	if (End - Begin <= 1) {
		return;
	}

	u32 Middle = (End + Begin) / 2;
	TopDownSplitMerge(SrcWindows, Begin, Middle, WorkBuffer);
	TopDownSplitMerge(SrcWindows, Middle, End, WorkBuffer);
	TopDownMerge(SrcWindows, Begin, Middle, End, WorkBuffer);
}

fn_internal void
	TopDownMergeSort(ui_window** WorkBuffer, ui_window** SrcWindows, u32 N) {

	TopDownSplitMerge(WorkBuffer, 0, N, SrcWindows);
}

fn_internal void
	UI_SortWindowByDepth(ui_win_stack* window, Stack_Allocator* Alloc ) {

	ui_window** WorkBuffer = stack_push(Alloc, ui_window*, window->Current);
	for (u32 i = 0; i < window->Current; i += 1) {
		WorkBuffer[i] = (window->Items[i]);
	}
	TopDownMergeSort(WorkBuffer, window->Items, window->Current);
}

fn_internal void UI_WindowBegin(ui_context* Context, rect_2d Rect, const char* Title, ui_lay_opt Options) {
    ui_window* win = stack_push(Context->TempAllocator, ui_window, 1);

    win->WindowRect = Rect;
    win->ContentSize = vec2{0, 0};

    TreeInit(&win->Objects, &UI_NULL_OBJECT);

    StackPush(&Context->Windows, win);

    TreeInit(&StackGetFront(&Context->Windows)->Objects, &UI_NULL_OBJECT);

    ui_window* NewWin = StackGetFront(&Context->Windows);

    ui_object* Object = &UI_NULL_OBJECT;

    ui_lay_opt WOpt = (ui_lay_opt)(UI_DrawText | UI_Interact);

    if( Options & UI_NoTitle ) {
        WOpt = (ui_lay_opt)(WOpt ^ UI_DrawText);
    }
	UI_SetNextParent(Context, &Context->RootObject);
    UI_SetNextTheme( Context, Context->DefaultTheme.Window );
    Object = UI_BuildObjectWithParent(Context, (const u8*)Title, (const u8*)Title, Rect, static_cast<ui_lay_opt>(WOpt | Options), &NewWin->Objects);
    Object->Type = UI_Window;
    // Make it the focus object if clicking on title bar or
    // on the resize box on the bottom-left part of the window
    //
    if( UI_ConsumeEvents(Context, Object) & Input_LeftClickPress ) {
        rect_2d TitleRect = rect_2d{Object->Rect.Pos, vec2{Object->Rect.Size.x, Object->Size.y}};
        vec2 CornerSize = vec2{20, 20};
        rect_2d ResizeRect = rect_2d{
            Vec2Add(Object->Rect.Pos, Vec2Sub(Object->Rect.Size, CornerSize)),
            vec2{20, 20}
        };
        if(IsCursorOnRect(Context, TitleRect) && (Options & UI_Select) ) {
			if (Object->DepthIdx < Context->MaxDepth) {
				Object->DepthIdx = Context->MaxDepth + 1;
			}
            Context->FocusObject = Object;
        } else if( Object->Option & UI_Resize && (Options & UI_Select) )  {
            if( IsCursorOnRect(Context, ResizeRect) ) {
                if (Object->DepthIdx <= Context->MaxDepth) {
					//Object->DepthIdx += 1;
				}
                Context->FocusObject = Object;
            }
        }

		Context->MaxDepth = Max(Context->MaxDepth, Object->DepthIdx);
    }

	if (UI_ConsumeEvents(Context, Object) & Input_LeftClickPress) {
		Context->FocusObject = Object;
	} else if (Context->LastInput & Input_LeftClickRelease && Object == Context->FocusObject) {
		Context->FocusObject = &UI_NULL_OBJECT;
	} else if (Object == Context->FocusObject) {
		vec2 CornerSize = (vec2){20, 20};
        rect_2d ResizeRect = (rect_2d){
            Vec2Add(Object->Rect.Pos, Vec2Sub(Object->Rect.Size, CornerSize)),
            CornerSize
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

    UI_PushNextLayout(Context, Object->Rect, static_cast<ui_lay_opt>(0));
    ui_layout* Layout = &StackGetFront(&Context->Layouts);
    Layout->BoxSize = (vec2){Object->Rect.Size.x, F_TextHeight(Object->Theme.Font)};
    Layout->ContentSize.v[Layout->AxisDirection] += Object->Size.v[Layout->AxisDirection];
    Layout->Padding = (vec2){0, 0};
    Context->CurrentParent = Object;

    NewWin->WindowRect = Object->Rect;
}

fn_internal void UI_WindowEnd(ui_context* Context) {
    if( !StackIsEmpty(&Context->Themes) ) {
        StackPop(&Context->Themes);
    }
    if( !StackIsEmpty(&Context->Layouts) ) {
        StackPop(&Context->Layouts);
    }
}

fn_internal U8_String* UI_GetTextFromBox(ui_context* Context, const char* Key) {
    spall_buffer_begin(&spall_ctx, &spall_buffer,
					  __FUNCTION__,             // name of your function
					  sizeof(__FUNCTION__) - 1, // name len minus the null terminator
					  get_time_in_nanos()      // timestamp in nanoseconds -- start of your timing block
					  );
	ui_object* parent = Context->CurrentParent;
    if( HashTableContains(&Context->TableObject, Key, parent->HashId) ) {
        entry* StoredWindowEntry = HashTableFindPointer(&Context->TableObject, Key, parent->HashId);
        ui_object* Value = (ui_object*)StoredWindowEntry->Value;
        return &Value->Text;
    }
	spall_buffer_end(&spall_ctx, &spall_buffer, get_time_in_nanos() // timestamp in nanoseconds -- end of your timing block
					 );
    return NULL;
}

fn_internal void UI_DrawRect2D(ui_context* Context, rect_2d Rect, vec4 color, vec4 color_border, f32 border, f32 radius) {
	spall_buffer_begin(&spall_ctx, &spall_buffer,
					   __FUNCTION__,             // name of your function
					   sizeof(__FUNCTION__) - 1, // name len minus the null terminator
					   get_time_in_nanos()      // timestamp in nanoseconds -- start of your timing block
					   );
	ui_object* Parent = Context->CurrentParent;

	ui_object* Object = stack_push(Context->TempAllocator, ui_object, 1);
	memset(Object, 0, sizeof(ui_object));
	Object->Rect = Rect;
	Object->Theme.BorderThickness = border;
	Object->Theme.Radius = radius;
	Object->Theme.Background = color;
	Object->Theme.Border = color_border;

	Object->Option = static_cast<ui_lay_opt>(UI_DrawRect | UI_DrawBorder);

	TreeInit(Object, &UI_NULL_OBJECT);
    TreePushSon(Parent, Object, &UI_NULL_OBJECT);
	spall_buffer_end(&spall_ctx, &spall_buffer, get_time_in_nanos() // timestamp in nanoseconds -- end of your timing block
					 );
}

fn_internal void UI_DrawText2D(ui_context* Context, rect_2d Rect, U8_String* String, FontCache* Font, vec4 color) {
	spall_buffer_begin(&spall_ctx, &spall_buffer,
					   __FUNCTION__,             // name of your function
					   sizeof(__FUNCTION__) - 1, // name len minus the null terminator
					   get_time_in_nanos()      // timestamp in nanoseconds -- start of your timing block
					   );
	ui_object* Parent = Context->CurrentParent;

	ui_object* Object = stack_push(Context->TempAllocator, ui_object, 1);
	memset(Object, 0, sizeof(ui_object));
	Object->Pos = Rect.Pos;
	Object->Size = Vec2New(F_TextWidth(Font, (const char*)String->data, String->idx), F_TextHeight(Font));
	Object->Theme.Foreground = color;
	Object->Text = StringCreate(String->len, Context->TempAllocator);
	Object->Theme.Font = Font;
	StringCpyStr(&Object->Text, String);

	Object->Option = UI_DrawText;

	TreeInit(Object, &UI_NULL_OBJECT);
	TreePushSon(Parent, Object, &UI_NULL_OBJECT);
	spall_buffer_end(&spall_ctx, &spall_buffer, get_time_in_nanos() // timestamp in nanoseconds -- end of your timing block
					 );
}

fn_internal ui_object* UI_BuildObjectWithParent(ui_context* Context, const u8* Key, const u8* Text, rect_2d Rect, ui_lay_opt Options, ui_object* Parent )
{
	spall_buffer_begin(&spall_ctx, &spall_buffer,
					   __FUNCTION__,             // name of your function
					   sizeof(__FUNCTION__) - 1, // name len minus the null terminator
					   get_time_in_nanos()      // timestamp in nanoseconds -- start of your timing block
					   );
    ui_object* Object = &UI_NULL_OBJECT;

    ui_object* IdParent = Context->CurrentParent;
	entry* StoredEntry = HashTableFindPointer(&Context->TableObject, (const char*)Key, Parent->HashId);

    if( StoredEntry != NULL ) {

		Object = (ui_object*)StoredEntry->Value;
    } else {
        Object = stack_push(Context->Allocator, ui_object, 1);
        memset(Object, 0, sizeof(ui_object));
        StoredEntry = HashTableAdd(&Context->TableObject, (const char*)Key, Object, Parent->HashId);
        Object->HashId = StoredEntry->HashId;
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

	if (!StackIsEmpty(&Context->Layouts)) {
		ui_layout* Layout = &StackGetFront(&Context->Layouts);
		if (Options & UI_HintSize) {
			Object->Rect.Size = Rect.Size;
		} else {
			Object->Rect.Size = Layout->BoxSize;
		}
		Object->Rect.Pos.v[Layout->AxisDirection] = Layout->Size.Pos.v[Layout->AxisDirection] + Layout->ContentSize.v[Layout->AxisDirection];
		vec2 RightPadding = Vec2ScalarMul(2, Layout->Padding);

        if( Layout->N_Columns == 0 && Layout->N_Rows == 0 ) {
        }
		if( Layout->N_Columns > 0 && Layout->AxisDirection == 0 && Layout->CurrentColumn < Layout->N_Columns) {
            Object->Rect.Size.x = Layout->ColumnSizes[Layout->CurrentColumn];
            Layout->CurrentColumn += 1;
        }
		if( Layout->N_Rows > 0 && Layout->AxisDirection == 1 && Layout->CurrentRow < Layout->N_Rows) {
            Object->Rect.Size.y = Layout->RowSizes[Layout->CurrentRow];
            Layout->CurrentRow += 1;
        }

        Object->Rect.Pos  = Vec2Add(Object->Rect.Pos, Layout->Padding);
		if (!(Options & UI_HintSize)) {
			Object->Rect.Size = Vec2Sub(Object->Rect.Size, RightPadding);
		}
		Object->Rect.Size = Vec2Add(Object->Rect.Size, Layout->Spacer);

		if( Layout->NextIconType != Icon_None ) {
			Object->Option = static_cast<ui_lay_opt>(Object->Option | UI_DrawIcon);
			Object->Type = static_cast<object_type>(Layout->NextIconType);
		  //Object->Pos += Object->Rect.Size.y;
		  Object->Size.x += Object->Rect.Size.y;
		}

        Layout->ContentSize.v[Layout->AxisDirection] += Object->Rect.Size.v[Layout->AxisDirection] + Layout->Padding.v[Layout->AxisDirection];
		Parent->ContentSize.v[Layout->AxisDirection] += Object->Rect.Size.v[Layout->AxisDirection] + Layout->Padding.v[Layout->AxisDirection];

		//Layout->ContentSize.v[Layout->AxisDirection] += Layout->Spacer.v[Layout->AxisDirection];
	}   //Parent->ContentSize.v[Layout->AxisDirection] += Layout->Spacer.v[Layout->AxisDirection];

    if( Options & UI_DrawText ) {
        u32 Len = CustomStrlen((const char*)Text);
        if( Object->Text.data == NULL ) {
            Object->Text = StringNew((const char*)Text, Len, Context->Allocator);
        } else if( Object->Type != UI_InputText ) {
            // If it is Input text we do not want to copy again the title text, as
            // it already stores input information from the user
            StringCpy(&Object->Text, (const char*)Text);
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
                if( LastSlash != -1 ) {
                    LastSlash += 1;
                }
                if( LastUnderScore != -1 ) {
                    LastUnderScore += 1;
                }
                if( LastSpace != -1 ) {
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
					Object->TextCursorIdx = 0;
                }

            } else if( Context->LastInput & Input_Backspace ) {
                //Object->Text.idx = Object->Text.idx > 0 ? Object->Text.idx - 1 : 0;
                StringErase(&Object->Text, Object->TextCursorIdx);
            }
        }

        Object->Pos  = Object->Rect.Pos;
        Object->Size = vec2{
            (f32)F_TextWidth(Object->Theme.Font, "H", 1) * Object->Text.idx,
            (f32)F_TextHeight(Object->Theme.Font)
        };

        if( Object->Option & UI_DrawIcon && !(Object->Option & UI_AlignCenter)) {
            Object->Pos.x += Object->Rect.Size.y + 5;
        }

		if (Object->Size.x > Object->Rect.Size.x) {

			// How much text is outside the render box
			//
			f32 Offset = Object->Size.x - Object->Rect.Size.x;
			// Average size of a character given the size of the string and the number of
			// characters is composed of
			//
			f32 C_Avg  = Object->Size.x / Object->Text.idx;

			u32 N_Chars = (u32) ceilf(Offset / C_Avg);

			if (N_Chars <= Object->Text.idx) {
				// @todo This would affect text boxes
				// Object->Text.idx -= N_Chars;
			}
		}

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
	spall_buffer_end(&spall_ctx, &spall_buffer, get_time_in_nanos() // timestamp in nanoseconds -- end of your timing block
					 );
    return Object;
}

fn_internal bool IsCursorOnRect(ui_context* Context, rect_2d Rect) {
    vec2 Cursor = Context->CursorPos;
    if( Cursor.x >= Rect.Pos.x && Cursor.x <= Rect.Pos.x + Rect.Size.x ) {
        if( Cursor.y >= Rect.Pos.y && Cursor.y <= Rect.Pos.y + Rect.Size.y ) {
            return true;
        }
    }
    return false;
}

fn_internal bool UI_CheckBoxInsideScrollView(ui_context* Context, ui_object* Object) {
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

fn_internal ui_input UI_ConsumeEvents(ui_context* Context, ui_object* Object) {
    if( (Object->Option & UI_Interact) && IsCursorOnRect(Context, Object->Rect) ) {
        if( UI_CheckBoxInsideScrollView(Context, Object)) {
            return Context->CursorAction | Input_CursorHover | Context->LastInput;
        }
    }

    return Input_None;
}

fn_internal ui_input UI_Button(ui_context* Context, const char* Title) {

    ui_object* Parent = Context->CurrentParent;

    ui_layout Layout = StackGetFront(&Context->Layouts);
    rect_2d Rect     = Layout.Size;
    Rect.Pos         = Vec2Add(Rect.Pos, Layout.ContentSize);
    Rect.Size        = Layout.BoxSize;

    ui_lay_opt Options = static_cast<ui_lay_opt>(UI_DrawRect | UI_DrawBorder | UI_DrawText | UI_AlignVertical | UI_AlignCenter | UI_Interact | UI_Select);

    UI_SetNextTheme( Context, Context->DefaultTheme.Button );
    ui_object* Button = UI_BuildObjectWithParent(Context, (const u8*)Title, (const u8*)Title, Rect, Options, Parent);

    ui_input Input = UI_ConsumeEvents(Context, Button);
    Button->LastInputSet = Input;
    //Button->Type = UI_ButtonType;

    if( Input & Input_LeftClickPress ) {
        Context->FocusObject = Button;
    } else if ( Input & Input_LeftClickRelease ) {
        Context->FocusObject = &UI_NULL_OBJECT;
    }

    if( Button->LastInputSet & Input_CursorHover ) {
		//rgba Background = HexToRGBA(0x0088FFFF);
		//Button->Theme.Background = RgbaToNorm(Background);
		Button->Theme.Background = Context->DefaultTheme.OnHoverBackground;
		Button->Theme.Foreground = Context->DefaultTheme.OnHoverForeground;
	} else if( Button == Context->FocusObject ) {
		Button->Theme.Background = Context->DefaultTheme.OnSuccessBackground;
		Button->Theme.Foreground = Context->DefaultTheme.OnSuccessForeground;
	}

    return Input;
}

fn_internal ui_input UI_Label(ui_context* Context, const char* text) {
    ui_object* Parent = Context->CurrentParent;

    ui_layout Layout = StackGetFront(&Context->Layouts);
    rect_2d Rect     = Layout.Size;
    Rect.Pos         = Vec2Add(Rect.Pos, Layout.ContentSize);
    Rect.Size        = Layout.BoxSize;

    ui_lay_opt Options = static_cast<ui_lay_opt>(UI_DrawText | UI_AlignVertical | UI_Interact | UI_Select | Layout.Option);
    UI_SetNextTheme( Context, Context->DefaultTheme.Label );
    ui_object* Label = UI_BuildObjectWithParent(Context, (const u8*)text, (const u8*)text, Rect, Options, Parent);

	StackPop(&Context->Themes);
    ui_input Input = UI_ConsumeEvents(Context, Label);
    Label->LastInputSet = Input;
    //Label->Type = UI_LabelType;

    return Input;
}

fn_internal ui_input UI_LabelWithKey(ui_context* Context, const char* Key, const char* text) {
    ui_object* Parent = Context->CurrentParent;

    ui_layout Layout = StackGetFront(&Context->Layouts);
    rect_2d Rect     = Layout.Size;
    Rect.Pos         = Vec2Add(Rect.Pos, Layout.ContentSize);
    Rect.Size        = Layout.BoxSize;

    ui_lay_opt Options = static_cast<ui_lay_opt>(UI_DrawText | UI_AlignVertical | UI_Interact | UI_Select);
    UI_SetNextTheme( Context, Context->DefaultTheme.Input );
    ui_object* Label = UI_BuildObjectWithParent(Context, (const u8*)Key, (const u8*)text, Rect, Options, Parent);
	StackPop(&Context->Themes);
    ui_input Input = UI_ConsumeEvents(Context, Label);
    Label->LastInputSet = Input;
    //Label->Type = UI_LabelType;

    return Input;
}

fn_internal ui_input UI_TextBox(ui_context* Context, const char* text) {
    ui_object* Parent = Context->CurrentParent;

    ui_layout Layout = StackGetFront(&Context->Layouts);
    rect_2d Rect     = Layout.Size;
    Rect.Pos         = Vec2Add(Rect.Pos, Layout.ContentSize);
    Rect.Size        = Layout.BoxSize;

    ui_lay_opt Options = static_cast<ui_lay_opt>(UI_DrawBorder | UI_DrawRect | UI_DrawText | UI_AlignVertical | UI_Interact | UI_Select);
    UI_SetNextTheme( Context, Context->DefaultTheme.Input );
    ui_object* TextBox = UI_BuildObjectWithParent(Context, (const u8*)text, (const u8*)text, Rect, Options, Parent);
	StackPop(&Context->Themes);
    ui_input Input = UI_ConsumeEvents(Context, TextBox);
    TextBox->LastInputSet = Input;
    TextBox->Type = UI_InputText;

	if (Input & Input_LeftClickPress) {

		Context->FocusObject = TextBox;
	}

	if( TextBox == Context->FocusObject ) {
        TextBox->LastInputSet |= Context->LastInput;
		Input |= Context->LastInput;
    }

	if ( Input & Input_KeyChar || Input & Input_KeyPressed ) {
		if (TextBox->Size.x > TextBox->Rect.Size.x) {
			// How much text is outside the render box
			//
			f32 Offset = TextBox->Size.x - TextBox->Rect.Size.x;
			// Average size of a character given the size of the string and the number of
			// characters is composed of
			//
			f32 C_Avg  = TextBox->Size.x / TextBox->Text.idx;

			u32 N_Chars = (u32) ceilf(Offset / C_Avg);

			if (TextBox->TextCursorIdx >= N_Chars && (Input & Input_Backspace || Input & Input_Left ) ) {
				TextBox->TextStartIdx = Max(0, TextBox->TextStartIdx - 1);
			} else if (TextBox->TextCursorIdx >= N_Chars) {
				TextBox->TextStartIdx = Min(TextBox->TextCursorIdx, TextBox->TextStartIdx + 1);
			}
		} else {
			TextBox->TextStartIdx = 0;
		}
	}

    if( TextBox->LastInputSet & Input_CursorHover ) {
		TextBox->Theme.Background.b += 0.2;
    }

    return TextBox->LastInputSet;
}


fn_internal ui_input UI_BeginTreeNode(ui_context* Context, const char* text) {
    ui_object* Parent = Context->CurrentParent;

    ui_layout Layout = StackGetFront(&Context->Layouts);
    rect_2d Rect     = Layout.Size;
    Rect.Pos         = Vec2Add(Rect.Pos, Layout.ContentSize);
    Rect.Size        = Layout.BoxSize;

    ui_lay_opt Options = static_cast<ui_lay_opt>(UI_DrawBorder | UI_DrawRect | UI_DrawText | UI_AlignVertical | UI_AlignCenter | UI_Interact | UI_Select);
    UI_SetNextTheme( Context, Context->DefaultTheme.Panel );
    ui_object* TreeNode = UI_BuildObjectWithParent(Context, (const u8*)text, (const u8*)text, Rect, Options, Parent);
	StackPop(&Context->Themes);
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
    }

	if ((Input & Input_LeftClickRelease) && Context->FocusObject == TreeNode) {
		Context->FocusObject = &UI_NULL_OBJECT;
	}

	UI_SetNextParent(Context, TreeNode);

    if( TreeNode->LastInputSet & Input_CursorHover ) {
        TreeNode->Theme.Background.b *= 1.8;
    }

    return TreeNode->LastInputSet;
}

fn_internal void UI_EndTreeNode(ui_context* Context) {
	UI_PopLastParent(Context);
}

fn_internal void UI_PushNextLayoutIcon(ui_context* Context, icon_type Type) {
    ui_layout* Layout = &StackGetFront(&Context->Layouts);
    Layout->NextIconType = Type;
}


fn_internal void UI_PopNextIcon(ui_context* Context) {
    ui_layout* Layout = &StackGetFront(&Context->Layouts);
    Layout->NextIconType = Icon_None;
}

fn_internal ui_input UI_SetIcon(ui_context* Context, const char* Name, icon_type Type) {
	ui_object* Parent = Context->CurrentParent;

    ui_layout Layout = StackGetFront(&Context->Layouts);
    rect_2d Rect     = Layout.Size;
    Rect.Pos         = Vec2Add(Rect.Pos, Layout.ContentSize);
    Rect.Size        = Vec2New(Layout.BoxSize.y, Layout.BoxSize.y);

	Layout.AxisDirection = 0;

	char KeyName[126];
	snprintf(KeyName, 126, "%s_%p", Name, Parent->Last);

    vec2 BoxSize = Layout.BoxSize;
    //UI_PushNextLayoutBoxSize(Context, Rect.Size);

	ui_lay_opt Options = static_cast<ui_lay_opt>(UI_DrawIcon | UI_HintSize);
	ui_object* Icon  = UI_BuildObjectWithParent(Context, (const u8*)KeyName, (const u8*)Name, Rect, Options, Parent);
	Icon->Type = static_cast<object_type>(Type);

	//UI_PushNextLayoutBoxSize(Context, BoxSize);

	Layout.AxisDirection = 1;

	switch (Type) {
		case Icon_ArrowDown: {

		} break;
		case Icon_File: {

		} break;
		case Icon_Folder: {

		} break;
		case Icon_Wave: {

		} break;
		case Icon_Download: {

		} break;
		case Icon_Details: {

		} break;
		case Icon_Send:	 {

		} break;
	}
}

fn_internal void UI_BeginScrollbarViewEx(ui_context* Context, vec2 Size) {

	spall_buffer_begin(&spall_ctx, &spall_buffer,
					   __FUNCTION__,             // name of your function
					   sizeof(__FUNCTION__) - 1, // name len minus the null terminator
					   get_time_in_nanos()      // timestamp in nanoseconds -- start of your timing block
					   );

    char name[] = "ScrollBarView";

    ui_layout *Layout = &StackGetFront(&Context->Layouts);

    ui_object* Parent = Context->CurrentParent;

    rect_2d Rect = Layout->Size;


    Rect.Pos.x = Rect.Pos.x + Rect.Size.x - 16;
	Rect.Pos.y += Layout->ContentSize.y;
	if (Size.x == 0 || Size.y == 0) {
		Rect.Size.x = 8;
		Rect.Size.y -= 2 * Layout->ContentSize.y;
	} else {
		Rect.Size = Size;
	}

	UI_DrawRect2D(Context, Rect, Context->DefaultTheme.Scrollbar.Background, Context->DefaultTheme.Scrollbar.Foreground, 1, 4);
    Parent->Last->Type = UI_ScrollbarType;

    Context->CurrentParent = Parent->Last;
	spall_buffer_end(&spall_ctx, &spall_buffer, get_time_in_nanos() // timestamp in nanoseconds -- end of your timing block
					 );
}

fn_internal void UI_EndScrollbarView(ui_context* Context) {
	spall_buffer_begin(&spall_ctx, &spall_buffer,
					   __FUNCTION__,             // name of your function
					   sizeof(__FUNCTION__) - 1, // name len minus the null terminator
					   get_time_in_nanos()      // timestamp in nanoseconds -- start of your timing block
					   );
    ui_object* Scrollbar   = Context->CurrentParent;
    float ContentHeight    = Scrollbar->ContentSize.y;
    float ViewportContent  = Scrollbar->Rect.Size.y;
    rect_2d ScrollableSize = Scrollbar->Rect;
    ContentHeight          = Max(1.0f, ContentHeight);
    ScrollableSize.Size.y  = (ViewportContent / ContentHeight);
	f32 ScrollableDistance = ContentHeight - ViewportContent;
	f32 TrackScrollableDistance = ViewportContent - ScrollableSize.Size.y;
	f32 ScrollRatio = 6;
    ScrollableSize.Size.y  = Max(ScrollableSize.Size.y, 20.0f);

    char buf[] = "EndOfScroll";

    assert(!StackIsEmpty(&Context->Layouts));

    ui_layout* Layout = &StackGetFront(&Context->Layouts);

    vec2 Padding = Layout->Padding;
    Layout->Padding = vec2{0, 0};

	vec2 BoxSize = Layout->BoxSize;
	UI_PushNextLayoutBoxSize(Context, ScrollableSize.Size);

    UI_SetNextTheme( Context, Context->DefaultTheme.Scrollbar );
    ui_object* Scrollable = UI_BuildObjectWithParent(Context, (const u8*)buf, NULL, ScrollableSize, static_cast<ui_lay_opt>(UI_DrawRect | UI_Select | UI_Interact), Scrollbar);
    StackPop(&Context->Themes);
	Scrollable->Type = UI_ScrollbarTypeButton;
    Scrollable->Theme = Context->DefaultTheme.Scrollbar;
    Layout->Padding = Padding;

    Layout->ContentSize.y -= Max(0, ContentHeight - ViewportContent + Scrollable->Rect.Size.y);

    vec2 VerticalDelta = Vec2Zero();
    if( Context->FocusObject == Scrollable ) {
        VerticalDelta = vec2{0, Context->CursorDelta.y};
    }

	if (Scrollable->Rect.Pos.y + Scrollable->Rect.Size.y > Scrollbar->Rect.Pos.y + Scrollbar->Rect.Size.y) {
		f32 OldPos = Scrollable->Rect.Pos.y;
		f32 FarOff = (Scrollable->Rect.Pos.y + Scrollable->Rect.Size.y) - (Scrollbar->Rect.Pos.y + Scrollbar->Rect.Size.y);
		Scrollable->Rect.Pos.y -= FarOff;
	} else {
		vec2 LastDelta = Scrollable->LastDelta;
		Scrollable->LastDelta = Vec2Add(Scrollable->LastDelta, VerticalDelta);
		Scrollable->ScrollRatio = ScrollRatio;
		Scrollable->Rect.Pos.y += Scrollable->LastDelta.y * ScrollRatio;
		Scrollable->Rect.Pos.y = Max(Scrollbar->Rect.Pos.y, Scrollable->Rect.Pos.y);
		Scrollable->Rect.Pos.y = Min(Scrollable->Rect.Pos.y, Scrollbar->Rect.Pos.y + ViewportContent);
	}

    if(Scrollable->Rect.Pos.y == Scrollbar->Rect.Pos.y) {
        Scrollable->LastDelta = Vec2Zero();
    } else if( Scrollable->Rect.Pos.y == Scrollbar->Rect.Pos.y + Scrollbar->Rect.Size.y ) {
		f32 OldPos = Scrollable->Rect.Pos.y;
		f32 FarOff = (Scrollable->Rect.Pos.y + Scrollable->Rect.Size.y) - (Scrollbar->Rect.Pos.y + Scrollbar->Rect.Size.y);
		Scrollable->Rect.Pos.y -= FarOff;
		//Scrollable->LastDelta = Vec2Sub(Scrollable->LastDelta, VerticalDelta);
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
	UI_PushNextLayoutBoxSize(Context, BoxSize);

	spall_buffer_end(&spall_ctx, &spall_buffer, get_time_in_nanos());
}

fn_internal void UI_SetNextParent(ui_context* Context, ui_object* Object) {
    Context->CurrentParent = Object;
}

fn_internal void UI_PopLastParent(ui_context* Context) {
    Context->CurrentParent = Context->CurrentParent->Parent;
}

fn_internal void UI_PushNextLayout(ui_context* Context, rect_2d Rect, ui_lay_opt Options) {
    StackPush(&Context->Layouts, (ui_layout){});
    ui_layout* Layout = &StackGetFront(&Context->Layouts);

    Layout->Size          = Rect;
    Layout->ContentSize   = Vec2Zero();
    Layout->Option        = Options;
    Layout->AxisDirection = 1; // y axis (goes vertically)
    Layout->BoxSize       = Vec2Zero();
	Layout->Padding = Vec2Zero();
	Layout->Spacer = Vec2Zero();
}


fn_internal void UI_PushNextLayoutRow(ui_context* Context, int N_Rows, const f32* Rows ) {

    assert(!StackIsEmpty(&Context->Layouts));

    ui_layout* Layout = &StackGetFront(&Context->Layouts);

    Layout->N_Rows   = N_Rows;
    Layout->RowSizes = Rows;
}

fn_internal void UI_PushNextLayoutColumn(ui_context* Context, int N_Columns, const f32* Columns ) {
    assert(!StackIsEmpty(&Context->Layouts));

    ui_layout* Layout = &StackGetFront(&Context->Layouts);

    Layout->N_Columns   = N_Columns;
    Layout->ColumnSizes = Columns;
}

fn_internal void UI_PushNextLayoutBoxSize(ui_context* Context, vec2 BoxSize) {
	assert(!StackIsEmpty(&Context->Layouts));

    ui_layout* Layout = &StackGetFront(&Context->Layouts);

	Layout->BoxSize = BoxSize;
}

fn_internal void
	UI_PushNextLayoutPadding(ui_context* Context, vec2 Padding) {
    assert(!StackIsEmpty(&Context->Layouts));

    ui_layout* Layout = &StackGetFront(&Context->Layouts);
    Layout->Padding    = Padding;
}

fn_internal void
	UI_PushNextLayoutOption(ui_context* Context, ui_lay_opt Options) {
    assert(!StackIsEmpty(&Context->Layouts));

    ui_layout* Layout = &StackGetFront(&Context->Layouts);
    Layout->Option    = Options;
}

fn_internal void UI_PushLayoutSpacer(ui_context* Context, vec2 Spacer) {
	assert(!StackIsEmpty(&Context->Layouts));

    ui_layout* Layout = &StackGetFront(&Context->Layouts);
    Layout->Spacer    = Spacer;
}

fn_internal void UI_Spacer(ui_context* Context, vec2 Spacer) {
	assert(!StackIsEmpty(&Context->Layouts));

    ui_layout* Layout = &StackGetFront(&Context->Layouts);

	Layout->ContentSize = Vec2Add(Layout->ContentSize, Spacer);
	Context->CurrentParent->ContentSize = Vec2Add(Context->CurrentParent->ContentSize, Spacer);
}

fn_internal void
	UI_PushNextLayoutDisableOption(ui_context* Context, ui_lay_opt Options) {
    assert(!StackIsEmpty(&Context->Layouts));

    ui_layout* Layout = &StackGetFront(&Context->Layouts);
    Layout->Option    = static_cast<ui_lay_opt>(Layout->Option ^ Options);
}

fn_internal vec2
	UI_UpdateObjectSize(ui_context* Context, ui_object* Object) {
	if (UI_ConsumeEvents(Context, Object) & Input_LeftClickPress) {
		Context->FocusObject = Object;
	} else if (Context->LastInput & Input_LeftClickRelease && Object == Context->FocusObject) {
		Context->FocusObject = &UI_NULL_OBJECT;
		Context->IsOnDrag = false;
		Context->IsOnResize = false;
	} else if (Object == Context->FocusObject) {
		vec2 CornerSize = (vec2){20, 20};
        rect_2d ResizeRect = (rect_2d){
            Vec2Add(Object->Rect.Pos, Vec2Sub(Object->Rect.Size, CornerSize)),
            CornerSize
        };
        if( Object->Option & UI_Resize && !Context->IsOnDrag)  {
            if( IsCursorOnRect(Context, ResizeRect) || Context->IsOnResize ) {
                Object->Rect.Size = Vec2Add(Object->Rect.Size, Context->CursorDelta);
				Context->IsOnResize = true;
            }
        }
        if( Object->Option & UI_Drag && !Context->IsOnResize ) {
            Object->Rect.Pos = Vec2Add(Object->Rect.Pos, Context->CursorDelta);
			if (IsCursorOnRect(Context, Object->Rect) || Context->IsOnDrag) {
				Context->IsOnDrag = true;
			}
        }

		return Context->CursorDelta;
	}

	return Vec2Zero();
}

fn_internal void
	UI_SetNextTheme(ui_context* Context, object_theme Theme) {
    StackPush(&Context->Themes, Theme);
}

fn_internal void
	UI_PushNextFont(ui_context* Context, FontCache* Font) {
    object_theme* Theme = &StackGetFront(&Context->Themes);
    Theme->Font = Font;
}

fn_internal void
	UI_PopTheme(ui_context* Context) {
    StackPop(&Context->Themes);
}

fn_internal void UI_BeginColumn(ui_context* Context) {
    assert(!StackIsEmpty(&Context->Layouts));

    ui_layout* Layout = &StackGetFront(&Context->Layouts);
    Layout->AxisDirection = 0;
    Layout->CurrentColumn = 0;
}

fn_internal void UI_SetNextCol(ui_context* Context) {
    assert(!StackIsEmpty(&Context->Layouts));

    ui_layout* Layout = &StackGetFront(&Context->Layouts);
	if (Layout->CurrentColumn < Layout->N_Columns) {
		Layout->ContentSize.x += Layout->ColumnSizes[Layout->CurrentColumn] + Layout->Padding.x;
		Context->CurrentParent->ContentSize = Layout->ContentSize;
		Layout->CurrentColumn += 1;
	}
}

fn_internal void UI_EndColumn(ui_context* Context) {
    assert(!StackIsEmpty(&Context->Layouts));
    ui_layout* Layout = &StackGetFront(&Context->Layouts);
    Layout->AxisDirection = 1;
	for (i32 it = 0; it < Layout->N_Columns; it += 1){
		Layout->ContentSize.x -= Layout->ColumnSizes[it] - Layout->Padding.x;
	}
    Layout->ContentSize.y += Layout->BoxSize.y;
}

fn_internal void UI_Column(ui_context* Context) {
    assert(!StackIsEmpty(&Context->Layouts));
    ui_layout* Layout = &StackGetFront(&Context->Layouts);
    Layout->AxisDirection = 0;
}

fn_internal void UI_Row(ui_context* Context) {
    assert(!StackIsEmpty(&Context->Layouts));
    ui_layout* Layout = &StackGetFront(&Context->Layouts);
    Layout->AxisDirection = 1;
}

fn_internal void UI_NextRow(ui_context* Context) {
    assert(!StackIsEmpty(&Context->Layouts));
    ui_layout* Layout = &StackGetFront(&Context->Layouts);
    Layout->AxisDirection = 1;
    Layout->ContentSize.x = 0;
}

fn_internal void UI_BeginRow(ui_context* Context) {
	assert(!StackIsEmpty(&Context->Layouts));

    ui_layout* Layout = &StackGetFront(&Context->Layouts);
    Layout->AxisDirection = 1;
    Layout->CurrentRow = 0;
	Layout->N_Rows = 0;
	if (Layout->CurrentColumn > 0) {
		Layout->ContentSize.x -= Layout->ColumnSizes[Layout->CurrentColumn - 1] - Layout->Padding.x;
		Layout->ContentSize.y += Context->CurrentParent->Last->Rect.Size.y;
		Context->CurrentParent->ContentSize = Layout->ContentSize;
	}
}
fn_internal void UI_EndRow(ui_context* Context)
{
	assert(!StackIsEmpty(&Context->Layouts));
    ui_layout* Layout = &StackGetFront(&Context->Layouts);
    Layout->AxisDirection = 0;
    //Layout->ContentSize.x = 0;
    Layout->ContentSize.y = 0;
}

fn_internal void UI_Divisor(ui_context* Context, f32 Width) {
	assert(!StackIsEmpty(&Context->Layouts));
    ui_layout* Layout = &StackGetFront(&Context->Layouts);

	ui_object* Parent = Context->CurrentParent;
	vec2 ContentSize = Layout->ContentSize;

	rect_2d Rect = NewRect2D(Parent->Rect.Pos.x + ContentSize.x + Layout->Padding.x, Parent->Rect.Pos.y + ContentSize.y, Parent->Rect.Size.x - ContentSize.x - 2 * Layout->Padding.x, Width);

	UI_DrawRect2D(Context, Rect, Context->DefaultTheme.Panel.Background, Parent->Theme.Border, 1, 4);
}

fn_internal ui_input UI_LastEvent(ui_context* Context, api_window* Window) {
    #ifndef NDEBUG
	spall_buffer_begin(&spall_ctx, &spall_buffer,
					   __FUNCTION__,             // name of your function
					   sizeof(__FUNCTION__) - 1, // name len minus the null terminator
					   get_time_in_nanos()      // timestamp in nanoseconds -- start of your timing block
					   );
	#endif
	ui_input Input = Input_None;
    u32 window_width  = Window->Width;
    u32 window_height = Window->Height;
    Input = GetNextEvent(Window);

    {
        vec2 Mouse = GetMousePosition(Window);
        vec2 LastCursor = Context->CursorPos;
        Context->CursorPos = Mouse;

        while( Context->LastCursorPos.x == Mouse.x && Context->LastCursorPos.y == Mouse.y && (Input & Input_None) ) {
            Mouse = GetMousePosition(Window);
            Input = GetNextEvent(Window);
        }

        Context->LastCursorPos = LastCursor;
        Context->CursorDelta = Vec2Sub(Context->CursorPos, LastCursor);
    }

    if( Input & FrameBufferResized ) {
    }
    if( Input & ClipboardPaste ) {
        StringAppend(&Context->TextInput, (const char*)&Window->ClipboardContent);
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
		//Context->FocusObject->TextCursorIdx -= 1;
	}
    if( Input & Input_Right ) {
        Context->FocusObject->TextCursorIdx = Min(Context->FocusObject->Text.idx, Context->FocusObject->TextCursorIdx + 1);
	}
    if( Input & DeleteWord ) {

    } else if( Input & Input_Backspace ) {
        Context->FocusObject->TextCursorIdx = Max(0, Context->FocusObject->TextCursorIdx - 1 );
        Context->TextInput.idx = Max(0, Context->TextInput.idx - 1);
    }
    if( Input & ClipboardPaste ) {
        StringAppend(&Context->TextInput, GetClipboard(Window));
    }
    if( Input & Input_KeyChar ) {
        Context->KeyPressed |= Window->KeyPressed;
        Context->KeyDown    |= Window->KeyPressed;
        char buffer[2] = {0};
        buffer[0] = Window->KeyPressed;
        StringAppend(&Context->TextInput, buffer);
    }

    if( Input & Input_F1 ) {
    }

    Context->LastInput = Input;

	#ifndef NDEBUG
	spall_buffer_end(&spall_ctx, &spall_buffer, get_time_in_nanos() // timestamp in nanoseconds -- end of your timing block
					 );
	#endif

    return Input;
}

#endif
>>>>>>> 154fcbcc98d4260859f45c905ed533ba04da06cf
