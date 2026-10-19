#ifndef GLTF_ENCODING_HPP
#define GLTF_ENCODING_HPP

#if __has_include("../third-party/cgltf/cgltf.h")
#include "../third-party/cgltf/cgltf.h"
#define GLTF_HAS_CGLTF 1
#elif __has_include("../third-party/cgltf.h")
#include "../third-party/cgltf.h"
#define GLTF_HAS_CGLTF 1
#else
#include <stddef.h>
#define GLTF_HAS_CGLTF 0
typedef size_t cgltf_size;
typedef struct cgltf_data cgltf_data;
#endif

cgltf_data* load_gltf_from_file(const char* filename);
cgltf_data* load_gltf_from_memory(const void* data, cgltf_size size);

void free_gltf_data(cgltf_data* data);


#endif // GLTF_ENCODING_HPP
