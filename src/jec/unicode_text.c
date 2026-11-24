/* This subset is specific to UTF-8 display/editing, not game mechanics.
 * Estimates terminal cells for the common CJK and emoji sequences handled
 * by modern Terminal.app and ncursesw. Rendering is the terminal's job. */
#include "unicode_text.h"
#include <stdint.h>
#include <string.h>

static size_t
scalar(const char *s, size_t n, uint32_t *value)
{
    uint32_t v;
    unsigned int i, count;
    unsigned char c;
    if (!n) return 0;
    c = (unsigned char)s[0];
    if (c < 0x80) { *value = c; return 1; }
    if (c >= 0xc2 && c <= 0xdf) { v = c & 31; count = 2; }
    else if (c >= 0xe0 && c <= 0xef) { v = c & 15; count = 3; }
    else if (c >= 0xf0 && c <= 0xf4) { v = c & 7; count = 4; }
    else return 0;
    if (n < count) return 0;
    for (i = 1; i < count; i++) {
        c = (unsigned char)s[i];
        if ((c & 0xc0) != 0x80) return 0;
        v = (v << 6) | (c & 63);
    }
    if ((count == 2 && v < 0x80) || (count == 3 && v < 0x800) ||
        (count == 4 && v < 0x10000) ||
        (v >= 0xd800 && v <= 0xdfff) || v > 0x10ffff) return 0;
    *value = v;
    return count;
}
size_t
rogue_utf8_char_bytes(const char *s, size_t available)
{
    uint32_t cp;
    if (!s) return 0;
    return scalar(s, available, &cp);
}
static int
is_extend(uint32_t c)
{
    return (c >= 0x0300 && c <= 0x036f) ||
        (c >= 0x0483 && c <= 0x0489) ||
        (c >= 0x0591 && c <= 0x05c7) ||
        (c >= 0x0610 && c <= 0x061a) ||
        (c >= 0x064b && c <= 0x065f) ||
        (c >= 0x0900 && c <= 0x0902) ||
        (c >= 0x093a && c <= 0x093c) ||
        (c >= 0x0941 && c <= 0x0948) ||
        (c >= 0x1ab0 && c <= 0x1aff) ||
        (c >= 0x1dc0 && c <= 0x1dff) ||
        (c >= 0x20d0 && c <= 0x20ff) ||
        (c >= 0x302a && c <= 0x302f) ||
        (c >= 0x3099 && c <= 0x309a) ||
        (c >= 0xfe00 && c <= 0xfe0f) ||
        (c >= 0xfe20 && c <= 0xfe2f) ||
        (c >= 0x1f3fb && c <= 0x1f3ff) ||
        (c >= 0xe0020 && c <= 0xe007f) ||
        (c >= 0xe0100 && c <= 0xe01ef);
}
static int is_ri(uint32_t c) { return c >= 0x1f1e6 && c <= 0x1f1ff; }
static int is_ep(uint32_t c)
{
    return (c >= 0x1f000 && c <= 0x1faff) ||
        (c >= 0x2600 && c <= 0x27bf) ||
        (c >= 0x2300 && c <= 0x23ff) || c == 0xa9 || c == 0xae;
}
static int scalar_width(uint32_t c)
{
    if (is_extend(c) || c == 0x200d) return 0;
    if (c < 0x20 || c == 0x7f) return 2; /* caret notation in options */
    if (c >= 0x1100 && (c <= 0x115f ||
        (c >= 0x2329 && c <= 0x232a) ||
        (c >= 0x2e80 && c <= 0xa4cf) ||
        (c >= 0xac00 && c <= 0xd7a3) ||
        (c >= 0xf900 && c <= 0xfaff) ||
        (c >= 0xfe10 && c <= 0xfe6f) ||
        (c >= 0xff01 && c <= 0xff60) ||
        (c >= 0xffe0 && c <= 0xffe6) ||
        (c >= 0x1f000 && c <= 0x1faff) ||
        (c >= 0x20000 && c <= 0x3fffd))) return 2;
    return 1;
}
/* Consume the next extended cluster, preserving combining accents, emoji
 * modifiers, variation selectors, ZWJ joins and regional-indicator pairs. */
