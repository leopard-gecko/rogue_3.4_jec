/*
 * Rogue
 * Exploring the dungeons of doom
 * Copyright (C) 1980 by Michael Toy and Glenn Wichman
 * All rights reserved
 *
 * @(#)main.c	X.XX (Berkeley) XX/XX/XX
 */

#include <signal.h>
#ifndef _WIN32
#include <pwd.h>
#endif
#include "rogue.h"
#include "jec/language.h"

int
main(int argc, char **argv, char **envp)
{
    register char *env;
#ifndef _WIN32
    register struct passwd *pw;
#endif
    register struct linked_list *item;
    register struct object *obj;

#ifdef _WIN32
    char *menu_save = NULL;
    rogue_windows_init_console();
    /* Before curses starts, resize only a console opened directly from the
     * Explorer-style GUI.  Existing shells keep their window size. */
    rogue_windows_resize_direct_launch();
#endif
    /* Initialize language before options and normal game startup. */
    jrogue_init_locale();
    jrogue_detect_language();

    /*
     * Check to see if he is a wizard
     */
    if (argc >= 2 && argv[1][0] == '\0') {
#ifdef _WIN32
        const char *password = rogue_windows_getpass(
            JSTR("魔法使いの合言葉は？", "Wizard's password: "));
        if (wizard_password_ok(password)) {
            wizard = TRUE;
            argv++;
            argc--;
        }
#else
        if (wizard_password_ok(getpass(JSTR("魔法使いの合言葉は？", "Wizard's password: ")))) {
            wizard = TRUE;
            argv++;
            argc--;
        }
#endif
    }

    /*
     * get home and options from environment
     */
    setbuf(stdout, outbuf);

    /* Filesystem paths are byte strings, not ASCII game messages.  Keep
       the full HOME value, including UTF-8 and spaces, or fail explicitly. */
#ifdef _WIN32
    if (rogue_windows_get_home(home, sizeof home) != 0) {
        fprintf(stderr, "Rogue 3.4: cannot find Windows home directory.\n");
        return 1;
    }
#else
    {
        const char *directory = getenv("HOME");
        size_t length;
        if (directory == NULL || *directory == '\0') {
            pw = getpwuid(getuid());
            directory = pw != NULL ? pw->pw_dir : "";
        }
        length = strlen(directory);
        if (length >= sizeof home ||
            length + (length && directory[length - 1] != '/' ? 1u : 0u) >= sizeof home) {
            fprintf(stderr, "Rogue 3.4: HOME directory path is too long.\n");
            return 1;
        }
        memcpy(home, directory, length + 1);
        if (length && home[length - 1] != '/') {
            home[length++] = '/';
            home[length] = '\0';
        }
    }

#endif

    if (rogue_default_save_path(file_name, sizeof file_name) != 0) {
        fprintf(stderr, "Rogue 3.4: configured save path is too long or invalid.\n");
        return 1;
    }

#ifdef _WIN32
    env = rogue_windows_getenv_utf8("ROGUEOPTS");
#else
    env = getenv("ROGUEOPTS");
#endif
    if (env != NULL && parse_opts(env) != 0) {
        fprintf(stderr, "Rogue 3.4: invalid ROGUEOPTS language or overlong save filename.\n");
#ifdef _WIN32
        free(env);
#endif
        return 1;
    }
    /*
     * check for print-score option
     */
    if (argc == 2 && strcmp(argv[1], "-s") == 0)
    {
	waswizard = TRUE;
        return score(0, -1, 0) == 0 ? 0 : 1;
    }
    if (env == NULL || whoami[0] == '\0') {
#ifdef _WIN32
        if (rogue_windows_get_username(whoami, sizeof whoami) != 0)
            return 1;
#else
	if ((pw = getpwuid(getuid())) == NULL)
	{
	    printf("%s", JSTR("あなたはいったい誰ですか？\n", "Say, who the hell are you?\n"));
	    exit(1);
	}
	else
	    rogue_copy_option(whoami, sizeof whoami, pw->pw_name, strlen(pw->pw_name));
#endif
    }
    if (env == NULL || fruit[0] == '\0')
	rogue_copy_option(fruit, sizeof fruit,
            JSTR("ネバネバかび", "slime-mold"),
            strlen(JSTR("ネバネバかび", "slime-mold")));

#ifdef _WIN32
    free(env);
#endif
    if (argc > 2) {
        fprintf(stderr, "Usage: %s [-s | -r | savefile]\n", argv[0]);
        return 1;
    }
#ifdef _WIN32
    /* Only a directly launched EXE without arguments gets the resume menu.
     * Shell commands and explicit -r/path arguments retain their behavior. */
    if (argc == 1) {
        int action = rogue_windows_launch_menu(file_name,
                           jrogue_lang_is_japanese, &menu_save);
        if (action <= 0) {
            free(menu_save);
            return action == 0 ? 0 : 1;
        }
    }
    if (menu_save != NULL || argc == 2) {
        char *utf8_arg = NULL;
        const char *save_path;
        if (menu_save == NULL)
            utf8_arg = rogue_windows_last_argument_utf8();
        save_path = menu_save != NULL ? menu_save :
                    utf8_arg != NULL ? utf8_arg : argv[1];
        /* Direct launch via menu or Explorer drag/drop: make the viewport
         * ready and reset scrollback before curses restores the dungeon. */
        rogue_windows_retry_direct_launch_resize(1);
        rogue_windows_wait_for_direct_launch_size();
        rogue_windows_clear_direct_launch_screen();
        {
            int restored = restore((char *)save_path, envp);
            free(menu_save);
            free(utf8_arg);
            if (!restored) {
                rogue_windows_wait_for_score_exit(JSTR(
                    "［リターンを押すと終了します］",
                    "[Press Enter to exit]"));
                return 1;
            }
        }
        playit();
        return 0;
    }
#else
    if (argc == 2) {
        if (!restore(argv[1], envp)) return 1;
        playit();
        return 0;
    }
#endif
    if (wizard && getenv("SEED") != NULL)
	dnum = atoi(getenv("SEED"));
    else
#ifdef _WIN32
        dnum = rogue_windows_process_id();
#else
	dnum = getpid();
#endif
    if (wizard)
	printf(JSTR("やあ、%s。#%dのダンジョンへようこそ・・・",
            "Hello %s, welcome to dungeon #%d"), whoami, dnum);
    else
	printf(JSTR("やあ、%s。 ダンジョンを掘っている間、ちょっと待っててね...",
            "Hello %s, just a moment while I dig the dungeon..."), whoami);
    fflush(stdout);
    /* Keep the original greeting readable before curses switches screens. */
    rogue_visual_delay(ROGUE_TEXT_NOTICE_DELAY_MS);
    seed = dnum;

#ifdef _WIN32
    /* A new Windows Terminal tab may appear after the first VT resize.
     * Retry after the normal one-second greeting delay; clear the initial
     * output only for direct launches, preventing an offset dungeon. */
    rogue_windows_retry_direct_launch_resize(0);
    rogue_windows_wait_for_direct_launch_size();
    rogue_windows_clear_direct_launch_screen();
#endif
    if (initscr() == NULL) {       /* Start up cursor package */
        fprintf(stderr, "Rogue 3.4: unable to initialize curses.\n");
        return 1;
    }
#ifdef _WIN32
    /* PDCurses may become ready a moment after the ConPTY frontend.
     * A further VT request is safe ONLY for an identified direct launch. */
    rogue_windows_retry_direct_launch_resize(0);
#endif
    if (LINES < ROGUE_LINES || COLS < ROGUE_COLS) {
        endwin();
        fprintf(stderr, "Rogue 3.4 requires a terminal of at least 80x24.\n");
        return 1;
    }
    setup();
    rogue_restore_fungi();
    init_player();			/* Roll up the rogue */
    init_names();			/* Set up names of scrolls */
    init_colors();			/* Set up colors of potions */
    /*
     * Set up windows
     */
    cw = newwin(ROGUE_LINES, ROGUE_COLS, 0, 0);
    mw = newwin(ROGUE_LINES, ROGUE_COLS, 0, 0);
    hw = newwin(ROGUE_LINES, ROGUE_COLS, 0, 0);
    if (cw == NULL || mw == NULL || hw == NULL) {
        /* Do not dereference a failed curses allocation.  No dungeon
           objects have been created yet, so these windows can be freed
           without changing any game state. */
        if (hw != NULL) delwin(hw);
        if (mw != NULL) delwin(mw);
        if (cw != NULL) delwin(cw);
        cw = mw = hw = NULL;
        endwin();
        fprintf(stderr, "Rogue 3.4: unable to allocate game windows.\n");
        return 1;
    }
    scrollok(cw, FALSE);
    scrollok(mw, FALSE);
    scrollok(stdscr, FALSE);
    scrollok(hw, FALSE);
#ifdef _WIN32
    /* Recognise special keys and preserve the visible window's cursor.
     * The Unicode string editor will be completed in stage 3. */
    keypad(cw, TRUE);
    keypad(hw, TRUE);
    keypad(stdscr, TRUE);
#endif
    waswizard = wizard;
    new_level();			/* Draw current level */
    lastscore = -1;
    /*
     * Start up daemons and fuses
     */
    start_daemon(doctor, 0, AFTER);
    fuse(swander, 0, WANDERTIME, AFTER);
    fuse(hungry, 0, HUNGERTIME, AFTER);
    start_daemon(runners, 0, AFTER);
    /*
     * Give the rogue his starting sword.
     */
    item = new_item(sizeof *obj);
    obj = (struct object *) ldata(item);
    attach(pack, item);
    inpack++;
    obj->o_type = WEAPON;
    obj->o_which = SWORD;
    init_weapon(obj, SWORD);
    obj->o_hplus = 1;
    obj->o_dplus = 1;
    cur_weapon = obj;
    /*
     * And his suit of armor
     */
    item = new_item(sizeof *obj);
    obj = (struct object *) ldata(item);
    attach(pack, item);
    inpack++;
    obj->o_type = ARMOR;
    obj->o_which = RING_MAIL;
    obj->o_ac = a_class[RING_MAIL] - 1;
    cur_armor = obj;
    /*
     * Give him some food too
     */
    item = new_item(sizeof *obj);
    obj = (struct object *) ldata(item);
    attach(pack, item);
    inpack++;
    obj->o_type = FOOD;
    obj->o_which = 0;
    playit();
}

