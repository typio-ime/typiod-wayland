#include "internal.h"
#include "ui/panel/panel.h"
#include "monotonic.h"
#include "typio/runtime/instance.h"
#include "typio/runtime/registry.h"
#include "typio/abi/config.h"
#include "typio/abi/log.h"
#include "typio/abi/string.h"

#include <sys/timerfd.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#define TYPIO_INDICATOR_DEFAULT_DURATION_MS 1500
#define TYPIO_INDICATOR_COMMIT_COOLDOWN_MS 8000

static int indicator_duration_ms(TypioWlFrontend *frontend) {
    if (!frontend || !frontend->instance) {
        return TYPIO_INDICATOR_DEFAULT_DURATION_MS;
    }
    TypioConfig *cfg = typio_instance_get_config(frontend->instance);
    if (!cfg) {
        return TYPIO_INDICATOR_DEFAULT_DURATION_MS;
    }
    int ms = typio_config_get_int(cfg, "display.indicator_duration_ms",
                                  TYPIO_INDICATOR_DEFAULT_DURATION_MS);
    if (ms < 100) ms = 100;
    if (ms > 10000) ms = 10000;
    return ms;
}

static bool indicator_enabled(TypioWlFrontend *frontend) {
    if (!frontend || !frontend->instance) {
        return true;
    }
    TypioConfig *cfg = typio_instance_get_config(frontend->instance);
    if (!cfg) {
        return true;
    }
    return typio_config_get_bool(cfg, "display.indicator_enabled", true);
}

static void arm_indicator_timer(TypioWlFrontend *frontend) {
    struct itimerspec its;
    memset(&its, 0, sizeof(its));
    int ms = indicator_duration_ms(frontend);
    its.it_value.tv_sec = ms / 1000;
    its.it_value.tv_nsec = (long)(ms % 1000) * 1000000L;
    if (frontend->indicator_timer_fd >= 0) {
        timerfd_settime(frontend->indicator_timer_fd, 0, &its, NULL);
    }
}

bool typio_wl_frontend_init_indicator(TypioWlFrontend *frontend) {
    if (!frontend) return false;
    frontend->indicator_timer_fd = timerfd_create(CLOCK_MONOTONIC,
                                                   TFD_CLOEXEC | TFD_NONBLOCK);
    if (frontend->indicator_timer_fd < 0) {
        typio_log_warning("Failed to create indicator timer: %s",
                          strerror(errno));
        return false;
    }
    frontend->indicator_active = false;
    return true;
}

static const char *mode_cache_lookup(TypioWlFrontend *frontend,
                                      const char *engine_name) {
    if (!frontend || !engine_name) return nullptr;
    for (size_t i = 0; i < frontend->indicator_mode_cache_count; i++) {
        if (strcmp(frontend->indicator_mode_cache[i].engine, engine_name) == 0) {
            if (frontend->indicator_mode_cache[i].display_label[0]) {
                return frontend->indicator_mode_cache[i].display_label;
            }
            return nullptr;
        }
    }
    return nullptr;
}

static void mode_cache_update(TypioWlFrontend *frontend,
                               const char *engine_name,
                               const TypioEngineStatus *mode) {
    if (!frontend || !engine_name || !mode) return;

    for (size_t i = 0; i < frontend->indicator_mode_cache_count; i++) {
        if (strcmp(frontend->indicator_mode_cache[i].engine, engine_name) == 0) {
            snprintf(frontend->indicator_mode_cache[i].display_label,
                     sizeof(frontend->indicator_mode_cache[i].display_label),
                     "%s", mode->display_label ? mode->display_label : "");
            return;
        }
    }

    if (frontend->indicator_mode_cache_count < TYPIO_INDICATOR_MODE_CACHE_CAP) {
        size_t idx = frontend->indicator_mode_cache_count++;
        snprintf(frontend->indicator_mode_cache[idx].engine,
                 sizeof(frontend->indicator_mode_cache[idx].engine),
                 "%s", engine_name);
        snprintf(frontend->indicator_mode_cache[idx].display_label,
                 sizeof(frontend->indicator_mode_cache[idx].display_label),
                 "%s", mode->display_label ? mode->display_label : "");
    }
}

