/*
 * This file contains misc functions for dealing with armor
 *
 * @(#)armor.c	X.XX (Berkeley) XX/XX/XX
 */

#include "rogue.h"
#include "jec/language.h"
#include "jec/game_text.h"

/*
 * wear:
 *	The player wants to wear something, so let him/her put it on.
 */
int
wear(void)
{
    register struct linked_list *item;
    register struct object *obj;

    if (cur_armor != NULL)
    {
	if (!terse)
	    msg(JSTR("すでに何かよろいを着ている。まず脱がなければならない。", "You are already wearing some.  You'll have to take it off first"));
	else
	    msg(JSTR("他のよろいを着ている。", "You are already wearing some"));
	return 0;
    }
    if ((item = get_item("wear", ARMOR)) == NULL)
	return 0;
    obj = (struct object *) ldata(item);
    if (obj->o_type != ARMOR)
    {
	msg(JSTR("それはよろいではない。", "You can't wear that."));
	return 0;
    }
    waste_time();
    if (!terse)
	msg(JSTR("%sを身につけた。", "You are now wearing %s."), jrogue_text(a_names[obj->o_which]));
    else
	msg(JSTR("%sを身につけた。", "Wearing %s."), jrogue_text(a_names[obj->o_which]));
    cur_armor = obj;
    obj->o_flags |= ISKNOW;
    return 0;
}

/*
 * take_off:
 *	Get the armor off of the players back
 */
int
take_off(void)
{
    register struct linked_list *item;
    register int c;
    register struct object *obj;

    if ((obj = cur_armor) == NULL)
    {
	if (terse)
	    msg(JSTR("よろいを着ていない。", "You aren't wearing any"));
	else
	    msg(JSTR("あなたはよろいを着ていない。", "You aren't wearing any"));
	return 0;
    }
    if (obj->o_flags & ISCURSED)
    {
	if (terse)
	    msg(JSTR("脱ぐことができない。", "Can't get it off."));
	else
	    msg(JSTR("どうしても脱げない。呪われているようだ。", "You can't seem to get it off.  It is cursed."));
	return 0;
    }
    waste_time();
    cur_armor = NULL;
    c = 'a';
    for (item = pack; item != NULL; item = next(item))
	if ((struct object *) ldata(item) == obj)
	    break;
	else
	    c++;
    if (terse)
	msg(JSTR("%c) %sを脱いだ。", "Was wearing %c) %s"), c, inv_name(obj, TRUE));
    else
	msg(JSTR("今まで着ていた%c) %sを脱いだ。", "You used to be wearing %c) %s"), c, inv_name(obj, TRUE));
    return 0;
}

/*
 * waste_time:
 *	Do nothing but let other things happen
 */
int
waste_time(void)
{
    do_daemons(BEFORE);
    do_fuses(BEFORE);
    do_daemons(AFTER);
    do_fuses(AFTER);
    return 0;
}
