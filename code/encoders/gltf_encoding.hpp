#ifndef GLTF_ENCODING_HPP
#define GLTF_ENCODING_HPP

#include "../third-party/cgltf/cgltf.h"

cgltf_data* load_gltf_from_file(const char* filename);
cgltf_data* load_gltf_from_memory(const void* data, cgltf_size size);

void free_gltf_data(cgltf_data* data);


#endif // GLTF_ENCODING_HPP
