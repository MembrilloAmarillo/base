#ifndef _FONT_ATLAS_H_
#define _FONT_ATLAS_H_

#include "load_font_ft2.h"
#include "hashtable.h"

typedef struct {
    u64 offset_x;
    u64 offset_y;
    u64 width;
    u64 height;
    FontCache *font;
} fa_font_info;

typedef struct {
    u64 max_width;
    u64 max_height;
    u64 current_x;
    u64 current_y;
    u64 row_height;
    u8* bitmap_array;
    hash_table font_info_table;
} fa_atlas;     

void fa_init_atlas(fa_atlas* atlas, u64 max_width, u64 max_height, Stack_Allocator* allocator);
fa_font_info* fa_add_font_to_atlas(fa_atlas* atlas, FontCache* font, const char* font_id);
fa_font_info* fa_get_font_info(fa_atlas* atlas, const char* font_id);


#endif //_FONT_ATLAS_H_