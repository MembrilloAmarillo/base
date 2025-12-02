# === VARIABLES DE USUARIO ===
CC   := clang++
CXX  := g++
CFLAGS  := -g -ggdb -std=c++17 -DDEBUG -Wall -Wno-unused-function -D_GNU_SOURCE
CXXFLAGS:= -g -ggdb -std=c++17 -Wall
CPPFLAGS:= -D_POSIX_PTHREAD_SEMANTICS
INC     := -Icode
LIBS    := -lzmq -lsocketcan -lm -lpthread -ldl -lrt -lX11 -lvulkan -lstdc++

ifeq ($(SDL_USAGE),1)
    CFLAGS   += -DSDL_USAGE
    CXXFLAGS += -DSDL_USAGE

    # Link SDL3 dynamically
    # SDL_CFLAGS := $(shell sdl3-config --cflags)
    SDL_LIBS   := -lSDL3

    # INC  += $(SDL_CFLAGS)
    LIBS += $(SDL_LIBS)

    # Optional: remove X11 Vulkan surface flags when using SDL Vulkan surface
    # LIBS := $(filter-out -DVK_USE_PLATFORM_XLIB_KHR -lX11, $(LIBS))
endif

SRC_C := code/Samples/ToDoList.cpp
VMA   := code/third-party/vk_mem_alloc.c

all: todolist shaders

xxhash.o: code/third-party/xxhash.c
	$(CXX) $(CXXFLAGS) -c $< -o $@

vma_impl.o: $(VMA)
	$(CXX) $(CXXFLAGS) -c $< -o $@

todolist: $(SRC_C) vma_impl.o xxhash.o
	@echo "Compiling $@..."
	$(CC) $(CPPFLAGS) $(INC) -o $@ $^ $(LIBS)

# -Wextra -fPIE -Wconversion
# -Wextra                   \
# -Werror                   \
# -g                        \
# -Wunused-but-set-variable \
# -Wpedantic                \
# -O0                       \
# -Wsign-compare            \
# -Wtype-limits             \
# -Wformat=2                \
# -Wshift-overflow          \
# -Wformat-security         \
# -Wnull-dereference        \
# -Wstack-protector         \
# -Wimplicit-fallthrough    \
# -Wcast-qual               \
# -Wconversion              \
# -Wshadow                  \
# -Wstrict-overflow=4       \
# -Wundef                   \
# -Wswitch-default          \
# -Wswitch-enum             \
# -Wcast-align              \
# -fstack-protector-strong  \
# -fstack-clash-protection  \
# -fPIE

shaders: 
	glslc ./data/compute.comp         -o ./data/compute.comp.spv
	glslc ./data/ColoredTriangle.vert -o ./data/ColoredTriangle.vert.spv
	glslc ./data/ColoredTriangle.frag -o ./data/ColoredTriangle.frag.spv
	glslc ./data/ui_render.vert       -o ./data/ui_render.vert.spv
	glslc ./data/ui_render.frag       -o ./data/ui_render.frag.spv

clean:
	rm -f UI_Sample ./data/*.spv

