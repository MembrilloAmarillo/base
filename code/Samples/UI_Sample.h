#include <X11/Xlib.h>
#include <X11/keysymdef.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <time.h>

#include "../types.h"
#include "../memory.h"
#include "../allocator.h"
#include "../strings.h"
#include "../vector.h"
#include "../queue.h"
#include "../files.h"
#include "../third-party/stb_image.h"
#include "../third-party/stb_truetype.h"
#include "../third-party/microui.h"
#include "../load_font.h"
#include "../vk_render.h"
#include "../new_ui.h"
#include "../ui_render.h"
#include "../HashTable.h"

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

#define MICROUI_IMPL
#include "../third-party/microui.h"

#define LOAD_FONT_IMPL
#include "../load_font.h"

#define VK_RENDER_IMPL
#include "../vk_render.h"

#include "../HashTable.h"
#include "../HashTable.c"

#define SP_UI_IMPL
#include "../new_ui.h"

#define UI_RENDER_IMPL
#include "../ui_render.h"

#include "../third-party/xxhash.c"
