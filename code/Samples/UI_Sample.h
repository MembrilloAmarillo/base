#ifndef _UI_SAMPLE_H_
#define _UI_SAMPLE_H_

#if __linux__
#include <X11/Xlib.h>
#include <X11/keysymdef.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <unistd.h>
#define VK_USE_PLATFORM_XLIB_KHR

#elif _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <vulkan/vulkan.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <time.h>
#include <stddef.h>

#include "../third-party/xxhash.h"
#include "../third-party/stb_image.h"
#include "../third-party/stb_truetype.h"

#include "../types.h"
#include "../memory.h"
#include "../allocator.h"
#include "../strings.h"
#include "../vector.h"
#include "../queue.h"
#include "../files.h"
#include "../HashTable.h"

#include "../load_font.h"
#include "../window_creation.h"
#include "../third-party/vk_mem_alloc.h"
#include "../vk_render.h"
#include "../ui_render.h"
#include "../draw.h"
#include "../new_ui.h"

#define MEMORY_IMPL
#include "../memory.h"

#define ALLOCATOR_IMPL
#include "../allocator.h"

#define STRINGS_IMPL
#include "../strings.h"

#define VECTOR_IMPL
#include "../vector.h"

#define QUEUE_IMPL
#include "../queue.h"

#define FILES_IMPL
#include "../files.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../third-party/stb_image.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "../third-party/stb_truetype.h"

#define LOAD_FONT_IMPL
#include "../load_font.h"

#define WINDOW_CREATION_IMPL
#include "../window_creation.h"

#define VK_RENDER_IMPL
#include "../vk_render.h"

#include "../HashTable.h"
#include "../HashTable.c"

#define SP_UI_IMPL
#include "../new_ui.h"

#define UI_RENDER_IMPL
#include "../ui_render.h"

#define DRAW_IMPL
#include "../draw.h"


#endif