/*
 * Read and execute the user commands
 *
 * @(#)command.c	X.XX (Berkeley) XX/XX/XX
 */

#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>
#ifndef _WIN32
#include <sys/wait.h>
#endif
#include "rogue.h"
#include "jec/game_text.h"
#include "jec/unicode_text.h"
#include "jec/language.h"

char rogue_countch, rogue_direction, rogue_newcount = FALSE;
static int rogue_passcount = 0;

/*
 * command:
 *	Process the user commands
 */
int
command(void)
{
    register char ch;
    register int ntimes = 1;			/* Number of player moves */


    if (on(player, ISHASTE)) ntimes++;
    /*
     * Let the daemons start up
     */
    do_daemons(BEFORE);
    do_fuses(BEFORE);
    firstmove = FALSE;
    while (ntimes--)
    {
	take = 0;
	after = TRUE;
	/*
	 * Read command or continue run
	 */
	if (!no_command)
	{
	    if (running) ch = runch;
	    else if (count) ch = rogue_countch;
	    else
	    {
		ch = readchar();
		if (mpos != 0 && !running)	/* Erase message if its there */
		    msg("");
	    }
	}
	else ch = ' ';
	if (no_command)
	{
	    if (--no_command == 0)
		msg(JSTR("また動けるようになった。", "You can move again."));
	}
	else
	{
	    /*
	     * check for prefixes
	     */
	    if (isdigit((unsigned char)ch))
	    {
		int count_overflow = FALSE;
		count = 0;
		rogue_newcount = TRUE;
		while (isdigit((unsigned char)ch))
		{
		    int digit = ch - '0';
		    /* Keep the original count for representable values, but
		       reject an unrepresentably large prefix rather than
		       overflowing or running billions of unintended turns. */
		    if (!count_overflow) {
		        if (count > (INT_MAX - digit) / 10)
		            count_overflow = TRUE;
		        else
		            count = count * 10 + digit;
		    }
		    ch = readchar();
		}
		if (count_overflow) {
		    count = 0;
		    rogue_newcount = FALSE;
		    after = FALSE;
		    msg(JSTR("回数が大きすぎる。", "Count is too large."));
		    break; /* Discard the complete, invalid prefix and command. */
		}
		rogue_countch = ch;
	    }

	    /*
	     */
	    switch (ch)
	    {
		when 'f':
		    door_stop = TRUE;
		    firstmove = TRUE;
		    if (count && !rogue_newcount)
			ch = rogue_direction;
		    else
			ch = readchar();
		    switch (ch)
		    {
			case 'h': case 'j': case 'k': case 'l':
			case 'y': case 'u': case 'b': case 'n':
			    ch = toupper(ch);
		    }
		    rogue_direction = ch;
		when 't':
		    throwing = TRUE;
		    if (count && !rogue_newcount)
			ch = rogue_direction;
		    else
			ch = readchar();
		    switch (ch)
		    {
			case 'H': case 'J': case 'K': case 'L':
			case 'Y': case 'U': case 'B': case 'N':
			    ch = tolower(ch);
		    }
		    rogue_direction = ch;
	    }
	    rogue_newcount = FALSE;
	    /*
	     * execute a command
	     */
	    if (count && !running)
		count--;
	    switch (ch)
	    {
		when '!' : shell();
		when 'h' :
		    if (!throwing)
			do_move(0, -1);
		    else
			missile(0, -1);
		when 'j' :
		    if (!throwing)
			do_move(1, 0);
		    else
			missile(1, 0);
		when 'k' :
		    if (!throwing)
			do_move(-1, 0);
		    else
			missile(-1, 0);
		when 'l' :
		    if (!throwing)
			do_move(0, 1);
		    else
			missile(0, 1);
		when 'y' :
		    if (!throwing)
			do_move(-1, -1);
		    else
			missile(-1, -1);
		when 'u' :
		    if (!throwing)
			do_move(-1, 1);
		    else
			missile(-1, 1);
		when 'b' :
		    if (!throwing)
			do_move(1, -1);
		    else
			missile(1, -1);
		when 'n' :
		    if (!throwing)
			do_move(1, 1);
		    else
			missile(1, 1);
		when 'H' : do_run('h');
		when 'J' : do_run('j');
		when 'K' : do_run('k');
		when 'L' : do_run('l');
		when 'Y' : do_run('y');
		when 'U' : do_run('u');
		when 'B' : do_run('b');
		when 'N' : do_run('n');
		when 'Q' : after = FALSE; quit();
		when 'i' : after = FALSE; inventory(pack, 0);
		when 'I' : after = FALSE; picky_inven();
		when 'd' : drop();
		when 'q' : quaff();
		when 'r' : read_scroll();
		when 'e' : eat();
		when 'w' : wield();
		when 'W' : wear();
		when 'T' : take_off();
		when 'o' : option();
		when '>' : after = FALSE; d_level();
		when '<' : after = FALSE; u_level();
		when '?' : after = FALSE; help();
		when '/' : after = FALSE; identify();
		when 's' : search();
		when 'v' : msg("Rogue version %s. (mctesq was here)", release);
		when 'M' : after = FALSE; msg(JSTR("確保した品物：%d", "Items allocated : %d"), total);
		when 'R' : after = FALSE; msg("%s", huh);
		when CTRL('L') : after = FALSE; clearok(curscr,TRUE);draw(curscr);
		when 'S' :
		    after = FALSE;
		    if (save_game())
		    {
			wmove(cw, ROGUE_LINES-1, 0);
			wclrtoeol(cw);
			draw(cw);
			endwin();
			exit(0);
		    }
		when ' ' : ;			/* Rest command */
		when CTRL('P') :
		    after = FALSE;
		    if (wizard)
		    {
			wizard = FALSE;
			msg(JSTR("もはや、魔法使いではない。", "Not wizard any more"));
		    }
		    else
		    {
			if ((wizard = passwd()))
			{
			    msg(JSTR("あなたは洞窟（#%d）の中で、突然ケン・アーノルドのように賢くなった。",
                         "You are suddenly as smart as Ken Arnold in dungeon #%d"), dnum);
			    waswizard = TRUE;
			}
			else
			    msg(JSTR("残念でした。", "Sorry"));
		    }
		when ESCAPE :	/* Escape */
		    if (door_stop || throwing || count)
		    {
			throwing = door_stop = FALSE;
			count = 0;
			after = FALSE;
		    }
		    else goto def;
		otherwise :
		def:
		    after = FALSE;
		    if (wizard) switch (ch)
		    {
			when '@' : msg("@ %d,%d", hero.y, hero.x);
			when 'C' : create_obj();
			when CTRL('I') : inventory(lvl_obj, 0);
			when CTRL('W') : whatis();
			when CTRL('D') : after = TRUE; level++; new_level();
			when CTRL('F') : show_win(stdscr, (char *)JSTR("--続く（この階の地図）--", "--More (level map)--"));
			when CTRL('X') : show_win(mw, (char *)JSTR("--続く（怪物）--", "--More (monsters)--"));
			otherwise :
			    msg(JSTR("'%s' コマンドが違います。", "Illegal command '%s'."), rogue_unctrl(ch));
			    count = 0;
		    }
		    else
		    {
			msg(JSTR("'%s' コマンドが違います。", "Illegal command '%s'."), rogue_unctrl(ch));
			count = 0;
		    }
	    }
	    /*
	     * turn off flags if no longer needed
	     */
	    throwing = FALSE;
	    if (!running)
		door_stop = FALSE;
	}
	/*
	 * If he ran into something to take, let him pick it up.
	 */
	if (take != 0)
	    pick_up(take);
	status();
	if (ntimes)
	{
	    look();
	    draw(cw);
	}
    }
    /*
     * Kick off the rest if the daemons and fuses
     */
    if (after)
    {
	do_daemons(AFTER);
	do_fuses(AFTER);
    }
    return 0;
}

