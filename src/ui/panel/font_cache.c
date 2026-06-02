/**
 * @file font_cache.c
 * @brief FT_Face + hb_font_t object cache (see header).
 */
#include "font_cache.h"

#include <harfbuzz/hb-ft.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MULTIPLE_MASTERS_H
#include FT_TRUETYPE_TABLES_H

#include <stdlib.h>
#include <string.h>

static FT_Library ft_library;

bool font_cache_init(void)
{
    if (ft_library) return true;
    if (FT_Init_FreeType(&ft_library) != 0) {
        ft_library = NULL;
        return false;
    }
    return true;
}

/* ── Object cache ───────────────────────────────────────────────────────── */
#define FONT_OBJ_CACHE_INIT_CAP 64

static FontObj  *cache;
static size_t    cache_count;
static size_t    cache_cap;
static uint32_t  next_font_id = 1;   /* monotonic; never reset (see header) */

void font_cache_clear(void)
{
    for (size_t i = 0; i < cache_count; ++i) {
        if (cache[i].hb_font) hb_font_destroy(cache[i].hb_font);
        if (cache[i].face)    FT_Done_Face(cache[i].face);
        free(cache[i].path);
    }
    free(cache);
    cache = NULL;
    cache_count = 0;
    cache_cap = 0;
}

static FontObj *cache_lookup(const char *path, float size, int32_t weight)
{
    for (size_t i = 0; i < cache_count; ++i) {
        if (cache[i].size == size && cache[i].weight == weight &&
            strcmp(cache[i].path, path) == 0) {
            return &cache[i];
        }
    }
    return NULL;
}

/* Returns false only on allocation failure (caller then disposes the face). */
static bool cache_insert(const char *path, float size, int32_t weight,
                         FT_Face face, hb_font_t *hb_font, uint32_t font_id)
{
    if (cache_count == cache_cap) {
        size_t newcap = cache_cap ? cache_cap * 2 : FONT_OBJ_CACHE_INIT_CAP;
        FontObj *grown = realloc(cache, newcap * sizeof(*grown));
        if (!grown) return false;
        cache = grown;
        cache_cap = newcap;
    }

    FontObj *e = &cache[cache_count++];
    e->path    = strdup(path);
    e->size    = size;
    e->weight  = weight;
    e->face    = face;
    e->hb_font = hb_font;
    e->font_id = font_id;
    return true;
}

/* Drive a variable font's 'wght' axis to @weight, if it has one. */
static bool set_face_weight(FT_Face face, int32_t weight)
{
    FT_MM_Var *amaster = NULL;
    FT_Fixed  *coords  = NULL;
    FT_Error   err;
    FT_UInt    i;
    bool       ok = false;

    err = FT_Get_MM_Var(face, &amaster);
    if (err != 0) return false;

    coords = (FT_Fixed *)calloc(amaster->num_axis, sizeof(FT_Fixed));
    if (!coords) goto done;

    err = FT_Get_Var_Design_Coordinates(face, amaster->num_axis, coords);
    if (err != 0) goto done;

    for (i = 0; i < amaster->num_axis; ++i) {
        if (amaster->axis[i].tag == ((FT_ULong)'w' << 24 |
                                     (FT_ULong)'g' << 16 |
                                     (FT_ULong)'h' << 8  | 't')) {
            coords[i] = (FT_Fixed)weight * 65536;
            ok = true;
            break;
        }
    }

    if (ok) {
        err = FT_Set_Var_Design_Coordinates(face, amaster->num_axis, coords);
        ok = (err == 0);
    }

done:
    free(coords);
    FT_Done_MM_Var(ft_library, amaster);
    return ok;
}

FontObj *font_cache_get_or_create(const char *path, float size, int32_t weight)
{
    FontObj *entry = cache_lookup(path, size, weight);
    if (entry) return entry;
    if (!ft_library) return NULL;

    FT_Face face = NULL;
    if (FT_New_Face(ft_library, path, 0, &face) != 0) return NULL;

    for (int i = 0; i < face->num_charmaps; i++) {
        if (FT_Get_CMap_Format(face->charmaps[i]) == 12) {
            FT_Set_Charmap(face, face->charmaps[i]);
            break;
        }
    }

    set_face_weight(face, weight);

    if (FT_Set_Pixel_Sizes(face, 0, (FT_UInt)(size + 0.5f)) != 0) {
        FT_Done_Face(face);
        return NULL;
    }

    hb_font_t *hb_font = hb_ft_font_create_referenced(face);
    if (!hb_font) {
        FT_Done_Face(face);
        return NULL;
    }

    uint32_t font_id = next_font_id++;
    if (!cache_insert(path, size, weight, face, hb_font, font_id)) {
        hb_font_destroy(hb_font);
        FT_Done_Face(face);
        return NULL;
    }
    return cache_lookup(path, size, weight);
}
