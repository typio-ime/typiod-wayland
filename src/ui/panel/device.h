/**
 * @file device.h
 * @brief Shared GPU render device (lazily created flux device).
 */

#ifndef TYPIO_WL_RENDER_DEVICE_H
#define TYPIO_WL_RENDER_DEVICE_H

#include "typio_build_config.h"

#ifdef HAVE_FLUX
#include <flux/flux.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Shared, lazily-created flux device.
 *
 * Created with the Wayland WSI instance extensions and the swapchain device
 * extension so the panel surface can present a Vulkan swapchain directly onto
 * its zwp_input_popup_surface_v2 wl_surface. Returns NULL if no Vulkan device
 * is available. The device lives for the process; it is owned here, not by any
 * single panel. */
flux_device *typio_render_device_get(void);

#ifdef __cplusplus
}
#endif

#endif /* HAVE_FLUX */

#endif /* TYPIO_WL_RENDER_DEVICE_H */
