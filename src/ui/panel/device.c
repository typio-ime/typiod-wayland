/**
 * @file device.c
 * @brief Shared GPU render device (lazily created flux device).
 *
 * The panel surface presents a swapchain directly onto its
 * zwp_input_popup_surface_v2 wl_surface, so the device is created with the
 * Wayland WSI instance extensions and the swapchain device extension. The
 * device is a process-wide singleton owned here.
 */

#include "device.h"

#include <flux/flux.h>

static flux_device *global_device;

static void flux_log_cb(flux_log_level level,
                        const char *file, int line,
                        const char *fmt, const char *msg,
                        void *user)
{
    (void)level; (void)file; (void)line; (void)fmt; (void)msg; (void)user;
}

flux_device *typio_render_device_get(void)
{
    if (global_device) return global_device;

    /* Wayland WSI: the panel presents a swapchain directly onto its
     * zwp_input_popup_surface_v2 wl_surface, so the device needs the
     * surface + wayland-surface instance extensions and the swapchain
     * device extension. The graphics queue is assumed present-capable
     * (true on Mesa/Wayland). */
    static const char *const instance_exts[] = {
        "VK_KHR_surface",
        "VK_KHR_wayland_surface",
    };
    static const char *const device_exts[] = {
        "VK_KHR_swapchain",
    };

    flux_device_desc desc = FLUX_DEVICE_DESC_INIT;
    desc.log                               = flux_log_cb;
    desc.headless                          = false;
    desc.required_instance_extensions      = instance_exts;
    desc.required_instance_extension_count = 2;
    desc.required_device_extensions        = device_exts;
    desc.required_device_extension_count   = 1;
    desc.frames_in_flight                  = 2;

    if (flux_device_create(&desc, &global_device) != FLUX_OK) {
        global_device = NULL;
        return NULL;
    }
    return global_device;
}
