/* POSIX terminal and portable save/score path helpers. */
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <termios.h>
#include <sys/ioctl.h>
#endif
#include <time.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include "rogue.h"

/* Build-time defaults; the Makefile supplies the selected names and dirs. */
#ifndef ROGUE_SAVEFILE
#define ROGUE_SAVEFILE "rogue.save"
#endif
#ifndef ROGUE_SAVEDIR
#define ROGUE_SAVEDIR "."
#endif
#ifndef ROGUE_SCOREFILE
#define ROGUE_SCOREFILE "rogue.scr"
#endif
#ifndef ROGUE_SCOREDIR
#define ROGUE_SCOREDIR "."
#endif


char rogue_glyph(chtype cell)
{
    if (cell == (chtype)ERR)
        return '\0';
#ifdef A_CHARTEXT
    return (char)(cell & A_CHARTEXT);
#else
    return (char)(cell & 0xffu);
#endif
}

#ifndef _WIN32
/* Brief visual pause for animations that already refresh the screen.
 * Only presentation timing changes: this does not take a game turn, read
 * input, or consume random numbers.  Restart an interrupted sleep so the
 * requested total delay remains consistent across macOS and Linux. */
void
rogue_visual_delay(unsigned int milliseconds)
{
    struct timespec remaining;
    struct timespec interrupted;

    remaining.tv_sec = (time_t)(milliseconds / 1000u);
    remaining.tv_nsec = (long)(milliseconds % 1000u) * 1000000L;
    while (nanosleep(&remaining, &interrupted) == -1 && errno == EINTR)
        remaining = interrupted;
}

int rogue_erase_char(void)
{
    struct termios t;
    return tcgetattr(STDIN_FILENO, &t) == 0 ? t.c_cc[VERASE] : 127;
}
int rogue_kill_char(void)
{
    struct termios t;
    return tcgetattr(STDIN_FILENO, &t) == 0 ? t.c_cc[VKILL] : 21;
}
#endif /* !_WIN32: visual delay and terminal control keys */

/* Construct a path without embedding the builder's HOME. The global home
 * value is initialized in main() and includes a trailing '/'. An absolute
 * filename, or one beginning with ~/, overrides the configured directory.
 * Relative directories (including '.') are relative to the working dir.
 * Return an error on overflow, never silently redirect to a different file. */
int
rogue_build_data_path(char *result, size_t cap,
                      const char *directory, const char *filename)
{
    const char *prefix = directory;
    const char *name = filename;
    int n;
    size_t len;
    int windows_absolute = 0;
#ifdef _WIN32
    if (name != NULL && (name[0] == '\\' ||
        (((name[0] >= 'A' && name[0] <= 'Z') ||
          (name[0] >= 'a' && name[0] <= 'z')) && name[1] == ':' &&
         (name[2] == '/' || name[2] == '\\'))))
        windows_absolute = 1;
#endif

    if (result == NULL || cap == 0 || name == NULL || name[0] == '\0')
        return -1;
    if (name[0] == '/' || windows_absolute || (name[0] == '~' &&
        (name[1] == '/' || name[1] == '\0'))) {
        prefix = "";
    } else if (prefix == NULL || prefix[0] == '\0') {
        prefix = ".";
    }

    /* Both the directory and the filename may use a leading ~/ notation. */
    if (prefix[0] == '~' && (prefix[1] == '/' || prefix[1] == '\0')) {
        if (home[0] == '\0') return -1;
        n = snprintf(result, cap, "%s%s", home,
                     prefix[1] == '/' ? prefix + 2 : "");
    } else {
        n = snprintf(result, cap, "%s", prefix);
    }
    if (n < 0 || (size_t)n >= cap) return -1;
    len = (size_t)n;

    if (name[0] == '~' && (name[1] == '/' || name[1] == '\0')) {
        if (home[0] == '\0') return -1;
        n = snprintf(result, cap, "%s%s", home,
                     name[1] == '/' ? name + 2 : "");
    } else if (name[0] == '/' || windows_absolute) {
        n = snprintf(result, cap, "%s", name);
    } else {
        n = snprintf(result + len, cap - len, "%s%s",
                     len != 0 && result[len - 1] != '/' ? "/" : "", name);
        if (n < 0 || (size_t)n >= cap - len) return -1;
        return 0;
    }
    return n < 0 || (size_t)n >= cap ? -1 : 0;
}

