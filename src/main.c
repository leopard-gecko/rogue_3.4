/*
 * Rogue
 * Exploring the dungeons of doom
 * Copyright (C) 1980 by Michael Toy and Glenn Wichman
 * All rights reserved
 *
 * @(#)main.c	X.XX (Berkeley) XX/XX/XX
 */

#include <signal.h>
#include <pwd.h>
#include "rogue.h"

int
main(int argc, char **argv, char **envp)
{
    register char *env;
    register struct passwd *pw;
    register struct linked_list *item;
    register struct object *obj;

    /*
     * check for print-score option
     */
    if (argc == 2 && strcmp(argv[1], "-s") == 0)
    {
	waswizard = TRUE;
	score(0, -1, 0);
	exit(0);
    }
    /*
     * Check to see if he is a wizard
     */
    if (argc >= 2 && argv[1][0] == '\0')
	if (strcmp(PASSWD, crypt(getpass("Wizard's password: "), "mT")) == 0)
	{
	    wizard = TRUE;
	    argv++;
	    argc--;
	}

    /*
     * get home and options from environment
     */
    setbuf(stdout, outbuf);

    /* Filesystem paths are byte strings, not ASCII game messages.  Keep
       the full HOME value, including UTF-8 and spaces, or fail explicitly. */
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

    /* The reconstructed 1981 main.c ultimately overwrites its HOME-based
     * candidate with the relative name "rogue.save".  Preserve that default:
     * the game starts in the caller's current working directory. */
    strcpy(file_name, "rogue.save");

    if ((env = getenv("ROGUEOPTS")) != NULL && parse_opts(env) != 0) {
        fprintf(stderr, "Rogue 3.4: ROGUEOPTS save filename is too long.\n");
        return 1;
    }
    if (env == NULL || whoami[0] == '\0') {
	if ((pw = getpwuid(getuid())) == NULL)
	{
	    printf("Say, who the hell are you?\n");
	    exit(1);
	}
	else
	    rogue_copy_option(whoami, sizeof whoami, pw->pw_name, strlen(pw->pw_name));
    }
    if (env == NULL || fruit[0] == '\0')
	strcpy(fruit, "slime-mold");

    if (argc > 2) {
        fprintf(stderr, "Usage: %s [-s | -r | savefile]\n", argv[0]);
        return 1;
    }
    if (argc == 2) {
        if (!restore(argv[1], envp))
            return 1;
        playit();
        return 0;
    }
    if (wizard && getenv("SEED") != NULL)
	dnum = atoi(getenv("SEED"));
    else
	dnum = getpid();
    if (wizard)
	printf("Hello %s, welcome to dungeon #%d", whoami, dnum);
    else
	printf("Hello %s, just a moment while I dig the dungeon...", whoami);
    fflush(stdout);
    /* Keep the original greeting readable before curses switches screens. */
    rogue_visual_delay(ROGUE_TEXT_NOTICE_DELAY_MS);
    seed = dnum;

    if (initscr() == NULL) {       /* Start up cursor package */
        fprintf(stderr, "Rogue 3.4: unable to initialize curses.\n");
        return 1;
    }
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
