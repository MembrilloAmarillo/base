#ifndef _LOAD_FONT_H_
#define _LOAD_FONT_H_

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

internal FontCache F_BuildFont(f32 FontSize, u32 Width, u32 Height, u8* BitmapArray, const char* path);

internal u32 F_GetKerningFromCodepoint(FontCache* fc, u32 g1, u32 g2);

internal u32 F_GetGlyphFromIdx(FontCache* fc, u64 idx);
internal f32 F_GetWidthFromIdx(FontCache* fc, u64 idx);
internal f32 F_GetHeightFromIdx(FontCache* fc, u64 idx);
internal f32 F_GetPosXInBitmapFromIdx(FontCache* fc, u64 idx);
internal f32 F_GetPosYInBitmapFromIdx(FontCache* fc, u64 idx);

internal int F_TextWidth(FontCache* fc, const char* str, int str_len);
internal int F_TextHeight(FontCache* fc);

/** \brief Saves a single-channel bitmap as a grayscale PPM image
 *
 *  \param filename Output filename
 *  \param bitmap Source bitmap data (single channel, 0-255)
 *  \param width Bitmap width
 *  \param height Bitmap height
 */
internal void SaveBitmapAsPPM(const char* filename, const u8* bitmap, int width, int height);

#endif // _LOAD_FONT_H_

#ifdef LOAD_FONT_IMPL

internal FontCache
F_BuildFont(f32 FontSize, u32 Width, u32 Height, u8* BitmapArray, const char* path) {
    FontCache fc = {
        .FontSize = FontSize,
        .BitmapArray = BitmapArray,
        .BitmapWidth = Width,
        .BitmapHeight = Height,
    };

    stbtt_pack_context ctx = {0};
    stbtt_PackBegin(&ctx, BitmapArray, (int)Width, (int)Height, 0, 1, NULL);

    f_file File = F_OpenFile(path, RDONLY);

    i32 FileSize = F_FileLength(&File);

    u8* data = 0;
    #if __linux__  
    data = mmap(NULL, FileSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, File.Fd, 0);
    #elif _WIN32
    data = (u8*)malloc(FileSize);  
    #endif 
    F_SetFileData(&File, data);
    i32 r_len = F_FileRead(&File);

    if( r_len == -1 ) {
        fprintf( stderr, "[ERROR] Could not read file %s\n", path);
        fprintf( stderr, "       %s\n", strerror(errno) );
        exit(-1);
    }

    stbtt_fontinfo info = {0};
    stbtt_InitFont(&info, data, 0);
    fc.scale = stbtt_ScaleForPixelHeight(&info, (f32)FontSize);

    i32 line_gap = 0;
    stbtt_GetFontVMetrics(&info, &fc.ascent, &fc.descent, &line_gap);
    fc.line_height = (f32)(fc.ascent - fc.descent + line_gap) * fc.scale;

    u8* codepoint = (u8*)malloc(96);
    codepoint[0] = 32;
    for( u32 i = 0; i < 95; i += 1 ) {
        codepoint[i+1] = i + 32;
    }

    stbtt_packedchar* char_data_range = (stbtt_packedchar*)malloc(96 * sizeof(stbtt_packedchar));
#if 0
    stbtt_pack_range range = {0};
    range.font_size                        = (f32)FontSize;
    range.first_unicode_codepoint_in_range = 0;
    range.array_of_unicode_codepoints      = codepoint;
    range.num_chars                        = 96;
    range.chardata_for_range               = char_data_range;
#else
    stbtt_pack_range range = {0};
    range.font_size  = FontSize;
    range.first_unicode_codepoint_in_range = 32; // printable ASCII start
    range.num_chars  = 96; // 32..126 inclusive -> 126-32+1 = 95
    range.chardata_for_range = char_data_range;
    range.array_of_unicode_codepoints = NULL;
#endif
    if (!stbtt_PackFontRanges(&ctx, data, 0, &range, 1)) {
        fprintf(stderr, "[ERROR] Failed to pack font ranges for %s\n", path);
    }

    for(u32 i = 0; i < 96; i += 1) {
        stbtt_packedchar q  = char_data_range[i];
        fc.glyph[i].glyph   = (u32)i + 32;
		fc.glyph[i].x_off   = q.xoff;
		fc.glyph[i].y_off   = q.yoff;
		fc.glyph[i].x       = (f32)q.x0;
		fc.glyph[i].y       = (f32)q.y0;
        fc.glyph[i].width   = (f32)q.x1 - (f32)q.x0;
        fc.glyph[i].height   = (f32)q.y1 - (f32)q.y0;
		fc.glyph[i].advance = q.xadvance;
    }

    int table_len   = stbtt_GetKerningTableLength(&info);
    fc.kerning_size = (u32)table_len;
    fc.kerning      = (f_kerning*)malloc(sizeof(f_kerning) * table_len);
    stbtt_kerningentry* table = (stbtt_kerningentry*)malloc(sizeof(stbtt_kerningentry) * table_len);

    stbtt_GetKerningTable(&info, table, table_len);

    for( u32 i = 0; i < table_len; i += 1) {
        stbtt_kerningentry k = table[i];
		fc.kerning[i].first = (u32)k.glyph1;
		fc.kerning[i].second = (u32)k.glyph2;
		fc.kerning[i].amount = (u32)k.advance;
    }

    free(table);
    free(codepoint);
    free(char_data_range);
    #if __linux__
    munmap(data, FileSize);
    #elif _WIN32
    free(data);
    #endif
    F_CloseFile(&File);
    stbtt_PackEnd(&ctx);

    return fc;
}

