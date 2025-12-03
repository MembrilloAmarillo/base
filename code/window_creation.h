#ifndef _WINDOW_CREATION_H_
#define _WINDOW_CREATION_H_

#ifdef SDL_USAGE

#include "strings.h"

#define ui_input u64

#define Input_None               ((ui_input)1 << 0)
#define Input_RightClickPress    ((ui_input)1 << 1)
#define Input_RightClickRelease  ((ui_input)1 << 2)
#define Input_LeftClickPress     ((ui_input)1 << 3)
#define Input_LeftClickRelease   ((ui_input)1 << 4)
#define Input_DoubleLeftClick    ((ui_input)1 << 5)
#define Input_CursorHover        ((ui_input)1 << 6)
#define Input_Backspace          ((ui_input)1 << 7)
#define Input_Ctrol              ((ui_input)1 << 8)
#define Input_Shift              ((ui_input)1 << 9)
#define Input_Alt                ((ui_input)1 << 10)
#define Input_Return             ((ui_input)1 << 11)
#define Input_MiddleMouse        ((ui_input)1 << 12)
#define Input_MiddleMouseUp      ((ui_input)1 << 13)
#define Input_MiddleMouseDown    ((ui_input)1 << 14)
#define Input_F1                 ((ui_input)1 << 15)
#define Input_F2                 ((ui_input)1 << 16)
#define Input_F3                 ((ui_input)1 << 17)
#define Input_F4                 ((ui_input)1 << 18)
#define Input_F5                 ((ui_input)1 << 19)
#define Input_Left               ((ui_input)1 << 20)
#define Input_Right              ((ui_input)1 << 21)
#define Input_Up                 ((ui_input)1 << 22)
#define Input_Down               ((ui_input)1 << 23)
#define Input_Esc                ((ui_input)1 << 24)
#define StopUI                   ((ui_input)1 << 25)
#define ActiveObject             ((ui_input)1 << 26)
#define DeleteWord               ((ui_input)1 << 27)
#define FrameBufferResized       ((ui_input)1 << 28)
#define ClipboardPaste           ((ui_input)1 << 29)
#define Input_KeyPressed         ((ui_input)1 << 30)
#define Input_KeyRelease         ((ui_input)1 << 31)
#define Input_KeyChar            ((ui_input)1 << 32)


typedef struct api_window api_window;
struct api_window {
	F64          Width;
	F64          Height;
	VkSurfaceKHR Surface;
	SDL_Window*  Win;
	vec2         MousePosition;
	char         ClipboardContent[256];
	char         KeyPressed;
};

fn_internal api_window SurfaceCreateWindow(F64 w, F64 h) {
	api_window Window = {};
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		fprintf(stderr, "[SDL ERROR] Failed to initialize SDL: %s\n", SDL_GetError());
		return api_window{};
	}

	if (!SDL_Vulkan_LoadLibrary(nullptr)) {
		fprintf(stderr, "[ERROR] Failed to load Vulkan library: %s\n", SDL_GetError());
		return api_window{};
	}

	fprintf(stdout, "[LOG] Creating surface with SDL\n");

	Window.Width  = w;
	Window.Height = h;
	Window.Win = SDL_CreateWindow(
		"GFX Context",
		w, h,
		SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
	);

	SDL_SetWindowResizable(Window.Win, true);

	if( Window.Win == NULL ) {
		fprintf( stderr, "[SDL ERROR] Failed to create window: %s", SDL_GetError());
		exit(1);
		//return Window;
	}

	return Window;
}

fn_internal vec2 SurfaceGetWindowSize(api_window* Window) {
	int w, h;
	SDL_GetWindowSize(Window->Win, &w, &h);
	//SDL_Vulkan_GetDrawableSize(Window->Win, &w, &h);
	fprintf(stdout, "[INFO] New window size: %d %d\n", w, h);
	return Vec2New(w, h);
}

fn_internal vec2 GetMousePosition(api_window* window) {
	f32 w, h;
	SDL_GetMouseState(&w, &h);

	return Vec2New(w, h);
}

