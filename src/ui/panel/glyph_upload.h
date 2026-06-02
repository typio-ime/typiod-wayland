/**
 * @file glyph_upload.h
 * @brief Persistent Vulkan staging context for glyph-atlas sub-region uploads.
 *
 * Owns a command pool, staging buffer, and fence reused across every glyph
 * upload, so a cache miss costs one buffer copy instead of a full
 * VkCommandPool + VkBuffer + VkFence create/destroy cycle (~50us each). The
 * upload is synchronous from the CPU's perspective (fence wait after submit);
 * the atlas batches misses so a warmed atlas uploads nothing during navigation.
 */
#ifndef TYPIO_WL_GLYPH_UPLOAD_H
#define TYPIO_WL_GLYPH_UPLOAD_H

#include <flux/flux.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Upload an R8 sub-region (@w×@h at offset @x,@y, @bytes of tightly packed
 * coverage) into @img. The staging buffer grows on demand and is reused.
 * Returns false on any device/allocation failure. Synchronous: the copy has
 * completed on return. */
bool glyph_upload_region(flux_image *img,
                         uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                         const void *data, size_t bytes);

/* Destroy the persistent context. The caller must have drained the device
 * (or there is no device yet). Safe to call when never initialised. */
void glyph_upload_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* TYPIO_WL_GLYPH_UPLOAD_H */
