/*
 * Contains functions for dealing with things
 *
 * @(#)things.c	X.XX (Berkeley) XX/XX/XX
 */

#include <ctype.h>
#include "rogue.h"
#include "jec/game_text.h"
#include "jec/language.h"
#include "jec/unicode_text.h"

/*
 * new_level:
 *	Dig and draw a new level
 */
int
new_level(void)
{
    register int rm, i;
    register char ch = TRAPDOOR; /* rnd(6) selects one of six trap types. */
    coord stairs;

    if (level > max_level)
	max_level = level;
    wclear(cw);
    wclear(mw);
    clear();
    status();
    /*
     * Free up the monsters on the last level
     */
    /* Monsters restored from a saved game can own item lists.  Discard
       those inventories before removing the old level's monster nodes. */
    {
        struct linked_list *item;
        for (item = mlist; item != NULL; item = next(item))
        {
            struct thing *monster = (struct thing *)ldata(item);
            free_list(monster->t_pack);
        }
    }
    free_list(mlist);
    do_rooms();				/* Draw rooms */
    do_passages();			/* Draw passages */
    no_food++;
    /* Trap positions belong to the previous level; when no traps are
       generated on this level, do not retain its obsolete count. */
    ntraps = 0;
    put_things();			/* Place objects (if any) */
    /*
     * Place the staircase down.
     */
    rm = rnd_room();
    if (!rogue_find_floor(&stairs, rm, FALSE))
        fatal("Unable to place the staircase on this level");
    mvaddch(stairs.y, stairs.x, STAIRS);
    /*
     * Place the traps
     */
    if (rnd(10) < level)
    {
        int wanted = rnd(level/4) + 1;
        if (wanted > MAXTRAPS)
            wanted = MAXTRAPS;
        /* Stop when there is no free floor, rather than looping forever.
           Store the actual number of traps placed in ntraps. */
        for (i = 0; i < wanted; i++)
        {
            if (!rogue_find_floor(&stairs, -1, FALSE))
                break;
	    switch(rnd(6))
	    {
		when 0: ch = TRAPDOOR;
		when 1: ch = BEARTRAP;
		when 2: ch = SLEEPTRAP;
		when 3: ch = ARROWTRAP;
		when 4: ch = TELTRAP;
		when 5: ch = DARTTRAP;
	    }
	    mvaddch(stairs.y, stairs.x, TRAP);
            traps[i].tr_type = ch;
            traps[i].tr_flags = 0;
            traps[i].tr_pos = stairs;
            ntraps++;
        }
    }
    if (!rogue_find_floor(&hero, -1, FALSE))
    {
        /* Extremely crowded deep levels can consume every available
           floor after placing traps.  Reclaim the last trap rather than
           terminating the game or generating an occupied player square. */
        if (ntraps == 0)
            fatal("Unable to place the player on this level");
        --ntraps;
        hero = traps[ntraps].tr_pos;
        mvaddch(hero.y, hero.x, FLOOR);
    }
    light(&hero);
    wmove(cw, hero.y, hero.x);
    waddch(cw, PLAYER);
    return 0;
}

/*
 * Pick a room that is really there
 */
int
rnd_room(void)
{
    register int rm;

    do
    {
	rm = rnd(MAXROOMS);
    } while (rooms[rm].r_flags & ISGONE);
    return rm;
}

/*
 * inv_name:
 *	return the name of something as it would appear in an
 *	inventory.
 */
/* Render Japanese labels without changing saved 3.4 item metadata.
 * All paths write through a bounded buffer; a guessed name can be UTF-8.
 */
