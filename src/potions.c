/*
 * Function(s) for dealing with potions
 *
 * @(#)potions.c	X.XX (Berkeley) XX/XX/XX
 */

#include <ctype.h>
#include "rogue.h"
#include "jec/language.h"
#include "jec/game_text.h"

int
quaff(void)
{
    register struct object *obj;
    register struct linked_list *item;
    char buf[80];

    item = get_item("quaff", POTION);
    /*
     * Make certain that it is somethings that we want to drink
     */
    if (item == NULL)
	return 0;
    obj = (struct object *) ldata(item);
    if (obj->o_type != POTION)
    {
	if (!terse)
	    msg(JSTR("おえっ。なんでそんなものを飲もうとするの？", "Yuk! Why would you want to drink that?"));
	else
	    msg(JSTR("それは飲めない。", "That's undrinkable"));
	return 0;
    }
    if (obj == cur_weapon)
	cur_weapon = NULL;

    /*
     * Calculate the effect it has on the poor guy.
     */
    switch(obj->o_which)
    {
	when P_CONFUSE:
	    /* A second potion extends the existing timer rather than
	       installing another unconfuse fuse which may expire first. */
	    if (on(player, ISHUH))
		lengthen(unconfuse, rnd(8)+HUHDURATION);
	    else
	    {
		msg(JSTR("ちょっと待った、何が起こったんだ？え？なに？だれ？", "Wait, what's going on here. Huh? What? Who?"));
		fuse(unconfuse, 0, rnd(8)+HUHDURATION, AFTER);
		player.t_flags |= ISHUH;
	    }
	    p_know[P_CONFUSE] = TRUE;
	when P_POISON:
	    chg_str(-(rnd(3)+1));
	    msg(JSTR("すごく具合が悪くなった。", "You feel very sick now."));
	    p_know[P_POISON] = TRUE;
	when P_HEALING:
	    if ((pstats.s_hpt += roll(pstats.s_lvl, 4)) > max_hp)
		pstats.s_hpt = ++max_hp;
	    msg(JSTR("気分がよくなってきた。", "You begin to feel better."));
	    p_know[P_HEALING] = TRUE;
	when P_STRENGTH:
	    msg(JSTR("強くなった気がするぞ。このはちきれそうな筋肉！", "You feel stronger, now.  What bulging muscles!"));
	    chg_str(1);
	    p_know[P_STRENGTH] = TRUE;
	when P_MFIND:
	    /*
	     * Potion of monster detection, if there are monters, detect them
	     */
	    if (mlist != NULL)
	    {
		wclear(hw);
		overwrite(mw, hw);
		show_win(hw,
		    JSTR("怪物の存在を感じ始めた。--続く--", "You begin to sense the presence of monsters.--More--"));
		p_know[P_MFIND] = TRUE;
	    }
	    else
		msg(JSTR("変な気分におそわれたが、すぐに消えていった。", "You have a strange feeling for a moment, then it passes."));
	when P_TFIND:
	    /*
	     * Potion of magic detection.  Show the potions and scrolls
	     */
	    if (lvl_obj != NULL)
	    {
		struct linked_list *mobj;
		struct object *tp;
		bool show;

		show = FALSE;
		wclear(hw);
		for (mobj = lvl_obj; mobj != NULL; mobj = next(mobj))
		{
		    tp = (struct object *) ldata(mobj);
		    if (is_magic(tp))
		    {
			show = TRUE;
			mvwaddch(hw, tp->o_pos.y, tp->o_pos.x, MAGIC);
		    }
		    p_know[P_TFIND] = TRUE;
		}
		if (show)
		{
		    show_win(hw,
			JSTR("この階で魔法の存在を感じた。--続く--", "You sense the presence of magic on this level.--More--"));
		    break;
		}
	    }
	    msg(JSTR("変な気分におそわれたが、すぐに消えていった。", "You have a strange feeling for a moment, then it passes."));
	when P_PARALYZE:
	    msg(JSTR("動けない。", "You can't move."));
	    no_command = HOLDTIME;
	    p_know[P_PARALYZE] = TRUE;
	when P_SEEINVIS:
	    msg(JSTR("この水薬は、%sジュースのような味がする。", "This potion tastes like %s juice."), fruit);
	    if (off(player, CANSEE))
	    {
		player.t_flags |= CANSEE;
		fuse(unsee, 0, SEEDURATION, AFTER);
		light(&hero);
	    }
	when P_RAISE:
	    msg(JSTR("突然に腕前がすごく上がったようだ。", "You suddenly feel much more skillful"));
	    p_know[P_RAISE] = TRUE;
	    raise_level();
	when P_XHEAL:
	    if ((pstats.s_hpt += roll(pstats.s_lvl, 8)) > max_hp)
		pstats.s_hpt = ++max_hp;
	    msg(JSTR("とても気分がよくなってきた。", "You begin to feel much better."));
	    p_know[P_XHEAL] = TRUE;
	when P_HASTE:
	    if (on(player, ISHASTE))
	    {
		msg(JSTR("消耗して気絶した。", "You faint from exhaustion."));
		no_command += rnd(8);
		extinguish(nohaste);
		player.t_flags &= ~ISHASTE;
	    }
	    else
	    {
		msg(JSTR("動きが素早くなったようだ。", "You feel yourself moving much faster."));
		player.t_flags |= ISHASTE;
		fuse(nohaste, 0, rnd(4)+4, AFTER);
	    }
	    p_know[P_HASTE] = TRUE;
	when P_RESTORE:
	    msg(JSTR("ああ、これはうまい。身体中があたたまる。", "Hey, this tastes great.  It make you feel warm all over."));
	    pstats.s_str = max_stats.s_str;
	otherwise:
	    msg(JSTR("なんてひどい味の水薬だ！", "What an odd tasting potion!"));
	    return 0;
    }
    status();
    if (p_know[obj->o_which] && p_guess[obj->o_which])
    {
	free(p_guess[obj->o_which]);
	p_guess[obj->o_which] = NULL;
    }
    else if (!p_know[obj->o_which] && askme && p_guess[obj->o_which] == NULL)
    {
	msg(terse ? JSTR("呼び名は？", "Call it: ") :
            JSTR("どんな呼び名をつける？", "What do you want to call it? "));
	mpos = 0;
	if (get_str(buf, cw) == NORM)
	{
	    p_guess[obj->o_which] = malloc((unsigned int) strlen(buf) + 1);
	    strcpy(p_guess[obj->o_which], buf);
	}
	msg("");
    }
    /*
     * Throw the item away
     */
    inpack--;
    detach(pack, item);
    discard(item);
    return 0;
}
int
read_scroll(void)
{
    register struct object *obj;
    register struct linked_list *item;
    register struct room *rp;
    register int i,j;
    register char ch, nch;
    char buf[80];

    item = get_item("read", SCROLL);
    if (item == NULL)
	return 0;
    obj = (struct object *) ldata(item);
    if (obj->o_type != SCROLL)
    {
	if (!terse)
	    msg(JSTR("それには読めるところはない。", "There is nothing on it to read"));
	else
	    msg(JSTR("それは巻き物ではない。", "Nothing to read"));
	return 0;
    }
    msg(JSTR("巻き物は読むと消えてしまった。", "As you read the scroll, it vanishes."));
    /*
     * Calculate the effect it has on the poor guy.
     */
    if (obj == cur_weapon)
	cur_weapon = NULL;
    switch(obj->o_which)
    {
	when S_CONFUSE:
	    /*
	     * Scroll of monster confusion.  Give him that power.
	     */
	    msg(JSTR("手が赤く光り始めた。", "Your hands begin to glow red"));
	    player.t_flags |= CANHUH;
	when S_LIGHT:
	    s_know[S_LIGHT] = TRUE;
	    if ((rp = roomin(&hero)) == NULL)
		msg(JSTR("通路が輝き、しばらくして光は消えていった。", "The corridor glows and then fades"));
	    else
	    {
		/* Keep the English wording of Rogue 3.4, but use the 3.6
		 * Japanese messages for both normal and terse displays. */
		msg(JSTR(terse ? "光が部屋を照らしている。" :
		    "青色に輝く光が部屋を照らしている。",
		    terse ? "The room is lit" :
		    "The room is lit by a shimmering blue light."));
		rp->r_flags &= ~ISDARK;
		/*
		 * Light the room and put the player back up
		 */
		light(&hero);
		mvwaddch(cw, hero.y, hero.x, PLAYER);
	    }
	when S_ARMOR:
	    if (cur_armor != NULL)
	    {
		msg(JSTR("よろいが一瞬かすかに光った。", "Your armor glows faintly for a moment"));
		cur_armor->o_ac--;
		cur_armor->o_flags &= ~ISCURSED;
	    }
	when S_HOLD:
	    /*
	     * Hold monster scroll.  Stop all monsters within two spaces
	     * from chasing after the hero.
	     */
	    {
		register int x,y;
		register struct linked_list *mon;

		for (x = hero.x-2; x <= hero.x+2; x++)
		    for (y = hero.y-2; y <= hero.y+2; y++)
		    {
			/* A holding scroll can be used beside the map border. */
			if (y <= 0 || y >= ROGUE_LINES - 1 ||
			    x <= 0 || x >= ROGUE_COLS - 1)
			    continue;
			if (isupper((unsigned char)rogue_glyph(mvwinch(mw, y, x))) &&
			    (mon = find_mons(y, x)) != NULL)
			{
			    struct thing *th = (struct thing *)ldata(mon);
			    th->t_flags &= ~ISRUN;
			    th->t_flags |= ISHELD;
			}
		    }
	    }
	when S_SLEEP:
	    /*
	     * Scroll which makes you fall asleep
	     */
	    s_know[S_SLEEP] = TRUE;
	    msg(JSTR("眠りにおちてしまった。", "You fall asleep."));
	    no_command += 4 + rnd(SLEEPTIME);
	when S_CREATE:
	    /*
	     * Create a monster
	     * First look in a circle around him, next try his room
	     * otherwise give up
	     */
	    {
		register int x, y;
		struct linked_list *titem;
		/* Count all eligible cells for reservoir sampling.  With C11
		   _Bool, ++appear never grows beyond one and would always
		   choose the last eligible cell, rather than sampling fairly
		   among the available cells. */
		register int appear = 0;
		coord mp;

		/*
		 * Search for an open place
		 */
		for (y = hero.y; y <= hero.y+1; y++)
		    for (x = hero.x; x <= hero.x+1; x++)
		    {
			/*
			 * Don't put a monster in top of the player.
			 */
			if ((y == hero.y && x == hero.x) ||
			    y <= 0 || y >= ROGUE_LINES - 1 ||
			    x <= 0 || x >= ROGUE_COLS - 1)
			    continue;
			/* Avoid reading or placing outside the fixed 80x24 map. */
			if (step_ok(winat(y, x)))
			{
			    if (rnd(++appear) == 0)
			    {
				mp.y = y;
				mp.x = x;
			    }
			}
		    }
		if (appear)
		{
		    titem = new_item(sizeof (struct thing));
		    new_monster(titem, randmonster(), &mp);
		}
		else
		    msg(JSTR("遠くから、かすかな苦悶の叫びが聞こえる。", "You hear a faint cry of anguish in the distance."));
	    }
	when S_IDENT:
	    /*
	     * Identify, let the rogue figure something out
	     */
	    msg(JSTR("これは識別の巻き物だ。", "This scroll is an identify scroll"));
	    s_know[S_IDENT] = TRUE;
	    whatis();
	when S_MAP:
	    /*
	     * Scroll of magic mapping.
	     */
	    s_know[S_MAP] = TRUE;
	    msg(JSTR("おや、巻き物に地図が現れたぞ。", "Oh, now this scroll has a map on it."));
	    overwrite(stdscr, hw);
	    /*
	     * Take all the things we want to keep hidden out of the window
	     */
	    for (i = 0; i < ROGUE_LINES; i++)
		for (j = 0; j < ROGUE_COLS; j++)
		{
		    switch (nch = ch = rogue_glyph(mvwinch(hw, i, j)))
		    {
			case SECRETDOOR:
			    mvaddch(i, j, nch = DOOR);
			case '-':
			case '|':
			case DOOR:
			case PASSAGE:
			case ' ':
			case STAIRS:
			    break;
			default:
			    nch = ' ';
		    }
		    if (nch != ch)
			waddch(hw, nch);
		}
	    /*
	     * Copy in what he has discovered
	     */
	    overlay(cw, hw);
	    /*
	     * And set up for display
	     */
	    overwrite(hw, cw);
	when S_GFIND:
	    /*
	     * Potion of gold detection
	     */
	    {
		int gtotal = 0;

		wclear(hw);
		for (i = 0; i < MAXROOMS; i++)
		{
		    gtotal += rooms[i].r_goldval;
		    if (rooms[i].r_goldval != 0 &&
			rogue_glyph(mvwinch(stdscr, rooms[i].r_gold.y, rooms[i].r_gold.x))
			== GOLD)
			mvwaddch(hw,rooms[i].r_gold.y,rooms[i].r_gold.x,GOLD);
		}
		if (gtotal)
		{
		    s_know[S_GFIND] = TRUE;
		    show_win(hw,
			JSTR("欲ばりな気分になり、金塊の気配を感じる。--続く--", "You begin to feel greedy and you sense gold.--More--"));
		}
		else msg(JSTR("下へ引っ張られるような感じがする。", "You begin to feel a pull downward"));
	    }
	when S_TELEP:
	    /*
	     * Scroll of teleportation:
	     * Make him dissapear and reappear
	     */
	    {
		int rm;
		struct room *cur_room;

		cur_room = roomin(&hero);
		rm = teleport();
                if (rm >= 0 && rm < MAXROOMS && cur_room != &rooms[rm])
                    s_know[S_TELEP] = TRUE;
	    }
	when S_ENCH:
	    if (cur_weapon == NULL)
		msg(JSTR("奇妙な喪失感をかんじた。", "You feel a strange sense of loss."));
	    else
	    {
		cur_weapon->o_flags &= ~ISCURSED;
		if (rnd(100) > 50)
		    cur_weapon->o_hplus++;
		else
		    cur_weapon->o_dplus++;
                /* A non-weapon may be held in Rogue 3.4.  Keep the
                   original enchantment/uncursing effect without using
                   its index to look up a weapon name. */
                if (cur_weapon->o_type == WEAPON)
                    msg(JSTR("%sが一瞬青く光った。", "Your %s glows blue for a moment."),
                        jrogue_text(w_names[cur_weapon->o_which]));
                else
                    msg(JSTR("手に持っているものが一瞬青く光った。", "What you are holding glows blue for a moment."));
	    }
	when S_SCARE:
	    /*
	     * A monster will refuse to step on a scare monster scroll
	     * if it is dropped.  Thus reading it is a mistake and produces
	     * laughter at the poor rogue's boo boo.
	     */
	    msg(JSTR("遠くから、狂ったような笑い声が聞こえてくる。", "You hear maniacal laughter in the distance."));
	when S_REMOVE:
	    if (cur_armor != NULL)
		cur_armor->o_flags &= ~ISCURSED;
	    if (cur_weapon != NULL)
		cur_weapon->o_flags &= ~ISCURSED;
	    msg(JSTR("誰かに見守られているような気がする。", "You feel as if somebody is watching over you."));
	when S_AGGR:
	    /*
	     * This scroll aggravates all the monsters on the current
	     * level and sets them running towards the hero
	     */
	    msg(JSTR("かん高いうなり声が聞こえてくる。", "You hear a high pitched humming noise."));
	    {
		register struct linked_list *mi;

		for (mi = mlist; mi != NULL; mi = next(mi))
		{
		    register struct thing *mtp = (struct thing *) ldata(mi);
		    /* A newly awakened monster needs a valid chase target.
		       The original only set ISRUN, leaving t_dest NULL. */
		    mtp->t_dest = &hero;
		    mtp->t_flags |= ISRUN;
		}
	    }
	otherwise:
	    msg(JSTR("なんてややこしい巻き物だ！", "What a puzzling scroll!"));
	    return 0;
    }
    look();	/* put the result of the scroll on the screen */
    status();
    if (s_know[obj->o_which] && s_guess[obj->o_which])
    {
	free(s_guess[obj->o_which]);
	s_guess[obj->o_which] = NULL;
    }
    else if (!s_know[obj->o_which] && askme && s_guess[obj->o_which] == NULL)
    {
	msg(terse ? JSTR("呼び名は？", "Call it: ") :
            JSTR("どんな呼び名をつける？", "What do you want to call it? "));
	mpos = 0;
	if (get_str(buf, cw) == NORM)
	{
	    s_guess[obj->o_which] = malloc((unsigned int) strlen(buf) + 1);
	    strcpy(s_guess[obj->o_which], buf);
	}
	msg("");
    }
    /*
     * Get rid of the thing
     */
    inpack--;
    detach(pack, item);
    discard(item);
    return 0;
}

/*
 * num:
 *	Figure out the plus number for armor/weapons
 */
char *
num(int n1, int n2)
{
    static char numbuf[80];

    if (n1 == 0)
	return "+0";
    if (n2 == 0)
	{
	sprintf(numbuf, "%s%d", n1 < 0 ? "" : "+", n1);
	return numbuf;
    }
    sprintf(numbuf, "%s%d,%s%d",
			    n1 < 0 ? "" : "+", n1, n2 < 0 ? "" : "+", n2);
    return numbuf;
}
