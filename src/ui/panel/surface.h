/**
 * @file surface.h
 * @brief TypioPanelSurface — the panel's on-screen presentation backend.
 *
 * Owns the input-popup wl_surface, the HiDPI helpers, the Vulkan swapchain
 * (via flux), the bounded present + stall-recovery loop, the frame-retire
 * ring, and the output/scale tracking. The orchestrating TypioPanel composes
 * a PanelGeometry and hands it here to be presented; this module never decides
 * *what* to draw, only *how* to put it on screen.
 */

#ifndef TYPIO_WL_PANEL_SURFACE_H
#define TYPIO_WL_PANEL_SURFACE_H

#include "layout.h"      /* PanelGeometry */
#include "text_shaper.h" /* TypioTextShape */

#include <stdbool.h>

struct wl_output;

typedef struct TypioWlFrontend TypioWlFrontend;
typedef struct TypioPanel TypioPanel;
typedef struct TypioPanelSurface TypioPanelSurface;

typedef enum {
    PANEL_PRESENT_OK,     /* frame presented */
    PANEL_PRESENT_RETRY,  /* transient stall; skip this frame, re-present later */
    PANEL_PRESENT_FAIL,   /* hard failure */
} PanelPresentResult;

/* Create the surface objects (wl_surface + input-popup surface + HiDPI
 * helpers). `owner` is the panel re-rendered on a scale/output change. Returns
 * NULL if the compositor/input-method globals are missing or on failure. */
TypioPanelSurface *panel_surface_create(TypioWlFrontend *frontend, TypioPanel *owner);
void panel_surface_destroy(TypioPanelSurface *s);

bool panel_surface_is_available(const TypioPanelSurface *s);
bool panel_surface_present_retry_pending(const TypioPanelSurface *s);
bool panel_surface_fx_ready(const TypioPanelSurface *s);

/* Logical-to-physical scale ratio resolved from the surface's scale signals. */
float panel_surface_scale(const TypioPanelSurface *s);

/* Clear the pending-retry flag at the start of an update cycle. */
void panel_surface_reset_retry(TypioPanelSurface *s);

/* Size the swapchain to `geom`, set the viewport/buffer-scale, and present one
 * frame. On OK the retire ring advances; on RETRY the retry flag is armed. */
PanelPresentResult panel_surface_present(TypioPanelSurface *s,
                                         const PanelGeometry *geom, int selected);

/* Unmap the surface (commit a null buffer); keeps the swapchain alive. */
void panel_surface_hide(TypioPanelSurface *s);

/* Frame-retire ring: defer freeing geometry/shapes until the GPU has consumed
 * the frames that may still reference them. */
void panel_surface_retire_geometry(TypioPanelSurface *s, PanelGeometry *g);
void panel_surface_retire_shape(TypioPanelSurface *s, TypioTextShape *shape);
/* Fence the device and drain every retire slot (config reload / teardown). */
void panel_surface_drain_retire_fenced(TypioPanelSurface *s);

void panel_surface_handle_output_change(TypioPanelSurface *s, struct wl_output *output);

#endif /* TYPIO_WL_PANEL_SURFACE_H */