static char *
inv_name_ja(struct object *obj)
{
    const char *name = "";
    int n = obj->o_count > 1 ? obj->o_count : 1;
    switch (obj->o_type) {
    case SCROLL:
        if (s_know[obj->o_which])
            snprintf(prbuf, sizeof prbuf, "%s巻き物",
                     jrogue_text(s_magic[obj->o_which].mi_name));
        else if (s_guess[obj->o_which])
            snprintf(prbuf, sizeof prbuf, "『%s』と呼ぶ巻き物",
                     s_guess[obj->o_which]);
        else
            snprintf(prbuf, sizeof prbuf, "「%s」という巻き物",
                     jrogue_scroll_title(s_names[obj->o_which]));
        break;
    case POTION:
        if (p_know[obj->o_which])
            snprintf(prbuf, sizeof prbuf, "%s水薬（%s）",
                     jrogue_text(p_magic[obj->o_which].mi_name),
                     jrogue_color(p_colors[obj->o_which]));
        else if (p_guess[obj->o_which])
            snprintf(prbuf, sizeof prbuf, "『%s』と呼ぶ水薬（%s）",
                     p_guess[obj->o_which],
                     jrogue_color(p_colors[obj->o_which]));
        else
            snprintf(prbuf, sizeof prbuf, "%sの水薬",
                     jrogue_color(p_colors[obj->o_which]));
        break;
    case FOOD:
        if (obj->o_which == 2)
            snprintf(prbuf, sizeof prbuf, "%s", fruit);
        else
            snprintf(prbuf, sizeof prbuf,
                     n > 1 ? "食糧" : "1袋の食糧");
        break;
    case WEAPON:
        name = jrogue_text(w_names[obj->o_which]);
        if (obj->o_flags & ISKNOW)
            snprintf(prbuf, sizeof prbuf, "（%s）%s",
                     num(obj->o_hplus, obj->o_dplus), name);
        else
            snprintf(prbuf, sizeof prbuf, "%s", name);
        break;
    case ARMOR:
        name = jrogue_text(a_names[obj->o_which]);
        if (obj->o_flags & ISKNOW)
            snprintf(prbuf, sizeof prbuf, "（%s）%s",
                     num(a_class[obj->o_which] - obj->o_ac, 0), name);
        else
            snprintf(prbuf, sizeof prbuf, "%s", name);
        break;
    case AMULET:
        snprintf(prbuf, sizeof prbuf, "イェンダーの魔除け");
        break;
    default:
        snprintf(prbuf, sizeof prbuf, "奇妙なもの %s", rogue_unctrl(obj->o_type));
        break;
    }
    if (n > 1) {
        char quantity[32];
        size_t length;
        snprintf(quantity, sizeof quantity, "%d%s", n,
                 obj->o_type == SCROLL ? "つの" :
                 obj->o_type == FOOD && obj->o_which != 2 ? "袋の" : "個の");
        length = strlen(quantity);
        if (length + strlen(prbuf) < sizeof prbuf) {
            memmove(prbuf + length, prbuf, strlen(prbuf) + 1);
            memcpy(prbuf, quantity, length);
        }
    }
    if (obj == cur_armor) {
        size_t used = strlen(prbuf);
        snprintf(prbuf + used, sizeof prbuf - used, "（着ている）");
    }
    if (obj == cur_weapon) {
        size_t used = strlen(prbuf);
        snprintf(prbuf + used, sizeof prbuf - used, "（使っている）");
    }
    return prbuf;
}

char *
inv_name(struct object *obj, bool drop)
{
    if (jrogue_lang_is_japanese)
        return inv_name_ja(obj);

    switch(obj->o_type)
    {
	when SCROLL:
	    if (s_know[obj->o_which])
		sprintf(prbuf, "A scroll of %s", s_magic[obj->o_which].mi_name);
	    else if (s_guess[obj->o_which])
		sprintf(prbuf, "A scroll called %s", s_guess[obj->o_which]);
	    else
		sprintf(prbuf, "A scroll titled '%s'",
                jrogue_scroll_title(s_names[obj->o_which]));
	when POTION:
	    if (p_know[obj->o_which])
		sprintf(prbuf, "A potion of %s(%s)",
		    p_magic[obj->o_which].mi_name,
		    p_colors[obj->o_which]);
	    else if (p_guess[obj->o_which])
		sprintf(prbuf, "A potion called %s",
		    p_guess[obj->o_which]);
	    else
		sprintf(prbuf, "A %s potion",
		    p_colors[obj->o_which]);
	when FOOD:
	    if (obj->o_which == 2)
		sprintf(prbuf, "A single %s", fruit);
	    else
		strcpy(prbuf, "Some food");
	when WEAPON:
	    if (obj->o_flags & ISKNOW)
		sprintf(prbuf, "A %s %s", num(obj->o_hplus, obj->o_dplus),
		    w_names[obj->o_which]);
	    else
		sprintf(prbuf, "A %s", w_names[obj->o_which]);
	when ARMOR:
	    if (obj->o_flags & ISKNOW)
		sprintf(prbuf, "%s %s",
		    num(a_class[obj->o_which] - obj->o_ac, 0),
		    a_names[obj->o_which]);
	    else
		sprintf(prbuf, "%s", a_names[obj->o_which]);
	when AMULET:
	    strcpy(prbuf, "The Amulet of Yendor");
	otherwise:
	    debug("Picked up something funny");
	    sprintf(prbuf, "Something bizarre %s", rogue_unctrl(obj->o_type));
    }
    if (obj == cur_armor)
	strcat(prbuf, " (being worn)");
    if (obj == cur_weapon)
	strcat(prbuf, " (weapon in hand)");
    if (drop && isupper(prbuf[0]))
	prbuf[0] = tolower(prbuf[0]);
    else if (!drop && islower(*prbuf))
	*prbuf = toupper(*prbuf);
    if (!drop)
	strcat(prbuf, ".");
    return prbuf;
}

