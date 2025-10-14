#ifndef _EVENTS_LINUX_H_
#define _EVENTS_LINUX_H_

#define ui_input u64

#define Input_None               ((ui_input)(1 << 0))
#define Input_RightClickPress    ((ui_input)(1 << 1))
#define Input_RightClickRelease  ((ui_input)(1 << 2))
#define Input_LeftClickPress     ((ui_input)(1 << 3))
#define Input_LeftClickRelease   ((ui_input)(1 << 4))
#define Input_DoubleLeftClick    ((ui_input)(1 << 5))
#define Input_CursorHover        ((ui_input)(1 << 6))
#define Input_Backspace          ((ui_input)(1 << 7))
#define Input_Ctrol               ((ui_input)(1 << 8))
#define Input_Shift              ((ui_input)(1 << 9))
#define Input_Alt                ((ui_input)(1 << 10))
#define Input_Return             ((ui_input)(1 << 11))
#define Input_MiddleMouse        ((ui_input)(1 << 12))
#define Input_MiddleMouseUp      ((ui_input)(1 << 13))
#define Input_MiddleMouseDown    ((ui_input)(1 << 14))
#define Input_F1                 ((ui_input)(1 << 15))
#define Input_F2                 ((ui_input)(1 << 16))
#define Input_F3                 ((ui_input)(1 << 17))
#define Input_F4                 ((ui_input)(1 << 18))
#define Input_F5                 ((ui_input)(1 << 19))
#define Input_Left               ((ui_input)(1 << 20))
#define Input_Right              ((ui_input)(1 << 21))
#define Input_Up                 ((ui_input)(1 << 22))
#define Input_Down               ((ui_input)(1 << 23))
#define Input_Esc                ((ui_input)(1 << 24))
#define StopUI                   ((ui_input)(1 << 25))
#define ActiveObject             ((ui_input)(1 << 26))
#define DeleteWord               ((ui_input)(1 << 27))
#define FrameBufferResized       ((ui_input)(1 << 28))
#define ClipboardPaste           ((ui_input)(1 << 29))
#define Input_KeyPressed         ((ui_input)(1 << 30))
#define Input_KeyRelease         ((ui_input)(1 << 31))
#define Input_KeyChar            ((ui_input)(1 << 32))

internal vec2 GetMousePosition(api_window* window);

internal ui_input GetNextEvent(api_window* window);

internal char* GetClipboard(api_window* Window);

#endif 

#ifdef EVENTS_IMPL

internal vec2
GetMousePosition(api_window* window) {
    Window window_returned;
    int root_x, root_y;
    int win_x, win_y;
    u32 mask_return;
    XQueryPointer(
                  window->Dpy,
                  window->Win,
                  &window_returned,
                  &window_returned,
                  &root_x, &root_y,
                  &win_x, &win_y,
                  &mask_return
                  );

    vec2 MousePosition = {.x = (f32)win_x, .y = (f32)win_y};

    return MousePosition;
}

