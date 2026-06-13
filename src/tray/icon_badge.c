/**
 * @file icon_badge.c
 * @brief CPU rasteriser for language text badges (see icon_badge.h).
 *
 * Pipeline: Fontconfig finds a face covering the badge's first codepoint →
 * FreeType opens it → HarfBuzz shapes the (possibly RTL/joining) run →
 * FreeType rasterises each glyph into an 8-bit coverage cell → the cell is
 * composited to straight-alpha ARGB32 with a dark halo for legibility, then
 * serialised big-endian for SNI.
 */

#include "icon_badge.h"

#include <fontconfig/fontconfig.h>
#include <harfbuzz/hb.h>
#include <harfbuzz/hb-ft.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <stdlib.h>
#include <string.h>

/* Process-lifetime, lazily initialised. The badge changes only on a language
 * switch, so re-matching a font per render is cheap; the heavyweight state
 * (FreeType library, Fontconfig config) is what we keep. */
static FT_Library g_ft;
static FcConfig  *g_fc;
static bool       g_init_failed;

static bool badge_ensure_init(void) {
    if (g_ft && g_fc) return true;
    if (g_init_failed) return false;
    if (!g_ft && FT_Init_FreeType(&g_ft) != 0) {
        g_init_failed = true;
        return false;
    }
    if (!g_fc) {
        g_fc = FcInitLoadConfigAndFonts();
        if (!g_fc) {
            g_init_failed = true;
            return false;
        }
    }
    return true;
}

void typio_icon_badge_shutdown(void) {
    if (g_ft) { FT_Done_FreeType(g_ft); g_ft = nullptr; }
    if (g_fc) { FcConfigDestroy(g_fc); g_fc = nullptr; }
    /* FcFini() is intentionally not called: other Fontconfig users may still be
     * live, and it is not required for clean process exit. */
    g_init_failed = false;
}

/* Decode the first UTF-8 codepoint of @s into @out_cp; returns false on empty
 * or malformed input. */
