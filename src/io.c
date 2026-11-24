/*
 * Various input/output functions
 *
 * @(#)io.c	X.XX (Berkeley) XX/XX/XX
 */

#include <ctype.h>
#include <stdarg.h>
#include <errno.h>
#ifndef _WIN32
#include <poll.h>
#endif
#include "rogue.h"
#include "jec/language.h"
#include "jec/unicode_text.h"

/*
 * msg:
 *	Display a message at the top of the screen.
 */
void
msg(const char *fmt, ...)
{
    char msgbuf[512];
    char *ep;
    va_list ap;
    if (*fmt == '\0')
    {
        wmove(cw, 0, 0);
        wclrtoeol(cw);
        mpos = 0;
        return;
    }
    va_start(ap, fmt);
    vsnprintf(msgbuf, sizeof msgbuf, fmt, ap);
    va_end(ap);
    for (ep = msgbuf; *ep; ep++)
        if (*ep == '\n') *ep = ' ';
    /* Keep UTF-8 grapheme boundaries intact in the saved message history.
     * mpos is a terminal-cell column, never a UTF-8 byte count. */
    rogue_text_copy(huh, sizeof huh, msgbuf, sizeof huh - 1, 78);
    {
        char visible[sizeof msgbuf];
        const char *prompt = JSTR(" --続く--", "--More--");
        int col = ROGUE_COLS - rogue_text_width(prompt) - 1;
        if (mpos) {
            wmove(cw, 0, mpos < col ? mpos : col);
            waddstr(cw, prompt);
            draw(cw);
            wait_for(' ');
        }
        rogue_text_copy(visible, sizeof visible, msgbuf, sizeof visible - 1,
                        ROGUE_COLS - 1);
        mvwaddstr(cw, 0, 0, visible);
        wclrtoeol(cw);
        mpos = rogue_text_width(visible);
    }
    draw(cw);
}

/*
 * step_ok:
 *	returns true if it is ok to step on ch
 */
int
step_ok(int ch)
{
    switch (ch)
    {
	case FLOOR:
	case PASSAGE:
	case DOOR:
	case STAIRS:
	case TRAP:
	case GOLD:
	case POTION:
	case SCROLL:
	case FOOD:
	case WEAPON:
	case ARMOR:
	case AMULET:
	    return TRUE;
	default:
	    return FALSE;
    }
}

/*
 * readchar:
 *	flushes stdout so that screen is up to date and then returns
 *	getchar.
 */
int
readchar(void)
{
#ifdef _WIN32
    int c;
    WINDOW *active;
    fflush(stdout);
    for (;;) {
        rogue_check_signals();
        rogue_check_terminal_size();
        active = rogue_windows_input_window();
        if (active == NULL) {
            fprintf(stderr, "Rogue 3.4: no input window.\n");
            exit(1);
        }
        /* PDCurses' getch may refresh its target.  Never pass cw while
         * another window (help, inventory, options) is on screen. */
        keypad(active, TRUE);
        wtimeout(active, 100);
        c = wgetch(active);
        if (c == ERR)
            continue;           /* dispatch pending Ctrl-C and size changes */
#ifdef KEY_RESIZE
        if (c == KEY_RESIZE)
            continue;           /* the game's 80x24 map never changes size */
#endif
#ifdef KEY_BACKSPACE
        if (c == KEY_BACKSPACE)
            return '\b';
#endif
        if (c == 127)
            return '\b';
        /* An unrecognised PDCurses KEY_* code must not be truncated to an
         * ASCII command by the 1981 command parser.  Stage 3 will provide
         * a separate Unicode/edit-key path for names and file names. */
        if (c < 0 || c > 255)
            continue;
        return c;
    }
#else
    unsigned char c;
    ssize_t n;

    fflush(stdout);
    for (;;) {
        struct pollfd input = { STDIN_FILENO, POLLIN, 0 };
        int ready;

        rogue_check_signals();
        rogue_check_terminal_size();
        /* Even when SIGWINCH arrives just before the blocking call, keep
           checking the actual terminal size.  This also ensures termination
           signals are handled without waiting for another keypress. */
        ready = poll(&input, 1, 200);
        if (ready == 0 || (ready < 0 && errno == EINTR))
            continue;
        if (ready < 0)
            break;
        /* A size change or termination may have arrived between the first
           check and input readiness.  Do not execute a game command while
           the terminal is smaller than 80x24. */
        rogue_check_signals();
        rogue_check_terminal_size();
        n = read(STDIN_FILENO, &c, 1);
        if (n == 1)
            return c;
        if (n < 0 && errno == EINTR)
            continue;
        break;
    }
    /* EOF or an actual read/poll error: leave the terminal usable. */
    endwin();
    fprintf(stderr, "Rogue 3.4: terminal input closed.\n");
    exit(1);
#endif
}

