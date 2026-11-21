/*
 * Various input/output functions
 *
 * @(#)io.c	X.XX (Berkeley) XX/XX/XX
 */

#include <ctype.h>
#include <stdarg.h>
#include <errno.h>
#include <poll.h>
#include "rogue.h"

/*
 * msg:
 *	Display a message at the top of the screen.
 */
void
msg(const char *fmt, ...)
{
    char msgbuf[512];
    char *ep;
    va_list ap;
    if (*fmt == '\0')
    {
        wmove(cw, 0, 0);
        wclrtoeol(cw);
        mpos = 0;
        return;
    }
    va_start(ap, fmt);
    vsnprintf(msgbuf, sizeof msgbuf, fmt, ap);
    va_end(ap);
    for (ep = msgbuf; *ep; ep++)
        if (*ep == '\n') *ep = ' ';
    /* Retain the original 80-byte message history without overflow. */
    {
        size_t n = strlen(msgbuf);
        if (n >= sizeof huh) n = sizeof huh - 1;
        memcpy(huh, msgbuf, n);
        huh[n] = '\0';
    }
    /* Game uses a fixed 80-column command line, never wrap into the map. */
    msgbuf[ROGUE_COLS - 1] = '\0';
    if (mpos)
    {
        wmove(cw, 0, mpos < ROGUE_COLS - 9 ? mpos : ROGUE_COLS - 9);
        waddstr(cw, "--More--");
        draw(cw);
        wait_for(' ');
    }
    mvwaddstr(cw, 0, 0, msgbuf);
    wclrtoeol(cw);
    mpos = strlen(msgbuf);
    draw(cw);
}

/*
 * step_ok:
 *	returns true if it is ok to step on ch
 */
int
step_ok(int ch)
{
    switch (ch)
    {
	case FLOOR:
	case PASSAGE:
	case DOOR:
	case STAIRS:
	case TRAP:
	case GOLD:
	case POTION:
	case SCROLL:
	case FOOD:
	case WEAPON:
	case ARMOR:
	case AMULET:
	    return TRUE;
	default:
	    return FALSE;
    }
}

/*
 * readchar:
 *	flushes stdout so that screen is up to date and then returns
 *	getchar.
 */
int
readchar(void)
{
    unsigned char c;
    ssize_t n;

    fflush(stdout);
    for (;;) {
        struct pollfd input = { STDIN_FILENO, POLLIN, 0 };
        int ready;

        rogue_check_signals();
        rogue_check_terminal_size();
        /* Even when SIGWINCH arrives just before the blocking call, keep
           checking the actual terminal size.  This also ensures termination
           signals are handled without waiting for another keypress. */
        ready = poll(&input, 1, 200);
        if (ready == 0 || (ready < 0 && errno == EINTR))
            continue;
        if (ready < 0)
            break;
        /* A size change or termination may have arrived between the first
           check and input readiness.  Do not execute a game command while
           the terminal is smaller than 80x24. */
        rogue_check_signals();
        rogue_check_terminal_size();
        n = read(STDIN_FILENO, &c, 1);
        if (n == 1)
            return c;
        if (n < 0 && errno == EINTR)
            continue;
        break;
    }
    /* EOF or an actual read/poll error: leave the terminal usable. */
    endwin();
    fprintf(stderr, "Rogue 3.4: terminal input closed.\n");
    exit(1);
}

/*
 * unctrl:
 *	Print a readable version of a certain character
 */
char *
rogue_unctrl(int ch)
{
    static char printable[4];
    unsigned char c = (unsigned char) ch & 0x7f;
    if (c == 0x7f) return "^?";
    if (c < 0x20) {
        printable[0] = '^'; printable[1] = c + '@'; printable[2] = '\0';
    } else {
        printable[0] = c; printable[1] = '\0';
    }
    return printable;
}

/*
 * status:
 *	Display the important stats line.  Keep the cursor where it was.
 */
int
status(void)
{
    register int oy, ox, temp;
    char buf[512];
    static int hpwidth = 0;
    static int s_max_hp = -1;
    static int s_lvl = -1, s_pur, s_hp = -1, s_str, s_add, s_ac;
    static long s_exp;

    /*
     * If nothing has changed since the last status, don't
     * bother.
     */
    if (s_hp == pstats.s_hpt && s_max_hp == max_hp &&
        s_exp == pstats.s_exp && s_pur == purse
	&& s_ac == (cur_armor != NULL ? cur_armor->o_ac : pstats.s_arm)
	&& s_str == pstats.s_str.st_str && s_add == pstats.s_str.st_add
	&& s_lvl == level)
	    return 0;

    getyx(cw, oy, ox);
    if (s_max_hp != max_hp)
    {
	temp = max_hp;
	for (hpwidth = 0; temp; hpwidth++)
	    temp /= 10;
    }
    if (pstats.s_str.st_add == 0)
    {
	snprintf(buf, sizeof buf,
	    "Level: %d  Gold: %-5d  Hp: %*d(%*d)  Str: %-2d  Ac: %-2d  Exp: %d/%ld",
	    level, purse, hpwidth, pstats.s_hpt, hpwidth, max_hp,
	    pstats.s_str.st_str,
	    cur_armor != NULL ? cur_armor->o_ac : pstats.s_arm, pstats.s_lvl,
	    pstats.s_exp);
    }
    else
    {
	snprintf(buf, sizeof buf,
	    "Level: %d  Gold: %-5d  Hp: %*d(%*d)  Str: %-2d/%-2d  Ac: %-2d  Exp: %d/%ld",
	    level, purse, hpwidth, pstats.s_hpt, hpwidth, max_hp,
	    pstats.s_str.st_str, pstats.s_str.st_add,
	    cur_armor != NULL ? cur_armor->o_ac : pstats.s_arm, pstats.s_lvl,
	    pstats.s_exp);
    }
    /*
     * Save old status
     */
    s_lvl = level;
    s_pur = purse;
    s_hp = pstats.s_hpt;
    s_max_hp = max_hp;
    s_str = pstats.s_str.st_str;
    s_add = pstats.s_str.st_add;
    s_exp = pstats.s_exp;
    s_ac = (cur_armor != NULL ? cur_armor->o_ac : pstats.s_arm);
    wmove(cw, ROGUE_LINES - 1, 0);
    wclrtoeol(cw);
    mvwaddstr(cw, ROGUE_LINES - 1, 0, buf);
    wmove(cw, oy, ox);
    return 0;
}

/*
 * wait_for
 *	Sit around until the guy types the right key
 */
int
wait_for(char ch)
{
    register char c;

    if (ch == '\n')
	while ((c = readchar()) != '\n' && c != '\r')
	    continue;
    else
	while (readchar() != ch)
	    continue;
    return 0;
}

/*
 * show_win:
 *	function used to display a window and wait before returning
 */
int
show_win(WINDOW *scr, char *message)
{
    mvwaddstr(scr, 0, 0, message);
    touchwin(scr);
    wmove(scr, hero.y, hero.x);
    draw(scr);
    wait_for(' ');
    clearok(cw, TRUE);
    touchwin(cw);
    return 0;
}
