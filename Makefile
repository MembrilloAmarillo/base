# === VARIABLES DE USUARIO ===
CC   := clang
CXX  := g++
CFLAGS  := -g -ggdb -DDEBUG -Wall -Wno-unused-function -std=gnu11 -D_GNU_SOURCE -DVK_USE_PLATFORM_XLIB_KHR
CXXFLAGS:= -g -ggdb -std=c++17 -Wall
CPPFLAGS:= -D_POSIX_PTHREAD_SEMANTICS
INC     := -Icode
LIBS    := -lzmq -lsocketcan -lm -lpthread -ldl -lrt -lX11 -lvulkan -lstdc++

CFLAGS_OPT := -Wall -Wno-unused-function -std=gnu11 -D_GNU_SOURCE -O3
CXXFLAGS_OPT := -std=c++17 -Wall -O3

XXHASH := code/third-party/xxhash.c
SRC_C := code/Samples/UI_Sample.c
VMA   := code/third-party/vk_mem_alloc.c

all: UI_Sample shaders

vma_impl.o: $(VMA)
	$(CXX) $(CXXFLAGS) -c $< -o $@

vma_impl_release.o: $(VMA)
	$(CXX) $(CXXFLAGS_OPT) -c $< -o $@

xxhash.o: $(XXHASH)
	$(CC) $(CFLAGS) -c $< -o $@

xxhash_release.o: $(XXHASH)
	$(CC) $(CFLAGS_OPT) -c $< -o $@

UI_Sample_Release: $(SRC_C) vma_impl_release.o xxhash.o
	@echo "Compiling $@..."
	$(CC) $(CFLAGS_OPT) $(CPPFLAGS) $(INC) -o $@ $^ $(LIBS)

UI_Sample: $(SRC_C) vma_impl.o xxhash_release.o
	@echo "Compiling $@..."
	$(CC) $(CFLAGS) $(CPPFLAGS) $(INC) -o $@ $^ $(LIBS)

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

.PHONY: all clean