static size_t
cluster(const char *s, size_t n, size_t pos, int *cells)
{
    size_t used, next;
    uint32_t cp, prev;
    int width, has_ep, zwj_ep = 0, ri, ri_total, keycap = 0, vs16 = 0;
    used = scalar(s + pos, n - pos, &cp);
    if (!used) { *cells = 1; return pos + 1; } /* untrusted path byte */
    width = scalar_width(cp);
    has_ep = is_ep(cp);
    ri = is_ri(cp) ? 1 : 0;
    ri_total = ri;
    prev = cp;
    next = pos + used;
    while (next < n) {
        uint32_t x;
        int join;
        used = scalar(s + next, n - next, &x);
        if (!used) break;
        join = is_extend(x) || x == 0x200d ||
            (zwj_ep && is_ep(x)) || (ri == 1 && is_ri(x));
        if (!join) break;
        if (x == 0x200d) zwj_ep = has_ep;
        else if (!is_extend(x)) zwj_ep = 0;
        if (!is_extend(x) && x != 0x200d) has_ep = is_ep(x);
        if (x == 0xfe0f) vs16 = 1;
        if (x == 0x20e3) keycap = 1;
        if (is_ri(x)) { ++ri; ++ri_total; }
        else ri = 0;
        if (scalar_width(x) > width) width = scalar_width(x);
        prev = x;
        next += used;
    }
    (void)prev;
    if (ri_total >= 2 || keycap || (vs16 && has_ep) || zwj_ep)
        width = 2;
    *cells = width;
    return next;
}
size_t
rogue_text_next(const char *s, size_t at)
{
    size_t pos = 0, n;
    int w;
    if (!s) return 0;
    n = strlen(s);
    if (at >= n) return n;
    while (pos < n) {
        size_t end = cluster(s, n, pos, &w);
        if (end > at) return end;
        pos = end;
    }
    return n;
}
size_t
rogue_text_prev(const char *s, size_t at)
{
    size_t pos = 0, n;
    int w;
    if (!s || at == 0) return 0;
    n = strlen(s);
    if (at > n) at = n;
    while (pos < n) {
        size_t end = cluster(s, n, pos, &w);
        if (end >= at) return pos;
        pos = end;
    }
    return pos;
}
int
rogue_text_width_n(const char *s, size_t bytes)
{
    size_t pos = 0, n;
    int cells = 0, w;
    if (!s) return 0;
    n = strlen(s);
    if (n > bytes) n = bytes;
    while (pos < n) {
        size_t end = cluster(s, n, pos, &w);
        if (end > n) break;
        cells += w;
        pos = end;
    }
    return cells;
}
int rogue_text_width(const char *s)
{
    return s ? rogue_text_width_n(s, strlen(s)) : 0;
}
size_t
rogue_text_clip(const char *s, size_t max_bytes, int max_cells)
{
    size_t pos = 0, n;
    int cells = 0, w;
    if (!s || max_cells < 0) return 0;
    n = strlen(s);
    while (pos < n) {
        size_t end = cluster(s, n, pos, &w);
        if (end > max_bytes || cells + w > max_cells) break;
        cells += w;
        pos = end;
    }
    return pos;
}
void
rogue_text_copy(char *dst, size_t cap, const char *src, size_t max_bytes, int max_cells)
{
    size_t n;
    if (!dst || !cap) return;
    if (!src) { dst[0] = '\0'; return; }
    if (max_bytes > cap - 1) max_bytes = cap - 1;
    n = rogue_text_clip(src, max_bytes, max_cells);
    if (n) memmove(dst, src, n);
    dst[n] = '\0';
}
