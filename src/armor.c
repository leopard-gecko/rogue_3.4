/*
 * This file contains misc functions for dealing with armor
 *
 * @(#)armor.c	X.XX (Berkeley) XX/XX/XX
 */

#include "rogue.h"

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
	    msg("You are already wearing some.  You'll have to take it off first");
	else
	    msg("You are already wearing some");
	return 0;
    }
    if ((item = get_item("wear", ARMOR)) == NULL)
	return 0;
    obj = (struct object *) ldata(item);
    if (obj->o_type != ARMOR)
    {
	msg("You can't wear that.");
	return 0;
    }
    waste_time();
    if (!terse)
	msg("You are now wearing %s.", a_names[obj->o_which]);
    else
	msg("Wearing %s.", a_names[obj->o_which]);
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
	msg("You aren't wearing any");
	return 0;
    }
    if (obj->o_flags & ISCURSED)
    {
	if (terse)
	    msg("Can't get it off.");
	else
	    msg("You can't seem to get it off.  It is cursed.");
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
	msg("Was wearing %c) %s", c, inv_name(obj, TRUE));
    else
	msg("You used to be wearing %c) %s", c, inv_name(obj, TRUE));
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
