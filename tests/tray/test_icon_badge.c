/**
 * @file test_icon_badge.c
 * @brief Data-contract tests for the tray language-badge rasteriser.
 *
 * Visual correctness on a real panel cannot be checked in CI, but the data
 * contract can: each requested size yields a correctly-dimensioned, big-endian
 * ARGB32 buffer with actual glyph coverage, and malformed input is rejected.
 */

#include "tray/icon_badge.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

/* A rendered badge must have at least one partially-opaque pixel — otherwise no
 * glyph was drawn. Alpha is byte 0 of each big-endian ARGB32 pixel. */
static bool has_coverage(const TypioBadgePixmap *p) {
    size_t px = (size_t)p->width * (size_t)p->height;
    for (size_t i = 0; i < px; i++) {
        if (p->argb[i * 4] != 0) return true;
    }
    return false;
}

static void test_renders_script(const char *label, const char *text) {
    const int sizes[2] = { 22, 44 };
    TypioBadgePixmap out[2] = {0};
    size_t n = typio_icon_badge_render(text, sizes, 2, 0xFFFFFF, out, 2);

    /* Font coverage for the script may be absent in a minimal CI image; a clean
     * 0 (caller falls back to an icon name) is acceptable. But when it renders,
     * the contract must hold exactly. */
    if (n == 0) {
        printf("  %s: no covering font in this environment — skipped\n", label);
        return;
    }
    assert(n == 2);
    for (size_t i = 0; i < n; i++) {
        assert(out[i].width == sizes[i]);
        assert(out[i].height == sizes[i]);
        assert(out[i].argb != nullptr);
        assert(has_coverage(&out[i]));
        typio_badge_pixmap_free(&out[i]);
        assert(out[i].argb == nullptr);
    }
    printf("  %s: rendered 22px + 44px with coverage\n", label);
}

static void test_rejects_bad_input(void) {
    const int sizes[1] = { 22 };
    TypioBadgePixmap out[1] = {0};
    assert(typio_icon_badge_render(nullptr, sizes, 1, 0xFFFFFF, out, 1) == 0);
    assert(typio_icon_badge_render("", sizes, 1, 0xFFFFFF, out, 1) == 0);
    assert(typio_icon_badge_render("X", sizes, 0, 0xFFFFFF, out, 1) == 0);
    /* Insufficient output capacity is rejected. */
    assert(typio_icon_badge_render("X", sizes, 1, 0xFFFFFF, out, 0) == 0);
    printf("  bad input: rejected with 0\n");
}

int main(void) {
    printf("icon_badge data-contract tests\n");
    test_rejects_bad_input();
    test_renders_script("Latin (EN)", "EN");
    test_renders_script("Han (中)", "\xe4\xb8\xad");           /* 中 */
    test_renders_script("Arabic (الد)", "\xd8\xa7\xd9\x84\xd8\xaf"); /* الد */
    typio_icon_badge_shutdown();
    printf("OK\n");
    return 0;
}
