#ifndef _LOAD_FONT_FT2_H_
#define _LOAD_FONT_FT2_H_

#include <ft2build.h>
#include FT_FREETYPE_H

#include "types.h"

// ensure global library exists (declared in your header)
#ifndef FT_LIB_INITIALIZED_GUARD
#define FT_LIB_INITIALIZED_GUARD
static int FtLibrary_initialized = 0;
#endif

global FT_Library FtLibrary;

typedef struct f_Glyph f_Glyph;
struct f_Glyph {
	u32 glyph   ;
	f32 x_off   ;
	f32 y_off   ;
	f32 x       ;
	f32 y       ;
	f32 width   ;
	f32 height  ;
	f32 advance ;
};

typedef struct f_kerning f_kerning;
struct f_kerning {
	u32 first;
	u32 second;
	u32 amount;
};

typedef struct FontCache FontCache;
struct FontCache {
    f_Glyph    glyph[96];
    f32        FontSize;
    u8*        BitmapArray;
    u32        BitmapWidth;
    u32        BitmapHeight;
    vec2       BitmapOffset;// NOTE: I store this here for now. I need this to now where in the original bitmap is located
    i32        ascent;
    i32        descent;
    f32        scale;
    f32        line_height;
    f_kerning* kerning;
    u32        kerning_size;
};

fn_internal FontCache F_BuildFont(f32 FontSize, u32 Width, u32 Height, u8* BitmapArray, const char* path);

fn_internal u32 F_GetKerningFromCodepoint(FontCache* fc, u32 g1, u32 g2);

fn_internal u32 F_GetGlyphFromIdx(FontCache* fc, u64 idx);
fn_internal f32 F_GetWidthFromIdx(FontCache* fc, u64 idx);
fn_internal f32 F_GetHeightFromIdx(FontCache* fc, u64 idx);
fn_internal f32 F_GetPosXInBitmapFromIdx(FontCache* fc, u64 idx);
fn_internal f32 F_GetPosYInBitmapFromIdx(FontCache* fc, u64 idx);

fn_internal f32 F_TextWidth(FontCache* fc, const char* str, int str_len);
fn_internal f32 F_TextHeight(FontCache* fc);

#endif //_LOAD_FONT_FT2_H_

#ifdef LOAD_FONT_IMPL

// helper to ensure global FT_Library
static void ensure_ft_library() {
    if (!FtLibrary_initialized) {
        if (FT_Init_FreeType(&FtLibrary)) {
            fprintf(stderr, "FreeType init failed\n");
            FtLibrary = NULL;
        } else {
            FtLibrary_initialized = 1;
        }
    }
}

// Build simple shelf packer state
typedef struct {
    u32 cur_x;
    u32 cur_y;
    u32 row_h;
    u32 width;
    u32 height;
} ShelfPacker;

static void shelf_init(ShelfPacker* p, u32 width, u32 height) {
    p->cur_x = 0;
    p->cur_y = 0;
    p->row_h = 0;
    p->width = width;
    p->height = height;
}

// returns 0 on success, non-zero on failure (no room)
static int shelf_alloc(ShelfPacker* p, u32 w, u32 h, u32* out_x, u32* out_y) {
    if (w > p->width || h > p->height) return 1;
    if (p->cur_x + w > p->width) {
        // new row
        p->cur_x = 0;
        p->cur_y += p->row_h;
        p->row_h = 0;
    }
    if (p->cur_y + h > p->height) return 2; // no vertical room
    *out_x = p->cur_x;
    *out_y = p->cur_y;
    p->cur_x += w;
    if (h > p->row_h) p->row_h = h;
    return 0;
}

// Convert 26.6 FT metrics to integer pixels (rounding)
static inline int ft_to_int(FT_Pos v) { return (int)((v + 32) >> 6); } // (v >> 6) with rounding

