/* Windows console handling for Rogue 3.4 JEC, derived from the working
 * Rogue 3.6 JEC VT design.  The dungeon remains 80x24; only the outer
 * terminal is resized, and only for a conservatively detected direct launch.
 */
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#include <windows.h>
#include <tlhelp32.h>
#include <string.h>
#include <stdio.h>
#ifdef MOUSE_MOVED
#undef MOUSE_MOVED
#endif
#include <curses.h>
#include "windows/rogue_windows_console.h"

static int direct_launch;

void
rogue_windows_init_console(void)
{
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode;
    /* Do not change a redirected output stream.  The VT backend and printf
     * both expect UTF-8 when connected to a Win32 console. */
    if (output != NULL && output != INVALID_HANDLE_VALUE &&
        GetConsoleMode(output, &mode)) {
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
    }
}

/* PDCursesMod's VT backend may set an explicit cursor shape when curses
 * starts or shuts down.  Restore the terminal profile's cursor shape with
 * DECSCUSR (CSI 0 SP q).  endwin() can disable VT output, so temporarily
 * enable it while sending the reset and then preserve the exact prior mode.
 * Never send VT sequences to redirected streams or non-console handles.
 *
 * Use this only at screen-mode transitions (endwin / first resumed draw),
 * not on each movement refresh: ordinary drawing already avoids flicker. */
void
rogue_windows_restore_default_cursor(void)
{
    static const char default_cursor[] = "\033[0 q";
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD saved_mode, vt_mode, written;

    if (output == NULL || output == INVALID_HANDLE_VALUE ||
        !GetConsoleMode(output, &saved_mode))
        return;

    vt_mode = saved_mode | ENABLE_PROCESSED_OUTPUT |
                           ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    /* PDCurses and stdio use different output paths.  Make sure their
     * previous screen writes complete before the shape reset is issued. */
    (void)fflush(stdout);
    if (vt_mode == saved_mode || SetConsoleMode(output, vt_mode)) {
        written = 0;
        (void)WriteFile(output, default_cursor,
                        (DWORD)(sizeof default_cursor - 1), &written, NULL);
        if (vt_mode != saved_mode)
            (void)SetConsoleMode(output, saved_mode);
    }
}

/* Do not let the VT backend leave a blinking block after quitting or when
 * handing terminal control to the temporary shell.  The next game-side
 * repaint will separately restore the default after reset_prog_mode(). */
int
rogue_windows_endwin(void)
{
    int result = endwin();
    rogue_windows_restore_default_cursor();
    return result;
}

/* Recognise common interactive shells before attempting a direct-launch
 * resize.  WT_SESSION and GetConsoleWindow() are not reliable identifiers
 * for a Windows Terminal ConPTY session. */
static int
is_shell_name(const wchar_t *name)
{
    static const wchar_t *const names[] = {
        L"cmd.exe", L"powershell.exe", L"pwsh.exe", L"bash.exe",
        L"sh.exe", L"mintty.exe", L"wsl.exe", L"zsh.exe",
        L"fish.exe", L"nu.exe"
    };
    unsigned int i;
    for (i = 0; i < sizeof names / sizeof names[0]; i++)
        if (lstrcmpiW(name, names[i]) == 0)
            return 1;
    return 0;
}

