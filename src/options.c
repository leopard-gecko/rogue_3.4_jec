/*
 * This file has all the code for the option command.  I would rather
 * this command were not necessary, but it is the only way to keep the
 * wolves off of my back.
 *
 * @(#)options.c	X.XX (Berkeley) XX/XX/XX
 */

#include <ctype.h>
#ifndef _WIN32
#include <poll.h>
#endif
#include "rogue.h"
#include "jec/language.h"
#include "jec/unicode_text.h"

#define	NUM_OPTS	(sizeof optlist / sizeof (OPTION))

/*
 * description of an option and what to do with it
 */
struct optstruct {
    char	*o_name;	/* option name */
    char	*o_prompt;	/* English label */
    const char *o_prompt_ja;	/* Japanese label (Rogue 3.6 JEC) */
    void	*o_opt;		/* pointer to thing to set */
    void	(*o_putfunc)(void *);	/* function to print value */
    int		(*o_getfunc)(void *, WINDOW *);	/* function to get value interactively */
};

typedef struct optstruct	OPTION;

static void put_lang(void *value);
static int get_lang(void *value, WINDOW *win);
static void option_redraw(void);
/* Keep only the two language-dependent default fruit names in sync.
 * User-supplied names are never translated or overwritten. */
static void sync_default_fruit(void);
#define OPT_REDRAW 3


/* The save filename is a filesystem byte sequence.  Passing it through
 * rogue_unctrl() would corrupt UTF-8 components of HOME and ROGUEOPTS.
 * Preserve all bytes (except the terminating NUL, which cannot occur in
 * the input) and reject overlength paths rather than silently truncating.
 * The historic leading '~' expands to the current HOME directory. */
int
rogue_set_save_path(const char *path, size_t length)
{
    const char *suffix = path;
    size_t prefix_length = 0;
    if (length != 0 && *suffix == '~') {
        prefix_length = strlen(home);
        suffix++;
        length--;
        while (length != 0 && *suffix == '/') {
            suffix++;
            length--;
        }
    }
    if (prefix_length > sizeof file_name - 1 ||
        length > sizeof file_name - 1 - prefix_length)
        return -1;
    if (prefix_length != 0)
        memcpy(file_name, home, prefix_length);
    if (length != 0)
        memcpy(file_name + prefix_length, suffix, length);
    file_name[prefix_length + length] = '\0';
    return 0;
}

/* UTF-8 byte boundaries and terminal cell widths must be handled
 * independently. In particular, never write individual UTF-8 bytes with
 * waddch(): a Japanese path or emoji name may corrupt a curses window. */
static void
show_option_value(WINDOW *win, const char *value)
{
    int y, x;
    char visible[ROGUE_PATH_CAP];
    getyx(win, y, x);
    (void)y;
    if (x >= ROGUE_COLS - 2) return;
    rogue_text_copy(visible, sizeof visible, value, sizeof visible - 1,
                    ROGUE_COLS - 2 - x);
    waddstr(win, visible);
}

/* The labels and order match Rogue 3.6 JEC; the game rules stay 3.4. */
static OPTION optlist[] = {
    {"terse", "Terse output", "簡潔な表示",
        &terse, put_bool, get_bool},
    {"flush", "Flush typeahead during battle", "戦闘時に先行入力をキャンセル",
        &fight_flush, put_bool, get_bool},
    {"jump", "Show position only at end of run", "移動中の表示を行わない",
        &jump, put_bool, get_bool},
    {"step", "Do inventories one line at a time", "持ちものを１行ずつ表示",
        &slow_invent, put_bool, get_bool},
    {"askme", "Ask me about unidentified things", "未識別の品物について尋ねる",
        &askme, put_bool, get_bool},
    {"name", "Name", "プレイヤー名",
        whoami, put_str, get_str},
    {"fruit", "Fruit", "好きな果物",
        fruit, put_str, get_str},
    {"language", "Language", "表示する言語",
        &jrogue_lang_is_japanese, put_lang, get_lang},
    {"file", "Save file", "セーブファイル名",
        file_name, put_str, get_str}
};

/* Do not rewrite a Japanese label over its second terminal cell.
 * First clear the entire row; print once; then position the input at the
 * cell column measured from the original label, as Rogue 3.6 does. */
