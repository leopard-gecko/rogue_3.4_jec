/* Windows-only rendering helpers. Unix curses output stays unchanged. */
#ifndef ROGUE34_UNICODE_OUTPUT_H
#define ROGUE34_UNICODE_OUTPUT_H
#include <curses.h>
#ifdef _WIN32
int rogue_draw_utf8(WINDOW *win);
int rogue_waddstr_utf8(WINDOW *win, const char *text);
int rogue_mvwaddstr_utf8(WINDOW *win, int y, int x, const char *text);
int rogue_wprintw_utf8(WINDOW *win, const char *format, ...);
int rogue_wclear_utf8(WINDOW *win);
int rogue_wclrtoeol_utf8(WINDOW *win);
void rogue_vt_clear_window(WINDOW *win);
void rogue_vt_clear_row_from(WINDOW *win, int row, int col);
void rogue_vt_repaint(WINDOW *win);
#endif
#endif /* ROGUE34_UNICODE_OUTPUT_H */
