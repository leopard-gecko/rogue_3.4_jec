/*
 * All the fighting gets done here
 *
 * @(#)fight.c	X.XX (Berkeley) XX/XX/XX
 */

#include <ctype.h>
#include "rogue.h"
#include "jec/language.h"
#include "jec/game_text.h"

/* The original fungi damage field is mutable during combat. */
static char rogue_fungi_damage[32] = "000d0";
void rogue_restore_fungi(void)
{
    if (fung_hit > 0)
        snprintf(rogue_fungi_damage, sizeof rogue_fungi_damage, "%dd1", fung_hit);
    else
        strcpy(rogue_fungi_damage, "000d0");
    monsters['F'-'A'].m_stats.s_dmg = rogue_fungi_damage;
}


long e_levels[] = {
    10L,20L,40L,80L,160L,320L,640L,1280L,2560L,5120L,10240L,20480L,
    40920L, 81920L, 163840L, 327680L, 655360L, 1310720L, 2621440L, 0L };

/*
 * fight:
 *	The player attacks the monster.
 */
int
fight(coord *mp, char mn, struct object *weap, bool thrown)
{
    register struct thing *tp;
    register struct linked_list *item;
    register struct linked_list *pitem, *next_pack;
    bool wake_flag = TRUE;
    register bool did_hit = FALSE;

    /*
     * Find the monster we want to fight
     */
    item = find_mons(mp->y, mp->x);
    if (item == NULL)
    {
        debug("Fight what @ %d,%d", mp->y, mp->x);
        return FALSE;  /* The visible glyph and monster list disagree. */
    }
    tp = (struct thing *) ldata(item);
    /*
     * Since we are fighting, things are not quiet so no healing takes
     * place.
     */
    quiet = 0;
    /*
     * Let him know it was really a mimic (if it was one).
     */
    if (tp->t_type == 'M' && tp->t_disguise != 'M')
    {
	msg(JSTR("待て！ それはミミックだ！", "Wait! That's a mimic!"));
	goto out;
    }
    if (roll_em(&pstats, &tp->t_stats, weap, thrown))
    {
	did_hit = TRUE;
	if (thrown)
	    thunk(weap, jrogue_text(monsters[mn-'A'].m_name));
	else
	    hit(NULL, jrogue_text(monsters[mn-'A'].m_name));
	if (on(player, CANHUH))
	{
	    msg(JSTR("手の赤い光がおさまった。", "Your hands stop glowing red"));
	    msg(JSTR("%sは混乱したようだ。", "The %s appears confused."), jrogue_text(monsters[mn-'A'].m_name));
	    tp->t_flags |= ISHUH;
	    player.t_flags &= ~CANHUH;
	}
	if (tp->t_stats.s_hpt <= 0)
	{
	    if (!terse)
		msg(JSTR("%sを倒した。", "You have defeated the %s."), jrogue_text(monsters[mn-'A'].m_name));
	    else
		msg(JSTR("%sを倒した。", "Defeated %s"), jrogue_text(monsters[mn-'A'].m_name));
	    /*
	     */
	    pstats.s_exp += tp->t_stats.s_exp;
	    check_level();
	    switch (tp->t_type)
	    {
		when 'F':
		    player.t_flags &= ~ISHELD;
		    fung_hit = 0;
		    strcpy(monsters['F'-'A'].m_stats.s_dmg, "000d0");
		when 'L':
		{
		    register struct room *rp;

		    /* A leprechaun can be killed outside a room. */
		    rp = roomin(mp);
		    if (rp == NULL)
			break;
		    if (rp->r_goldval == 0)
		    {
			rp->r_gold = *mp;
		    }
		    if (!save_throw(VS_MAGIC))
			rp->r_goldval += GOLDCALC;
		    else
			rp->r_goldval += GOLDCALC * 5;
		    mvaddch(rp->r_gold.y, rp->r_gold.x, GOLD);
		    look();
		}
	    }
	    /*
	     */
	    {
		register struct object *obj;

		pitem = tp->t_pack;
		while (pitem != NULL)
		{
                    /* detach() clears l_next, and fall() may free the
                       item; never follow its link after either call. */
                    next_pack = next(pitem);
		    obj = (struct object *) ldata(pitem);
		    obj->o_pos = tp->t_pos;
		    detach(tp->t_pack, pitem);
		    fall(pitem, FALSE);
		    pitem = next_pack;
		}
		remove_monster(mp, item);
		wake_flag = FALSE;
	    }
	}
    }
    else
    {
	if (thrown)
	    bounce(weap, jrogue_text(monsters[mn-'A'].m_name));
	else
	    miss(NULL, jrogue_text(monsters[mn-'A'].m_name));
    }
out:
    if (wake_flag)
	runto(mp, &hero);
    return did_hit;
}

