/*
 * Special wizard commands (some of which are also non-wizard commands
 * under strange circumstances)
 *
 * @(#)wizard.c	X.XX (Berkeley) XX/XX/XX
 */

#include "rogue.h"
#include "jec/language.h"
#include "jec/game_text.h"

/* Use the same historical DES password hash in every supported OS.
 * Windows supplies FreeSec crypt(3), Unix uses the system crypt(3). */
int
wizard_password_ok(const char *plain)
{
    char *encoded;
    if (plain == NULL || *plain == '\0') return FALSE;
#ifdef _WIN32
    encoded = rogue_windows_crypt(plain, "mT");
#else
    encoded = crypt(plain, "mT");
#endif
    return encoded != NULL && strcmp(PASSWD, encoded) == 0;
}

/*
 * whatis:
 *	What a certin object is
 */
int
whatis(void)
{
    register struct object *obj;
    register struct linked_list *item;

    if ((item = get_item("identify", 0)) == NULL)
	return 0;
    obj = (struct object *) ldata(item);
    if (obj->o_type == SCROLL)
    {
	msg(JSTR("%s巻き物。", "A scroll of %s."), jrogue_text(s_magic[obj->o_which].mi_name));
	s_know[obj->o_which] = TRUE;
	if (s_guess[obj->o_which])
	{
	    free(s_guess[obj->o_which]);
	    s_guess[obj->o_which] = NULL;
	}
	return 0;
    }
    else if (obj->o_type == POTION)
    {
	msg(JSTR("%s水薬。", "A potion of %s."), jrogue_text(p_magic[obj->o_which].mi_name));
	p_know[obj->o_which] = TRUE;
	if (p_guess[obj->o_which])
	{
	    free(p_guess[obj->o_which]);
	    p_guess[obj->o_which] = NULL;
	}
	return 0;
    }
    else if (obj->o_type == WEAPON)
    {
	msg(JSTR("（%s）%s", "A %s %s"), num(obj->o_hplus, obj->o_dplus),
	    jrogue_text(w_names[obj->o_which]));
	obj->o_flags |= ISKNOW;
    }
    else if (obj->o_type == ARMOR)
    {
	msg(JSTR("（%s）%s", "%s %s"), num(a_class[obj->o_which] - obj->o_ac, 0),
	    jrogue_text(a_names[obj->o_which]));
	obj->o_flags |= ISKNOW;
    }
    return 0;
}

/*
 * eat:
 *	She wants to eat something, so let her try
 */
int
eat(void)
{
    register struct linked_list *item;
    register struct object *obj;

    if ((item = get_item("eat", FOOD)) == NULL)
	return 0;
    obj = (struct object *) ldata(item);
    if (obj->o_type != FOOD)
    {
	if (!terse)
	    msg(JSTR("うげっ、そんなものを食べたら病気になってしまう。", "Ugh, you would get ill if you ate that."));
	else
	    msg(JSTR("それは食べられない。", "That's Inedible!"));
	return 0;
    }
    /* The item is freed by discard(): retain its kind before freeing.
       Eating the second food variety grants one experience point. */
    int food_kind = obj->o_which;
    if (obj == cur_weapon)
        cur_weapon = NULL;
    inpack--;
    detach(pack, item);
    discard(item);
    if (food_kind == 1)
    {
	msg(JSTR("げげっ、ひどい味だ。", "Yuk, this food tastes awful"));
	pstats.s_exp++;
    }
    else
	msg(JSTR("うーむ、おいしかった。", "Yum, that tasted good"));
    /*
     * Kill off any pending hunger fuses/daemons and restart
     */
    kill_daemon(faint);
    extinguish(weak);
    extinguish(starve);
    extinguish(hungry);
    fuse(hungry, 0, HUNGERTIME, AFTER);
    return 0;
}

/*
 * create_obj:
 *	Wizard command for getting anything he wants
 */
int
create_obj(void)
{
    register struct linked_list *item;
    register struct object *obj;
    register char ch, bless;

    int type, which, limit;

    if (inpack >= MAXPACK)
    {
        msg(JSTR("これ以上持ちものを運べない。", "You cannot carry anything else."));
        return 0;
    }
    msg(JSTR("アイテムの種類：", "Type of item: "));
    type = readchar();
    switch (type)
    {
    case WEAPON: limit = MAXWEAPONS; break;
    case ARMOR:  limit = MAXARMORS; break;
    case POTION: limit = MAXPOTIONS; break;
    case SCROLL: limit = MAXSCROLLS; break;
    case FOOD:   limit = 3; break;
    case AMULET: limit = 1; break;
    default:
        msg(JSTR("そんな種類の品物はない。", "Unknown item type."));
        return 0;
    }
    mpos = 0;
    msg(JSTR("どの%cがほしいですか？（0-f）", "Which %c do you want? (0-f)"), type);
    ch = readchar();
    which = (ch >= '0' && ch <= '9') ? ch - '0' :
            (ch >= 'a' && ch <= 'f') ? ch - 'a' + 10 : -1;
    if (which < 0 || which >= limit)
    {
        msg(JSTR("その番号の品物はない。", "Unknown item number."));
        return 0;
    }
    mpos = 0;
    msg(JSTR("祝福は？ (+,-,n)", "Blessing? (+,-,n)"));
    bless = readchar();

    item = new_item(sizeof *obj);
    obj = (struct object *) ldata(item);
    obj->o_type = type;
    obj->o_which = which;
    obj->o_pos = hero;
    obj->o_damage = obj->o_hurldmg = "0d0";
    obj->o_count = 1;
    obj->o_ac = 11;
    if (type == WEAPON)
    {
        init_weapon(obj, (char)which);
        /* init_weapon() initializes the flags; set the curse afterwards. */
        if (bless == '-') obj->o_hplus -= rnd(3)+1;
        if (bless == '+') obj->o_hplus += rnd(3)+1;
    }
    if (type == ARMOR)
    {
        obj->o_ac = a_class[which];
        if (bless == '-') obj->o_ac += rnd(3)+1;
        if (bless == '+') obj->o_ac -= rnd(3)+1;
    }
    if (bless == '-') obj->o_flags |= ISCURSED;
    attach(pack, item);
    inpack++;
    return 0;
}

