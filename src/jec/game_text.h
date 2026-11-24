/* Rogue 3.4 JEC: render-time vocabulary derived from Rogue 3.6 JEC.
 * Keep original English data, appearance probabilities and serialized state.
 */
#ifndef ROGUE34_GAME_TEXT_H
#define ROGUE34_GAME_TEXT_H
#include <stddef.h>
const char *jrogue_text(const char *english);
const char *jrogue_color(const char *english);
/* Generated scroll titles must fit in 40 UTF-8 bytes in either language. */
#define JROGUE_SCROLL_TITLE_MAX_BYTES 40
size_t jrogue_scroll_title_ja_bytes(const char *english);
const char *jrogue_scroll_title(const char *english);
#endif