int
rogue_default_save_path(char *result, size_t cap)
{
    return rogue_build_data_path(result, cap, ROGUE_SAVEDIR, ROGUE_SAVEFILE);
}

const char *
rogue_score_path(void)
{
    static char path[1024];
    if (rogue_build_data_path(path, sizeof path,
                              ROGUE_SCOREDIR, ROGUE_SCOREFILE) != 0)
        return NULL;
    return path;
}

#ifndef _WIN32
/* Signal handlers only set sig_atomic_t flags.  Calling curses, stdio, or
 * game functions inside a POSIX signal handler is not async-signal-safe.
 * Keep SA_RESTART disabled so read() can dispatch pending signals promptly. */
static volatile sig_atomic_t pending_interrupt;
static volatile sig_atomic_t pending_quit;
static volatile sig_atomic_t pending_terminate;
#ifdef SIGWINCH
static volatile sig_atomic_t pending_resize;
#endif

/* The three 80x24 game windows must not be resized below 80x24: their
 * cells are also used by the 1981 dungeon logic, not merely for display. */
static WINDOW *last_window;
static int resize_dispatching;
/* endwin() stays TRUE until a subsequent wrefresh().  During Ctrl-Z or
 * shell escape recovery, permit a size check *before* that first refresh:
 * otherwise the game could paint into a terminal smaller than 80x24. */
static int resuming_screen;
#ifdef SIGTSTP
static volatile sig_atomic_t pending_suspend;
#endif

/* No terminal I/O, allocation or game logic is allowed in a handler. */
static void
on_signal(int sig)
{
    switch (sig) {
    case SIGHUP:
    case SIGTERM:
        pending_terminate = sig;
        break;
    case SIGINT:
        pending_interrupt = 1;
        break;
    case SIGQUIT:
        pending_quit = 1;
        break;
#ifdef SIGTSTP
    case SIGTSTP:
        pending_suspend = 1;
        break;
#endif
#ifdef SIGWINCH
    case SIGWINCH:
        pending_resize = 1;
        break;
#endif
    }
}

static int
set_handler(int sig, void (*func)(int))
{
    struct sigaction action;
    memset(&action, 0, sizeof action);
    sigemptyset(&action.sa_mask);
    action.sa_handler = func;
    action.sa_flags = 0;     /* read() should return EINTR */
    return sigaction(sig, &action, NULL);
}

int
rogue_install_signal_handlers(void)
{
    if (set_handler(SIGHUP, on_signal) < 0 ||
        set_handler(SIGTERM, on_signal) < 0 ||
        set_handler(SIGINT, on_signal) < 0 ||
        set_handler(SIGQUIT, on_signal) < 0)
        return -1;
#ifdef SIGTSTP
    if (set_handler(SIGTSTP, on_signal) < 0)
        return -1;
#endif
#ifdef SIGWINCH
    if (set_handler(SIGWINCH, on_signal) < 0)
        return -1;
#endif
    return 0;
}

/* Both normal signal dispatch and the too-small-terminal wait exit through
 * this same path, after restoring curses when it is active. */
static void
terminate_from_signal(int sig)
{
    if (!isendwin())
        endwin();
    fprintf(stderr, "Rogue 3.4: terminated by signal %d (not saved).\n", sig);
    exit(128 + sig);
}

/* Use the kernel's actual PTY dimensions: because Rogue reads keys with
 * read(2), ncurses does not automatically process a KEY_RESIZE event. */
static int
terminal_dimensions(int *rows, int *cols)
{
    struct winsize ws;
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) != 0 ||
        ws.ws_row == 0 || ws.ws_col == 0)
        return 0;
    *rows = ws.ws_row;
    *cols = ws.ws_col;
    return 1;
}

/* Checking this from the normal execution path (not SIGWINCH's handler)
 * avoids curses calls in asynchronous context.  When a terminal is smaller
 * than the game's map, leave the alternate screen and wait for the user to
 * enlarge it.  In particular, do NOT call resizeterm() while too small:
 * ncurses may truncate cw/mw/hw and destroy dungeon state. */