/*
 * attack:
 *	The monster attacks the player
 */
int
attack(struct thing *mp)
{
    bool monster_removed = FALSE;
    /*
     * Since this is an attack, stop running and any healing that was
     * going on at the time.
     */
    running = FALSE;
    quiet = 0;
    if (mp->t_type == 'M')
	mp->t_disguise = 'M';
    if (roll_em(&mp->t_stats, &pstats, NULL, FALSE))
    {
	if (mp->t_type != 'E')
	    hit(jrogue_text(monsters[mp->t_type-'A'].m_name), NULL);
	if (pstats.s_hpt <= 0)
	    death(mp->t_type);	/* Bye bye life ... */
	switch (mp->t_type)
	{
	    when 'R':
		/*
		 * If a rust monster hits, you lose armor
		 */
		if (cur_armor != NULL && cur_armor->o_ac < 9)
		{
		    if (!terse)
			msg(JSTR("よろいがもろくなったようだ。なんてこった！", "Your armor appears to be weaker now. Oh my!"));
		    else
			msg(JSTR("よろいがもろくなった。", "Your armor weakens"));
		    cur_armor->o_ac++;
		}
	    when 'E':
		/*
		 * The gaze of the floating eye hypnotizes you
		 */
		if (!no_command)
		{
		    if (terse)
			msg(JSTR("あなたは身動きできない。", "You are transfixed."));
		    else
			msg(JSTR("あなたは身動きできない。浮遊する目の視線に捕らえられた。", "You are transfixed by the gaze of the floating eye."));
		}
		no_command += rnd(2)+2;
	    when 'A':
		/*
		 * Ants have poisonous bites
		 */
		if (!save_throw(VS_POISON))
		{
		    chg_str(-1);
		    if (!terse)
			msg(JSTR("腕に刺すような痛みを感じ、力が弱くなった。", "You feel a sting in your arm and now feel weaker"));
		    else
			msg(JSTR("刺すような痛みで力が弱くなった。", "A sting has weakened you"));
		}
	    when 'W':
		/*
		 * Wraiths might drain energy levels
		 */
		if (rnd(100) < 15)
		{
		    int fewer;

		    if (pstats.s_exp == 0)
			death('W');		/* All levels gone */
		    msg(JSTR("急に弱くなったようだ。", "You suddenly feel weaker."));
		    if (--pstats.s_lvl == 0)
		    {
			pstats.s_exp = 0;
			pstats.s_lvl = 1;
		    }
		    else
			pstats.s_exp = e_levels[pstats.s_lvl-1]+1;
		    fewer = roll(1, 10);
		    pstats.s_hpt -= fewer;
		    max_hp -= fewer;
		    if (pstats.s_hpt < 1)
			pstats.s_hpt = 1;
		    if (max_hp < 1)
			death('W');
		}
	    when 'F':
		/*
		 * Violet fungi stops the poor guy from moving
		 */
		player.t_flags |= ISHELD;
		{
	    ++fung_hit;
	    rogue_restore_fungi();
	}
	    when 'L':
	    {
		/*
		 * Leperachaun steals some gold
		 */
		register long lastpurse;

		lastpurse = purse;
		if (save_throw(VS_MAGIC))
		    purse -= GOLDCALC;
		else
		    purse -= GOLDCALC * 5;
		if (purse < 0)
		    purse = 0;
		if (purse != lastpurse)
		    msg(JSTR("財布が軽くなった気がするぞ。", "Your purse feels lighter"));
		remove_monster(&mp->t_pos, find_mons(mp->t_pos.y, mp->t_pos.x));
                monster_removed = TRUE;
	    }
	    when 'N':
	    {
		register struct linked_list *list, *steal;
		register struct object *obj;
		register int nobj;

		/*
		 * Nymph's steal a magic item, look through the pack
		 * and pick out one we like.
		 */
		steal = NULL;
		for (nobj = 0, list = pack; list != NULL; list = next(list))
		{
		    obj = (struct object *) ldata(list);
		    if (obj != cur_armor && obj != cur_weapon &&
			is_magic(obj) && rnd(++nobj) == 0)
			    steal = list;
		}
		if (steal != NULL)
		{
		    remove_monster(&mp->t_pos, find_mons(mp->t_pos.y, mp->t_pos.x));
                    monster_removed = TRUE;
		    msg(JSTR("彼女は%sを盗んでいった！", "She stole a %s!\n"),
                inv_name((struct object *)ldata(steal), TRUE));
		    detach(pack, steal);
		    discard(steal);
		    inpack--;
		}
	    }
	    otherwise:
		break;
	}
    }
    else if (mp->t_type != 'E')
    {
	if (mp->t_type == 'F')
	    pstats.s_hpt -= fung_hit;
	miss(jrogue_text(monsters[mp->t_type-'A'].m_name), NULL);
    }
    /*
     * Check to see if this is a regenerating monster and let it heal if
     * it is.
     */
    /* L and N may delete themselves during their special attacks.
       Do not dereference the freed monster after that point. */
    if (!monster_removed && on(*mp, ISREGEN) && rnd(100) < 33)
	mp->t_stats.s_hpt++;
    if (fight_flush)
    {
	raw();	/* flush typeahead */
	noraw();
    }
    count = 0;
    status();
    return 0;
}

