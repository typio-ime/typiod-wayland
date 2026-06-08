/**
 * @file focus_controller.c
 * @brief Pure lifecycle decision functions: reduce, diff, done classifier,
 *        and guard predicates.
 *
 * These functions reason only about enums and structs — no frontend, no
 * Wayland, no I/O. They are the single source of truth for how input facts
 * derive desired resource configuration, and how desired-vs-actual gaps
 * project to minimal idempotent effects.
 */

#include "focus_controller.h"

/* ── Name helpers (pure, for tracing) ──────────────────────────────────── */

const char *
typio_wl_grab_want_name(TypioWlGrabWant want)
{
    switch (want) {
    case TYPIO_WL_GRAB_WANT_NONE:      return "NONE";
    case TYPIO_WL_GRAB_WANT_SOFT_PAUSE: return "SOFT_PAUSE";
    case TYPIO_WL_GRAB_WANT_YES:        return "YES";
    }
    return "UNKNOWN";
}

const char *
typio_wl_grab_resource_state_name(TypioWlGrabResourceState state)
{
    switch (state) {
    case TYPIO_WL_GRAB_RES_ABSENT:        return "ABSENT";
    case TYPIO_WL_GRAB_RES_NEEDS_KEYMAP:  return "NEEDS_KEYMAP";
    case TYPIO_WL_GRAB_RES_READY:          return "READY";
    case TYPIO_WL_GRAB_RES_BROKEN:         return "BROKEN";
    }
    return "UNKNOWN";
}

const char *
typio_wl_done_action_name(TypioWlDoneAction action)
{
    switch (action) {
    case TYPIO_WL_DONE_NOOP:           return "NOOP";
    case TYPIO_WL_DONE_FIRST_ACTIVATE: return "FIRST_ACTIVATE";
    case TYPIO_WL_DONE_DEACTIVATE:     return "DEACTIVATE";
    case TYPIO_WL_DONE_REACTIVATE:     return "REACTIVATE";
    }
    return "UNKNOWN";
}

/* ── Reduce: facts → desired ───────────────────────────────────────────── */

TypioWlDesiredState
typio_wl_focus_reduce(const TypioWlInputFacts *facts,
                        const TypioWlDesiredState *prev)
{
    TypioWlDesiredState d = {
        .grab = TYPIO_WL_GRAB_WANT_NONE,
        .focus_in = false,
        .focus_out = false,
        .reactivate = false,
    };

    if (!facts || !prev)
        return d;

    /* Grab want: hard boundary first, then activation, then deactivation.
     *
     * Hard boundary (suspend / connection lost) wins over everything.
     *
     * Activation is checked before deactivation: a focus move that batches
     * both events (deactivate-old + activate-new committed by one done) must
     * resolve to YES (a reactivation), not soft-pause.
     *
     * Both activate and deactivate are read from their batch-surviving flags
     * (im_done_had_*) as well as the raw per-event flags. The raw deactivate
     * flag alone is not enough: a done() commits and clears it in the same
     * dispatch batch, so a plain focus-out would otherwise be lost before
     * this runs and the grab would never soft-pause. */
    if (!facts->connection_alive || facts->suspend_gap_detected) {
        d.grab = TYPIO_WL_GRAB_WANT_NONE;
    } else if (facts->im_activate_seen || facts->im_done_had_activate) {
        /* Skip the grab entirely if no engine is registered: focusing the
         * input context is still meaningful (state in sync), but a grab
         * with no consumer would be wasted work. */
        d.grab = facts->engine_present
                     ? TYPIO_WL_GRAB_WANT_YES
                     : TYPIO_WL_GRAB_WANT_NONE;
    } else if (facts->im_deactivate_seen || facts->im_done_had_deactivate) {
        d.grab = TYPIO_WL_GRAB_WANT_SOFT_PAUSE;
    } else {
        d.grab = prev->grab;
    }

    /* Edge-triggered focus events. */
    d.focus_in =
        (d.grab == TYPIO_WL_GRAB_WANT_YES && prev->grab != TYPIO_WL_GRAB_WANT_YES);
    d.focus_out =
        (d.grab != TYPIO_WL_GRAB_WANT_YES && prev->grab == TYPIO_WL_GRAB_WANT_YES);

    /* Reactivate: a fresh activate inside a done batch while we are stably
     * YES means the compositor moved us to a new caret without an
     * intervening deactivate. Preserve the grab (and the in-flight
     * composition); re-anchor the panel to the new caret. */
    d.reactivate =
        (d.grab == TYPIO_WL_GRAB_WANT_YES &&
         prev->grab == TYPIO_WL_GRAB_WANT_YES &&
         facts->im_done_had_activate);

    return d;
}