void
rogue_check_terminal_size(void)
{
    int rows, cols;
    int paused = 0;
    int changed = 0;

    if (resize_dispatching || in_shell || cw == NULL || stdscr == NULL ||
        (isendwin() && !resuming_screen) ||
        !terminal_dimensions(&rows, &cols))
        return;
    resize_dispatching = 1;
    if (rows < ROGUE_LINES || cols < ROGUE_COLS) {
        struct timespec interval = { 0, 150000000L };
        /* The screen may already be in shell mode after Ctrl-Z or '!'.
         * Do not enter curses mode merely to issue the size warning. */
        if (!isendwin())
            endwin();
        fprintf(stderr, "\nRogue 3.4: terminal is %dx%d; enlarge to at least "
                "80x24 to continue.\n", cols, rows);
        fflush(stderr);
        paused = 1;
        /* No key or game turn is consumed while the window is too small.
         * Check termination and suspension even during the resize wait. */
        do {
            if (pending_terminate) {
                int sig = pending_terminate;
                pending_terminate = 0;
                terminate_from_signal(sig);
            }
#ifdef SIGTSTP
            if (pending_suspend) {
                pending_suspend = 0;
                raise(SIGSTOP);
            }
#endif
            nanosleep(&interval, NULL);
        } while (!terminal_dimensions(&rows, &cols) ||
                 rows < ROGUE_LINES || cols < ROGUE_COLS);
        reset_prog_mode();
        cbreak();
        noecho();
    }
    if (rows != LINES || cols != COLS) {
        if (resizeterm(rows, cols) != OK) {
            /* A failed resize must not leave the user in raw terminal mode. */
            endwin();
            fprintf(stderr, "Rogue 3.4: unable to resize curses screen.\n");
            exit(1);
        }
        changed = 1;
        /* resizeterm() is documented as attempting to resize *all* windows.
         * The 1981 dungeon uses cw/mw/hw cells as game state, so ensure the
         * original 80x24 geometry is retained after every terminal resize. */
        {
            WINDOW *game_windows[] = { cw, mw, hw };
            size_t i;
            for (i = 0; i < sizeof game_windows / sizeof game_windows[0]; i++) {
                int h, w;
                getmaxyx(game_windows[i], h, w);
                if ((h != ROGUE_LINES || w != ROGUE_COLS) &&
                    wresize(game_windows[i], ROGUE_LINES, ROGUE_COLS) != OK) {
                    endwin();
                    fprintf(stderr, "Rogue 3.4: unable to preserve dungeon screen.\n");
                    exit(1);
                }
            }
        }
    }
#ifdef SIGWINCH
    if (pending_resize)
        changed = 1;
    pending_resize = 0;
#endif
    if (changed || paused) {
        WINDOW *window = last_window != NULL ? last_window : cw;
        clearok(window, TRUE);
        touchwin(window);
        wrefresh(window);
    }
    resize_dispatching = 0;
}

/* Track the active overlay so inventory, options and tombstones can be
 * repainted in place after a resize, without losing their state or input. */
int
rogue_draw(WINDOW *window)
{
    if (window == NULL)
        return ERR;
    last_window = window;
    rogue_check_terminal_size();
    return wrefresh(window);
}

/* Re-enter program mode and repaint the map after Ctrl-Z or !. */
void
rogue_resume_screen(void)
{
    last_window = cw;
    /* Check dimensions while still out of curses mode, rather than painting
     * an invalid (possibly too small) screen and detecting it afterward. */
    resuming_screen = 1;
    rogue_check_terminal_size();
    resuming_screen = 0;
    reset_prog_mode();
    cbreak();
    noecho();
    clearok(cw, TRUE);
    touchwin(cw);
    wrefresh(cw);
}

/* Call only from ordinary game execution, never from a signal handler. */
void
rogue_check_signals(void)
{
    static int dispatching = 0;
    int sig;

    /* HUP/TERM, QUIT and suspend must work even while an interrupted
     * Ctrl-C quit dialog is active. Only a second Ctrl-C is suppressed. */
    sig = pending_terminate;
    if (sig != 0) {
        pending_terminate = 0;
        terminate_from_signal(sig);
    }
    if (pending_quit) {
        pending_quit = 0;
        endit();
    }
#ifdef SIGTSTP
    if (pending_suspend) {
        pending_suspend = 0;
        tstp();
    }
#endif
    if (dispatching)
        return;
    dispatching = 1;
    if (pending_interrupt) {
        pending_interrupt = 0;
        quit();
        /* Discard further Ctrl-C events received during the quit question. */
        pending_interrupt = 0;
    }
    dispatching = 0;
}

#endif /* !_WIN32: POSIX signal / terminal handling */