/*
 * swing:
 *	returns true if the swing hits
 */
int
swing(int at_lvl, int op_arm, int wplus)
{
    register int res = rnd(20)+1;
    register int need = (21-at_lvl)-op_arm;

    return (res+wplus >= need);
}

/*
 * check_level:
 *	Check to see if the guy has gone up a level.
 */
int
check_level(void)
{
    register int i, add;

    for (i = 0; e_levels[i] != 0; i++)
	if (e_levels[i] > pstats.s_exp)
	    break;
    i++;
    if (i > pstats.s_lvl)
    {
	add = roll(i-pstats.s_lvl,10);
	max_hp += add;
	if ((pstats.s_hpt += add) > max_hp)
	    pstats.s_hpt = max_hp;
	msg(JSTR("レベル%dへようこそ。", "Welcome to level %d"), i);
    }
    pstats.s_lvl = i;
    return 0;
}

/*
 * roll_em:
 *	Roll several attacks
 */
int
roll_em(struct stats *att, struct stats *def, struct object *weap, bool hurl)
{
    register char *cp;
    register int ndice, nsides;
    int def_arm;
    bool did_hit = FALSE;

    for (;;)
    {
	if (weap == NULL)
	{
	    cp = att->s_dmg;
	    break;
	}
	if (hurl)
	{
	    if ((weap->o_flags&ISMISL) && cur_weapon != NULL &&
              cur_weapon->o_type == WEAPON &&
	      cur_weapon->o_which == weap->o_launch)
		cp = weap->o_hurldmg;
	    else
		cp = (weap->o_flags&ISMISL ? weap->o_damage : weap->o_hurldmg);
	    break;
	}
	cp = weap->o_damage;
	break;
    }
    /* The original 'w' command allows food, armor, potions and scrolls
       to be held as weapons.  Most generated non-weapons get "0d0", but
       the starter food and armor have no damage pointer at all.  Treat
       that as zero-dice damage rather than passing NULL to atoi().
       This applies equally to thrown non-weapons. */
    if (cp == NULL)
        cp = "0d0";
    for (;;)
    {
	int damage;
	int hplus = (weap == NULL ? 0 : weap->o_hplus);
	int dplus = (weap == NULL ? 0 : weap->o_dplus);

	ndice = atoi(cp);
	if ((cp = strchr(cp, 'd')) == NULL)
	    break;
	nsides = atoi(++cp);
	if (def == &pstats)
	{
	    if (cur_armor != NULL)
		def_arm = cur_armor->o_ac;
	    else
		def_arm = def->s_arm;
	}
	else
	    def_arm = def->s_arm;
	if (swing(att->s_lvl, def_arm, hplus+str_plus(&att->s_str)))
	{
	    int proll;

	    proll = roll(ndice, nsides);
	    if (ndice + nsides > 0 && proll < 1)
		debug("Damage for %dd%d came out zero.", ndice, nsides);
	    damage = add_dam(&att->s_str) + proll + dplus;
	    def->s_hpt -= max(0, damage);
	    did_hit = TRUE;
	}
	if ((cp = strchr(cp, '/')) == NULL)
	    break;
	cp++;
    }
    return did_hit;
}