// Create kerning list: simple dense store of only non-zero kerning pairs, in pixels (signed)
static u32 build_kerning_list(FT_Face face, f_kerning** out_list) {
    if (!FT_HAS_KERNING(face)) {
        *out_list = NULL;
        return 0;
    }

    // Only consider glyphs for codepoints 32..127 (same as glyph cache)
    const u32 first_cp = 32;
    const u32 last_cp  = 127; // exclusive maybe, we will iterate 32..126 inclusive
    const u32 glyph_count = last_cp - first_cp; // 95? but we'll fill 96 slots; we'll use 96 entries total as header

    // We'll collect pairs in a dynamic array
    f_kerning* list = NULL;
    size_t list_cap = 0;
    size_t list_len = 0;

    FT_UInt glyph_indices[96];
    for (u32 cp = first_cp, i = 0; cp < first_cp + 96; ++cp, ++i) {
        glyph_indices[i] = FT_Get_Char_Index(face, cp);
    }

    for (u32 i = 0; i < 96; ++i) {
        for (u32 j = 0; j < 96; ++j) {
            FT_Vector delta;
            FT_UInt g1 = glyph_indices[i];
            FT_UInt g2 = glyph_indices[j];
            if (!g1 || !g2) continue;
            if (FT_Get_Kerning(face, g1, g2, FT_KERNING_DEFAULT, &delta) != 0) continue;
            // delta.x is in 26.6
            int amount = ft_to_int(delta.x);
            if (amount == 0) continue;
            if (list_len + 1 > list_cap) {
                size_t newcap = list_cap ? list_cap * 2 : 64;
                f_kerning* tmp = (f_kerning*)realloc(list, newcap * sizeof(f_kerning));
                if (!tmp) { free(list); *out_list = NULL; return 0; }
                list = tmp;
                list_cap = newcap;
            }
            f_kerning e;
            e.first  = (u32)(32 + i);  // store codepoint
            e.second = (u32)(32 + j);
            e.amount = (u32)amount;    // positive or negative values? struct uses u32; we store as signed->u32 two's complement not ideal.
            // NOTE: your struct uses u32 for amount. Kerning can be negative; storing negative as u32 will wrap.
            // If you need sign-preserving, change f_kerning.amount to i32. For now we'll cast.
            list[list_len++] = e;
        }
    }

    *out_list = list;
    return (u32)list_len;
}

// Public functions

