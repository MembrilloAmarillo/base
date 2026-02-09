#ifndef _SURFACE_LINUX_H_
#define _SURFACE_LINUX_H_

#include <X11/Xlib.h>
#include <X11/keysymdef.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>

typedef struct api_clipboard api_clipboard;
struct api_clipboard {
    Atom Clipboard;
    Atom Utf8;
    Atom Paste;
};

typedef struct api_window api_window;
struct api_window {
    F64          Width;
    F64          Height;
    VkSurfaceKHR Surface;
    Display     *Dpy;
    Window       Win;
    XIC          xic;
    api_clipboard  Clipboard;
    vec2           MousePosition;
    char           ClipboardContent[256];
    char           KeyPressed;
};

/** \brief Creates a new window with specified dimensions
	*
	*  Initializes and returns a window handle with the requested width and height.
	*  The window is configured for Vulkan rendering and appropriate event handling.
	*
	*  \param w Width of the window in pixels
	*  \param h Height of the window in pixels
	*  \return Handle to the created window
	*/
fn_internal api_window SurfaceCreateWindow(F64 w, F64 h);

fn_internal vec2 SurfaceGetWindowSize(api_window* window);

#endif // _SURFACE_LINUX_H_


#ifdef SURFACE_IMPL


///////////////////////////////////////////////////////////////////////////////
//
fn_internal api_window
	SurfaceCreateWindow(F64 w, F64 h) {
    api_window app;

    app.Dpy = XOpenDisplay(NULL);
    if (!app.Dpy) { fprintf(stderr, "Cannot open display\n"); exit(1); }

    const int screen = DefaultScreen(app.Dpy);
    app.Clipboard.Clipboard = XInternAtom(app.Dpy, "CLIPBOARD", false);
    app.Clipboard.Utf8      = XInternAtom(app.Dpy, "UTF8_STRING", false);
    app.Clipboard.Paste     = XInternAtom(app.Dpy, "X_PASTE_PROPERTY", False);

    Atom wm_delete_window = XInternAtom(app.Dpy, "WM_DELETE_WINDOW", False);

    Window win = XCreateSimpleWindow(
		app.Dpy, RootWindow(app.Dpy, screen),
		0, 0, w, h, 0,
		BlackPixel(app.Dpy, screen),
		WhitePixel(app.Dpy, screen)
	);

    XSelectInput(app.Dpy, win,
                 ExposureMask     |
                 PointerMotionMask|
                 ButtonPressMask  |
                 ButtonReleaseMask|
                 KeyPressMask     |
                 KeyReleaseMask   |
                 FocusChangeMask
                 );

    app.Width = w;
    app.Height = h;
    XMapWindow(app.Dpy, win);
    XStoreName(app.Dpy, win, "GFX Context");
    XFlush(app.Dpy);

    app.Win = win;

    XIM xim = XOpenIM(app.Dpy, NULL, NULL, NULL);
    if (!xim) {
        /* Fallback: no IME support */
        fprintf(stderr, "[ERROR] No IME Support\n");
    }

    /* 4. Create an XIC for each window that needs text input */
    XIC xic = XCreateIC(xim,
                        XNInputStyle,   XIMPreeditNothing | XIMStatusNothing,
                        XNClientWindow, win,
                        XNFocusWindow,  win,
                        NULL);

    app.xic = xic;

    XSetWMProtocols(app.Dpy, app.Win, &wm_delete_window, 1);

    return app;
}

fn_internal vec2
	SurfaceGetWindowSize(api_window* window) {
    XWindowAttributes attr;

    XGetWindowAttributes(window->Dpy, window->Win, &attr);

    return (vec2){(float)attr.width, (float)attr.height};
}

#endif