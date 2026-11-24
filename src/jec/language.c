/* Rogue 3.4 JEC: runtime display-language selection only.
 * Keep game rules, random state and save serialization entirely separate.
 */
#include "language.h"

#include <ctype.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include "windows/rogue_windows.h"
#endif

#if DEFAULT_LANGUAGE == 1
int jrogue_lang_is_japanese = 1;
#else
int jrogue_lang_is_japanese = 0;
#endif

/* Select a multibyte locale BEFORE initscr() and before any Japanese
 * curses output.  Setting the Windows console code page to CP_UTF8 is not
 * sufficient: PDCursesMod's narrow-string conversion also consults the UCRT
 * C locale.  setlocale(LC_CTYPE, "") can select the system ANSI code page
 * (e.g. CP932), making UTF-8 status labels split into bogus characters and
 * corrupt the VT cursor-positioned display.  Rogue 3.6 JEC uses .UTF8 on
 * Windows for precisely this reason.  Keep the locale UTF-8 in both language
 * modes so switching language at runtime never changes character encoding. */
void
jrogue_init_locale(void)
{
#ifdef _WIN32
    if (setlocale(LC_ALL, ".UTF8") == NULL &&
        setlocale(LC_ALL, "C.UTF-8") == NULL) {
        /* Do not silently fall back to the local ANSI code page: that would
         * render UTF-8 with the wrong glyph widths and break the dungeon. */
        fputs("Rogue 3.4: this Windows build requires the UCRT UTF-8 locale.\n",
              stderr);
        exit(EXIT_FAILURE);
    }
#else
    (void)setlocale(LC_CTYPE, "");
#endif
}

static int
locale_is_japanese(void)
{
    const char *lang = getenv("LC_ALL");
    if (lang == NULL || *lang == '\0')
        lang = getenv("LC_MESSAGES");
    if (lang == NULL || *lang == '\0')
        lang = getenv("LANG");
#ifdef _WIN32
    /* Explorer launches may have no LANG: use the Windows UI language. */
    if (lang == NULL || *lang == '\0')
        return rogue_windows_ui_is_japanese();
#endif
    /* ja, ja_JP, ja-JP, ja_JP.UTF-8, ... */
    return lang != NULL &&
           (lang[0] == 'j' || lang[0] == 'J') &&
           (lang[1] == 'a' || lang[1] == 'A') &&
           (lang[2] == '\0' || lang[2] == '_' || lang[2] == '-' ||
            lang[2] == '.');
}

void
jrogue_detect_language(void)
{
#if DEFAULT_LANGUAGE == 1
    jrogue_lang_is_japanese = 1;
#elif DEFAULT_LANGUAGE == 2
    jrogue_lang_is_japanese = 0;
#else
    jrogue_lang_is_japanese = locale_is_japanese();
#endif
}

/* The option parser passes a bounded token; no temporary strings or
 * implicit substring matches (e.g. language=japanese) are allowed. */
int
jrogue_set_language_n(const char *value, size_t length)
{
    if (value == NULL)
        return 0;
    while (length && isspace((unsigned char)*value)) {
        value++;
        length--;
    }
    while (length && isspace((unsigned char)value[length - 1]))
        length--;
    if (length == 2 && memcmp(value, "ja", 2) == 0)
        jrogue_lang_is_japanese = 1;
    else if (length == 2 && memcmp(value, "en", 2) == 0)
        jrogue_lang_is_japanese = 0;
    else if (length == 4 && memcmp(value, "auto", 4) == 0)
        jrogue_lang_is_japanese = locale_is_japanese();
    else
        return 0;
    return 1;
}

int
jrogue_set_language(const char *value)
{
    return value != NULL && jrogue_set_language_n(value, strlen(value));
}

const char *
jrogue_language_code(void)
{
    return jrogue_lang_is_japanese ? "ja" : "en";
}

const char *
jrogue_language_name(void)
{
    return jrogue_lang_is_japanese ? "Japanese" : "English";
}

const char *
jrogue_str(const char *ja, const char *en)
{
    return jrogue_lang_is_japanese ? ja : en;
}