/* Return 1 for a known direct parent, -1 for a shell, and 0 if unknown. */
static int
launch_parent_kind(void)
{
    HANDLE snapshot;
    PROCESSENTRY32W entry;
    DWORD parent_pid = 0;
    int kind = 0;
    snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return 0;
    memset(&entry, 0, sizeof entry);
    entry.dwSize = sizeof entry;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (entry.th32ProcessID == GetCurrentProcessId()) {
                parent_pid = entry.th32ParentProcessID;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    memset(&entry, 0, sizeof entry);
    entry.dwSize = sizeof entry;
    if (parent_pid != 0 && Process32FirstW(snapshot, &entry)) {
        do {
            const wchar_t *name;
            if (entry.th32ProcessID != parent_pid)
                continue;
            name = entry.szExeFile;
            if (lstrcmpiW(name, L"explorer.exe") == 0 ||
                lstrcmpiW(name, L"WindowsTerminal.exe") == 0 ||
                lstrcmpiW(name, L"WindowsTerminalPreview.exe") == 0 ||
                lstrcmpiW(name, L"OpenConsole.exe") == 0 ||
                lstrcmpiW(name, L"conhost.exe") == 0 ||
                lstrcmpiW(name, L"wt.exe") == 0)
                kind = 1;
            else if (is_shell_name(name))
                kind = -1;
            break;
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return kind;
}

static int
only_console_client(void)
{
    DWORD pids[16], count, i;
    int found = 0;
    count = GetConsoleProcessList(pids, 16);
    if (count == 0 || count > 16)
        return 0;
    for (i = 0; i < count; i++) {
        if (pids[i] == GetCurrentProcessId())
            found = 1;
        else if (pids[i] != 0)
            return 0;
    }
    return found;
}

/* In ConPTY a helper process may also be attached.  Do not resize if any
 * of the attached processes is an interactive shell. */
static int
console_has_shell(void)
{
    DWORD pids[64], count, i;
    HANDLE snapshot;
    PROCESSENTRY32W entry;
    int found = 0;
    count = GetConsoleProcessList(pids, 64);
    if (count == 0 || count > 64)
        return 0;
    snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return 0;
    memset(&entry, 0, sizeof entry);
    entry.dwSize = sizeof entry;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            for (i = 0; i < count; i++) {
                if (entry.th32ProcessID != GetCurrentProcessId() &&
                    entry.th32ProcessID == pids[i] &&
                    is_shell_name(entry.szExeFile)) {
                    found = 1;
                    break;
                }
            }
            if (found)
                break;
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return found;
}

/* Match the ordering used by Rogue 3.6 JEC: shrink the visible viewport
 * before shrinking the buffer.  Set both to exactly 80x24 when possible. */
static void
resize_console_host(HANDLE output)
{
    CONSOLE_SCREEN_BUFFER_INFO info;
    COORD largest, target;
    SMALL_RECT interim, final_rect;
    SHORT width, height;
    largest = GetLargestConsoleWindowSize(output);
    if (largest.X < 80 || largest.Y < 24 ||
        !GetConsoleScreenBufferInfo(output, &info))
        return;
    width = (SHORT)(info.srWindow.Right - info.srWindow.Left + 1);
    height = (SHORT)(info.srWindow.Bottom - info.srWindow.Top + 1);
    if (width <= 0 || height <= 0)
        return;
    if (info.dwSize.X == 80 && info.dwSize.Y == 24 &&
        width == 80 && height == 24)
        return;
    interim.Left = interim.Top = 0;
    interim.Right = (SHORT)((width < 80 ? width : 80) - 1);
    interim.Bottom = (SHORT)((height < 24 ? height : 24) - 1);
    if (!SetConsoleWindowInfo(output, TRUE, &interim))
        return;
    target.X = 80;
    target.Y = 24;
    if (!SetConsoleScreenBufferSize(output, target)) {
        (void)SetConsoleWindowInfo(output, TRUE, &info.srWindow);
        return;
    }
    final_rect.Left = final_rect.Top = 0;
    final_rect.Right = 79;
    final_rect.Bottom = 23;
    if (!SetConsoleWindowInfo(output, TRUE, &final_rect)) {
        (void)SetConsoleScreenBufferSize(output, info.dwSize);
        (void)SetConsoleWindowInfo(output, TRUE, &info.srWindow);
    }
}

static void
request_vt_resize(HANDLE output, DWORD mode)
{
    static const char request[] = "\033[8;24;80t";
    DWORD written;
    if ((mode & (ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) !=
        (ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
        if (!SetConsoleMode(output, mode | ENABLE_PROCESSED_OUTPUT |
                            ENABLE_VIRTUAL_TERMINAL_PROCESSING))
            return;
    }
    (void)WriteFile(output, request, (DWORD)(sizeof request - 1),
                    &written, NULL);
}

void
rogue_windows_resize_direct_launch(void)
{
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode;
    HWND window;
    int parent_kind, sole_client;
    if (output == NULL || output == INVALID_HANDLE_VALUE ||
        !GetConsoleMode(output, &mode))
        return;
    parent_kind = launch_parent_kind();
    if (parent_kind < 0)
        return;                 /* never resize a shell-launched game */
    sole_client = only_console_client();
    if (console_has_shell() || (parent_kind == 0 && !sole_client))
        return;
    window = GetConsoleWindow();
    if (window != NULL && IsWindowVisible(window) && sole_client)
        resize_console_host(output);
    direct_launch = 1;
    /* ConPTY may have a proxy HWND and no WT_SESSION.  Send VT resize even
     * when the Console Host API path was attempted. */
    request_vt_resize(output, mode);
}

void
rogue_windows_retry_direct_launch_resize(int restoring)
{
    HANDLE output;
    DWORD mode;
    if (!direct_launch)
        return;
    if (restoring)
        Sleep(350);
    output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (output == NULL || output == INVALID_HANDLE_VALUE ||
        !GetConsoleMode(output, &mode))
        return;
    request_vt_resize(output, mode);
}

int
rogue_windows_is_direct_launch(void)
{
    return direct_launch;
}

/* ConPTY window management is asynchronous.  Allow a short settling
 * interval *before* initscr(), so PDCurses first observes a usable
 * viewport.  An existing shell never enters this wait or gets resized. */
void
rogue_windows_wait_for_direct_launch_size(void)
{
    int rows, cols, attempt;
    if (!direct_launch)
        return;
    for (attempt = 0; attempt < 12; attempt++) {
        if (rogue_windows_terminal_dimensions(&rows, &cols) &&
            rows >= 24 && cols >= 80)
            return;
        if (attempt == 4 || attempt == 8)
            rogue_windows_retry_direct_launch_resize(0);
        Sleep(100);
    }
}


/* Clear only a separately launched terminal, and only outside curses.
 * This removes stale scrollback offsets after a resize. */
void
rogue_windows_clear_direct_launch_screen(void)
{
    static const char erase[] = "\033[2J\033[H";
    HANDLE output;
    DWORD mode, written;
    if (!direct_launch)
        return;
    output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (output == NULL || output == INVALID_HANDLE_VALUE ||
        !GetConsoleMode(output, &mode))
        return;
    if ((mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0) {
        (void)WriteFile(output, erase, (DWORD)(sizeof erase - 1),
                        &written, NULL);
    } else {
        CONSOLE_SCREEN_BUFFER_INFO info;
        COORD origin = {0, 0};
        DWORD cells;
        if (!GetConsoleScreenBufferInfo(output, &info))
            return;
        cells = (DWORD)info.dwSize.X * (DWORD)info.dwSize.Y;
        (void)FillConsoleOutputCharacterW(output, L' ', cells, origin, &written);
        (void)FillConsoleOutputAttribute(output, info.wAttributes,
                                         cells, origin, &written);
        (void)SetConsoleCursorPosition(output, origin);
    }
}

/* Current visible viewport dimensions, not the potentially larger scrollback
 * buffer dimensions.  Unsupported/redirected handles return 0. */
int
rogue_windows_terminal_dimensions(int *rows, int *cols)
{
    CONSOLE_SCREEN_BUFFER_INFO info;
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (rows == NULL || cols == NULL || output == NULL ||
        output == INVALID_HANDLE_VALUE ||
        !GetConsoleScreenBufferInfo(output, &info))
        return 0;
    *rows = info.srWindow.Bottom - info.srWindow.Top + 1;
    *cols = info.srWindow.Right - info.srWindow.Left + 1;
    return *rows > 0 && *cols > 0;
}

/* Windows Unicode option editor input, based on Rogue 3.6 JEC's two
 * transports. A native console supplies UTF-16 KEY_EVENT_RECORDs; mintty
 * supplies UTF-8 bytes through a pipe. Do not mix these input sources with
 * PDCurses wgetch() while an option string is being edited. */
int
rogue_windows_has_edit_input(void)
{
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode;
    return input != NULL && input != INVALID_HANDLE_VALUE &&
           (GetConsoleMode(input, &mode) ||
            GetFileType(input) == FILE_TYPE_PIPE);
}

/* Millisecond timeout only for Escape sequences or UTF-8 continuation
 * bytes; ordinary input waits without busy polling. */
static int
pipe_byte(HANDLE input, int timeout_ms)
{
    unsigned char byte;
    DWORD received = 0, available = 0;
    int elapsed;
    if (timeout_ms < 0)
        return ReadFile(input, &byte, 1, &received, NULL) && received == 1
               ? byte : -1;
    for (elapsed = 0; elapsed <= timeout_ms; elapsed += 5) {
        if (!PeekNamedPipe(input, NULL, 0, NULL, &available, NULL))
            return -1;
        if (available)
            return ReadFile(input, &byte, 1, &received, NULL) && received == 1
                   ? byte : -1;
        if (elapsed != timeout_ms)
            Sleep(5);
    }
    return -1;
}

static int
pipe_edit_key(HANDLE input)
{
    int c, next, need, i;
    unsigned int scalar, min_scalar;
    for (;;) {
        c = pipe_byte(input, -1);
        if (c < 0)
            return -1;
        if (c == 27) {
            int seq = pipe_byte(input, 65);
            if (seq < 0)
                return 27;
            if (seq == '[' || seq == 'O') {
                int final = pipe_byte(input, 65);
                if (final == 'D') return KEY_LEFT;
                if (final == 'C') return KEY_RIGHT;
                if (final == '3') {
                    (void)pipe_byte(input, 65); /* '~' */
                    return KEY_DC;
                }
                /* Discard bracketed-paste delimiters but preserve text. */
                if (seq == '[' && final == '2') {
                    int a = pipe_byte(input, 65);
                    int b = pipe_byte(input, 65);
                    int end = pipe_byte(input, 65);
                    if (a == '0' && (b == '0' || b == '1') && end == '~')
                        continue;
                }
            }
            return 27;
        }
        if (c < 0x80)
            return c;
        if (c >= 0xc2 && c <= 0xdf) {
            need = 1; scalar = (unsigned int)(c & 0x1f); min_scalar = 0x80;
        } else if (c >= 0xe0 && c <= 0xef) {
            need = 2; scalar = (unsigned int)(c & 0x0f); min_scalar = 0x800;
        } else if (c >= 0xf0 && c <= 0xf4) {
            need = 3; scalar = (unsigned int)(c & 7); min_scalar = 0x10000;
        } else {
            continue;
        }
        for (i = 0; i < need; i++) {
            next = pipe_byte(input, 250);
            if (next < 0 || (next & 0xc0) != 0x80)
                break;
            scalar = (scalar << 6) | (unsigned int)(next & 0x3f);
        }
        if (i == need && scalar >= min_scalar && scalar <= 0x10ffff &&
            (scalar < 0xd800 || scalar > 0xdfff))
            return (int)scalar;
        /* Malformed input is discarded rather than inserted as a fragment. */
    }
}

/* Win32 console input is UTF-16. On some ConPTY versions a surrogate pair
 * arrives on key-up rather than key-down; recognise these only when VT
 * input mode is active and suppress duplicate releases. */
static int
console_edit_key(HANDLE input, DWORD mode)
{
    INPUT_RECORD record;
    DWORD received;
    static unsigned int pending_high;
    static unsigned int down_high;
    static unsigned int down_low;
    const int vt_input = (mode & ENABLE_VIRTUAL_TERMINAL_INPUT) != 0;
    for (;;) {
        unsigned int c;
        WORD vk;
        int down, surrogate;
        if (!ReadConsoleInputW(input, &record, 1, &received) || received != 1)
            return -1;
        if (record.EventType != KEY_EVENT)
            continue;
        down = record.Event.KeyEvent.bKeyDown != 0;
        c = (unsigned int)record.Event.KeyEvent.uChar.UnicodeChar;
        vk = record.Event.KeyEvent.wVirtualKeyCode;
        surrogate = c >= 0xd800 && c <= 0xdfff;
        if (!down) {
            if (!vt_input || !surrogate)
                continue;
            if (c >= 0xd800 && c <= 0xdbff && down_high == c) {
                down_high = 0;
                continue;
            }
            if (c >= 0xdc00 && c <= 0xdfff && down_low == c) {
                down_low = 0;
                continue;
            }
        } else if (c >= 0xd800 && c <= 0xdbff) {
            down_high = c;
        } else if (c >= 0xdc00 && c <= 0xdfff) {
            down_low = c;
        }
        if (!c) {
            switch (vk) {
            case VK_LEFT: pending_high = 0; return KEY_LEFT;
            case VK_RIGHT: pending_high = 0; return KEY_RIGHT;
            case VK_DELETE: pending_high = 0; return KEY_DC;
            case VK_BACK: pending_high = 0; return KEY_BACKSPACE;
            case VK_RETURN: pending_high = 0; return 13;
            case VK_ESCAPE: pending_high = 0; return 27;
            default: continue;
            }
        }
        if (pending_high) {
            unsigned int high = pending_high;
            pending_high = 0;
            if (c >= 0xdc00 && c <= 0xdfff)
                return (int)(0x10000u + ((high - 0xd800u) << 10) +
                             (c - 0xdc00u));
        }
        if (c >= 0xd800 && c <= 0xdbff) {
            pending_high = c;
            continue;
        }
        if (c >= 0xdc00 && c <= 0xdfff)
            continue;
        return (int)c;
    }
}

int
rogue_windows_edit_key(void)
{
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode;
    if (input == NULL || input == INVALID_HANDLE_VALUE)
        return -1;
    if (GetConsoleMode(input, &mode))
        return console_edit_key(input, mode);
    if (GetFileType(input) == FILE_TYPE_PIPE)
        return pipe_edit_key(input);
    return -1;
}
/* After endwin() the curses screen no longer owns keyboard input.
 * Read one Return from the same Unicode console/pipe path as the menu.
 * Unlike stdio getchar(), this does not mix CRT buffering and console events. */
void
rogue_windows_wait_for_plain_return(void)
{
    int key;
    do {
        key = rogue_windows_edit_key();
    } while (key >= 0 && key != 13 && key != 10);
}

void
rogue_windows_wait_for_score_exit(const char *prompt)
{
    HANDLE input, output;
    DWORD input_mode, output_mode;
    if (!direct_launch) return; /* never block a shell-launched game */
    input = GetStdHandle(STD_INPUT_HANDLE);
    output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (input == INVALID_HANDLE_VALUE || input == NULL ||
        output == INVALID_HANDLE_VALUE || output == NULL ||
        !GetConsoleMode(input, &input_mode) ||
        !GetConsoleMode(output, &output_mode))
        return;
    /* Discard buffered quit/tombstone input before showing the prompt. */
    (void)FlushConsoleInputBuffer(input);
    printf("\n%s", prompt != NULL ? prompt : "[Press Enter to exit]");
    fflush(stdout);
    rogue_windows_wait_for_plain_return();
}

#endif /* _WIN32 */
