/**
 * @file panel.h
 * @brief Public API for the candidate Panel — the floating IME UI.
 *
 * One TypioPanel owns the input-popup wl_surface, the GPU present pipeline,
 * the layout/shaper caches, and the theme. The frontend builds a
 * TypioPanelContent (candidates, preedit, status) and pushes it here.
 */

#ifndef TYPIO_WL_PANEL_H
#define TYPIO_WL_PANEL_H

#include "content.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct wl_output;

typedef struct TypioInputContext TypioInputContext;
typedef struct TypioWlFrontend TypioWlFrontend;
typedef struct TypioPanel TypioPanel;

typedef enum {
    TYPIO_PANEL_UPDATE_OK = 0,
    TYPIO_PANEL_UPDATE_RETRY,
    TYPIO_PANEL_UPDATE_FAIL,
} TypioPanelUpdateResult;

/* Create the panel: its wl_surface, input-popup surface, HiDPI helpers, and
 * shaper/layout caches. Returns NULL if the compositor or input-method
 * globals are unavailable (Panel disabled) or on allocation failure. */
TypioPanel *typio_panel_create(TypioWlFrontend *frontend);
void typio_panel_destroy(TypioPanel *panel);

/* Whether the panel has a usable input-popup surface. */
bool typio_panel_is_available(TypioPanel *panel);

/* Primary update path: composite the given content and present one frame. */
TypioPanelUpdateResult typio_panel_update_content(TypioPanel *panel,
                                                  const TypioPanelContent *content);

/* Convenience wrapper for the InputContext-only case (candidates + preedit). */
TypioPanelUpdateResult typio_panel_update(TypioPanel *panel, TypioInputContext *ctx);

/* Re-render the current content (called by the surface on a scale/output
 * change). No-op unless the panel is visible. */
void typio_panel_refresh(TypioPanel *panel);

void typio_panel_hide(TypioPanel *panel);
void typio_panel_invalidate_config(TypioPanel *panel);
void typio_panel_handle_output_change(TypioPanel *panel, struct wl_output *output);

/* Transient status banner (e.g. voice "[Recording...]") shown in the panel
 * when there are no candidates. Internally builds a TypioPanelContent. */
bool typio_panel_show_status(TypioPanel *panel, const char *text);
void typio_panel_hide_status(TypioPanel *panel);

#ifdef __cplusplus
}
#endif

#endif /* TYPIO_WL_PANEL_H */