/*
 * pick_up:
 *	Add something to characters pack.
 */
int
pick_up(char ch)
{
    switch(ch)
    {
	case GOLD:
	    money();
	    break;
	default:
	    debug("Where did you pick that up???");
	    goto pick_item;
	case ARMOR:
	case POTION:
	case FOOD:
	case WEAPON:
	case SCROLL:
	case AMULET:
	pick_item:
	    add_pack();
	    break;
    }
    return 0;
}

/*
 * money:
 *	Add to characters purse
 */
int
money(void)
{
    register int i;

    for (i = 0; i < MAXROOMS; i++)
	if (ce(hero, rooms[i].r_gold))
	{
	    if (notify) {
                if (jrogue_lang_is_japanese)
                    msg("%d個の金塊を見つけた。", rooms[i].r_goldval);
                else
                    msg("%s%d gold pieces.", terse ? "" : "You found ", rooms[i].r_goldval);
            }
            purse += rooms[i].r_goldval;
	    rooms[i].r_goldval = 0;
	    cmov(rooms[i].r_gold);
	    addch(FLOOR);
	    return 0;
	}
    msg(JSTR("その金塊はニセモノに違いない。", "That gold must have been counterfeit"));
    return 0;
}

/*
 * add_pack:
 *	Pick up an object from the floor and add it to the pack.
 */
int
add_pack(void)
{
    register struct linked_list *item, *ip, *lp;
    register struct object *obj, *op;
    register char ch;

    /*
     * Check if there is room
     */
    if (inpack == MAXPACK)
    {
	msg(JSTR("これ以上持ちものを運べない。", "You can't carry anything else."));
	return 0;
    }
    if ((item = find_obj(hero.y, hero.x)) == NULL)
	return 0;
    obj = (struct object *) ldata(item);
    inpack++;
    detach(lvl_obj, item);
    mvaddch(hero.y, hero.x, (roomin(&hero) == NULL ? PASSAGE : FLOOR));
    /*
     * Search for an object of the same type to find insertion point
     */
    for (ip = pack; ip != NULL; ip = next(ip))
    {
	op = (struct object *) ldata(ip);
	if (obj->o_type == op->o_type)
	    break;
    }
    if (ip == NULL)
    {
	/*
	 */
	for (ip = pack; ip != NULL; ip = next(ip))
	{
	    op = (struct object *) ldata(ip);
	    if (op->o_type != FOOD)
		break;
	    lp = ip;
	}
    }
    else
    {
	/*
	 */
	while (ip != NULL && op->o_type == obj->o_type)
	{
	    if (op->o_which == obj->o_which)
		break;
	    lp = ip;
	    if ((ip = next(ip)) == NULL)
		break;
	    op = (struct object *) ldata(ip);
	}
    }
    /*
     */
    if (ip == NULL)
    {
	if (pack == NULL)
	    pack = item;
	else
	{
	    lp->l_next = item;
	    item->l_prev = lp;
	    item->l_next = NULL;
	}
    }
    else
    {
	if ((item->l_prev = prev(ip)) != NULL)
	    item->l_prev->l_next = item;
	else
	    pack = item;
	item->l_next = ip;
	ip->l_prev = item;
    }
    /*
     * Notify the user
     */
    ch = 'a';
    for (ip = pack; ip != NULL; ip = next(ip))
    {
	if (ip == item)
	    break;
	ch++;
    }
    obj = (struct object *) ldata(item);
    if (notify)
    {
        if (jrogue_lang_is_japanese) {
            /* The item letter precedes the verb, as in Rogue 3.6 JEC. */
            if (terse)
                msg("%s (%c)", inv_name(obj, FALSE), ch);
            else
                msg("あなたは%s (%c)を手に入れた。", inv_name(obj, TRUE), ch);
        } else if (terse)
            msg("%s (%c)", inv_name(obj, FALSE), ch);
        else
            msg("You found %s (%c).", inv_name(obj, TRUE), ch);
    }
    if (obj->o_type == AMULET)
	amulet = FALSE;
    return 0;
}

