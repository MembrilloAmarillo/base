#ifndef _WINDOW_CREATION_H_
#define _WINDOW_CREATION_H_

#ifdef __linux__
    #include "os_linux/surface.h"
    #include "os_linux/events.h"
#elif _WIN32
    #include "os_windows/surface.h"
    #include "os_windows/events.h"
#endif

#endif 


#ifdef WINDOW_CREATION_IMPL

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