static int
option_column(const OPTION *op)
{
    const char *label = JSTR(op->o_prompt_ja, op->o_prompt);
    return rogue_text_width(label) + 2; /* ": " */
}

static void
option_redraw(void)
{
    OPTION *op;
    int row;
    wclear(hw);
    for (op = optlist, row = 0; op < &optlist[NUM_OPTS]; op++, row++) {
        wmove(hw, row, 0);
        wprintw(hw, "%s: ", JSTR(op->o_prompt_ja, op->o_prompt));
        (*op->o_putfunc)(op->o_opt);
    }
    touchwin(hw);
    draw(hw);
}

int
option(void)
{
    OPTION *op;
    int retval;
    size_t index;

    option_redraw();
    for (index = 0; index < NUM_OPTS; index++) {
        op = &optlist[index];
        wmove(hw, (int)index, option_column(op));
        retval = (*op->o_getfunc)(op->o_opt, hw);
        if (retval == QUIT) break;
        if (retval == OPT_REDRAW) {
            option_redraw();
        } else if (retval == MINUS) {
            if (index > 0) index -= 2;
            else index = (size_t)-1; /* for-loop increment returns to 0 */
        } else {
            option_redraw();
        }
    }
    wmove(hw, ROGUE_LINES - 1, 0);
    wclrtoeol(hw);
    mvwaddstr(hw, ROGUE_LINES - 1, 0,
              JSTR("--スペースを押してください--", "--Press space to continue--"));
    draw(hw);
    wait_for(' ');
    clearok(cw, TRUE);
    touchwin(cw);
    after = FALSE;
    return 0;
}

void
put_bool(void *value)
{
    bool *b = value;
    waddstr(hw, *b ? JSTR("有効(T)", "True") : JSTR("無効(F)", "False"));
}

void
put_str(void *value)
{
    show_option_value(hw, (const char *)value);
}

/* Match Rogue 3.6 JEC's invalid-key guidance without splitting a UTF-8
 * character or wrapping onto the next option row.  The hint is drawn
 * immediately (before the next input), and the caller's option_redraw()
 * clears it after a valid choice.  Keep the input cursor at the value. */
static void
option_key_hint(WINDOW *win, int row, int value_col, int offset,
                const char *message)
{
    int col = value_col + offset;
    char visible[96];

    if (col >= ROGUE_COLS - 1)
        return;
    rogue_text_copy(visible, sizeof visible, message, strlen(message),
                    ROGUE_COLS - 1 - col);
    mvwaddstr(win, row, col, visible);
    wmove(win, row, value_col);
    draw(win);
}

int
get_bool(void *value, WINDOW *win)
{
    bool *bp = value;
    int oy, ox, ch;
    getyx(win, oy, ox);
    for (;;) {
        wmove(win, oy, ox);
        draw(win);
        ch = readchar();
        if (ch == 't' || ch == 'T') { *bp = TRUE; return NORM; }
        if (ch == 'f' || ch == 'F') { *bp = FALSE; return NORM; }
        if (ch == '\r' || ch == '\n') return NORM;
        if (ch == 27 || ch == 7) return QUIT;
        if (ch == '-') return MINUS;
        option_key_hint(win, oy, ox, 10,
                        JSTR("［有効(T)、無効(F)］", "(T or F)"));
    }
}

static void
sync_default_fruit(void)
{
    const char *default_fruit;

    /* Match Rogue 3.6 JEC: either spelling of the built-in default
     * follows the current language, including after loading a save.
     * Any other fruit name is a user choice and must remain untouched. */
    if (fruit[0] != '\0' &&
        strcmp(fruit, "slime-mold") != 0 &&
        strcmp(fruit, "ネバネバかび") != 0)
        return;

    default_fruit = JSTR("ネバネバかび", "slime-mold");
    if (strcmp(fruit, default_fruit) != 0)
        rogue_copy_option(fruit, sizeof fruit,
                          default_fruit, strlen(default_fruit));
}

static void
put_lang(void *value)
{
    (void)value;
    waddstr(hw, jrogue_lang_is_japanese ?
           JSTR("日本語(J)", "Japanese(J)") :
           JSTR("英語(E)", "English(E)"));
}

