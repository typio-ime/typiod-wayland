/**
 * @file icon_badge.h
 * @brief CPU rasteriser for language text badges into SNI ARGB32 pixmaps.
 *
 * The tray base icon's floor (ADR-0032) is a 1–3 glyph label in the language's
 * own script (中 / あ / الد / EN). The StatusNotifierItem `IconPixmap` channel
 * carries raw ARGB32 bitmaps, so the host must rasterise the glyphs itself —
 * the flux/GPU glyph atlas (ADR-0012) is an R8 coverage texture for the Vulkan
 * composer and cannot feed D-Bus.
 *
 * This unit is deliberately independent of the panel/flux stack: it uses
 * FreeType + HarfBuzz (correct shaping is mandatory — Arabic badges such as الد
 * require contextual joining) + Fontconfig (to find a face covering the badge's
 * script), and nothing else. It builds in systray-only configurations.
 *
 * Output bytes are big-endian ARGB32 as SNI/KStatusNotifierItem require.
 */
#ifndef TYPIO_TRAY_ICON_BADGE_H
#define TYPIO_TRAY_ICON_BADGE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* One rasterised badge bitmap at a single size. `argb` is width*height pixels,
 * 4 bytes each, big-endian ARGB32 (SNI byte order); owned by the struct. */
typedef struct TypioBadgePixmap {
    int32_t  width;
    int32_t  height;
    uint8_t *argb;
} TypioBadgePixmap;

/**
 * @brief Rasterise @p text into one pixmap per requested size.
 *
 * @p text is a short UTF-8 badge (1–3 glyphs), single script. @p sizes lists
 * square pixel sizes (e.g. {22, 44} for HiDPI). @p fg_rgb is the glyph colour
 * as 0xRRGGBB; glyphs are drawn at full coverage with a thin dark outline so
 * they stay legible on both light and dark panels. Results are written into
 * @p out (caller-provided, at least @p size_count entries).
 *
 * @return number of pixmaps produced (== size_count on success, 0 on any
 *         failure — caller must then fall back to an icon name). On partial
 *         failure nothing is written and 0 is returned; @p out is untouched.
 *
 * Free each produced pixmap with typio_badge_pixmap_free().
 */
size_t typio_icon_badge_render(const char *text,
                               const int *sizes, size_t size_count,
                               uint32_t fg_rgb,
                               TypioBadgePixmap *out, size_t out_cap);

/** Release a pixmap's buffer and zero the struct. */
void typio_badge_pixmap_free(TypioBadgePixmap *pixmap);

/** Release process-lifetime FreeType/Fontconfig state. Call at tray teardown. */
void typio_icon_badge_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* TYPIO_TRAY_ICON_BADGE_H */