/*
 * put_things:
 *	put things on this level
 */
int
put_things(void)
{
    register int i, j, k;
    register struct linked_list *item;
    register struct object *cur;
    register int rm;
    coord tp;

    /*
     * Throw away stuff left on the previous level (if anything)
     */
    free_list(lvl_obj);
    /*
     * Do MAXOBJ attempts to put things on a level
     */
    for (i = 0; i < MAXOBJ; i++)
	if (rnd(100) < 35)
	{
	    /*
	     * Pick a new object and link it in the list
	     */
	    item = new_item(sizeof *cur);
	    attach(lvl_obj, item);
	    cur = (struct object *) ldata(item);
	    cur->o_hplus = cur->o_dplus = 0;
	    cur->o_damage = cur->o_hurldmg = "0d0";
	    cur->o_ac = 11;
	    /*
	     * Decide what kind of object it will be
	     * If we haven't had food for a while, let it be food.
	     */
	    switch (no_food > 3 ? 4 : rnd(10))
	    {
		when 0: case 1: case 5:
		    cur->o_type = POTION;
		    for (j = 0, k = rnd(100); j < MAXPOTIONS; j++)
			if (k < p_magic[j].mi_prob)
			    break;
		    if (j == MAXPOTIONS)
		    {
			j = 0;
			if (wizard)
			    msg(JSTR("不正な水薬 %d を選んだ。", "Picked bad potion %d"), k);
		    }
		    cur->o_which = j;
		when 2: case 3: case 6:
		    cur->o_type = SCROLL;
		    for (j = 0, k = rnd(100); j < MAXSCROLLS; j++)
			if (k < s_magic[j].mi_prob)
			    break;
		    if (j == MAXSCROLLS)
		    {
			j = 0;
			if (wizard)
			    msg(JSTR("不正な巻き物 %d を選んだ。", "Picked bad scroll %d"), k);
		    }
		    cur->o_which = j;
		when 4: case 7:
		    no_food = 0;
		    cur->o_type = FOOD;
		    if (rnd(100) > 10)
			cur->o_which = 0;
		    else if (rnd(100) > 50)
			cur->o_which = 1;
		    else
			cur->o_which = 2;
		when 8:
		    cur->o_type = WEAPON;
		    cur->o_which = rnd(MAXWEAPONS);
		    init_weapon(cur, cur->o_which);
		    if ((k = rnd(100)) < 10)
		    {
			cur->o_flags |= ISCURSED;
			cur->o_hplus -= rnd(3)+1;
		    }
		    else if (k < 15)
			cur->o_hplus += rnd(3)+1;
		when 9:
		    cur->o_type = ARMOR;
		    for (j = 0, k = rnd(100); j < MAXARMORS; j++)
			if (k < a_chances[j])
			    break;
		    if (j == MAXARMORS)
		    {
			debug("Picked a bad armor %d", k);
			j = 0;
		    }
		    cur->o_which = j;
		    cur->o_ac = a_class[j];
		    if ((k = rnd(100)) < 20)
		    {
			cur->o_flags |= ISCURSED;
			cur->o_ac += rnd(3)+1;
		    }
		    else if (k < 28)
			cur->o_ac -= rnd(3)+1;
		otherwise:
		    debug("Picked a bad kind of object");
		    wait_for(' ');
	    }
	    /*
	     * Put it somewhere
	     */
            rm = rnd_room();
            if (!rogue_find_floor(&tp, rm, FALSE))
            {
                detach(lvl_obj, item);
                discard(item);
                continue;
            }
            mvaddch(tp.y, tp.x, cur->o_type);
            cur->o_pos = tp;
	}
    /*
     * If he is really deep in the dungeon and he hasn't found the
     * amulet yet, put it somewhere on the ground.
     */
    if (level > 25 && amulet)
    {
	item = new_item(sizeof *cur);
	attach(lvl_obj, item);
	cur = (struct object *) ldata(item);
	cur->o_hplus = cur->o_dplus = 0;
	cur->o_damage = cur->o_hurldmg = "0d0";
	cur->o_ac = 11;
	cur->o_type = AMULET;
	/*
	 * Put it somewhere
	 */
        rm = rnd_room();
        if (rogue_find_floor(&tp, rm, FALSE))
        {
            mvaddch(tp.y, tp.x, cur->o_type);
            cur->o_pos = tp;
        }
        else
        {
            detach(lvl_obj, item);
            discard(item); /* Try to place the amulet on a later level. */
        }
    }
    return 0;
}