internal ui_input
GetNextEvent(api_window* Window) {
    ui_input Input = Input_None;
    XEvent ev = {0};
    while (XPending(Window->Dpy)) {
        XNextEvent(Window->Dpy, &ev);

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
            { XK_Input_Backspace, Input_Backspace},
            { XK_Input_Return,    Input_Return},
            { XK_Input_Shift_L,   Input_Shift},
            { XK_Input_Shift_R,   Input_Shift},
            { XK_Control_L, Input_Ctrol},
            { XK_Control_R, Input_Ctrol},
            { XK_Meta_L,    Input_Alt },
            { XK_Meta_R,    Input_Alt },
            { XK_Input_F1,        Input_F1 },
            { XK_Input_F2,        Input_F2 },
            { XK_Input_F3,        Input_F3 },
            { XK_Input_F4,        Input_F4 },
            { XK_Input_F5,        Input_F5 },
            { XK_Left,      Input_Left},
            { XK_Right,     Input_Right},
            { XK_Up,        Up},
            { XK_Down,      Down},
            { XK_Escape,    Input_Esc},
        };

        switch (ev.type) {
            // --- Window Events ---
            case ConfigureNotify: {
                XConfigureEvent* ce = (XConfigureEvent*)&ev;
                if (ce->width != window_width || ce->height != window_height) {
                    Input |= FrameBufferResized;
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

                    XGetWindowProperty(Window->Dpy,
                                       ev.xselection.requestor, // Our window
                                       ev.xselection.property,  // The property we asked for
                                       0, 1024, False, AnyPropertyType,
                                       &actual_type, &actual_format, &nitems, &bytes_after, &data);

                    if (data && nitems > 0) {
                        printf("Pasted content: %s\n", data);
                        memset(Window->ClipboardContent, 0, 256);
                        memcpy(Window->ClipboardContent, data, UCF_Strlen(data));
                        XFree(data);
                    } else {
                        printf("Clipboard is empty or data could not be read.\n");
                    }
                    // Clean up the property
                    XDeleteProperty(Window->Dpy, ev.xselection.requestor, ev.xselection.property);
                    Input |= ClipboardPaste;
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

                if (be->button == Button4) {
                    Input |= Input_MiddleMouseUp;
                } else if (be->button == Button5) {
                    Input |= Input_MiddleMouseDown;
                } else {
                    Input |= Input_LeftClickPress;
                }
                break;
            }

            case ButtonRelease: {
                Input |= Input_LeftClickRelease;
            } break;

            case KeyPress: {

                Input |= Input_KeyPressed;

                char buffer[256] = {0};
                KeySym keysym_from_utf8 = NoSymbol;
                int num_bytes = Xutf8LookupString(
                    Window->xic, 
                    &ev.xkey, 
                    buffer, sizeof(buffer), 
                    &keysym_from_utf8, 
                    NULL
                );

                int key = 0;
                for (size_t i = 0; i < ArrayCount(keys_to_check); ++i) {
                    if (keysym_from_utf8 == keys_to_check[i].x_key) {
                        key = keys_to_check[i].input;
                        Input |= key;
                    }
                }
                if (key) {
                    Input |= Input_KeyChar;
                    Window->KeyPressed |= key;
                }

                if (keysym_from_utf8 == XK_Escape) {
                    Input = StopUI;
                    return Input; 
                }

                if ( (ev.xkey.state & ControlMask) && (keysym_from_utf8 == XK_v || keysym_from_utf8 == XK_V)) {
                    printf("Input_Ctrol+V detected! Requesting clipboard content...\n");
                    XConvertSelection(
                                      Window->Dpy,
                                      Window->Clipboard.Clipboard,
                                      Window->Clipboard.Utf8,
                                      Window->Clipboard.Paste,
                                      Window->Win,
                                      CurrentTime
                                      );
                    XFlush(Window->Dpy); // Ensure the request is sent immediately
                    break;
                }

                switch( keysym_from_utf8 ) {
                    case XK_Input_F1: {
                        Input |= Input_F1;
                    }break;
                    case XK_Input_F2: {
                        Input |= Input_F2;
                    }break;
                    case XK_Input_F3: {
                        Input |= Input_F3;
                    }break;
                    case XK_Input_F4: {
                        Input |= Input_F4;
                    }break;
                    case XK_Input_F5: {
                        Input |= Input_F5;
                    }break;
                    case XK_Left: {
                        Input |= Input_Left;
                    }break;
                    case XK_Right: {
                        Input |= Input_Right;
                    }break;
                    case XK_Up: {
                        Input |= Up;
                    }break;
                    case XK_Down: {
                        Input |= Down;
                    }break;
                    case XK_Input_Return: {
                        Input |= Input_Return;
                    } break;
                    case XK_Control_L:
                    case XK_Control_R: {
                        Input |= Input_Ctrol;
                    } break;
                    case XK_Input_Shift_L:
                    case XK_Input_Shift_R: {
                        Input |= Input_Shift;
                    } break;
                    case XK_Meta_L:
                    case XK_Meta_R: {
                        Input |= Input_Alt;
                    } break;
                    case XK_Input_Backspace: {
                        Input |= Input_Backspace;
                        unsigned int state = ev.xkey.state;
                        bool is_Input_Ctrol_pressed  = (state & ControlMask)  != 0;
                        if(is_Input_Ctrol_pressed) {
                            Input |= DeleteWord;
                        }
                    } break;
                    default: {}break;
                }

                break;
            }

            case KeyRelease: {
                KeySym keysym = XkbKeycodeToKeysym(gfx->base->Window.Dpy, ev.xkey.keycode, 0, 0);
                for (size_t i = 0; i < sizeof(keys_to_check)/sizeof(keys_to_check[0]); ++i) {
                    if (keysym == keys_to_check[i].x_key) {
                        window->KeyPressed &= ~keysym;
                        break;
                    }
                }
                Input |= Input_KeyRelease;
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
    return Input;
}

internal char* 
GetClipboard(api_window* Window) {
    return Window->ClipboardContent;
}


#endif 