/*
 * quit:
 *	Have player make certain, then exit.
 */
int
quit(void)
{
    static int awaiting_answer = 0;

    /* A second SIGINT during a Q/Ctrl-C prompt must not nest another prompt. */
    if (awaiting_answer)
        return 0;
    awaiting_answer = 1;
    msg(JSTR("本当に終了してよいのですか？ y/n ", "Really quit?"));
    draw(cw);
    if (readchar() == 'y')
    {
	clear();
	score_wait_on_screen();
	endwin();
	score(purse, 1, 0);
	exit(0);
    }
    else
    {
	wmove(cw, 0, 0);
	wclrtoeol(cw);
	status();
	draw(cw);
	mpos = 0;
	count = 0;
    }
    awaiting_answer = 0;
    return 0;
}

/*
 * search:
 *	Player gropes about him to find hidden things.
 */
int
search(void)
{
    register int x, y;
    char ch;

    /*
     * Look all around the hero, if there is something hidden there,
     * give him a chance to find it.  If its found, display it.
     */
    for (x = hero.x - 1; x <= hero.x + 1; x++)
	for (y = hero.y - 1; y <= hero.y + 1; y++)
	{
	    ch = winat(y, x);
	    switch (ch)
	    {
		case SECRETDOOR:
		    if (rnd(100) < 20) {
			mvaddch(y, x, DOOR);
			count = 0;
		    }
		    break;
		case TRAP:
		{
		    register int i;

		    if (rogue_glyph(mvwinch(cw, y, x)) == TRAP)
			break;
		    if (rnd(100) > 50)
			break;
		    i = trap_at(y, x);
                    if (i < 0)
                        break;
		    traps[i].tr_flags |= ISFOUND;
		    mvwaddch(cw, y, x, TRAP);
		    count = 0;
		    msg("%s", tr_name(traps[i].tr_type));
		}
	    }
	}
    return 0;
}

