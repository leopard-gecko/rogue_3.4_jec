/* Windows platform layer for Rogue 3.4 JEC.  Console sizing and
 * presentation are in rogue_windows_console.c, game rules stay unchanged.
 * Unicode editing and UTF-8 drawing are in stage 3; file locking is stage 4.
 */
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
/* Win32's min/max macros conflict with Rogue's max() helper. */
#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#include <windows.h>
#include <shellapi.h>
#include <commdlg.h>
/* wincontypes.h and PDCurses both define MOUSE_MOVED differently.
 * Rogue uses the PDCurses definition; this platform module does not use
 * the Win32 mouse movement flag.  Drop only the conflicting Win32 macro
 * before rogue.h includes <curses.h>. */
#ifdef MOUSE_MOVED
#undef MOUSE_MOVED
#endif
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rogue.h"
#include "windows/rogue_windows.h"
#include "windows/rogue_windows_console.h"

static char *
utf16_to_utf8(const wchar_t *source)
{
    int bytes;
    char *result;
    if (source == NULL) return NULL;
    bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                                source, -1, NULL, 0, NULL, NULL);
    if (bytes <= 0) return NULL;
    result = (char *)malloc((size_t)bytes);
    if (result == NULL) return NULL;
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                            source, -1, result, bytes, NULL, NULL) != bytes) {
        free(result);
        return NULL;
    }
    return result;
}

/* The UCRT narrow argv may have been decoded using the system code page
 * before our .UTF8 locale is selected.  Read the original Unicode command
 * line when a save path is passed (including Explorer drag-and-drop). */
/* Win32 paths and file-dialog selections are UTF-16; game paths remain
 * UTF-8 and are checked by the existing restore() validator. */
static wchar_t *
utf8_to_utf16(const char *source)
{
    int count;
    wchar_t *wide;
    if (source == NULL) return NULL;
    count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                source, -1, NULL, 0);
    if (count <= 0) return NULL;
    wide = (wchar_t *)calloc((size_t)count, sizeof *wide);
    if (wide == NULL) return NULL;
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                            source, -1, wide, count) != count) {
        free(wide);
        return NULL;
    }
    return wide;
}

/* Desktop double-click menu only when the configured default save exists.
 * Keyboard is read through the already tested Unicode console input path.
 * No dialog or resize is introduced for an ordinary terminal launch. */
static int
windows_save_exists(const char *path)
{
    wchar_t *wide = utf8_to_utf16(path);
    DWORD attributes;
    if (wide == NULL) return 0;
    attributes = GetFileAttributesW(wide);
    free(wide);
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static char *
windows_choose_save_file(int *error)
{
    OPENFILENAMEW ofn;
    wchar_t *filename;
    char *choice = NULL;
    static const wchar_t filter[] =
        L"Rogue saved games (*.save)\0*.save\0All files (*.*)\0*.*\0";
    *error = 0;
    filename = (wchar_t *)calloc(32768, sizeof *filename);
    if (filename == NULL) { *error = 1; return NULL; }
    memset(&ofn, 0, sizeof ofn);
    ofn.lStructSize = sizeof ofn;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = 32768;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST |
                OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&ofn)) {
        choice = utf16_to_utf8(filename);
        if (choice == NULL) *error = 1;
    } else if (CommDlgExtendedError() != 0) {
        *error = 1;
    }
    free(filename);
    return choice;
}

int
rogue_windows_launch_menu(const char *default_save, int japanese,
                          char **selected_file)
{
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode;
    int key;
    if (selected_file == NULL) return -1;
    *selected_file = NULL;
    if (!rogue_windows_is_direct_launch() ||
        input == INVALID_HANDLE_VALUE || input == NULL ||
        !GetConsoleMode(input, &mode) ||
        !windows_save_exists(default_save))
        return 1;       /* no existing save: start a new adventure */
    rogue_windows_retry_direct_launch_resize(1);
    rogue_windows_wait_for_direct_launch_size();
    for (;;) {
        int error;
        char *selected;
        rogue_windows_clear_direct_launch_screen();
        printf("\n                 Rogue 3.4\n\n");
        printf("%s\n\n", japanese ?
            "セーブした冒険が見つかりました。" :
            "A saved adventure was found.");
        printf("[R/Enter] %s\n", japanese ?
            "セーブした冒険を再開する" : "Resume saved adventure");
        printf("[N]       %s\n", japanese ?
            "新しい冒険を始める" : "Start a new adventure");
        printf("[F]       %s\n", japanese ?
            "別のセーブファイルを選ぶ" : "Choose another save file");
        printf("[Q]       %s\n\n", japanese ? "終了する" : "Quit");
        printf("%s", japanese ? "キーを押してください: " :
                              "Press a key: ");
        fflush(stdout);
        key = rogue_windows_edit_key();
        if (key < 0) return -1;
        if (key == 'n' || key == 'N') return 1;
        if (key == 'q' || key == 'Q' || key == 27) return 0;
        if (key == 'r' || key == 'R' || key == 13 || key == 10) {
            selected = (char *)malloc(strlen(default_save) + 1);
            if (selected == NULL) return -1;
            strcpy(selected, default_save);
            *selected_file = selected;
            return 2;
        }
        if (key != 'f' && key != 'F') continue;
        selected = windows_choose_save_file(&error);
        if (error) {
            fprintf(stderr, "Rogue 3.4: cannot open Windows save file picker.\n");
            return -1;
        }
        if (selected != NULL) {
            *selected_file = selected;
            return 2;
        }
        /* Cancel returns to the same menu and leaves the save untouched. */
    }
}