/*
 * endit:
 *	Exit the program abnormally.
 */
int
endit(void)
{
    fatal("Ok, if you want to exit that badly, I'll have to allow it\n");
    return 0;
}

/*
 * fatal:
 *	Exit the program, printing a message.
 */
int
fatal(char *s)
{
    clear();
    move(ROGUE_LINES-2, 0);
    printw("%s", s);
    draw(stdscr);
    endwin();
    exit(0);
}

/*
 * rnd:
 *	Pick a very random number.
 */
int
rnd(int range)
{
    return range == 0 ? 0 : RN % range;
}

/*
 * roll:
 *	roll a number of dice
 */
int
roll(int number, int sides)
{
    register int dtotal = 0;

    while(number--)
	dtotal += rnd(sides)+1;
    return dtotal;
}

int
setup(void)
{
    /* Install async-signal-safe flag handlers; game work runs at safe points. */
    if (rogue_install_signal_handlers() != 0) {
        endwin();
        perror("Rogue 3.4: sigaction");
        exit(1);
    }
    cbreak();
    noecho();
    return 0;
}

/*
 * playit:
 *	The main loop of the program.  Loop until the game is over,
 * refreshing things and looking at the proper times.
 */
int
playit(void)
{
    register bool wasrunning;

    /* Initial options were parsed by main() before creating the dungeon.
       A restored game uses the options serialized in its save file. */
    oldpos = hero;
    while (playing)
    {
        rogue_check_signals();
        rogue_check_terminal_size();
	wasrunning = running;
	look();
	if (!running && wasrunning)
	{
	    door_stop = FALSE;
	    wasrunning = FALSE;
	}
	status();
	lastscore = purse;
	wmove(cw, hero.y, hero.x);
	if (!((running || count) && jump))
	{
	    /* Animate only automatic movement, not ordinary one-step moves
	     * or non-movement commands with a numeric repeat prefix.  This
	     * refresh occurs before the next repeated command; no extra turn
	     * or random draw is introduced.  With jump enabled, the original
	     * intermediate refresh (and our delay) stays suppressed. */
	    bool continuous_move = running ||
	        (count > 0 && rogue_countch != '\0' &&
	         strchr("hjklyubn", rogue_countch) != NULL);
	    draw(cw);
	    if (continuous_move)
	        rogue_visual_delay(ROGUE_VISUAL_STEP_DELAY_MS);
	}
	command();
	if (!running && wasrunning)
	    door_stop = FALSE;
    }
    endit();
    return 0;
}

#ifdef SIGTSTP
/* Suspend cleanly outside the signal handler, then restore curses when the
 * controlling shell resumes the process. SIGSTOP cannot be caught or ignored
 * and avoids SIGTSTP's special treatment of orphaned process groups. */
int
tstp(void)
{
    endwin();
    fflush(stdout);
    raise(SIGSTOP);
    rogue_resume_screen();
    return 0;
}
#endif