/*
 * tr_name:
 *	print the name of a trap
 */
const char *
tr_name(char ch)
{
    const char *s = JSTR("わなを見つけた。", "an unknown trap");

    switch (ch)
    {
	when TRAPDOOR:
	    s = terse ? JSTR("落とし戸のわな。", "A trapdoor.") : JSTR("落とし戸のわなを見つけた。", "You found a trapdoor.");
	when BEARTRAP:
	    s = terse ? JSTR("とらばさみのわな。", "A beartrap.") : JSTR("とらばさみのわなを見つけた。", "You found a beartrap.");
	when SLEEPTRAP:
	    s = terse ? JSTR("眠りガスのわな。", "A sleeping gas trap."):JSTR("眠りガスのわなを見つけた。", "You found a sleeping gas trap.");
	when ARROWTRAP:
	    s = terse ? JSTR("矢のわな。", "An arrow trap.") : JSTR("矢のわなを見つけた。", "You found an arrow trap.");
	when TELTRAP:
	    s = terse ? JSTR("テレポートのわな。", "A teleport trap.") : JSTR("テレポートのわなを見つけた。", "You found a teleport trap.");
	when DARTTRAP:
	    /* Fix only the original English spelling mistake: posion -> poison. */
	    s = terse ? JSTR("投げ矢のわな。", "A dart trap.") : JSTR("毒投げ矢のわなを見つけた。", "You found a poison dart trap.");
    }
    return s;
}

/*
 * help:
 *	Give single character help, or the whole mess if he wants it
 */
int
help(void)
{
    register struct h_list *strp = helpstr;
    register char helpch;
    register int cnt;

    msg(JSTR("どのコマンドのヘルプを表示しますか？ ('*'で全て表示): ", "Character you want help for (* for all): "));
    helpch = readchar();
    /*
     * If its not a *, print the right help string
     * or an error if he typed a funny character.
     */
    if (helpch != '*')
    {
	wmove(cw, 0, 0);
	while (strp->h_ch)
	{
	    if (strp->h_ch == helpch)
	    {
		msg("%s%s", rogue_unctrl(strp->h_ch), jrogue_text(strp->h_desc));
		break;
	    }
	    strp++;
	}
	if (strp->h_ch != helpch)
	    msg(JSTR("'%s' コマンドが違います。", "Unknown character '%s'"), rogue_unctrl(helpch));
	return 0;
    }
    /*
     * Here we print help for everything.
     * Then wait before we return to command mode
     */
    wclear(hw);
    cnt = 0;
    while (strp->h_ch)
    {
	mvwaddstr(hw, cnt % 23, cnt > 22 ? 40 : 0, rogue_unctrl(strp->h_ch));
        {
            char visible[200];
            int row, col;
            getyx(hw, row, col);
            (void)row;
            rogue_text_copy(visible, sizeof visible, jrogue_text(strp->h_desc),
                            sizeof visible - 1,
                            (cnt > 22 ? 79 : 39) - col);
            waddstr(hw, visible);
        }
	cnt++;
	strp++;
    }
    wmove(hw, ROGUE_LINES-1, 0);
    wprintw(hw, "%s", JSTR("--スペースを押してください--", "--Press space to continue--"));
    draw(hw);
    wait_for(' ');
    wclear(hw);
    draw(hw);
    wmove(cw, 0, 0);
    wclrtoeol(cw);
    status();
    touchwin(cw);
    return 0;
}

