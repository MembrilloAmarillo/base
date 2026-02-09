#include "font_atlas.hpp"

void fa_init_atlas(fa_atlas* atlas, u64 max_width, u64 max_height, Stack_Allocator* allocator) {
    atlas->max_width = max_width;
    atlas->max_height = max_height;
    atlas->current_x = 0;
    atlas->current_y = 0;
    atlas->row_height = 0;
    atlas->bitmap_array = (u8*)allocator_alloc(allocator, max_width * max_height);
    hash_table_init(&atlas->font_info_table, allocator);
}

fa_font_info* fa_add_font_to_atlas(fa_atlas* atlas, FontCache* font, const char* font_id) {
    if (atlas->current_x + font->width > atlas->max_width) {
        atlas->current_x = 0;
        atlas->current_y += atlas->row_height;
        atlas->row_height = 0;
    }

    if (atlas->current_y + font->height > atlas->max_height) {
        // Atlas is full, handle this case as needed
        return NULL;
    }

    // Copy the font bitmap to the atlas bitmap array
    for (u64 y = 0; y < font->height; y++) {
        for (u64 x = 0; x < font->width; x++) {
            u64 atlas_index = ((atlas->current_y + y) * atlas->max_width) + (atlas->current_x + x);
            u64 font_index = (y * font->width) + x;
            atlas->bitmap_array[atlas_index] = font->bitmap[font_index];
        }
    }

    fa_font_info info = {
        .offset_x = atlas->current_x,
        .offset_y = atlas->current_y,
        .width = font->width,
        .height = font->height,
        .font = font
    };

    hash_table_add(&atlas->font_info_table, font_id, &info, 0);

    atlas->current_x += font->width;
    if (font->height > atlas->row_height) {
        atlas->row_height = font->height;
    }

    return &info;
}

fa_font_info* fa_get_font_info(fa_atlas* atlas, const char* font_id) {
    entry* entry = hash_table_find_pointer(&atlas->font_info_table, font_id, 0);
    if (entry) {
        return (fa_font_info*)entry->value;
    }
    return NULL;
}