// ------------------------------------------------------
// SDL3 Event Processing (equivalent semantics)
// ------------------------------------------------------
fn_internal ui_input GetNextEvent(api_window* Window)
{
	ui_input Input = 0;
	SDL_Event ev;

	int window_width  = Window->Width;
	int window_height = Window->Height;

	while (SDL_PollEvent(&ev)) {

		switch (ev.type) {

			// --------------------------------------------------
			// Window Resize → FrameBufferResized
			// --------------------------------------------------
			case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
				int w = ev.window.data1;
				int h = ev.window.data2;

				printf("[LOG] Window resize\n");

				if (w != window_width || h != window_height)
					Input |= FrameBufferResized;

				break;
			}

			// --------------------------------------------------
			// Mouse Motion
			// --------------------------------------------------
			case SDL_EVENT_MOUSE_MOTION: {
				Window->MousePosition.x = (float)ev.motion.x;
				Window->MousePosition.y = (float)ev.motion.y;
				break;
			}

			// --------------------------------------------------
			// Mouse Buttons
			// --------------------------------------------------
			case SDL_EVENT_MOUSE_BUTTON_DOWN: {
				if (ev.button.button == SDL_BUTTON_LEFT)
					Input |= Input_LeftClickPress;
				else if (ev.button.button == SDL_BUTTON_RIGHT)
					Input |= Input_RightClickPress;
				else if (ev.button.button == SDL_BUTTON_MIDDLE)
					Input |= Input_MiddleMouse;

				break;
			}

			case SDL_EVENT_MOUSE_BUTTON_UP: {
				if (ev.button.button == SDL_BUTTON_LEFT)
					Input |= Input_LeftClickRelease;
				else if (ev.button.button == SDL_BUTTON_RIGHT)
					Input |= Input_RightClickRelease;
				else if (ev.button.button == SDL_BUTTON_MIDDLE)
					Input |= Input_MiddleMouse;

				break;
			}

			// --------------------------------------------------
			// Scroll Wheel → keep X11 semantics
			// Up = Button4 → Input_MiddleMouseUp
			// Down = Button5 → Input_MiddleMouseDown
			// --------------------------------------------------
			case SDL_EVENT_MOUSE_WHEEL: {
				if (ev.wheel.y > 0)
					Input |= Input_MiddleMouseUp;
				else if (ev.wheel.y < 0)
					Input |= Input_MiddleMouseDown;
				break;
			}

			// --------------------------------------------------
			// Text Input (UTF-8)
			// --------------------------------------------------
			case SDL_EVENT_TEXT_INPUT: {
				const char *c = ev.text.text;
				if (c && c[0] >= ' ' && c[0] <= '~') {
					Input |= Input_KeyChar;
					Window->KeyPressed = c[0];
				}
				break;
			}

			// --------------------------------------------------
			// Key Down
			// --------------------------------------------------
			case SDL_EVENT_KEY_DOWN: {
				Input |= Input_KeyPressed;

				SDL_Keycode key = ev.key.key;

				switch (key) {
					case SDLK_BACKSPACE: Input |= Input_Backspace; break;
					case SDLK_RETURN:    Input |= Input_Return;    break;
					case SDLK_LSHIFT:
					case SDLK_RSHIFT:    Input |= Input_Shift;     break;
					case SDLK_LCTRL:
					case SDLK_RCTRL:     Input |= Input_Ctrol;     break;
					case SDLK_LALT:
					case SDLK_RALT:      Input |= Input_Alt;       break;

					case SDLK_F1:        Input |= Input_F1;        break;
					case SDLK_F2:        Input |= Input_F2;        break;
					case SDLK_F3:        Input |= Input_F3;        break;
					case SDLK_F4:        Input |= Input_F4;        break;
					case SDLK_F5:        Input |= Input_F5;        break;

					case SDLK_LEFT:      Input |= Input_Left;      break;
					case SDLK_RIGHT:     Input |= Input_Right;     break;
					case SDLK_UP:        Input |= Input_Up;        break;
					case SDLK_DOWN:      Input |= Input_Down;      break;

					case SDLK_ESCAPE:
						Input = StopUI;
						return Input;
				}

				// Ctrl+V → request clipboard
				if ((ev.key.mod & SDL_KMOD_CTRL) &&
					(key == SDL_SCANCODE_V))
				{
					char *clip = SDL_GetClipboardText();
					if (clip) {
						memset(Window->ClipboardContent, 0, 256);
						memcpy(Window->ClipboardContent,
							   clip,
							   CustomStrlen(clip));
						SDL_free(clip);
					}
					Input |= ClipboardPaste;
				}

				// Ctrl + Backspace → DeleteWord
				if (key == SDLK_BACKSPACE &&
					(ev.key.mod & SDL_KMOD_CTRL))
				{
					Input |= DeleteWord;
				}

				break;
			}

			// --------------------------------------------------
			// Key Up
			// --------------------------------------------------
			case SDL_EVENT_KEY_UP: {
				Input |= Input_KeyRelease;
				break;
			}

			// --------------------------------------------------
			// Window close
			// --------------------------------------------------
			case SDL_EVENT_QUIT: {
				Input = StopUI;
				return Input;
			}

			default:
				break;
		}
	}

	if (Input == 0)
		Input = Input_None;

	return Input;
}


// ------------------------------------------------------
// Clipboard Read
// ------------------------------------------------------
fn_internal char*
    GetClipboard(api_window* Window)
{
	return Window->ClipboardContent;
}


// ------------------------------------------------------
// DPI retrieval (SDL3)
// ------------------------------------------------------
fn_internal int
    GetScreenDPI(api_window* Window)
{
	return (int)SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
}
#else
	#ifdef __linux__
		#include "os_linux/surface.h"
		#include "os_linux/events.h"
	#elif _WIN32
		#include "os_windows/surface.h"
		#include "os_windows/events.h"
	#endif
#endif

#endif //_WINDOW_CREATION_H_

#ifdef WINDOW_CREATION_IMPL

	#ifndef SDL_USAGE
		#define EVENTS_IMPL
		#define SURFACE_IMPL
		#ifdef __linux__
			#include "os_linux/surface.h"
			#include "os_linux/events.h"
		#elif _WIN32
			#include "os_windows/surface.h"
			#include "os_windows/events.h"
		#endif
	#endif
#endif