/*
 * wield:
 *	Pull out a certain weapon
 */
int
wield(void)
{
    register struct linked_list *item;
    register struct object *obj;

    if (cur_weapon != NULL && cur_weapon->o_flags & ISCURSED)
    {
	msg(JSTR("武器を持ち替えられない。呪われている。", "You can't change weapons, the thing is cursed."));
	return 0;
    }
    if ((item = get_item("wield", WEAPON)) == NULL)
	return 0;

    obj = (struct object *) ldata(item);
    /* Rogue 3.4 permits any inventory object to be held with 'w'.
       The WEAPON filter above limits the displayed inventory, not the
       selected letter.  Preserve that original behavior. */
    /* Preserve the original English wording, apart from the obsolete
     * embedded newline. Japanese follows the 3.6 wording, but deliberately
     * does NOT add an inventory letter (neither language does in Rogue 3.4). */
    if (terse)
	msg(JSTR("%sに持ちかえた。", "Wielding %s"), inv_name(obj, TRUE));
    else
	msg(JSTR("武器を%sに持ちかえた。", "You are now wielding %s"), inv_name(obj, TRUE));
    cur_weapon = obj;
    return 0;
}

/*
 * chg_str:
 *	Used to modify the player's strength.  It keeps track
 *	of the highest it has been, just in case.
 */
int
chg_str(int amt)
{
    if (amt == 0)
	return 0;
    if (amt > 0)
    {
	while (amt--)
	{
	    if (pstats.s_str.st_str < 18)
		pstats.s_str.st_str++;
	    else if (pstats.s_str.st_add == 0)
		pstats.s_str.st_add = rnd(50) + 1;
	    else if (pstats.s_str.st_add <= 50)
		pstats.s_str.st_add = 51 + rnd(24);
	    else if (pstats.s_str.st_add <= 75)
		pstats.s_str.st_add = 76 + rnd(14);
	    else if (pstats.s_str.st_add <= 90)
		pstats.s_str.st_add = 91;
	    else if (pstats.s_str.st_add < 100)
		pstats.s_str.st_add++;
	}
	if (pstats.s_str.st_str > max_stats.s_str.st_str ||
	    (pstats.s_str.st_str == 18 &&
	     pstats.s_str.st_add > max_stats.s_str.st_add))
		max_stats.s_str = pstats.s_str;
    }
    else
    {
	while (amt++)
	{
	    if (pstats.s_str.st_str < 18 || pstats.s_str.st_add == 0)
		pstats.s_str.st_str--;
	    else if (pstats.s_str.st_add < 51)
		pstats.s_str.st_add = 0;
	    else if (pstats.s_str.st_add < 76)
		pstats.s_str.st_add = 1 + rnd(50);
	    else if (pstats.s_str.st_add < 91)
		pstats.s_str.st_add = 51 + rnd(25);
	    else if (pstats.s_str.st_add < 100)
		pstats.s_str.st_add = 76 + rnd(14);
	    else
		pstats.s_str.st_add = 91 + rnd(8);
	}
	if (pstats.s_str.st_str < 3)
	    pstats.s_str.st_str = 3;
    }
    return 0;
}

/*
 * telport:
 *	Bamf the hero someplace else
 */
int
teleport(void)
{
    coord c;

    c = hero;
    mvwaddch(cw, hero.y, hero.x, mvwinch(stdscr, hero.y, hero.x));
    /* A bad RNG cycle must not trap teleportation in an endless loop. */
    if (!rogue_find_floor(&hero, -1, FALSE))
        hero = c;  /* No vacant floor: stay at the original position. */
    light(&c);
    light(&hero);
    mvwaddch(cw, hero.y, hero.x, PLAYER);
    /*
     * turn off ISHELD in case teleportation was done while fighting
     * a Fungi
     */
    if (on(player, ISHELD)) {
	player.t_flags &= ~ISHELD;
	fung_hit = 0;
	strcpy(monsters['F'-'A'].m_stats.s_dmg, "000d0");
    }
    {
        struct room *rp = roomin(&hero);
        return rp == NULL ? -1 : (int)(rp - rooms);
    }
}

