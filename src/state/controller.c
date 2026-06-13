/**
 * @file controller.c
 * @brief StateController implementation
 */

#include "state/controller.h"
#include "typio/runtime/registry.h"
#include "typio/runtime/instance.h"
#include "typio/abi/config.h"
#include "typio/abi/engine.h"
#include "typio/abi/string.h"
#include "typio/typio.h"
#include "typio/abi/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct TypioStateController {
    TypioInstance *instance;

    /* -- cached state snapshots ------------------------------------------- */
    char *active_engine_name;
    char *active_engine_display_name;
    char *active_voice_engine_name;
    char *active_voice_engine_display_name;
    char *active_language;
    char *status_icon;
    /* When the icon resolves to the language floor (ADR-0032), the tray renders
     * a text badge instead of looking up status_icon as a freedesktop name.
     * status_icon then holds the generic name as a render-failure fallback. */
    bool  status_icon_is_badge;
    char *status_badge_text;

    bool engine_active;

    bool has_status;
    TypioKeyboardEngineMode status;
    char *status_id;
    char *status_label;
    char *status_display_label;
    char *status_icon_name;

    /* -- listeners -------------------------------------------------------- */
    TypioStateListener *listeners;
    size_t listener_count;
    size_t listener_capacity;
};

/* -------------------------------------------------------------------------- */
/* Helpers                                                                    */
/* -------------------------------------------------------------------------- */

static char *typio_state_strdup(const char *src) {
    if (!src || !*src) {
        return nullptr;
    }
    return strdup(src);
}

/* Resolve the tray/indicator status icon by a language-first precedence chain
 * (ADR-0031). `info` is the just-changed keyboard engine (NULL when the slot
 * was emptied — e.g. switching to a layout-only language). `engine_changed`
 * gates reuse of the global dynamic icon, which is keyed to whatever engine
 * last reported a status icon.
 *
 *   1. dynamic mode/schema icon (same engine only) — most specific runtime state
 *   2. engine manifest `icon`                       — the engine's identity
 *   3. [languages.<tag>].icon config                — per-language override
 *   4. active language present                      — generic "on" (badge: Phase 2)
 *   5. engine present but iconless                  — generic "on"
 *   6. nothing active (no language, no engine)      — "off"
 *
 * Returns a freshly allocated string (never NULL); caller owns it. */
static char *typio_state_controller_resolve_status_icon(
    TypioStateController *ctrl, const TypioEngineInfo *info,
    bool engine_changed) {
    /* Default: a named icon, not a badge. Layer 4 flips this on. */
    ctrl->status_icon_is_badge = false;
    free(ctrl->status_badge_text);
    ctrl->status_badge_text = nullptr;

    /* 1. Same-engine dynamic icon (mode/schema set via the status-icon
     *    callback). Stale across engine changes, so gated on !engine_changed. */
    if (!engine_changed) {
        const char *dyn = typio_instance_get_last_status_icon(ctrl->instance);
        if (dyn && *dyn) {
            return strdup(dyn);
        }
    }
    /* 2. Engine's static manifest icon. */
    if (info && info->icon && info->icon[0]) {
        return strdup(info->icon);
    }
    /* 3/4. Language layers: a layout-only language (empty keyboard slot) still
     *      gets a meaningful, "on" icon instead of the off glyph. */
    TypioRegistry *registry =
        ctrl->instance ? typio_instance_get_registry(ctrl->instance) : nullptr;
    char *tag = registry ? typio_registry_get_active_language(registry) : nullptr;
    if (tag && tag[0]) {
        TypioConfig *cfg =
            ctrl->instance ? typio_instance_get_config(ctrl->instance) : nullptr;
        if (cfg) {
            char key[160];
            snprintf(key, sizeof(key), "languages.%s.icon", tag);
            const char *cfg_icon = typio_config_get_string(cfg, key, nullptr);
            if (cfg_icon && cfg_icon[0]) {
                free(tag);
                return strdup(cfg_icon); /* 3. explicit per-language icon */
            }
        }
        /* 4. Language floor: render a text badge in the language's script.
         *    status_icon keeps the generic name as a render-failure fallback. */
        char badge[32];
        typio_language_badge(tag, badge, sizeof(badge));
        if (badge[0]) {
            ctrl->status_icon_is_badge = true;
            ctrl->status_badge_text = strdup(badge);
        }
        free(tag);
        return strdup("typio-keyboard-symbolic"); /* active language, generic on */
    }
    free(tag);
    /* 5. Engine active but iconless. 6. Nothing active. */
    return strdup(info ? "typio-keyboard-symbolic" : "typio-keyboard-off-symbolic");
}

