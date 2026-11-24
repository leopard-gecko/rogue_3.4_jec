/* UTF-8 scalar, grapheme and display-cell helpers for Rogue 3.4 JEC.
 * Modelled on the platform-neutral Unicode text rules of Rogue 3.6 JEC.
 * No curses dependency; input validation never reads past its buffer.
 */
#ifndef ROGUE34_UNICODE_TEXT_H
#define ROGUE34_UNICODE_TEXT_H
#include <stddef.h>
size_t rogue_utf8_char_bytes(const char *s, size_t available);
size_t rogue_text_next(const char *s, size_t byte_pos);
size_t rogue_text_prev(const char *s, size_t byte_pos);
size_t rogue_text_clip(const char *s, size_t max_bytes, int max_cells);
int rogue_text_width(const char *s);
int rogue_text_width_n(const char *s, size_t nbytes);
void rogue_text_copy(char *dst, size_t cap, const char *src, size_t max_bytes, int max_cells);
#endif