/*
 * identify:
 *	Tell the player what a certain thing is.
 */
int
identify(void)
{
    register char ch;
    const char *str;

    msg(JSTR("どの文字？", "What do you want identified? "));
    draw(cw);
    wmove(cw, 0, 0);
    wclrtoeol(cw);
    status();
    ch = readchar();
    mpos = 0;
    if (ch == ESCAPE)
    {
	msg("");
	return 0;
    }
    if (isalpha(ch) && isupper(ch))
	str = jrogue_text(monsters[ch-'A'].m_name);
    else switch(ch)
    {
	case '|':
	case '-':
	    str = JSTR("部屋の壁", "wall of a room");
	when GOLD: str = JSTR("金塊", "gold");
	when STAIRS : str = JSTR("隣の階への階段", "passage leading down");
	when DOOR: str = JSTR("部屋のドア", "door");
	when FLOOR: str = JSTR("部屋の床", "room floor");
	when PLAYER: str = JSTR("プレイヤー", "you");
	when PASSAGE: str = JSTR("部屋と部屋をむすぶ通路", "passage");
	when TRAP: str = JSTR("隠されたわな", "trap");
	when POTION: str = JSTR("水薬", "potion");
	when SCROLL: str = JSTR("巻き物", "scroll");
	when FOOD: str = JSTR("食糧", "food");
	when WEAPON: str = JSTR("武器", "weapon");
	when ' ' : str = JSTR("硬い岩", "solid rock");
	when ARMOR: str = JSTR("よろい", "armor");
	when AMULET: str = JSTR("イェンダーの魔除け", "The Amulet of Yendor");
	otherwise: str = JSTR("その文字は使われていない", "unknown character");
    }
    msg(JSTR("'%s'：%s", "'%s' : %s"), rogue_unctrl(ch), str);
    return 0;
}

/*
 * Look:
 *	A quick glance all around the player
 */
int
look(void)
{
    register int x, y;
    register char ch;
    register int oldx, oldy;
    register bool inpass;

    register struct room *rp;

    /* Like the original game, passcount is local to each look() call.
       Do not carry it across turns or store it in save files. */
    rogue_passcount = 0;
    getyx(cw, oldy, oldx);
    if ((rp = roomin(&oldpos)) != NULL && (rp->r_flags & ISDARK))
    {
	for (x = oldpos.x - 1; x <= oldpos.x + 1; x++)
	    for (y = oldpos.y - 1; y <= oldpos.y + 1; y++)
		if (show(y, x) == FLOOR && (y != hero.y || x != hero.x))
		    mvwaddch(cw, y, x, ' ');
    }
    inpass = (roomin(&hero) == NULL);
    for (x = hero.x - 1; x <= hero.x + 1; x++)
	if (x >= 0 && x < ROGUE_COLS) for (y = hero.y - 1; y <= hero.y + 1; y++)
	{
	    if (y <= 0 || y >= ROGUE_LINES - 1)
		continue;
	    /*
	     * Secret doors show as walls
	     */
	    ch = show(y, x);
	    if (ch == SECRETDOOR)
		ch = secretdoor(y, x);
	    /*
	     * Don't show room walls if he is in a passage
	     */
	    if ((y == hero.y && x == hero.x)
		|| (inpass && (ch == '-' || ch == '|')))
		    continue;
	    wmove(cw, y, x);
	    waddch(cw, ch);
	    if (door_stop && !firstmove && running)
	    {
		if (ch != PASSAGE)
		switch (runch)
		{
		    when 'h':
			if (x == hero.x + 1)
			    continue;
		    when 'j':
			if (y == hero.y - 1)
			    continue;
		    when 'k':
			if (y == hero.y + 1)
			    continue;
		    when 'l':
			if (x == hero.x - 1)
			    continue;
		    when 'y':
			if ((x + y) - (hero.x + hero.y) >= 1)
			    continue;
		    when 'u':
			if ((y - x) - (hero.y - hero.x) >= 1)
			    continue;
		    when 'n':
			if ((x + y) - (hero.x + hero.y) <= -1)
			    continue;
		    when 'b':
			if ((y - x) - (hero.y - hero.x) <= -1)
			    continue;
		}
		switch (ch)
		{
		    case DOOR:
			if (x == hero.x || y == hero.y)
			    running = FALSE;
			break;
		    case PASSAGE:
			if (x == hero.x || y == hero.y)
			    rogue_passcount++;
			break;
		    case FLOOR:
		    case '|':
		    case '-':
		    case ' ':
			break;
		    default:
			running = FALSE;
			break;
		}
	    }
	}
    if (door_stop && !firstmove && rogue_passcount > 2)
	running = FALSE;
    wmove(cw, oldy, oldx);
    oldpos = hero;
    return 0;
}

