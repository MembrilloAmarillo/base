
#include "gltf_encoding.hpp"

#if GLTF_HAS_CGLTF
cgltf_data* load_gltf_from_file(const char* filename) {
    cgltf_options options = {};
    cgltf_data* data = NULL;
    cgltf_result result = cgltf_parse_file(&options, filename, &data);
    if (result != cgltf_result_success) {
        // Handle error
        return NULL;
    }
    result = cgltf_load_buffers(&options, data, filename);
    if (result != cgltf_result_success) {
        cgltf_free(data);
        return NULL;
    }
    return data;
}

cgltf_data* load_gltf_from_memory(const void* data, cgltf_size size) {
    cgltf_options options = {};
    cgltf_data* out_data = NULL;
    cgltf_result result = cgltf_parse(&options, data, size, &out_data);
    if (result != cgltf_result_success) {
        // Handle error
        return NULL;
    }
    return out_data;
}

void free_gltf_data(cgltf_data* data) {
    if (data) {
        cgltf_free(data);
    }
}
#else
cgltf_data* load_gltf_from_file(const char* filename) {
    (void)filename;
    return NULL;
}

cgltf_data* load_gltf_from_memory(const void* data, cgltf_size size) {
    (void)data;
    (void)size;
    return NULL;
}

void free_gltf_data(cgltf_data* data) {
    (void)data;
}
#endif