/* Console password entry before curses starts.  Accept only printable ASCII
 * for the historical 8-character DES password; never echo its characters.
 * The game-time Ctrl-P route instead uses curses readchar() consistently. */
const char *
rogue_windows_getpass(const char *prompt)
{
    static char password[80];
    size_t len = 0;
    int key;
    memset(password, 0, sizeof password);
    if (prompt != NULL) {
        fputs(prompt, stdout);
        fflush(stdout);
    }
    for (;;) {
        key = rogue_windows_edit_key();
        if (key < 0 || key == 13 || key == 10 || key == 27) break;
        if (key == 8 || key == 127 || key == KEY_BACKSPACE) {
            if (len != 0) len--;
            continue;
        }
        if (key == 21) { len = 0; continue; }
        if (key >= 32 && key <= 126 && len + 1 < sizeof password)
            password[len++] = (char)key;
    }
    password[len] = '\0';
    fputc('\n', stdout);
    fflush(stdout);
    return password;
}

/* Windows UCRT has no crypt(3). Use the same FreeSec DES implementation as
 * Rogue 3.6 JEC; the original Rogue 3.4 PASSWD hash remains unchanged. */
extern char *xcrypt(const char *key, const char *setting);
char *
rogue_windows_crypt(const char *plain, const char *salt)
{
    return xcrypt(plain, salt);
}

char *
rogue_windows_last_argument_utf8(void)
{
    LPWSTR *args;
    int argc;
    char *last = NULL;
    args = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (args == NULL) return NULL;
    if (argc >= 2)
        last = utf16_to_utf8(args[argc - 1]);
    LocalFree(args);
    return last;
}

char *
rogue_windows_getenv_utf8(const char *name)
{
    wchar_t wname[80];
    DWORD needed;
    wchar_t *wide;
    char *result;
    int n;
    if (name == NULL) return NULL;
    n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1,
                            wname, (int)(sizeof wname / sizeof wname[0]));
    if (n <= 0) return NULL;
    SetLastError(ERROR_SUCCESS);
    needed = GetEnvironmentVariableW(wname, NULL, 0);
    if (needed == 0) return NULL;
    wide = (wchar_t *)malloc((size_t)needed * sizeof *wide);
    if (wide == NULL) return NULL;
    if (GetEnvironmentVariableW(wname, wide, needed) >= needed) {
        free(wide);
        return NULL;
    }
    result = utf16_to_utf8(wide);
    free(wide);
    return result;
}

int
rogue_windows_get_home(char *result, size_t capacity)
{
    char *value = rogue_windows_getenv_utf8("USERPROFILE");
    size_t len;
    if (value == NULL) value = rogue_windows_getenv_utf8("HOME");
    if (value == NULL || *value == '\0') {
        free(value);
        return -1;
    }
    len = strlen(value);
    if (len + 2 > capacity) {
        free(value);
        return -1;
    }
    memcpy(result, value, len);
    if (result[len - 1] != '/' && result[len - 1] != '\\')
        result[len++] = '/';
    result[len] = '\0';
    free(value);
    return 0;
}

int
rogue_windows_get_username(char *result, size_t capacity)
{
    wchar_t wide[256];
    DWORD len = (DWORD)(sizeof wide / sizeof wide[0]);
    char *name;
    size_t n;
    if (!GetUserNameW(wide, &len)) return -1;
    name = utf16_to_utf8(wide);
    if (name == NULL) return -1;
    n = strlen(name);
    if (n >= capacity) {
        free(name);
        return -1;
    }
    memcpy(result, name, n + 1);
    free(name);
    return 0;
}