/*
 * d_level:
 *	He wants to go down a level
 */
int
d_level(void)
{
    if (winat(hero.y, hero.x) != STAIRS)
	msg(JSTR("降りる階段がない。", "I see no way down."));
    else
    {
	level++;
	new_level();
    }
    return 0;
}

/*
 * u_level:
 *	He wants to go up a level
 */
int
u_level(void)
{
    register struct linked_list *item;
    register struct object *obj;

    if (winat(hero.y, hero.x) == STAIRS)
    {
	for (item = pack; item != NULL; item = next(item))
	{
	    obj = (struct object *) ldata(item);
	    if (obj->o_type == AMULET)
		break;
	}
	if (item != NULL)
	{
	    msg(JSTR("はらわたをねじ切られるような感じだ。", "You feel a wrenching sensation in your gut."));
	    level--;
	    if (level == 0)
		total_winner();
	    new_level();
	    return 0;
	}
    }
    msg(JSTR("登る階段がない。", "I see no way up."));
    return 0;
}

/*
 * secretdoor:
 *	Figure out what a wall looks like at this position.
 */
int
secretdoor(int y, int x)
{
    register int i;
    register struct room *rp;
    register coord *cpp;
    coord cp;

    cp.y = y;
    cp.x = x;
    cpp = &cp;
    for (rp = rooms, i = 0; i < MAXROOMS; rp++, i++)
	if (!(rp->r_flags & ISGONE) && inroom(rp, cpp))
	{
	    if (y == rp->r_pos.y || y == rp->r_pos.y + rp->r_max.y - 1)
		return('-');
	    else
		return('|');
	}

    return('p');
}

/*
 * Let him escape for a while
 */