static bool build_indicator_label(TypioWlFrontend *frontend,
                                  const TypioEngineStatus *mode,
                                  char *label,
                                  size_t label_size) {
    TypioRegistry *registry;
    char *engine_name;
    char *display;

    if (!frontend || !frontend->instance || !label || label_size == 0) {
        return false;
    }

    registry = typio_instance_get_registry(frontend->instance);
    engine_name = registry ? typio_registry_get_active_keyboard(registry) : nullptr;
    display = (engine_name && registry)
        ? typio_registry_get_engine_display_name(registry, engine_name) : nullptr;

    const char *eng = (display && display[0]) ? display
                     : (engine_name && engine_name[0]) ? engine_name : nullptr;

    if (!eng || !eng[0]) {
        typio_free_string(display);
        typio_free_string(engine_name);
        return false;
    }

    if (mode) {
        mode_cache_update(frontend, engine_name, mode);
    }

    const char *suffix = nullptr;
    if (mode && mode->display_label && mode->display_label[0]) {
        suffix = mode->display_label;
    } else if (engine_name) {
        suffix = mode_cache_lookup(frontend, engine_name);
    }

    if (suffix) {
        snprintf(label, label_size, "%s · %s", eng, suffix);
    } else {
        snprintf(label, label_size, "%s", eng);
    }

    typio_free_string(display);
    typio_free_string(engine_name);
    return true;
}

void typio_wl_frontend_show_indicator_for_state(TypioWlFrontend *frontend,
                                                  const TypioEngineStatus *mode) {
    char label[TYPIO_POSITIONED_UI_LABEL_CAP];

    if (!frontend || !frontend->instance) return;
    if (!indicator_enabled(frontend)) return;
    if (!frontend->panel) return;
    if (!build_indicator_label(frontend, mode, label, sizeof(label))) return;

    typio_wl_frontend_show_indicator(frontend, label);
}

void typio_wl_frontend_show_indicator_on_focus(TypioWlFrontend *frontend,
                                                const TypioEngineStatus *mode) {
    if (!frontend) return;

    uint64_t now = typio_wl_monotonic_ms();
    if (frontend->last_commit_ms > 0 &&
        now - frontend->last_commit_ms < TYPIO_INDICATOR_COMMIT_COOLDOWN_MS) {
        return;
    }

    typio_wl_frontend_show_indicator_for_state(frontend, mode);
}

void typio_wl_frontend_record_commit(TypioWlFrontend *frontend) {
    if (!frontend) return;
    frontend->last_commit_ms = typio_wl_monotonic_ms();
}

void typio_wl_frontend_show_indicator(TypioWlFrontend *frontend,
                                       const char *text) {
    bool anchor_ready;

    if (!frontend || !text || !text[0]) return;
    if (!indicator_enabled(frontend)) return;
    if (!frontend->panel) return;

    anchor_ready = typio_wl_panel_coordinator_anchor_ready(frontend);
    if (typio_wl_panel_coordinator_show_status(frontend, TYPIO_WL_UI_OWNER_INDICATOR, text) &&
        anchor_ready) {
        frontend->indicator_active = true;
        arm_indicator_timer(frontend);
    }
}

void typio_wl_frontend_hide_indicator(TypioWlFrontend *frontend) {
    struct itimerspec its;

    if (!frontend) return;
    typio_wl_panel_coordinator_hide(frontend, TYPIO_WL_UI_OWNER_INDICATOR);
    frontend->indicator_active = false;

    if (frontend->indicator_timer_fd >= 0) {
        memset(&its, 0, sizeof(its));
        timerfd_settime(frontend->indicator_timer_fd, 0, &its, NULL);
    }
}

int typio_wl_frontend_get_indicator_fd(TypioWlFrontend *frontend) {
    return frontend ? frontend->indicator_timer_fd : -1;
}

void typio_wl_frontend_dispatch_indicator_timer(TypioWlFrontend *frontend) {
    uint64_t expirations;

    if (!frontend || frontend->indicator_timer_fd < 0) return;

    if (read(frontend->indicator_timer_fd, &expirations, sizeof(expirations)) < 0) {
        return;
    }
    typio_wl_frontend_hide_indicator(frontend);
}

void typio_wl_frontend_destroy_indicator(TypioWlFrontend *frontend) {
    if (!frontend) return;
    if (frontend->indicator_timer_fd >= 0) {
        close(frontend->indicator_timer_fd);
        frontend->indicator_timer_fd = -1;
    }
    frontend->indicator_active = false;
}