static int
get_lang(void *value, WINDOW *win)
{
    int oy, ox, c;
    int old = jrogue_lang_is_japanese;
    (void)value;
    getyx(win, oy, ox);
    for (;;) {
        wmove(win, oy, ox);
        draw(win);
        c = readchar();
        if (c == 'j' || c == 'J') {
            (void)jrogue_set_language("ja");
            break;
        }
        if (c == 'e' || c == 'E') {
            (void)jrogue_set_language("en");
            break;
        }
        if (c == '\n' || c == '\r') return NORM;
        if (c == 27 || c == 7) return QUIT;
        if (c == '-') return MINUS;
        option_key_hint(win, oy, ox, 15,
                        JSTR("［日本語(J)、英語(E)］", "(J or E)"));
    }
    sync_default_fruit();
    return old != jrogue_lang_is_japanese ? OPT_REDRAW : NORM;
}

/* Raw terminal input is kept for Rogue 3.4's signal/resize handling.
 * Treat a multi-byte input scalar as a unit. A broken UTF-8 sequence is
 * rejected rather than inserted partially into an option value. */
static int
read_utf8_scalar(int first, char out[5])
{
    int need, i, c;
#ifdef _WIN32
    if (rogue_windows_has_edit_input()) {
        /* The Windows key reader already decoded UTF-16 or UTF-8.
         * Convert the Unicode scalar back into UTF-8 for the common
         * grapheme editor. Do not fetch spurious continuation keystrokes. */
        unsigned int cp;
        if (first < 0 || first > 0x10ffff ||
            (first >= 0xd800 && first <= 0xdfff))
            return 0;
        cp = (unsigned int)first;
        if (cp < 0x80) {
            out[0] = (char)cp; need = 1;
        } else if (cp < 0x800) {
            out[0] = (char)(0xc0 | (cp >> 6));
            out[1] = (char)(0x80 | (cp & 0x3f)); need = 2;
        } else if (cp < 0x10000) {
            out[0] = (char)(0xe0 | (cp >> 12));
            out[1] = (char)(0x80 | ((cp >> 6) & 0x3f));
            out[2] = (char)(0x80 | (cp & 0x3f)); need = 3;
        } else {
            out[0] = (char)(0xf0 | (cp >> 18));
            out[1] = (char)(0x80 | ((cp >> 12) & 0x3f));
            out[2] = (char)(0x80 | ((cp >> 6) & 0x3f));
            out[3] = (char)(0x80 | (cp & 0x3f)); need = 4;
        }
        out[need] = '\0';
        return need;
    }
#endif
    if (first < 0 || first > 255) return 0;
    if (first < 128) need = 1;
    else if (first >= 0xc2 && first <= 0xdf) need = 2;
    else if (first >= 0xe0 && first <= 0xef) need = 3;
    else if (first >= 0xf0 && first <= 0xf4) need = 4;
    else return 0;
    out[0] = (char)first;
    for (i = 1; i < need; i++) {
        c = readchar();
        if (c < 0x80 || c > 0xbf) return 0;
        out[i] = (char)c;
    }
    out[need] = '\0';
    return rogue_utf8_char_bytes(out, (size_t)need) == (size_t)need ? need : 0;
}

#define EDIT_LEFT   0x1001
#define EDIT_RIGHT  0x1002
#define EDIT_DELETE 0x1003

/* Terminal.app and xterm-style terminals send cursor movement keys as
 * small ESC sequences. Unlike the game command loop, the string editor
 * consumes those sequences as one editing key. A standalone Escape still
 * exits option editing. Do not block waiting for an absent sequence. */
static int
edit_followup(void)
{
#ifdef _WIN32
    /* A fallback curses stream cannot reliably look ahead for ESC.  Native
     * console and mintty use rogue_windows_edit_key() instead. */
    return 0;
#else
    struct pollfd fds = { STDIN_FILENO, POLLIN, 0 };
    return poll(&fds, 1, 60) > 0;
#endif
}