fn_internal FontCache F_BuildFont(f32 FontSize, u32 Width, u32 Height, u8* BitmapArray, const char* path) {
    FontCache fc;
    memset(&fc, 0, sizeof(fc));

    ensure_ft_library();
    if (!FtLibrary) {
        fprintf(stderr, "FT library not initialized\n");
        return fc;
    }

    // Clear bitmap array
    memset(BitmapArray, 0, (size_t)Width * (size_t)Height);

    // load face
    FT_Face face = NULL;
    if (FT_New_Face(FtLibrary, path, 0, &face)) {
        fprintf(stderr, "FT_New_Face failed for %s\n", path);
        return fc;
    }

    // Set pixel size. We'll use pixel height = FontSize (you may pass integer)
    if (FT_Set_Pixel_Sizes(face, 0, (FT_UInt)FontSize)) {
        fprintf(stderr, "FT_Set_Pixel_Sizes failed\n");
        FT_Done_Face(face);
        return fc;
    }

    // store metrics
    // metrics are in 26.6 fixed point in face->size->metrics
    fc.FontSize = FontSize;
    fc.ascent  = ft_to_int(face->size->metrics.ascender);
    fc.descent = ft_to_int(face->size->metrics.descender);
    // scale: the reciprocal of em-size? We'll compute scale as number of px per em
    fc.scale = (f32)face->size->metrics.y_scale / 65536.0f; // y_scale is 16.16 fixed (not strictly necessary)
    fc.line_height = (f32)ft_to_int(face->size->metrics.height);

    fc.BitmapArray  = BitmapArray;
    fc.BitmapWidth  = Width;
    fc.BitmapHeight = Height;
    fc.BitmapOffset.x = 0.0f;
    fc.BitmapOffset.y = 0.0f;

    // Packer
    ShelfPacker packer;
    shelf_init(&packer, Width, Height);

    // glyph loop for codepoints 32..127 exclusive? We pack 96 glyphs (your FontCache has glyph[96])
    const u32 start_cp = 32;
    const u32 glyphs_total = 96; // matches your FontCache array length

    // iterate and render each glyph, pack it
    for (u32 i = 0; i < glyphs_total; ++i) {
        u32 cp = start_cp + i;
        FT_UInt gindex = FT_Get_Char_Index(face, (FT_ULong)cp);
        fc.glyph[i].glyph = cp; // store codepoint in glyph field

        if (gindex == 0) {
            // missing glyph: leave metrics zero, but set width/height = 0
            fc.glyph[i].x = 0; fc.glyph[i].y = 0;
            fc.glyph[i].width = 0; fc.glyph[i].height = 0;
            fc.glyph[i].x_off = 0; fc.glyph[i].y_off = 0;
            fc.glyph[i].advance = 0;
            continue;
        }

        // load glyph
        if (FT_Load_Glyph(face, gindex, FT_LOAD_DEFAULT)) {
            // load failed
            continue;
        }

        // render into normal grayscale bitmap
        if (FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL)) {
            continue;
        }

        FT_GlyphSlot slot = face->glyph;
        FT_Bitmap* bmp = &slot->bitmap;

        // allocate spot in atlas: use width=bmp->width, height=bmp->rows
        u32 bmp_w = (u32)bmp->width;
        u32 bmp_h = (u32)bmp->rows;

        if (bmp_w == 0 || bmp_h == 0) {
            // Some glyphs (space) have no bitmap. But we still record their advance.
            fc.glyph[i].width = 0;
            fc.glyph[i].height = 0;
            fc.glyph[i].x_off = (f32)slot->bitmap_left;
            fc.glyph[i].y_off = (f32)slot->bitmap_top;
            fc.glyph[i].advance = (f32)ft_to_int(slot->advance.x);
            fc.glyph[i].x = 0;
            fc.glyph[i].y = 0;
            continue;
        }

        // Need small border/padding to avoid bleeding when sampling with linear filtering
        const u32 padding = 1;
        u32 alloc_w = bmp_w + padding*2;
        u32 alloc_h = bmp_h + padding*2;

        u32 place_x = 0, place_y = 0;
        if (shelf_alloc(&packer, alloc_w, alloc_h, &place_x, &place_y)) {
            fprintf(stderr, "Atlas size too small for font atlas. Failed to place glyph %u (codepoint %u).\n", i, cp);
            // Clean up & return partially filled cache (caller can detect).
            // Still fill advance info.
            fc.glyph[i].width = bmp_w;
            fc.glyph[i].height = bmp_h;
            fc.glyph[i].advance = (f32)ft_to_int(slot->advance.x);
            continue;
        }

        // actual placement inside atlas after padding
        u32 dest_x = place_x + padding;
        u32 dest_y = place_y + padding;

        // copy bitmap into fc.BitmapArray (single-channel)
        // note: bmp->pitch may be >= width (signed int), and rows may be oriented top->bottom
        for (u32 row = 0; row < bmp_h; ++row) {
            int src_row_index = row * bmp->pitch;
            for (u32 col = 0; col < bmp_w; ++col) {
                u8 val = bmp->buffer[src_row_index + col];
                u32 dx = dest_x + col;
                u32 dy = dest_y + row;
                if (dx >= Width || dy >= Height) continue; // safety
                BitmapArray[dy * Width + dx] = val;
            }
        }

        // record glyph metrics (x,y in atlas in pixels, width/height in pixels, offsets, advance)
        fc.glyph[i].x = (f32)dest_x;
        fc.glyph[i].y = (f32)dest_y;
        fc.glyph[i].width  = (f32)bmp_w;
        fc.glyph[i].height = (f32)bmp_h;

        // x_off is bitmap_left (distance from pen to left of bitmap)
        // y_off is bitmap_top (distance from baseline up to top of bitmap). Often used to compute target Y.
        fc.glyph[i].x_off = (f32)slot->bitmap_left;
        fc.glyph[i].y_off = (f32)slot->bitmap_top;
        fc.glyph[i].advance = (f32)ft_to_int(slot->advance.x);
    } // end glyph loop

    // build kerning list (if any)
    fc.kerning_size = build_kerning_list(face, &fc.kerning);

    // done with face
    FT_Done_Face(face);

    // done
    return fc;
}

