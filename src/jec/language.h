#ifndef ROGUE34_JEC_LANGUAGE_H
#define ROGUE34_JEC_LANGUAGE_H

#include <stddef.h>

/*
 * Display language is presentation state, independent from Rogue 3.4 game
 * state and the portable save format.
 *
 * Startup order:
 *   DEFAULT_LANGUAGE=0: locale (auto); 1: Japanese; 2: English
 *   ROGUEOPTS language=ja/en/auto overrides the build default.
 * In-game 'o' can switch to Japanese/English without restarting.
 * ROGUE_LANG is deliberately not supported.
 */
#ifndef DEFAULT_LANGUAGE
#define DEFAULT_LANGUAGE 0
#endif
#if DEFAULT_LANGUAGE != 0 && DEFAULT_LANGUAGE != 1 && DEFAULT_LANGUAGE != 2
#error "DEFAULT_LANGUAGE must be 0 (auto), 1 (ja), or 2 (en)"
#endif

extern int jrogue_lang_is_japanese;

void jrogue_init_locale(void);
void jrogue_detect_language(void);
int jrogue_set_language(const char *value);
int jrogue_set_language_n(const char *value, size_t length);
const char *jrogue_language_code(void);
const char *jrogue_language_name(void);
const char *jrogue_str(const char *ja, const char *en);

#define JSTR(ja, en) jrogue_str((ja), (en))

#endif /* ROGUE34_JEC_LANGUAGE_H */