/*
 * unctrl:
 *	Print a readable version of a certain character
 */
char *
rogue_unctrl(int ch)
{
    static char printable[4];
    unsigned char c = (unsigned char) ch & 0x7f;
    if (c == 0x7f) return "^?";
    if (c < 0x20) {
        printable[0] = '^'; printable[1] = c + '@'; printable[2] = '\0';
    } else {
        printable[0] = c; printable[1] = '\0';
    }
    return printable;
}

/*
 * status:
 *	Display the important stats line.  Keep the cursor where it was.
 */
int
status(void)
{
    register int oy, ox, temp;
    char buf[512];
    static int hpwidth = 0;
    static int s_max_hp = -1;
    static int s_lvl = -1, s_pur, s_hp = -1, s_str, s_add, s_ac;
    static int s_lang = -1;
    static long s_exp;

    /*
     * If nothing has changed since the last status, don't
     * bother.
     */
    if (s_hp == pstats.s_hpt && s_max_hp == max_hp &&
        s_lang == jrogue_lang_is_japanese &&
        s_exp == pstats.s_exp && s_pur == purse
	&& s_ac == (cur_armor != NULL ? cur_armor->o_ac : pstats.s_arm)
	&& s_str == pstats.s_str.st_str && s_add == pstats.s_str.st_add
	&& s_lvl == level)
	    return 0;

    getyx(cw, oy, ox);
    if (s_max_hp != max_hp)
    {
	temp = max_hp;
	for (hpwidth = 0; temp; hpwidth++)
	    temp /= 10;
    }
    {
        size_t off = 0;
        int n = snprintf(buf + off, sizeof buf - off,
            JSTR("階: %d  金塊: %-5d  体力: %*d(%*d)  強さ: %-2d",
                 "Level: %d  Gold: %-5d  Hp: %*d(%*d)  Str: %-2d"),
            level, purse, hpwidth, pstats.s_hpt, hpwidth, max_hp,
            pstats.s_str.st_str);
        if (n > 0) off += (size_t)n < sizeof buf - off ?
                          (size_t)n : sizeof buf - off - 1;
        if (pstats.s_str.st_add && off < sizeof buf - 1) {
            /* English uses the original 3.4 %-2d field width, including the
             * trailing space when the strength bonus has one digit.
             * Japanese keeps its existing compact status layout. */
            n = snprintf(buf + off, sizeof buf - off,
                jrogue_lang_is_japanese ? "/%d" : "/%-2d",
                pstats.s_str.st_add);
            if (n > 0) off += (size_t)n < sizeof buf - off ?
                              (size_t)n : sizeof buf - off - 1;
        }
        if (off < sizeof buf - 1)
            snprintf(buf + off, sizeof buf - off,
                JSTR("  守備: %-2d  経験: %d/%ld", "  Ac: %-2d  Exp: %d/%ld"),
                cur_armor != NULL ? cur_armor->o_ac : pstats.s_arm,
                pstats.s_lvl, pstats.s_exp);
    }
    /*
     * Save old status
     */
    s_lang = jrogue_lang_is_japanese;
    s_lvl = level;
    s_pur = purse;
    s_hp = pstats.s_hpt;
    s_max_hp = max_hp;
    s_str = pstats.s_str.st_str;
    s_add = pstats.s_str.st_add;
    s_exp = pstats.s_exp;
    s_ac = (cur_armor != NULL ? cur_armor->o_ac : pstats.s_arm);
    wmove(cw, ROGUE_LINES - 1, 0);
    wclrtoeol(cw);
    {
        char visible[sizeof buf];
        rogue_text_copy(visible, sizeof visible, buf, sizeof visible - 1,
                        ROGUE_COLS - 1);
        mvwaddstr(cw, ROGUE_LINES - 1, 0, visible);
    }
    wmove(cw, oy, ox);
    return 0;
}

/*
 * wait_for
 *	Sit around until the guy types the right key
 */
int
wait_for(char ch)
{
    register char c;

    if (ch == '\n')
	while ((c = readchar()) != '\n' && c != '\r')
	    continue;
    else
	while (readchar() != ch)
	    continue;
    return 0;
}

/*
 * show_win:
 *	function used to display a window and wait before returning
 */
int
show_win(WINDOW *scr, const char *message)
{
    mvwaddstr(scr, 0, 0, message);
    touchwin(scr);
    wmove(scr, hero.y, hero.x);
    draw(scr);
    wait_for(' ');
    clearok(cw, TRUE);
    touchwin(cw);
    return 0;
}