int
shell(void)
{
#ifdef _WIN32
    const char *shell_command = getenv("COMSPEC");
    wclear(hw);
    wmove(hw, ROGUE_LINES - 1, 0);
    draw(hw);
    endwin();
    fflush(stdout);
    in_shell = TRUE;
    (void)system(shell_command != NULL && *shell_command != '\0' ? shell_command : "cmd.exe");
    printf("%s", JSTR("\n［リターンを押してください］", "\n[Press return to continue]"));
    fflush(stdout);
    rogue_windows_wait_for_plain_return();
    in_shell = FALSE;
    rogue_resume_screen();
    return 0;
#else
    pid_t pid;
    const char *sh = getenv("SHELL");
    struct sigaction ignore, old_int, old_quit;
#ifdef SIGTSTP
    struct sigaction old_tstp;
#endif
    int status;
    int saved_errno;

    if (sh == NULL || *sh == '\0')
        sh = "/bin/sh";
    wclear(hw);
    wmove(hw, ROGUE_LINES - 1, 0);
    draw(hw);
    endwin();
    fflush(stdout);
    in_shell = TRUE;

    /* The child should receive interactive Ctrl-C/Ctrl-\ normally.  Ignore
     * them in the parent while waiting, but restore the precise previous
     * dispositions rather than switching to the game's quit/fatal handlers. */
    memset(&ignore, 0, sizeof ignore);
    ignore.sa_handler = SIG_IGN;
    sigemptyset(&ignore.sa_mask);
    if (sigaction(SIGINT, &ignore, &old_int) < 0) {
        saved_errno = errno;
        goto shell_failed;
    }
    if (sigaction(SIGQUIT, &ignore, &old_quit) < 0) {
        saved_errno = errno;
        sigaction(SIGINT, &old_int, NULL);
        goto shell_failed;
    }
#ifdef SIGTSTP
    if (sigaction(SIGTSTP, &ignore, &old_tstp) < 0) {
        saved_errno = errno;
        sigaction(SIGQUIT, &old_quit, NULL);
        sigaction(SIGINT, &old_int, NULL);
        goto shell_failed;
    }
#endif

    pid = fork();
    if (pid == 0) {
        struct sigaction normal;
        memset(&normal, 0, sizeof normal);
        normal.sa_handler = SIG_DFL;
        sigemptyset(&normal.sa_mask);
        sigaction(SIGINT, &normal, NULL);
        sigaction(SIGQUIT, &normal, NULL);
#ifdef SIGTSTP
        sigaction(SIGTSTP, &normal, NULL);
#endif
        /* Preserve historical user/group drop, in the safe order. */
        if (setgid(getgid()) < 0 || setuid(getuid()) < 0) {
            perror("Rogue 3.4: setuid/setgid");
            _exit(127);
        }
        execl(sh, "shell", "-i", (char *)NULL);
        perror("No shelly");
        _exit(127);
    }
    if (pid < 0) {
        saved_errno = errno;
    } else {
        /* An interrupted wait must not accidentally abandon the child. */
        do {
            pid_t waited = waitpid(pid, &status, 0);
            if (waited == pid) {
                saved_errno = 0;
                break;
            }
            if (waited < 0 && errno == EINTR)
                continue;
            saved_errno = errno;
            break;
        } while (1);
    }
#ifdef SIGTSTP
    sigaction(SIGTSTP, &old_tstp, NULL);
#endif
    sigaction(SIGQUIT, &old_quit, NULL);
    sigaction(SIGINT, &old_int, NULL);

    if (saved_errno != 0) {
        errno = saved_errno;
        perror("Rogue 3.4: shell");
    }
    printf("%s", JSTR("\n［リターンを押してください］",
                              "\n[Press return to continue]"));
    fflush(stdout);
    wait_for('\n');
    in_shell = FALSE;
    rogue_resume_screen();
    rogue_check_signals();
    return 0;

shell_failed:
    errno = saved_errno;
    perror("Rogue 3.4: shell");
    in_shell = FALSE;
    rogue_resume_screen();
    rogue_check_signals();
    return 0;
#endif
}

/*
 * passwd:
 *	see if user knows password
 */
int
passwd(void)
{
    char buf[80];
    size_t used = 0;
    int c;

    msg(JSTR("魔法使いの合言葉は？", "Wizard's Password:"));
    mpos = 0;
    for (;;) {
        c = readchar();
        if (c == '\n' || c == '\r' || c == ESCAPE || c == EOF)
            break;
        if (c == rogue_kill_char() || c == CTRL('U')) {
            used = 0;
            continue;
        }
        if (c == rogue_erase_char() || c == '\b' || c == 127) {
            if (used != 0) used--;
            continue;
        }
        if (c < ' ' || c > '~')
            continue;
        if (used + 1 < sizeof buf)
            buf[used++] = (char)c;
    }
    buf[used] = '\0';
    {
        int result = wizard_password_ok(buf);
        memset(buf, 0, sizeof buf);
        return result;
    }
}

/*
 * do_run:
 *	Set the running flag and rogue_direction character.
 *	Called from command() with a movement key (h,j,k,l,y,u,b,n).
 */
int
do_run(char ch)
{
    running = TRUE;
    after = FALSE;
    runch = ch;
    return 0;
}