const char *typio_language_endonym(const char *tag) {
    if (!tag || !tag[0]) {
        return nullptr;
    }
    /* Match the primary subtag, ignoring any region/script suffix. */
    static const struct { const char *prefix; const char *name; } table[] = {
        { "ary", "الدارجة" },   /* Moroccan Darija (layout-only) */
        { "ar",  "العربية" },
        { "zh",  "中文" },
        { "ja",  "日本語" },
        { "ko",  "한국어" },
        { "en",  "English" },
        { "fr",  "Français" },
        { "de",  "Deutsch" },
        { "es",  "Español" },
        { "ru",  "Русский" },
    };
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
        size_t n = strlen(table[i].prefix);
        if (strncmp(tag, table[i].prefix, n) == 0 &&
            (tag[n] == '\0' || tag[n] == '-' || tag[n] == '_')) {
            return table[i].name;
        }
    }
    return tag;
}

void typio_language_badge(const char *tag, char *out, size_t out_size) {
    if (!out || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (!tag || !tag[0]) {
        return;
    }
    /* Match the primary subtag, ignoring any region/script suffix. */
    static const struct { const char *prefix; const char *badge; } table[] = {
        { "ary", "الد" },   /* Moroccan Darija — distinct from MSA's ع */
        { "ar",  "ع" },
        { "zh",  "中" },
        { "ja",  "あ" },
        { "ko",  "한" },
        { "en",  "EN" },
        { "fr",  "FR" },
        { "de",  "DE" },
        { "es",  "ES" },
        { "ru",  "Рус" },
    };
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
        size_t n = strlen(table[i].prefix);
        if (strncmp(tag, table[i].prefix, n) == 0 &&
            (tag[n] == '\0' || tag[n] == '-' || tag[n] == '_')) {
            snprintf(out, out_size, "%s", table[i].badge);
            return;
        }
    }
    /* Fallback: the uppercased primary subtag (e.g. "ary-x" -> "ARY"). */
    size_t i = 0;
    for (; tag[i] && tag[i] != '-' && tag[i] != '_' && i + 1 < out_size; i++) {
        char c = tag[i];
        out[i] = (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
    }
    out[i] = '\0';
}

static void typio_state_controller_broadcast(TypioStateController *ctrl,
                                             TypioStateChangeType change_type) {
    if (!ctrl) {
        return;
    }
    for (size_t i = 0; i < ctrl->listener_count; i++) {
        TypioStateListener *l = &ctrl->listeners[i];
        if (l->callback) {
            l->callback(l->user_data, change_type);
        }
    }
}

/* Refresh the active-language snapshot from the registry. Returns true when
 * the language changed. The registry has no dedicated language callback;
 * every language activation fires the keyboard/voice engine callbacks, so
 * diffing here catches all transitions — including layout-only languages
 * where the new slot state is "no engine" (ADR-0031). */
static bool typio_state_controller_refresh_language(TypioStateController *ctrl) {
    if (!ctrl || !ctrl->instance) {
        return false;
    }
    TypioRegistry *registry = typio_instance_get_registry(ctrl->instance);
    char *lang = registry ? typio_registry_get_active_language(registry) : nullptr;
    bool changed;
    if (!lang || !ctrl->active_language) {
        changed = (lang != nullptr) != (ctrl->active_language != nullptr);
    } else {
        changed = strcmp(lang, ctrl->active_language) != 0;
    }
    if (changed) {
        free(ctrl->active_language);
        ctrl->active_language = typio_state_strdup(lang);
    }
    typio_free_string(lang);
    return changed;
}

static void typio_state_controller_update_engine_active(
    TypioStateController *ctrl) {
    if (!ctrl || !ctrl->instance) {
        return;
    }
    TypioRegistry *registry = typio_instance_get_registry(ctrl->instance);
    char *active_name =
        registry ? typio_registry_get_active_keyboard(registry) : nullptr;
    ctrl->engine_active = active_name != nullptr;
    typio_free_string(active_name);
}

static void typio_state_controller_clear_mode(TypioStateController *ctrl) {
    free(ctrl->status_id);
    free(ctrl->status_label);
    free(ctrl->status_display_label);
    free(ctrl->status_icon_name);
    ctrl->status_id = nullptr;
    ctrl->status_label = nullptr;
    ctrl->status_display_label = nullptr;
    ctrl->status_icon_name = nullptr;
    ctrl->has_status = false;
    memset(&ctrl->status, 0, sizeof(ctrl->status));
}

static void typio_state_controller_set_mode(TypioStateController *ctrl,
                                            const TypioKeyboardEngineMode *mode) {
    typio_state_controller_clear_mode(ctrl);
    if (!mode) {
        return;
    }
    ctrl->has_status = true;
    ctrl->status.id = ctrl->status_id = typio_state_strdup(mode->id);
    ctrl->status.label =
        ctrl->status_label = typio_state_strdup(mode->label);
    ctrl->status.display_label =
        ctrl->status_display_label = typio_state_strdup(mode->display_label);
    ctrl->status.icon_name = ctrl->status_icon_name = typio_state_strdup(mode->icon_name);
}

/* -------------------------------------------------------------------------- */
/* Lifecycle                                                                  */
/* -------------------------------------------------------------------------- */

TypioStateController *typio_state_controller_new(TypioInstance *instance) {
    if (!instance) {
        return nullptr;
    }
    TypioStateController *ctrl = calloc(1, sizeof(TypioStateController));
    if (!ctrl) {
        return nullptr;
    }
    ctrl->instance = instance;
    ctrl->listener_capacity = 4;
    ctrl->listeners = calloc(ctrl->listener_capacity, sizeof(TypioStateListener));
    if (!ctrl->listeners) {
        free(ctrl);
        return nullptr;
    }
    return ctrl;
}

void typio_state_controller_free(TypioStateController *ctrl) {
    if (!ctrl) {
        return;
    }
    free(ctrl->active_engine_name);
    free(ctrl->active_engine_display_name);
    free(ctrl->active_voice_engine_name);
    free(ctrl->active_voice_engine_display_name);
    free(ctrl->active_language);
    free(ctrl->status_icon);
    free(ctrl->status_badge_text);
    typio_state_controller_clear_mode(ctrl);
    free(ctrl->listeners);
    free(ctrl);
}

/* -------------------------------------------------------------------------- */
/* Listeners                                                                  */
/* -------------------------------------------------------------------------- */

void typio_state_controller_add_listener(TypioStateController *ctrl,
                                         TypioStateListener listener) {
    if (!ctrl) {
        return;
    }
    if (ctrl->listener_count >= ctrl->listener_capacity) {
        size_t new_cap = ctrl->listener_capacity * 2;
        TypioStateListener *new_list =
            realloc(ctrl->listeners, new_cap * sizeof(TypioStateListener));
        if (!new_list) {
            typio_log_error("Failed to grow state-controller listener list");
            return;
        }
        ctrl->listeners = new_list;
        ctrl->listener_capacity = new_cap;
    }
    ctrl->listeners[ctrl->listener_count++] = listener;
}

void typio_state_controller_remove_listener(TypioStateController *ctrl,
                                            void *user_data) {
    if (!ctrl) {
        return;
    }
    for (size_t i = 0; i < ctrl->listener_count; i++) {
        if (ctrl->listeners[i].user_data == user_data) {
            /* shift remaining entries down */
            size_t rest = ctrl->listener_count - i - 1;
            if (rest > 0) {
                memmove(&ctrl->listeners[i],
                        &ctrl->listeners[i + 1],
                        rest * sizeof(TypioStateListener));
            }
            ctrl->listener_count--;
            return;
        }
    }
}

/* -------------------------------------------------------------------------- */
/* Queries                                                                    */
/* -------------------------------------------------------------------------- */

const char *typio_state_controller_get_active_engine_name(
    TypioStateController *ctrl) {
    return ctrl ? ctrl->active_engine_name : nullptr;
}

const char *typio_state_controller_get_active_engine_display_name(
    TypioStateController *ctrl) {
    return ctrl ? ctrl->active_engine_display_name : nullptr;
}

const char *typio_state_controller_get_active_voice_engine_name(
    TypioStateController *ctrl) {
    return ctrl ? ctrl->active_voice_engine_name : nullptr;
}

const char *typio_state_controller_get_active_voice_engine_display_name(
    TypioStateController *ctrl) {
    return ctrl ? ctrl->active_voice_engine_display_name : nullptr;
}

const char *typio_state_controller_get_active_language(
    TypioStateController *ctrl) {
    return ctrl ? ctrl->active_language : nullptr;
}

const char *typio_state_controller_get_status_icon(
    TypioStateController *ctrl) {
    return ctrl ? ctrl->status_icon : nullptr;
}

bool typio_state_controller_get_status_icon_is_badge(
    TypioStateController *ctrl) {
    return ctrl ? ctrl->status_icon_is_badge : false;
}

const char *typio_state_controller_get_status_badge_text(
    TypioStateController *ctrl) {
    return ctrl ? ctrl->status_badge_text : nullptr;
}

bool typio_state_controller_get_engine_active(
    TypioStateController *ctrl) {
    return ctrl ? ctrl->engine_active : false;
}

const TypioKeyboardEngineMode *typio_state_controller_get_current_status(
    TypioStateController *ctrl) {
    if (!ctrl || !ctrl->has_status) {
        return nullptr;
    }
    return &ctrl->status;
}

/* -------------------------------------------------------------------------- */
/* Core notifications                                                         */
/* -------------------------------------------------------------------------- */

void typio_state_controller_notify_engine_changed(
    TypioStateController *ctrl,
    const TypioEngineInfo *info) {
    if (!ctrl) {
        return;
    }

    /* Determine if the engine actually changed before freeing old name. */
    bool engine_changed = true;
    if (ctrl->active_engine_name && info && info->name) {
        engine_changed = strcmp(ctrl->active_engine_name, info->name) != 0;
    }

    free(ctrl->active_engine_name);
    free(ctrl->active_engine_display_name);
    ctrl->active_engine_name =
        (info && info->name) ? strdup(info->name) : nullptr;
    ctrl->active_engine_display_name =
        (info && info->display_name) ? strdup(info->display_name) : nullptr;

    /* Re-evaluate the status icon via the language-first precedence chain.
     * A layout-only language (info == NULL but a language is active) resolves
     * to an "on" icon rather than the off glyph (ADR-0031). */
    free(ctrl->status_icon);
    ctrl->status_icon =
        typio_state_controller_resolve_status_icon(ctrl, info, engine_changed);

    typio_state_controller_update_engine_active(ctrl);
    typio_state_controller_broadcast(ctrl, TYPIO_STATE_CHANGE_ENGINE);
    if (typio_state_controller_refresh_language(ctrl)) {
        typio_state_controller_broadcast(ctrl, TYPIO_STATE_CHANGE_LANGUAGE);
    }
}

void typio_state_controller_notify_voice_engine_changed(
    TypioStateController *ctrl,
    const TypioEngineInfo *info) {
    if (!ctrl) {
        return;
    }
    free(ctrl->active_voice_engine_name);
    free(ctrl->active_voice_engine_display_name);
    ctrl->active_voice_engine_name =
        (info && info->name) ? strdup(info->name) : nullptr;
    ctrl->active_voice_engine_display_name =
        (info && info->display_name) ? strdup(info->display_name) : nullptr;
    typio_state_controller_broadcast(ctrl, TYPIO_STATE_CHANGE_VOICE_ENGINE);
    if (typio_state_controller_refresh_language(ctrl)) {
        typio_state_controller_broadcast(ctrl, TYPIO_STATE_CHANGE_LANGUAGE);
    }
}

void typio_state_controller_notify_status_changed(
    TypioStateController *ctrl,
    const TypioKeyboardEngineMode *mode) {
    if (!ctrl) {
        return;
    }
    typio_state_controller_set_mode(ctrl, mode);
    if (mode && mode->icon_name && mode->icon_name[0]) {
        /* A mode icon is the most specific layer (1): a named icon, not a
         * badge, so it supersedes any language-floor badge. */
        free(ctrl->status_icon);
        ctrl->status_icon = strdup(mode->icon_name);
        ctrl->status_icon_is_badge = false;
        free(ctrl->status_badge_text);
        ctrl->status_badge_text = nullptr;
    }
    typio_state_controller_broadcast(ctrl, TYPIO_STATE_CHANGE_STATUS);
}

void typio_state_controller_notify_status_icon_changed(
    TypioStateController *ctrl,
    const char *icon_name) {
    if (!ctrl) {
        return;
    }
    free(ctrl->status_icon);
    ctrl->status_icon = typio_state_strdup(icon_name);
    if (icon_name && icon_name[0]) {
        /* An engine-pushed named icon supersedes the language-floor badge. */
        ctrl->status_icon_is_badge = false;
        free(ctrl->status_badge_text);
        ctrl->status_badge_text = nullptr;
    }
    typio_state_controller_broadcast(ctrl, TYPIO_STATE_CHANGE_STATUS_ICON);
}

/* -------------------------------------------------------------------------- */
/* Sync                                                                       */
/* -------------------------------------------------------------------------- */

void typio_state_controller_sync(TypioStateController *ctrl) {
    if (!ctrl || !ctrl->instance) {
        return;
    }

    TypioRegistry *registry = typio_instance_get_registry(ctrl->instance);

    /* Engine */
    char *active_kb_name = registry
        ? typio_registry_get_active_keyboard(registry) : nullptr;
    char *active_kb_display = (active_kb_name && registry)
        ? typio_registry_get_engine_display_name(registry, active_kb_name)
        : nullptr;
    char *active_kb_icon = (active_kb_name && registry)
        ? typio_registry_get_engine_icon(registry, active_kb_name)
        : nullptr;
    {
        free(ctrl->active_engine_name);
        free(ctrl->active_engine_display_name);
        ctrl->active_engine_name = active_kb_name
            ? typio_state_strdup(active_kb_name) : nullptr;
        ctrl->active_engine_display_name = (active_kb_display && *active_kb_display)
            ? typio_state_strdup(active_kb_display) : nullptr;
        ctrl->engine_active = active_kb_name != nullptr;
    }

    /* Voice engine */
    {
        char *voice_name = registry
            ? typio_registry_get_active_voice(registry) : nullptr;
        char *voice_display = (voice_name && registry)
            ? typio_registry_get_engine_display_name(registry, voice_name)
            : nullptr;
        free(ctrl->active_voice_engine_name);
        free(ctrl->active_voice_engine_display_name);
        ctrl->active_voice_engine_name = voice_name
            ? typio_state_strdup(voice_name) : nullptr;
        ctrl->active_voice_engine_display_name = (voice_display && *voice_display)
            ? typio_state_strdup(voice_display) : nullptr;
        typio_free_string(voice_name);
        typio_free_string(voice_display);
    }

    /* Status icon */
    {
        free(ctrl->status_icon);
        const char *icon = typio_instance_get_last_status_icon(ctrl->instance);
        if (icon && *icon) {
            ctrl->status_icon = strdup(icon);
        } else if (active_kb_icon && *active_kb_icon) {
            ctrl->status_icon = strdup(active_kb_icon);
        } else if (active_kb_name) {
            ctrl->status_icon = strdup("typio-keyboard-symbolic");
        } else {
            ctrl->status_icon = strdup("typio-keyboard-off-symbolic");
        }
    }

    typio_free_string(active_kb_icon);
    typio_free_string(active_kb_display);
    typio_free_string(active_kb_name);

    /* Mode — we cannot query current mode directly from instance, so we
     * clear it and wait for the next mode notification from Core. */
    typio_state_controller_clear_mode(ctrl);

    typio_state_controller_refresh_language(ctrl);

    /* Broadcast every change type so listeners perform a full refresh. */
    typio_state_controller_broadcast(ctrl, TYPIO_STATE_CHANGE_ENGINE);
    typio_state_controller_broadcast(ctrl, TYPIO_STATE_CHANGE_VOICE_ENGINE);
    typio_state_controller_broadcast(ctrl, TYPIO_STATE_CHANGE_LANGUAGE);
    typio_state_controller_broadcast(ctrl, TYPIO_STATE_CHANGE_STATUS_ICON);
}
