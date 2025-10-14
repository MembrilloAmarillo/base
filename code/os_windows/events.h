#ifndef _EVENTS_WINDOWS_H_
#define _EVENTS_WINDOWS_H_

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
#define Input_ESC                ((ui_input)1 << 24)
#define StopUI                   ((ui_input)1 << 25)
#define ActiveObject             ((ui_input)1 << 26)
#define DeleteWord               ((ui_input)1 << 27)
#define FrameBufferResized       ((ui_input)1 << 28)
#define ClipboardPaste           ((ui_input)1 << 29)
#define Input_KeyPressed         ((ui_input)1 << 30)
#define Input_KeyRelease         ((ui_input)1 << 31)
#define Input_KeyChar            ((ui_input)1 << 32)

internal vec2 GetMousePosition(api_window* window);

internal ui_input GetNextEvent(api_window* window);

internal char* GetClipboard(api_window* window);

internal ui_input ProcessMessage(api_window* window, MSG* msg);

#endif 

#ifdef EVENTS_IMPL

internal vec2
GetMousePosition(api_window* window) {
    POINT p;
    GetCursorPos(&p);
    ScreenToClient(window->Win, &p);
    return (vec2){.x = (f32)p.x, .y = (f32)p.y};
}

// This function is called every frame in your main loop.
internal char*
GetClipboard(api_window* window) {
    if (OpenClipboard(window->Win)) {
        HGLOBAL hMem = GetClipboardData(CF_UNICODETEXT);
        if (hMem) {
            wchar_t* wstr = (wchar_t*)GlobalLock(hMem);
            if (wstr) {
                // Convert from UTF-16 (wchar_t) to UTF-8 (char*)
                int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
                if (size_needed > 0) {
                    char* utf8_str = (char*)malloc(size_needed);
                    if (utf8_str) {
                        WideCharToMultiByte(CP_UTF8, 0, wstr, -1, utf8_str, size_needed, NULL, NULL);
                        // Append the pasted text to your UI context's text buffer
                        memset(window->ClipboardContent, 0, 256);
                        memcpy(window->ClipboardContent, utf8_str, UCF_Strlen(utf8_str));
                        free(utf8_str);
                    }
                }
            }
            GlobalUnlock(hMem);
        }
        CloseClipboard();
    }

    return window->ClipboardContent;
}


internal ui_input
GetNextEvent(api_window* window) {
    ui_input Input = 0;
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        Input |= ProcessMessage(window, &msg);
        // Additional processing if needed
    }
    return Input;
}

internal ui_input
ProcessMessage(api_window* window, MSG* msg) {
    ui_input InputState = 0;
    switch (msg->message) {
        case WM_MOUSEMOVE:
            break;
        case WM_LBUTTONDOWN:
            InputState |= Input_LeftClickPress;
            break;
        case WM_LBUTTONUP:
            InputState |= Input_LeftClickRelease;
            break;
        case WM_RBUTTONDOWN:
            InputState |= Input_RightClickPress;
            break;
        case WM_RBUTTONUP:
            InputState |= Input_RightClickRelease;
            break;
        case WM_MBUTTONDOWN:
            InputState |= Input_MiddleMouseDown;
            break;
        case WM_MBUTTONUP:
            InputState |= Input_MiddleMouseUp;
            break;
        case WM_MOUSEWHEEL:
            if ((short)HIWORD(msg->wParam) > 0) {
                InputState |= Input_MiddleMouseDown;
            } else {
                InputState |= Input_MiddleMouseUp;
            }
            break;
        case WM_CHAR:
        {
            wchar_t ch = (wchar_t) msg->wParam;  // or char if ANSI
            if( ch >= ' ' && ch <= '~' ) {
                window->KeyPressed = ch;
                InputState |= Input_KeyChar;
            }
        }
        case WM_KEYUP: {
            InputState |= Input_KeyRelease;
            //window->KeyPressed = msg->wParam;
        } break;
        case WM_KEYDOWN:
            InputState |= Input_KeyPressed;
            switch (msg->wParam) {
                case VK_BACK:
                    InputState |= Input_Backspace;
                    if (GetKeyState(VK_CONTROL) & 0x8000) {
                        InputState |= DeleteWord;
                    }
                    break;
                case VK_RETURN:
                    InputState |= Input_Return;
                    break;
                case VK_CONTROL:
                    InputState |= Input_Ctrol;
                    break;
                case VK_SHIFT:
                    InputState |= Input_Shift;
                    break;
                case VK_MENU:
                    InputState |= Input_Alt;
                    break;
                case VK_F1:
                    InputState |= Input_F1;
                    break;
                case VK_F2:
                    InputState |= Input_F2;
                    break;
                case VK_F3:
                    InputState |= Input_F3;
                    break;
                case VK_F4:
                    InputState |= Input_F4;
                    break;
                case VK_F5:
                    InputState |= Input_F5;
                    break;
                case VK_LEFT:
                    InputState |= Input_Left;
                    break;
                case VK_RIGHT:
                    InputState |= Input_Right;
                    break;
                case VK_UP:
                    InputState |= Input_Up;
                    break;
                case VK_DOWN:
                    InputState |= Input_Down;
                    break;
                case VK_ESCAPE:
                    InputState = StopUI;
                    break;
            }
            // Clipboard paste (Input_Ctrol+V)
            if ((GetKeyState(VK_CONTROL) & 0x8000) && (msg->wParam == 'V' || msg->wParam == 'v')) {
                InputState |= ClipboardPaste;
            }
            break;
        case WM_SIZE:
            if (msg->wParam != SIZE_MINIMIZED) {
                InputState |= FrameBufferResized;
            }
            break;
        case WM_QUIT:
            InputState = StopUI;
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            TranslateMessage(msg);
            DispatchMessage(msg);
            break;
    }

    if( InputState == 0 ) {
        InputState = Input_None;
    }
    
    return InputState;
}


#endif 