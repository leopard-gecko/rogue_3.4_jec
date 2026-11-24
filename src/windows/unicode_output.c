/* Windows VT text renderer adapted from Rogue 3.6 JEC.
 *
 * PDCursesMod's Windows wchar_t is 16 bits. Its VT backend can truncate
 * non-BMP characters (e.g. U+1F34E) even though the source is UTF-8.
 * We reserve two screen cells and repaint such a glyph in UTF-8 after
 * PDCurses' normal refresh, without changing dungeon state.
 *
 * The 3.6 cursor workaround hides the physical terminal cursor only while
 * PDCurses and the supplementary-glyph repaint are updating the screen.
 * It is explicitly shown again when drawing completes (do not curs_set(0)).
 */
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#include <windows.h>
#ifdef MOUSE_MOVED
#undef MOUSE_MOVED
#endif
#include <curses.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jec/unicode_output.h"

#define VT_GLYPH_COUNT 256
struct vt_glyph {
    WINDOW *window;
    int y, x;
    char bytes[5];
};
static struct vt_glyph glyphs[VT_GLYPH_COUNT];
static size_t used_glyphs;

void
rogue_vt_clear_window(WINDOW *win)
{
    size_t i, dst;
    for (i = dst = 0; i < used_glyphs; i++)
        if (glyphs[i].window != win)
            glyphs[dst++] = glyphs[i];
    used_glyphs = dst;
}

void
rogue_vt_clear_row_from(WINDOW *win, int row, int x)
{
    size_t i, dst;
    for (i = dst = 0; i < used_glyphs; i++)
        if (glyphs[i].window != win || glyphs[i].y != row ||
            glyphs[i].x + 1 < x)
            glyphs[dst++] = glyphs[i];
    used_glyphs = dst;
}

static void
remember_glyph(WINDOW *win, int y, int x, const char *bytes)
{
    size_t i, dst;
    /* New text overwrites the screen cells of any old supplementary glyph. */
    for (i = dst = 0; i < used_glyphs; i++)
        if (glyphs[i].window != win || glyphs[i].y != y ||
            glyphs[i].x + 1 < x || glyphs[i].x > x + 1)
            glyphs[dst++] = glyphs[i];
    used_glyphs = dst;
    if (used_glyphs < VT_GLYPH_COUNT) {
        struct vt_glyph *entry = &glyphs[used_glyphs++];
        entry->window = win;
        entry->y = y;
        entry->x = x;
        memcpy(entry->bytes, bytes, 4);
        entry->bytes[4] = '\0';
    }
}

void
rogue_vt_repaint(WINDOW *win)
{
    int by, bx;
    size_t i;
    int painted = 0;
    if (win == NULL)
        return;
    getbegyx(win, by, bx);
    for (i = 0; i < used_glyphs; i++) {
        if (glyphs[i].window == win) {
            if (!painted++)
                fputs("\033[s", stdout);
            fprintf(stdout, "\033[%d;%dH%s", by + glyphs[i].y + 1,
                    bx + glyphs[i].x + 1, glyphs[i].bytes);
        }
    }
    if (painted) {
        fputs("\033[u", stdout);
        fflush(stdout);
    }
}

int
rogue_draw_utf8(WINDOW *win)
{
    static const char hide[] = "\033[?25l";
    static const char show[] = "\033[?25h";
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode, written;
    int vt_cursor = output != NULL && output != INVALID_HANDLE_VALUE &&
                    GetConsoleMode(output, &mode) &&
                    (mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
    int result;

    /* Win32 WriteFile and CRT stdio are independently buffered; flush at
     * each boundary to enforce hide -> draw -> repaint -> show ordering. */
    if (vt_cursor) {
        fflush(stdout);
        (void)WriteFile(output, hide, (DWORD)(sizeof hide - 1), &written, NULL);
    }
    result = wrefresh(win);
    if (result != ERR)
        rogue_vt_repaint(win);
    if (vt_cursor) {
        fflush(stdout);
        (void)WriteFile(output, show, (DWORD)(sizeof show - 1), &written, NULL);
    }
    return result;
}

/* No rogue.h here: these calls must access real curses functions rather
 * than the Windows UTF-8 wrapper macros defined for the game code. */
int
rogue_waddstr_utf8(WINDOW *win, const char *s)
{
    const unsigned char *p;
    char *segment;
    size_t bytes, out = 0;
    int result = OK;
    if (win == NULL || s == NULL)
        return ERR;
    bytes = strlen(s);
    segment = (char *)malloc(bytes + 1);
    if (segment == NULL)
        return waddstr(win, s);
    p = (const unsigned char *)s;
    while (*p != '\0') {
        unsigned int cp = 0;
        size_t offset = (size_t)(p - (const unsigned char *)s);
        if (p[0] >= 0xf0 && p[0] <= 0xf4 && offset + 4 <= bytes &&
            (p[1] & 0xc0) == 0x80 && (p[2] & 0xc0) == 0x80 &&
            (p[3] & 0xc0) == 0x80 &&
            (p[0] != 0xf0 || p[1] >= 0x90) &&
            (p[0] != 0xf4 || p[1] <= 0x8f)) {
            cp = ((unsigned int)(p[0] & 7) << 18) |
                 ((unsigned int)(p[1] & 63) << 12) |
                 ((unsigned int)(p[2] & 63) << 6) |
                 (unsigned int)(p[3] & 63);
        }
        if (cp >= 0x10000 && cp <= 0x10ffff) {
            int y, x;
            if (out > 0) {
                segment[out] = '\0';
                if (waddstr(win, segment) == ERR)
                    result = ERR;
                out = 0;
            }
            getyx(win, y, x);
            if (waddstr(win, "  ") == ERR)
                result = ERR;
            else
                remember_glyph(win, y, x, (const char *)p);
            p += 4;
        } else {
            segment[out++] = (char)*p++;
        }
    }
    if (out) {
        segment[out] = '\0';
        if (waddstr(win, segment) == ERR)
            result = ERR;
    }
    free(segment);
    return result;
}

int
rogue_mvwaddstr_utf8(WINDOW *win, int y, int x, const char *s)
{
    return wmove(win, y, x) == ERR ? ERR : rogue_waddstr_utf8(win, s);
}

int
rogue_wprintw_utf8(WINDOW *win, const char *format, ...)
{
    char line[8192];
    va_list args;
    va_start(args, format);
    (void)vsnprintf(line, sizeof line, format, args);
    va_end(args);
    return rogue_waddstr_utf8(win, line);
}

int
rogue_wclear_utf8(WINDOW *win)
{
    rogue_vt_clear_window(win);
    return wclear(win);
}

int
rogue_wclrtoeol_utf8(WINDOW *win)
{
    int y, x;
    getyx(win, y, x);
    rogue_vt_clear_row_from(win, y, x);
    return wclrtoeol(win);
}
#endif /* _WIN32 */
