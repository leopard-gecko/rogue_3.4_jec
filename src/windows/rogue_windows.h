#ifndef ROGUE34_WINDOWS_H
#define ROGUE34_WINDOWS_H

#ifdef _WIN32
#include <stddef.h>
#include <curses.h>
#include "windows/rogue_windows_console.h"
/* Windows-native UTF-16 environment lookup.  Returned strings are UTF-8. */
char *rogue_windows_getenv_utf8(const char *name);
/* UTF-8 copy of the last native Unicode command-line argument; caller frees. */
char *rogue_windows_last_argument_utf8(void);
int rogue_windows_get_home(char *result, size_t capacity);
int rogue_windows_get_username(char *result, size_t capacity);
int rogue_windows_process_id(void);
int rogue_windows_ui_is_japanese(void);
/* Direct Explorer launch: prompt for resume only when a default save exists.
 * 0 quit; 1 start new game; 2 resume (*selected_file allocated);
 * -1 fatal menu/file-dialog error. */
int rogue_windows_launch_menu(const char *default_save, int japanese,
                              char **selected_file);
/* Plain-console wizard password prompt; caller must not echo the password. */
const char *rogue_windows_getpass(const char *prompt);
char *rogue_windows_crypt(const char *plain, const char *salt);

/* Track the currently displayed overlay for PDCurses keyboard input. */
WINDOW *rogue_windows_input_window(void);
#endif

#endif /* ROGUE34_WINDOWS_H */