/*
 * prname:
 *	The print name of a combatant
 */
char *
prname(const char *who, bool upper)
{
    static char tbuf[80];

    *tbuf = '\0';
    if (who == 0)
	strcpy(tbuf, "you");
    else
    {
	strcpy(tbuf, "the ");
	strcat(tbuf, who);
    }
    if (upper)
	*tbuf = toupper(*tbuf);
    return tbuf;
}

/*
 * hit:
 *	Print a message to indicate a succesful hit
 */
/* The original four Rogue 3.4 combat variants also occur in 3.6,
 * 5.2, and 5.4.  Use the same Japanese text as Rogue 3.6 JEC.
 * Keep the original number of RNG calls: one rnd(4) when !terse, none
 * in terse mode.  The English path below remains the original 3.4 text.
 */
static int
jrogue_combat_result(int successful, const char *attacker,
                      const char *defender)
{
    static const char *const hits[4] = {
        "激しい一撃を食らわせた。",
        "攻撃し命中した。",
        "傷を負わせた。",
        "殴りかかり命中した。"
    };
    static const char *const misses[4] = {
        "攻撃したが外れた。",
        "殴りかかったが外れた。",
        "攻撃したがぎりぎりでそれた。",
        "攻撃したが当たらなかった。"
    };
    const char *actor = attacker != NULL ? attacker : "あなた";
    const char *target = defender != NULL ? defender : "あなた";

    if (terse) {
        if (successful)
            msg("%sの攻撃は命中した。", actor);
        else
            msg("%sは攻撃したが外れた。", actor);
    } else {
        int style = rnd(4);
        msg("%sは%sに%s", actor, target,
            successful ? hits[style] : misses[style]);
    }
    return 0;
}

int
hit(const char *er, const char *ee)
{
    if (jrogue_lang_is_japanese)
        return jrogue_combat_result(TRUE, er, ee);
    /* rnd(4) always selects one of the cases; initialize for static analysis. */
    register char *s = " hit ";

    *prbuf = '\0';
    strcat(prbuf, prname(er, TRUE));
    if (terse)
	s = " hit.";
    else
	switch (rnd(4))
	{
	    when 0: s = " scored an excellent hit on ";
	    when 1: s = " hit ";
	    when 2: s = (er == 0 ? " have injured " : " has injured ");
	    when 3: s = (er == 0 ? " swing and hit " : " swings and hits ");
	}
    strcat(prbuf, s);
    if (!terse)
	strcat(prbuf, prname(ee, FALSE));
    msg("%s", prbuf);
    return 0;
}

/*
 * miss:
 *	Print a message to indicate a poor swing
 */