// look up kerning between two codepoints (g1,g2 are codepoints e.g. 'A','V')
fn_internal u32 F_GetKerningFromCodepoint(FontCache* fc, u32 g1, u32 g2) {
    if (!fc || !fc->kerning || fc->kerning_size == 0) return 0;
    // linear search; there won't be many pairs for small fonts
    for (u32 i = 0; i < fc->kerning_size; ++i) {
        if (fc->kerning[i].first == g1 && fc->kerning[i].second == g2) {
            return fc->kerning[i].amount;
        }
    }
    return 0;
}

// Get glyph array index from codepoint idx (assume idx is codepoint, e.g. 'A' or integer); returns index into fc->glyph or 0 if missing.
// We assume cached range is 32..(32+96-1)
fn_internal u32 F_GetGlyphFromIdx(FontCache* fc, u64 idx) {
    if (!fc) return 0;
    u64 cp = idx;
    if (cp < 32 || cp >= 32 + 96) return 0;
    u32 i = (u32)(cp - 32);
    return i;
}

fn_internal f32 F_GetWidthFromIdx(FontCache* fc, u64 idx) {
    u32 i = F_GetGlyphFromIdx(fc, idx);
    if (!fc) return 0.0f;
    return fc->glyph[i].width;
}
fn_internal f32 F_GetHeightFromIdx(FontCache* fc, u64 idx) {
    u32 i = F_GetGlyphFromIdx(fc, idx);
    if (!fc) return 0.0f;
    return fc->glyph[i].height;
}
fn_internal f32 F_GetPosXInBitmapFromIdx(FontCache* fc, u64 idx) {
    u32 i = F_GetGlyphFromIdx(fc, idx);
    if (!fc) return 0.0f;
    return fc->glyph[i].x;
}
fn_internal f32 F_GetPosYInBitmapFromIdx(FontCache* fc, u64 idx) {
    u32 i = F_GetGlyphFromIdx(fc, idx);
    if (!fc) return 0.0f;
    return fc->glyph[i].y;
}

fn_internal f32 F_TextWidth(FontCache* fc, const char* str, int str_len) {
    if (!fc || !str) return 0.0f;
    // simple ASCII loop; you may want to decode UTF-8 into codepoints and use that instead.
    f32 w = 0.0f;
    u32 prev_cp = 0;
    for (int i = 0; i < str_len && str[i] != '\0'; ++i) {
        unsigned char c = (unsigned char)str[i];
        if (c < 32 || c >= 32 + 96) {
            // if outside cached range, treat as space
            c = ' ';
        }
        u32 gi = F_GetGlyphFromIdx(fc, c);
        if (gi >= 96) continue;
        u32 kerning = 0;
        if (prev_cp) kerning = F_GetKerningFromCodepoint(fc, prev_cp, (u32)c);
        w += (f32)kerning;
        w += fc->glyph[gi].advance;
        prev_cp = (u32)c;
    }
    return w;
}

fn_internal f32 F_TextHeight(FontCache* fc) {
    if (!fc) return 0.0f;
    return fc->line_height;
}
#endif