static int
edit_key(void)
{
    int c, c2;
#ifdef _WIN32
    /* The Win32 console delivers UTF-16; mintty delivers UTF-8 bytes.
     * Rogue 3.6's input layer normalizes both to a Unicode scalar.  Do
     * not mix this reader with wgetch() while editing the same string. */
    if (rogue_windows_has_edit_input()) {
        c = rogue_windows_edit_key();
        if (c == KEY_LEFT) return EDIT_LEFT;
        if (c == KEY_RIGHT) return EDIT_RIGHT;
        if (c == KEY_DC) return EDIT_DELETE;
#ifdef KEY_BACKSPACE
        if (c == KEY_BACKSPACE) return '\b';
#endif
        return c;
    }
#endif
    c = readchar();
    if (c != 27 || !edit_followup()) return c;
    c2 = readchar();
    if (c2 != '[' && c2 != 'O') return 27;
    if (!edit_followup()) return 27;
    c2 = readchar();
    if (c2 == 'D') return EDIT_LEFT;
    if (c2 == 'C') return EDIT_RIGHT;
    if (c2 == '3' && edit_followup() && readchar() == '~')
        return EDIT_DELETE;
    return 27;
}

/* The current value stays visible until the first replacement keystroke.
 * Backspace removes one grapheme, not one UTF-8 byte. Save path fields may
 * use the full path buffer, while name/fruit and call-names are 80 bytes. */
int
get_str(void *value, WINDOW *win)
{
    char *opt = value;
    char buf[ROGUE_PATH_CAP], glyph[5], visible[ROGUE_PATH_CAP];
    size_t used = 0, cursor = 0, capacity, prior;
    int oy, ox, c, started = 0, available;
    int max_width;
    if (opt == file_name) capacity = sizeof file_name;
    else if (opt == fruit) capacity = sizeof fruit;
    else capacity = sizeof whoami; /* potions.c and command.c use 80-byte arrays */
    buf[0] = '\0';
    /* Call-name buffers supplied by potions.c have no old option value. */
    if (opt != file_name && opt != fruit && opt != whoami)
        opt[0] = '\0';
    getyx(win, oy, ox);
    max_width = ROGUE_COLS - 2 - ox;
    if (max_width > 50) max_width = 50;
    if (max_width < 1) max_width = 1;
    for (;;) {
        wmove(win, oy, ox);
        if (started) {
            wclrtoeol(win);
            rogue_text_copy(visible, sizeof visible, buf, sizeof visible - 1, max_width);
            waddstr(win, visible);
            wmove(win, oy, ox + rogue_text_width_n(visible, cursor));
        }
        draw(win);
        c = edit_key();
        if (c == '\n' || c == '\r' || c == 27 || c == 7) break;
        if (c == EDIT_LEFT) {
            if (started) cursor = rogue_text_prev(buf, cursor);
            continue;
        }
        if (c == EDIT_RIGHT) {
            if (started) cursor = rogue_text_next(buf, cursor);
            continue;
        }
        if (c == EDIT_DELETE) {
            if (started && cursor < used) {
                size_t next = rogue_text_next(buf, cursor);
                memmove(buf + cursor, buf + next, used - next + 1);
                used -= next - cursor;
            }
            continue;
        }
        if (c == rogue_erase_char() || c == 8 || c == 127) {
            if (cursor > 0) {
                prior = rogue_text_prev(buf, cursor);
                memmove(buf + prior, buf + cursor, used - cursor + 1);
                used -= cursor - prior;
                cursor = prior;
            }
            continue;
        }
        if (c == rogue_kill_char()) {
            if (started) { used = 0; cursor = 0; buf[0] = '\0'; }
            continue;
        }
        if (c < 32 || c == 127) continue;
        if (!started && c == '-') break;
        if (!started && c == '~' && opt != file_name && win == hw) {
            /* Preserve the traditional tilde expansion, bounded by the
             * destination rather than a temporary 80-column buffer. */
            rogue_text_copy(buf, capacity, home, capacity - 1, max_width);
            used = strlen(buf);
            cursor = used;
            started = 1;
            continue;
        }
        available = read_utf8_scalar(c, glyph);
        if (!available || used + (size_t)available >= capacity) {
            beep();
            continue;
        }
        memmove(buf + cursor + available, buf + cursor, used - cursor + 1);
        memcpy(buf + cursor, glyph, (size_t)available);
        if (rogue_text_width(buf) > max_width) {
            memmove(buf + cursor, buf + cursor + available,
                    used - cursor + 1);
            beep();
            continue;
        }
        used += (size_t)available;
        cursor += (size_t)available;
        /* Insertion may join a following combining/emoji cluster. */
        if (cursor < used)
            cursor = rogue_text_next(buf, cursor - 1);
        started = 1;
    }
    if (started && used > 0 && c != 27 && c != 7) {
        if (opt == file_name) {
            if (rogue_set_save_path(buf, used) != 0) beep();
        } else {
            rogue_copy_option(opt, capacity, buf, used);
        }
    }
    wmove(win, oy, ox);
    wclrtoeol(win);
    show_option_value(win, opt);
    draw(win);
    if (c == '-' && !started) return MINUS;
    if (c == 27 || c == 7) return QUIT;
    return NORM;
}