static bool utf8_first(const char *s, uint32_t *out_cp) {
    const unsigned char *p = (const unsigned char *)s;
    if (!p[0]) return false;
    if (p[0] < 0x80) { *out_cp = p[0]; return true; }
    if ((p[0] & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) {
        *out_cp = (uint32_t)((p[0] & 0x1F) << 6 | (p[1] & 0x3F));
        return true;
    }
    if ((p[0] & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
        *out_cp = (uint32_t)((p[0] & 0x0F) << 12 | (p[1] & 0x3F) << 6 |
                             (p[2] & 0x3F));
        return true;
    }
    if ((p[0] & 0xF8) == 0xF0 && (p[1] & 0xC0) == 0x80 &&
        (p[2] & 0xC0) == 0x80 && (p[3] & 0xC0) == 0x80) {
        *out_cp = (uint32_t)((p[0] & 0x07) << 18 | (p[1] & 0x3F) << 12 |
                             (p[2] & 0x3F) << 6 | (p[3] & 0x3F));
        return true;
    }
    return false;
}

/* Find a font file covering @cp via Fontconfig. Returns a strdup'd path (caller
 * frees) and sets *out_index, or NULL. */
static char *badge_match_font(uint32_t cp, int *out_index) {
    FcPattern *pat = FcPatternCreate();
    if (!pat) return nullptr;
    FcCharSet *cs = FcCharSetCreate();
    if (!cs) { FcPatternDestroy(pat); return nullptr; }
    FcCharSetAddChar(cs, cp);
    FcPatternAddCharSet(pat, FC_CHARSET, cs);
    FcPatternAddBool(pat, FC_SCALABLE, FcTrue);
    FcConfigSubstitute(g_fc, pat, FcMatchPattern);
    FcDefaultSubstitute(pat);

    FcResult res;
    FcPattern *match = FcFontMatch(g_fc, pat, &res);
    char *path = nullptr;
    if (match) {
        FcChar8 *file = nullptr;
        if (FcPatternGetString(match, FC_FILE, 0, &file) == FcResultMatch && file) {
            path = strdup((const char *)file);
        }
        int idx = 0;
        FcPatternGetInteger(match, FC_INDEX, 0, &idx);
        *out_index = idx;
        FcPatternDestroy(match);
    }
    FcCharSetDestroy(cs);
    FcPatternDestroy(pat);
    return path;
}

/* Coverage cell: an 8-bit, size×size buffer the glyphs are rasterised into. */
static void cell_stamp(uint8_t *cell, int size, const FT_Bitmap *bmp,
                       int x0, int y0) {
    for (unsigned row = 0; row < bmp->rows; row++) {
        int y = y0 + (int)row;
        if (y < 0 || y >= size) continue;
        const unsigned char *src = bmp->buffer + (size_t)row * (size_t)bmp->pitch;
        for (unsigned col = 0; col < bmp->width; col++) {
            int x = x0 + (int)col;
            if (x < 0 || x >= size) continue;
            uint8_t v = src[col];
            uint8_t *dst = &cell[(size_t)y * (size_t)size + (size_t)x];
            if (v > *dst) *dst = v; /* max-composite overlapping marks */
        }
    }
}

/* Render one square pixmap of side @size for the shaped run. Returns false on
 * failure (out untouched). */
static bool render_one(FT_Face face, hb_buffer_t *buf, const char *text,
                       int size, uint32_t fg_rgb, TypioBadgePixmap *out) {
    /* Glyphs occupy ~72% of the cell, leaving panel margin. */
    FT_UInt px = (FT_UInt)((size * 72) / 100);
    if (px < 6) px = (FT_UInt)size;
    if (FT_Set_Pixel_Sizes(face, 0, px) != 0) return false;

    hb_font_t *hb_font = hb_ft_font_create_referenced(face);
    if (!hb_font) return false;
    hb_buffer_clear_contents(buf);
    hb_buffer_add_utf8(buf, text, -1, 0, -1);
    hb_buffer_guess_segment_properties(buf); /* script/direction/language */
    hb_shape(hb_font, buf, nullptr, 0);

    unsigned n = 0;
    hb_glyph_info_t *info = hb_buffer_get_glyph_infos(buf, &n);
    hb_glyph_position_t *gpos = hb_buffer_get_glyph_positions(buf, &n);
    if (n == 0) { hb_font_destroy(hb_font); return false; }

    /* Total advance for horizontal centring. */
    long total_adv = 0;
    for (unsigned i = 0; i < n; i++) total_adv += gpos[i].x_advance;
    int run_w = (int)(total_adv / 64);

    long asc = face->size->metrics.ascender / 64;
    long desc = face->size->metrics.descender / 64; /* negative */
    int origin_x = (size - run_w) / 2;
    int baseline_y = (int)((size + (asc + desc)) / 2);

    uint8_t *cell = calloc((size_t)size * (size_t)size, 1);
    if (!cell) { hb_font_destroy(hb_font); return false; }

    long pen = 0;
    for (unsigned i = 0; i < n; i++) {
        if (FT_Load_Glyph(face, info[i].codepoint, FT_LOAD_RENDER) != 0) continue;
        FT_GlyphSlot s = face->glyph;
        int gx = origin_x + (int)(pen / 64) + (int)(gpos[i].x_offset / 64)
                 + s->bitmap_left;
        int gy = baseline_y - (int)(gpos[i].y_offset / 64) - s->bitmap_top;
        cell_stamp(cell, size, &s->bitmap, gx, gy);
        pen += gpos[i].x_advance;
    }
    hb_font_destroy(hb_font);

    /* Composite to big-endian ARGB32: white (fg) glyph over a black halo so the
     * badge reads on both light and dark panels. The halo is the 3×3 dilation
     * of the coverage minus the glyph itself. */
    uint8_t r = (uint8_t)(fg_rgb >> 16), g = (uint8_t)(fg_rgb >> 8),
            b = (uint8_t)fg_rgb;
    uint8_t *argb = malloc((size_t)size * (size_t)size * 4u);
    if (!argb) { free(cell); return false; }

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            uint8_t cov = cell[(size_t)y * (size_t)size + (size_t)x];
            uint8_t dil = cov;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int ny = y + dy, nx = x + dx;
                    if (ny < 0 || ny >= size || nx < 0 || nx >= size) continue;
                    uint8_t v = cell[(size_t)ny * (size_t)size + (size_t)nx];
                    if (v > dil) dil = v;
                }
            }
            /* Straight-alpha over: fg(α=cov) over black(α=halo). */
            float aF = cov / 255.0f;
            float aB = dil / 255.0f;
            float outA = aF + aB * (1.0f - aF);
            uint8_t A = (uint8_t)(outA * 255.0f + 0.5f);
            uint8_t R = 0, G = 0, B = 0;
            if (outA > 0.0f) {
                float scale = aF / outA; /* black contributes 0 to rgb */
                R = (uint8_t)(r * scale + 0.5f);
                G = (uint8_t)(g * scale + 0.5f);
                B = (uint8_t)(b * scale + 0.5f);
            }
            uint8_t *p = &argb[((size_t)y * (size_t)size + (size_t)x) * 4u];
            p[0] = A; p[1] = R; p[2] = G; p[3] = B; /* big-endian ARGB */
        }
    }
    free(cell);

    out->width = size;
    out->height = size;
    out->argb = argb;
    return true;
}

size_t typio_icon_badge_render(const char *text,
                               const int *sizes, size_t size_count,
                               uint32_t fg_rgb,
                               TypioBadgePixmap *out, size_t out_cap) {
    if (!text || !text[0] || !sizes || size_count == 0 || !out ||
        out_cap < size_count) {
        return 0;
    }
    if (!badge_ensure_init()) return 0;

    uint32_t cp;
    if (!utf8_first(text, &cp)) return 0;

    int index = 0;
    char *path = badge_match_font(cp, &index);
    if (!path) return 0;

    FT_Face face = nullptr;
    if (FT_New_Face(g_ft, path, index, &face) != 0) {
        free(path);
        return 0;
    }
    free(path);

    hb_buffer_t *buf = hb_buffer_create();
    if (!buf || !hb_buffer_allocation_successful(buf)) {
        if (buf) hb_buffer_destroy(buf);
        FT_Done_Face(face);
        return 0;
    }

    size_t produced = 0;
    for (size_t i = 0; i < size_count; i++) {
        if (sizes[i] <= 0) goto fail;
        if (!render_one(face, buf, text, sizes[i], fg_rgb, &out[produced])) {
            goto fail;
        }
        produced++;
    }

    hb_buffer_destroy(buf);
    FT_Done_Face(face);
    return produced;

fail:
    for (size_t i = 0; i < produced; i++) typio_badge_pixmap_free(&out[i]);
    hb_buffer_destroy(buf);
    FT_Done_Face(face);
    return 0;
}

void typio_badge_pixmap_free(TypioBadgePixmap *pixmap) {
    if (!pixmap) return;
    free(pixmap->argb);
    pixmap->argb = nullptr;
    pixmap->width = 0;
    pixmap->height = 0;
}
