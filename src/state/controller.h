/**
 * @file controller.h
 * @brief Centralized state controller — single source of truth for runtime surfaces
 *
 * The StateController sits between the Core layer and external runtime surfaces
 * (system tray, D-Bus status bus, etc.). It:
 *
 *   1. Maintains a snapshot of user-visible state (active engine, mode, icon).
 *   2. Provides query APIs so surfaces read state from ONE place instead of
 *      reaching directly into TypioInstance.
 *   3. Broadcasts change notifications to registered listeners so every surface
 *      updates uniformly.
 *
 * All external surfaces (tray, status bus, and any future UI) should base their
 * behaviour on this layer.
 */

#ifndef TYPIO_STATE_CONTROLLER_H
#define TYPIO_STATE_CONTROLLER_H

#include "typio/abi/types.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TypioStateController TypioStateController;

typedef enum {
    TYPIO_STATE_CHANGE_ENGINE,
    TYPIO_STATE_CHANGE_VOICE_ENGINE,
    TYPIO_STATE_CHANGE_LANGUAGE,
    TYPIO_STATE_CHANGE_STATUS,
    TYPIO_STATE_CHANGE_STATUS_ICON,
} TypioStateChangeType;

typedef void (*TypioStateChangeCallback)(void *user_data,
                                         TypioStateChangeType change_type);

typedef struct TypioStateListener {
    void *user_data;
    TypioStateChangeCallback callback;
} TypioStateListener;

/* -------------------------------------------------------------------------- */
/* Lifecycle                                                                  */
/* -------------------------------------------------------------------------- */

TypioStateController *typio_state_controller_new(TypioInstance *instance);
void typio_state_controller_free(TypioStateController *ctrl);

/* -------------------------------------------------------------------------- */
/* Listener registration                                                      */
/* -------------------------------------------------------------------------- */

void typio_state_controller_add_listener(TypioStateController *ctrl,
                                         TypioStateListener listener);
void typio_state_controller_remove_listener(TypioStateController *ctrl,
                                            void *user_data);

/* -------------------------------------------------------------------------- */
/* State queries — single source of truth for external surfaces               */
/* -------------------------------------------------------------------------- */

const char *typio_state_controller_get_active_engine_name(
    TypioStateController *ctrl);
const char *typio_state_controller_get_active_engine_display_name(
    TypioStateController *ctrl);
const char *typio_state_controller_get_active_voice_engine_name(
    TypioStateController *ctrl);
const char *typio_state_controller_get_active_voice_engine_display_name(
    TypioStateController *ctrl);
const char *typio_state_controller_get_active_language(
    TypioStateController *ctrl);
const char *typio_state_controller_get_status_icon(
    TypioStateController *ctrl);
/* True when the status icon resolved to the language floor and should be drawn
 * as a text badge (ADR-0032) rather than looked up as a freedesktop name.
 * get_status_icon() then holds a generic name for render-failure fallback. */
bool typio_state_controller_get_status_icon_is_badge(
    TypioStateController *ctrl);
/* The badge text (language script glyphs) when is_badge is true, else NULL. */
const char *typio_state_controller_get_status_badge_text(
    TypioStateController *ctrl);
bool typio_state_controller_get_engine_active(
    TypioStateController *ctrl);
const TypioKeyboardEngineMode *typio_state_controller_get_current_status(
    TypioStateController *ctrl);

/**
 * @brief Map a BCP-47 language tag to its endonym for display.
 *
 * The registry exposes only the raw tag (ADR-0031); libtypio has no
 * language-display API, so the host owns this presentation table. Returns the
 * tag itself for anything unlisted (so new languages still render), or NULL for
 * a NULL/empty tag. The returned string is static — do not free.
 */
const char *typio_language_endonym(const char *tag);

/**
 * @brief Compact one-to-three glyph badge for a BCP-47 tag (e.g. 中 / あ / الد
 *        / EN), written into @p out.
 *
 * The language is the reliable visual identity (ADR-0031): it is always present
 * — even for layout-only languages with no engine — and stable across engine /
 * mode churn. This badge is the icon-sized form used by the on-screen indicator
 * (and, in future, the tray pixmap). Unlisted tags fall back to the uppercased
 * primary subtag (e.g. `ary-x` → `ARY`). @p out is set to an empty string for a
 * NULL/empty tag.
 */
void typio_language_badge(const char *tag, char *out, size_t out_size);

/* -------------------------------------------------------------------------- */
/* Notifications from Core — called by the daemon's Rust→C callbacks          */
/* -------------------------------------------------------------------------- */

void typio_state_controller_notify_engine_changed(
    TypioStateController *ctrl,
    const TypioEngineInfo *info);
void typio_state_controller_notify_voice_engine_changed(
    TypioStateController *ctrl,
    const TypioEngineInfo *info);
void typio_state_controller_notify_status_changed(
    TypioStateController *ctrl,
    const TypioKeyboardEngineMode *mode);
void typio_state_controller_notify_status_icon_changed(
    TypioStateController *ctrl,
    const char *icon_name);

/**
 * @brief Re-read all state from Core and broadcast changes.
 *
 * Call once after all listeners have registered (e.g. at startup) so every
 * surface receives an initial sync.
 */
void typio_state_controller_sync(TypioStateController *ctrl);

#ifdef __cplusplus
}
#endif

#endif /* TYPIO_STATE_CONTROLLER_H */