/*
 * parse options from string, usually taken from the environment.
 * the string is a series of comma seperated values, with booleans
 * being stated as "name" (true) or "noname" (false), and strings
 * being "name=....", with the string being defined up to a comma
 * or the end of the entire option string.
 */
int
parse_opts(char *str)
{
    const char *token = str;
    while (*token) {
        const char *end, *equal;
        size_t name_len, value_len;
        OPTION *op;

        while (*token == ',' || isspace((unsigned char)*token))
            token++;
        if (*token == '\0')
            break;
        for (end = token; *end != '\0' && *end != ','; end++)
            ;
        equal = memchr(token, '=', (size_t)(end - token));
        name_len = (size_t)((equal != NULL ? equal : end) - token);
        while (name_len && isspace((unsigned char)token[name_len - 1]))
            name_len--;
        /* Handle language separately from string options: it is an
         * integer flag, not a destination buffer. */
        if (name_len == sizeof "language" - 1 &&
            strncmp(token, "language", name_len) == 0) {
            if (equal == NULL ||
                !jrogue_set_language_n(equal + 1, (size_t)(end - equal - 1)))
                return -1;
            token = end;
            continue;
        }
        for (op = optlist; op < &optlist[NUM_OPTS]; op++) {
            size_t op_len = strlen(op->o_name);
            if (op->o_putfunc == put_bool) {
                if (name_len == op_len && !strncmp(token, op->o_name, op_len))
                    *(bool *)op->o_opt = TRUE;
                else if (name_len == op_len + 2 && !strncmp(token, "no", 2)
                         && !strncmp(token + 2, op->o_name, op_len))
                    *(bool *)op->o_opt = FALSE;
                continue;
            }
            if (op->o_putfunc == put_lang)
                continue;
            if (equal == NULL || name_len != op_len || strncmp(token, op->o_name, op_len))
                continue;
            {
                char *dest = op->o_opt;
                size_t cap = dest == fruit ? sizeof fruit : sizeof whoami;
                const char *value = equal + 1;
                char expanded[sizeof file_name];
                size_t prefix = 0;
                value_len = (size_t)(end - value);
                if (dest == file_name) {
                    if (rogue_set_save_path(value, value_len) != 0)
                        return -1;
                    break;
                }
                if (value_len && *value == '~') {
                    prefix = strlen(home);
                    if (prefix >= sizeof expanded)
                        prefix = sizeof expanded - 1;
                    memcpy(expanded, home, prefix);
                    value++;
                    value_len--;
                    while (value_len && *value == '/') {
                        value++;
                        value_len--;
                    }
                }
                expanded[prefix] = '\0';
                rogue_copy_option(expanded + prefix, sizeof expanded - prefix, value, value_len);
                rogue_copy_option(dest, cap, expanded, strlen(expanded));
            }
            break;
        }
        token = end;
    }
    /* Finalize after all ROGUEOPTS tokens: fruit= and language= can
     * appear in either order, and a custom fruit must survive both. */
    sync_default_fruit();
    return 0;
}

/* Convert input bytes to the historical printable ^X representation, with
   an explicit destination bound. Both environment and interactive options
   pass through this single helper. */
void
rogue_copy_option(char *dest, size_t capacity, const char *source, size_t len)
{
    size_t pos = 0, used = 0;
    if (!capacity) return;
    while (pos < len && used < capacity - 1) {
        unsigned char c = (unsigned char)source[pos];
        size_t size;
        if (c < 0x20 || c == 0x7f) {
            if (capacity - 1 - used < 2) break;
            dest[used++] = '^';
            dest[used++] = c == 0x7f ? '?' : (char)(c + '@');
            pos++;
            continue;
        }
        size = rogue_utf8_char_bytes(source + pos, len - pos);
        if (!size) {
            dest[used++] = '?';
            pos++;
            continue;
        }
        if (size > capacity - 1 - used) break;
        memcpy(dest + used, source + pos, size);
        pos += size;
        used += size;
    }
    dest[used] = '\0';
}
