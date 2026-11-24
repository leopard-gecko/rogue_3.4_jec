/* Windows console presentation helpers.  No dungeon rules live here. */
#ifndef ROGUE34_WINDOWS_CONSOLE_H
#define ROGUE34_WINDOWS_CONSOLE_H
#ifdef _WIN32
void rogue_windows_init_console(void);
/* Restore the configured shape after curses changes terminal screen modes.
 * The helper preserves the console's current VT mode. */
void rogue_windows_restore_default_cursor(void);
/* Call real PDCurses endwin(), then restore the cursor shape. */
int rogue_windows_endwin(void);
void rogue_windows_resize_direct_launch(void);
void rogue_windows_retry_direct_launch_resize(int restoring);
void rogue_windows_wait_for_direct_launch_size(void);
int rogue_windows_is_direct_launch(void);
void rogue_windows_clear_direct_launch_screen(void);
int rogue_windows_terminal_dimensions(int *rows, int *cols);
/* Unicode scalar input for option strings (including supplementary emoji). */
int rogue_windows_has_edit_input(void);
int rogue_windows_edit_key(void);
/* Input after endwin(): use console events, not curses or stdio input. */
void rogue_windows_wait_for_plain_return(void);
/* Only direct launches: wait after the final score/quit screen. */
void rogue_windows_wait_for_score_exit(const char *prompt);
#endif
#endif