internal u32
F_GetKerningFromCodepoint(FontCache* fc, u32 g1, u32 g2) {
    for( u32 i = 0; i < fc->kerning_size; i += 1 ) {
		if( fc->kerning[i].first == g1 && fc->kerning[i].second == g2) {
			return fc->kerning[i].amount;
		}
	}
    return 0;
}

internal u32 F_GetGlyphFromIdx(FontCache* fc, u64 idx) {
    return fc->glyph[idx].glyph;
}

internal f32 F_GetWidthFromIdx(FontCache* fc, u64 idx) {
    return fc->glyph[idx].width;
}

internal f32 F_GetHeightFromIdx(FontCache* fc, u64 idx) {
    return fc->glyph[idx].height;
}

internal f32 F_GetPosXInBitmapFromIdx(FontCache* fc, u64 idx) {
    return fc->glyph[idx].x;
}

internal f32 F_GetPosYInBitmapFromIdx(FontCache* fc, u64 idx) {
    return fc->glyph[idx].y;
}

internal int
F_TextWidth(FontCache* fc, const char* str, int str_len) {
    if (!fc || !str) return 0;
    if (str_len < 0) str_len = (int)strlen(str);

    int width = 0;
    for (int i = 0; i < str_len && str[i] != '\0'; ++i) {
        unsigned char ch = (unsigned char)str[i];
        if (ch < 32 || ch > 126) {
            // skip non-printable / unsupported
            continue;
        }
        int g_idx = (int)ch - 32;
        f_Glyph glyph = fc->glyph[g_idx];
        // Use advance (xadvance) to advance pen, not bitmap width
        width += (int)glyph.advance;
        if( i < str_len - 1 ) {
           u32 kern = F_GetKerningFromCodepoint( fc, (int)g_idx, (int) str[i+1] - 32 );
            width += kern;
        }
    }
    return width;
}

internal int F_TextHeight(FontCache* fc) {
    if (!fc) return 0;
    // line_height was computed as (ascent-descent+gap)*scale (a float). Round up to integer pixels.
    return (int)ceilf(fc->FontSize + fc->line_height / 2);
}

void SaveBitmapAsPPM(const char* filename, const u8* bitmap, int width, int height) {
    FILE* f = fopen(filename, "wb");
    if (!f) {
        fprintf(stderr, "Failed to open %s for writing\n", filename);
        return;
    }

    // P5 = binary PGM (grayscale) format
    fprintf(f, "P5\n%d %d\n255\n", width, height);

    // Write raw grayscale data (1 byte per pixel)
    fwrite(bitmap, 1, width * height, f);

    fclose(f);
    printf("Saved bitmap to %s (%dx%d)\n", filename, width, height);
}



#endif