/* ── Diff: desired vs actual → effects ─────────────────────────────────── */

TypioWlEffectSet
typio_wl_focus_diff(const TypioWlDesiredState *desired,
                      const TypioWlActualState *actual)
{
    TypioWlEffectSet e = {0};

    if (!desired || !actual)
        return e;

    /* Hard teardown: we do not want the grab at all.
     * Scrub the key generation as well: any transition to NONE is a
     * hard boundary that must fence stale in-flight key state. */
    if (desired->grab == TYPIO_WL_GRAB_WANT_NONE &&
        actual->grab != TYPIO_WL_GRAB_RES_ABSENT) {
        e.destroy_grab = true;
        e.scrub_generation = true;
        e.discard_composition = true;
        e.clear_preedit = true;
        e.commit = true;
    }

    /* Creation: we need a grab but it is absent. Covers normal activation
     * (YES → ABSENT), the soft-pause recovery case where the grab was
     * silently dropped while paused, and the "no engine, no grab"
     * degenerate path (handled in reduce, this branch won't fire there). */
    if ((desired->grab == TYPIO_WL_GRAB_WANT_YES ||
         desired->grab == TYPIO_WL_GRAB_WANT_SOFT_PAUSE) &&
        actual->grab == TYPIO_WL_GRAB_RES_ABSENT) {
        e.create_grab = true;
        e.scrub_generation = true;
    }

    /* Broken recovery: tear down and rebuild in the same tick. */
    if (desired->grab == TYPIO_WL_GRAB_WANT_YES &&
        actual->grab == TYPIO_WL_GRAB_RES_BROKEN) {
        e.destroy_grab = true;
        e.create_grab = true;
        e.scrub_generation = true;
    }

    /* Focus edges. */
    if (desired->focus_in)
        e.send_focus_in = true;
    if (desired->focus_out) {
        e.send_focus_out = true;
        /* Leaving an active field abandons any in-flight composition.
         * Discard it engine-side and blank the compositor preedit so a
         * half-typed attempt cannot leak into the next field on its
         * focus_in. The soft-pause (deactivate) defocus path reaches the
         * teardown only through this edge — the grab==NONE branch above
         * covers the hard-teardown path. */
        e.discard_composition = true;
        e.clear_preedit = true;
        e.commit = true;
    }

    /* Reactivate: re-anchor the panel to the new caret. The grab and the
     * engine state are preserved. */
    if (desired->reactivate)
        e.reactivate = true;

    return e;
}

/* ── Done classifier (pure helper, for tracing) ───────────────────────── */

TypioWlDoneAction
typio_wl_focus_classify_done(bool was_active,
                               bool now_active,
                               bool activate_seen)
{
    if (now_active && !was_active)
        return TYPIO_WL_DONE_FIRST_ACTIVATE;
    if (was_active && !now_active)
        return TYPIO_WL_DONE_DEACTIVATE;
    /* Still active. Only a fresh `activate` in this batch means a genuine
     * re-activation (a move to a new field); otherwise this `done` is
     * just a text-state update and must leave focus state untouched. */
    if (was_active && now_active && activate_seen)
        return TYPIO_WL_DONE_REACTIVATE;
    return TYPIO_WL_DONE_NOOP;
}

/* ── Guard predicates (pure, on a snapshotted actual) ────────────────── */

bool
typio_wl_focus_can_route_keys(const TypioWlActualState *actual)
{
    if (!actual)
        return false;
    return actual->ic_focused && actual->grab == TYPIO_WL_GRAB_RES_READY;
}

bool
typio_wl_focus_can_route_modifiers(const TypioWlActualState *actual)
{
    if (!actual)
        return false;
    /* Modifiers (and their keymap handoff) can flow during the
     * NEEDS_KEYMAP window; only ABSENT or BROKEN forbids them. */
    return actual->grab == TYPIO_WL_GRAB_RES_NEEDS_KEYMAP ||
           actual->grab == TYPIO_WL_GRAB_RES_READY;
}

bool
typio_wl_focus_is_transitioning(const TypioWlActualState *actual)
{
    if (!actual)
        return false;
    /* Mid-handshake: the input context is focused but the grab is not
     * yet ready, or the keymap path is broken. The keyboard guard's
     * stuck-press failsafe uses this to avoid tearing down the daemon
     * while a normal activation is still in flight. */
    return actual->ic_focused &&
           (actual->grab == TYPIO_WL_GRAB_RES_ABSENT ||
            actual->grab == TYPIO_WL_GRAB_RES_NEEDS_KEYMAP ||
            actual->grab == TYPIO_WL_GRAB_RES_BROKEN);
}