/*
 * inventory:
 *	list what is in the pack
 */
int
inventory(struct linked_list *list, int type)
{
    register struct object *obj;
    register char ch;
    register int n_objs;
    char inv_temp[512];

    n_objs = 0;
    for (ch = 'a'; ch <= 'w' && list != NULL; list = next(list), ch++)
    {
	obj = (struct object *) ldata(list);
	if (type && type != obj->o_type)
		continue;
	switch (n_objs++)
	{
	    /*
	     * For the first thing in the inventory, just save the string
	     * in case there is only one.
	     */
	    case 0:
        {
            char namebuf[400];
            rogue_text_copy(namebuf, sizeof namebuf, inv_name(obj, FALSE),
                            sizeof namebuf - 1, ROGUE_COLS - 4);
            snprintf(inv_temp, sizeof inv_temp, "%c) %s\n", ch, namebuf);
        }
		break;
	    /*
	     * If there is more than one, clear the screen, print the
	     * saved message and fall through to ...
	     */
	    case 1:
		if (slow_invent)
		    msg("%s", inv_temp);
		else
		{
		    wclear(hw);
		    waddstr(hw, inv_temp);
		}
		goto print_inventory_item;
	    /*
	     * Print the line for this object
	     */
	    default:
	    print_inventory_item:
		if (slow_invent)
		    msg("%c) %s\n", ch, inv_name(obj, FALSE));
		else
        {
            char namebuf[400];
            rogue_text_copy(namebuf, sizeof namebuf, inv_name(obj, FALSE),
                            sizeof namebuf - 1, ROGUE_COLS - 4);
            wprintw(hw, "%c) %s\n", ch, namebuf);
        }
	}
    }
    if (n_objs == 0)
    {
	if (terse)
	    if (type == 0)
		msg(JSTR("何も持っていない。", "Empty handed."));
	    else
		msg(JSTR("それは１つも持っていない。", "Nothing appropriate"));
	else if (type == 0)
	    msg(JSTR("何も持っていない。", "You are empty handed."));
	else
	    msg(JSTR("それは１つも持っていない。", "You don't have anything appropriate"));
	return 0;
    }
    if (n_objs == 1)
    {
	msg("%s", inv_temp);
	return 0;
    }
    if (!slow_invent)
    {
	mvwaddstr(hw, 23, 0, JSTR("--スペースを押してください--",
                            "--Press space to continue--"));
	draw(hw);
	wait_for(' ');
	clearok(cw, TRUE);
	touchwin(cw);
    }
    return 0;
}

/*
 * drop:
 *	put something down
 */