int
rogue_windows_ui_is_japanese(void)
{
    return PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_JAPANESE;
}

int
rogue_windows_process_id(void)
{
    return (int)GetCurrentProcessId();
}

void
rogue_visual_delay(unsigned int milliseconds)
{
    Sleep(milliseconds);
}

int
rogue_erase_char(void)
{
    return '\b';
}

int
rogue_kill_char(void)
{
    return 21;
}

static int size_check_in_progress;
static volatile sig_atomic_t interrupt_pending = 0;
static volatile sig_atomic_t terminate_pending = 0;

static void
rogue_windows_signal(int sig)
{
    if (sig == SIGINT) interrupt_pending = 1;
    else terminate_pending = sig;
}

int
rogue_install_signal_handlers(void)
{
    if (signal(SIGINT, rogue_windows_signal) == SIG_ERR ||
        signal(SIGTERM, rogue_windows_signal) == SIG_ERR)
        return -1;
    return 0;
}

void
rogue_check_signals(void)
{
    if (terminate_pending) {
        int sig = terminate_pending;
        terminate_pending = 0;
        if (cw != NULL) endwin();
        exit(128 + sig);
    }
    if (interrupt_pending) {
        interrupt_pending = 0;
        if (size_check_in_progress) {
            /* A quit dialog cannot be drawn after endwin() and while the
             * physical viewport is smaller than the dungeon.  During this
             * exceptional wait, Ctrl-C exits safely instead. */
            fprintf(stderr, "Rogue 3.4: interrupted while terminal is too small.\n");
            exit(130);
        }
        quit();
        interrupt_pending = 0;
    }
}

/* The dungeon window, the monster window, and overlay window are all
 * 80x24 data structures.  Their sizes must NEVER follow terminal changes.
 * Remember which is displayed: readchar() must not refresh the game map
 * over a visible help/inventory/options window while waiting for a key. */
static WINDOW *last_visible_window;

WINDOW *
rogue_windows_input_window(void)
{
    if (last_visible_window != NULL && last_visible_window != curscr)
        return last_visible_window;
    return cw != NULL ? cw : stdscr;
}

void
rogue_check_terminal_size(void)
{
    int rows, cols;
    WINDOW *visible;

    if (size_check_in_progress || in_shell || cw == NULL ||
        !rogue_windows_terminal_dimensions(&rows, &cols))
        return;
    if (rows >= ROGUE_LINES && cols >= ROGUE_COLS)
        return;

    size_check_in_progress = 1;
    visible = rogue_windows_input_window();
    if (!isendwin())
        endwin();
    fprintf(stderr, "\nRogue 3.4: terminal is %dx%d; enlarge to at least "
            "80x24 to continue.\n", cols, rows);
    fflush(stderr);
    /* Delays take no game turns and never consume a key.  Check signals at
     * a safe execution point, not inside the Win32 console event handler. */
    while (!rogue_windows_terminal_dimensions(&rows, &cols) ||
           rows < ROGUE_LINES || cols < ROGUE_COLS) {
        rogue_check_signals();
        Sleep(150);
    }
    reset_prog_mode();
    cbreak();
    noecho();
    if (visible != NULL) {
        clearok(visible, TRUE);
        touchwin(visible);
        rogue_draw_utf8(visible);
        /* reset_prog_mode() and its first PDCurses refresh may overwrite
         * the cursor style.  Restore it only AFTER that first repaint. */
        rogue_windows_restore_default_cursor();
    }
    size_check_in_progress = 0;
}

int
rogue_draw(WINDOW *window)
{
    if (window == NULL)
        return ERR;
    /* curscr is the internal physical-screen image; it is never a keyboard
     * window.  It can be refreshed by the original Ctrl-L command. */
    if (window != curscr)
        last_visible_window = window;
    rogue_check_terminal_size();
    return rogue_draw_utf8(window);
}

void
rogue_resume_screen(void)
{
    if (cw == NULL)
        return;
    reset_prog_mode();
    cbreak();
    noecho();
    last_visible_window = cw;
    rogue_check_terminal_size();
    clearok(cw, TRUE);
    touchwin(cw);
    rogue_draw_utf8(cw);
    /* The shell escape's reset_prog_mode() / refresh can select an
     * underline cursor.  Reapply the user's configured shape after the
     * completed repaint; ordinary frame updates need no extra VT output. */
    rogue_windows_restore_default_cursor();
}
#endif /* _WIN32 */
