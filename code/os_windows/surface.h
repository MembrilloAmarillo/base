<<<<<<< HEAD
#ifndef _SURFACE_WIN32_H_
#define _SURFACE_WIN32_H_

typedef struct api_window api_window;
struct api_window {
    F64          Width;
    F64          Height;
    VkSurfaceKHR Surface;
    HINSTANCE    Instance;  
    HWND         Win;
    HDC          Dc;
    char         ClipboardContent[256];
    char         KeyPressed;
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

#endif 

#ifdef SURFACE_IMPL

#include <windows.h>
#include <stdio.h>

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_SIZE: {
            // Handle resize if needed
            break;
        }
        case WM_KEYDOWN: {
            break;
        }
        // Add other message handlers as needed
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}


fn_internal api_window 
SurfaceCreateWindow(F64 w, F64 h) {
    api_window app = {0};
    app.Width = w;
    app.Height = h;

    // 1. Register window class
    HINSTANCE hInstance = GetModuleHandle(NULL);
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "GFXContextClass";
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassEx(&wc)) {
        fprintf(stderr, "Failed to register window class\n");
        exit(1);
    }

    // 2. Create window
    HWND hwnd = CreateWindowEx(
        0,                              // Optional window styles
        "GFXContextClass",              // Window class
        "GFX Context",                  // Window text
        WS_OVERLAPPEDWINDOW,            // Window style
        CW_USEDEFAULT, CW_USEDEFAULT,   // Position (x, y)
        (int)w, (int)h,                 // Size (width, height)
        NULL,                           // Parent window
        NULL,                           // Menu
        hInstance,                      // Instance handle
        NULL                            // Additional application data
    );

    if (!hwnd) {
        fprintf(stderr, "Failed to create window\n");
        exit(1);
    }

    // 3. Store Win32 handles
    app.Instance = hInstance;
    app.Win = hwnd;
    app.Dc = GetDC(hwnd); // Device context for drawing

    // 4. Show and update window
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    return app;
}

fn_internal vec2 
SurfaceGetWindowSize(api_window* window) {
    RECT r;
    vec2 size = Vec2Zero();
    if (GetClientRect(window->Win, &r)) {
        size.x = r.right  - r.left;
        size.y = r.bottom - r.top;
    } 
    return size;
}


=======
#ifndef _SURFACE_WIN32_H_
#define _SURFACE_WIN32_H_

typedef struct api_window api_window;
struct api_window {
    F64          Width;
    F64          Height;
    VkSurfaceKHR Surface;
    HINSTANCE    Instance;  
    HWND         Win;
    HDC          Dc;
    char         ClipboardContent[256];
    char         KeyPressed;
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

#endif 

#ifdef SURFACE_IMPL

#include <windows.h>
#include <stdio.h>

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_SIZE: {
            // Handle resize if needed
            break;
        }
        case WM_KEYDOWN: {
            break;
        }
        // Add other message handlers as needed
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}


fn_internal api_window 
SurfaceCreateWindow(F64 w, F64 h) {
    api_window app = {0};
    app.Width = w;
    app.Height = h;

    // 1. Register window class
    HINSTANCE hInstance = GetModuleHandle(NULL);
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "GFXContextClass";
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassEx(&wc)) {
        fprintf(stderr, "Failed to register window class\n");
        exit(1);
    }

    // 2. Create window
    HWND hwnd = CreateWindowEx(
        0,                              // Optional window styles
        "GFXContextClass",              // Window class
        "GFX Context",                  // Window text
        WS_OVERLAPPEDWINDOW,            // Window style
        CW_USEDEFAULT, CW_USEDEFAULT,   // Position (x, y)
        (int)w, (int)h,                 // Size (width, height)
        NULL,                           // Parent window
        NULL,                           // Menu
        hInstance,                      // Instance handle
        NULL                            // Additional application data
    );

    if (!hwnd) {
        fprintf(stderr, "Failed to create window\n");
        exit(1);
    }

    // 3. Store Win32 handles
    app.Instance = hInstance;
    app.Win = hwnd;
    app.Dc = GetDC(hwnd); // Device context for drawing

    // 4. Show and update window
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    return app;
}

fn_internal vec2 
SurfaceGetWindowSize(api_window* window) {
    RECT r;
    vec2 size = Vec2Zero();
    if (GetClientRect(window->Win, &r)) {
        size.x = r.right  - r.left;
        size.y = r.bottom - r.top;
    } 
    return size;
}


>>>>>>> 154fcbcc98d4260859f45c905ed533ba04da06cf
#endif