int
drop(void)
{
    register char ch;
    register struct linked_list *obj;
    register struct object *op;

    if ((obj = get_item("drop", 0)) == NULL)
	return 0;
    op = (struct object *) ldata(obj);
    /* An unsuccessful drop must not unequip the weapon or armor. */
    ch = rogue_glyph(mvwinch(stdscr, hero.y, hero.x));
    if (ch != FLOOR && ch != PASSAGE)
    {
        msg(JSTR("そこにはすでに何かが置かれている。", "There is something there already"));
        return 0;
    }
    if (op == cur_weapon)
    {
	if (op->o_flags & ISCURSED)
	{
	    msg(JSTR("落とすことができない。呪われているようだ。", "You can't drop it.  It appears to be cursed."));
	    return 0;
	}
	cur_weapon = NULL;
    }
    if (op == cur_armor)
    {
	if (op->o_flags & ISCURSED)
	{
	    msg(JSTR("落とすことができない。呪われているようだ。", "You can't drop it.  It appears to be cursed."));
	    return 0;
	}
	waste_time();
	cur_armor = NULL;
    }
    detach(pack, obj);
    inpack--;
    /*
     */
    if (op->o_type == SCROLL && op->o_which == S_SCARE)
    {
	if (op->o_flags & ISFOUND)
	{
	    msg(JSTR("巻き物は地面に落ちると粉々になった。", "The scroll turns to dust as it hits the ground."));
            discard(obj);  /* Already detached: do not leak the scroll. */
	    return 0;
	}
	op->o_flags |= ISFOUND;
    }
    /*
     * Link it into the level object list
     */
    attach(lvl_obj, obj);
    mvaddch(hero.y, hero.x, op->o_type);
    op->o_pos = hero;
    msg(JSTR("%sを落とした。", "Dropped %s"), inv_name(op, TRUE));
    return 0;
}

/*
 * find_obj:
 *	find the unclaimed object at y, x
 */
struct linked_list *
find_obj(int y, int x)
{
    register struct linked_list *obj;
    register struct object *op;

    for (obj = lvl_obj; obj != NULL; obj = next(obj))
    {
	op = (struct object *) ldata(obj);
	if (op->o_pos.y == y && op->o_pos.x == x)
		return obj;
    }
    debug("Non-object %d,%d", y, x);
    return NULL;
}

/*
 * get_item:
 *	pick something out of a pack for a purpose
 */
struct linked_list *
get_item(char *purpose, int type)
{
    register struct linked_list *obj;
    register char ch, och;

    if (pack == NULL)
	msg(JSTR("何も持っていない。", "You aren't carrying anything."));
    else
    {
	for (;;)
	{
	    if (!terse)
		msg(JSTR("どの%s？ ('*'でリスト表示、ESCでキャンセル) ",
		    "Which object do you want to %s? (* for a list): "),
		    jrogue_text(purpose));
	    else
		msg(JSTR("どの%s？ ('*'でリスト表示) ",
		    "%s what? (* for list): "), jrogue_text(purpose));
	    ch = readchar();
	    mpos = 0;
	    /*
	     * Give the poor player a chance to abort the command
	     */
	    if (ch == ESCAPE || ch == CTRL('G'))
	    {
		after = FALSE;
		msg("");
		return NULL;
	    }
	    if (ch == '*')
	    {
		mpos = 0;
		inventory(pack, type);
		continue;
	    }
	    for (obj = pack, och = 'a'; obj != NULL; obj = next(obj), och++)
		if (ch == och)
		    break;
	    if (obj == NULL)
	    {
		msg(JSTR("'a'から'%c'までの文字を指定してください。", "Please specify a letter between 'a' and '%c'"), och-1);
		continue;
	    }
	    else
		return obj;
	}
    }
    return NULL;
}

/*
 * picky_inven:
 *	Allow player to inventory a single item
 */
int
picky_inven(void)
{
    register struct linked_list *item;
    register char ch, mch;

    if (pack == NULL)
	msg(JSTR("何も持っていない。", "You aren't carrying anything"));
    else if (next(pack) == NULL)
	msg("a) %s", inv_name((struct object *) ldata(pack), FALSE));
    else
    {
	msg(terse ? JSTR("調べる持ちものは？", "Item: ") :
            JSTR("調べる持ちものの種類は？", "Which item do you wish to inventory: "));
	mpos = 0;
	mch = readchar();
	for (ch = 'a', item = pack; item != NULL; item = next(item), ch++)
	    if (ch == mch)
	    {
		msg("%c) %s",ch,inv_name((struct object *) ldata(item), FALSE));
		return 0;
	    }
	msg(JSTR("'%s'は持ちものにありません。", "'%s' not in pack"), rogue_unctrl(mch));
    }
    return 0;
}