int
miss(const char *er, const char *ee)
{
    if (jrogue_lang_is_japanese)
        return jrogue_combat_result(FALSE, er, ee);
    /* rnd(4) always selects one of the cases; initialize for static analysis. */
    register char *s = " miss ";

    *prbuf = '\0';
    strcat(prbuf, prname(er, TRUE));
    if (terse)
	s = (er == 0 ? " miss." : " misses.");
    else
	switch (rnd(4))
	{
	    when 0: s = (er == 0 ? " miss " : " misses ");
	    when 1: s = (er == 0 ? " swing and miss " : " swings and misses ");
	    when 2: s = (er == 0 ? " barely miss " : " barely misses ");
	    when 3: s = (er == 0 ? " don't hit " : " doesn't hit ");
	}
    strcat(prbuf, s);
    if (!terse)
	strcat(prbuf, prname(ee, FALSE));
    msg("%s", prbuf);
    return 0;
}

/*
 * save_throw:
 *	See if a creature save against something
 */
int
save_throw(int which)
{
    register int need;

    need = 14 + which - pstats.s_lvl / 2;
    return (roll(1, 20) >= need);
}

/*
 * str_plus:
 *	compute bonus/penalties for strength on the "to hit" roll
 */
int
str_plus(str_t *str)
{
    if (str->st_str == 18)
    {
	if (str->st_add == 100)
	    return 3;
	if (str->st_add > 50)
	    return 2;
    }
    if (str->st_str >= 17)
	return 1;
    if (str->st_str > 6)
	return 0;
    return str->st_str - 7;
}

/*
 * add_dam:
 *	compute additional damage done for exceptionally high or low strength
 */
int
add_dam(str_t *str)
{
    if (str->st_str == 18)
    {
	if (str->st_add == 100)
	    return 6;
	if (str->st_add > 90)
	    return 5;
	if (str->st_add > 75)
	    return 4;
	if (str->st_add != 0)
	    return 3;
	return 2;
    }
    if (str->st_str > 15)
	return 1;
    if (str->st_str > 6)
	return 0;
    return str->st_str - 7;
}

/*
 * raise_level:
 *	The guy just magically went up a level.
 */
int
raise_level(void)
{
    pstats.s_exp = e_levels[pstats.s_lvl-1] + 1L;
    check_level();
    return 0;
}

/*
 * thunk:
 *	A missile hits a monster
 */
int
thunk(struct object *weap, const char *mname)
{
    if (weap->o_type == WEAPON)
	msg(JSTR("%sは%sに命中した。", "The %s hits the %s"), jrogue_text(w_names[weap->o_which]), mname);
    else
	msg(JSTR("%sに命中した。", "You hit the %s."), mname);
    return 0;
}

/*
 * bounce:
 *	A missile misses a monster
 */
int
bounce(struct object *weap, const char *mname)
{
    if (weap->o_type == WEAPON)
	msg(JSTR("%sはそれて、%sには当たらなかった。", "The %s misses the %s"), jrogue_text(w_names[weap->o_which]), mname);
    else
	msg(JSTR("%sには当たらなかった。", "You missed the %s."), mname);
    return 0;
}

/*
 * remove a monster from the screen
 */
int
remove_monster(coord *mp, struct linked_list *item)
{
    register char ch;

    if (item == NULL)
        return 0;
    ch = rogue_glyph(mvinch(mp->y, mp->x));
    wmove(mw, mp->y, mp->x);
    waddch(mw, ' ');
    wmove(cw, mp->y, mp->x);
    waddch(cw, ch);
    /* A disappearing monster can still own items (especially when a
     * restored monster is removed by a special attack).  Ordinary kills
     * have already dropped their inventory, so this is a no-op there. */
    {
        struct thing *monster = (struct thing *)ldata(item);
        free_list(monster->t_pack);
    }
    detach(mlist, item);
    discard(item);
    return 0;
}

/*
 * is_magic:
 *	Returns true if an object radiates magic
 */
int
is_magic(struct object *obj)
{
    register bool magic = FALSE;

    switch (obj->o_type)
    {
	case ARMOR:
	    if (obj->o_ac != a_class[obj->o_which])
		magic = TRUE;
	when WEAPON:
	    if (obj->o_hplus != 0 || obj->o_dplus != 0)
		magic = TRUE;
	when POTION:
	case SCROLL:
	case AMULET:
	    magic = TRUE;
    }
    return magic;
}
