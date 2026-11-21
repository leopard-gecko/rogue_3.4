/*
 * This file has all the code for the option command.  I would rather
 * this command were not necessary, but it is the only way to keep the
 * wolves off of my back.
 *
 * @(#)options.c	X.XX (Berkeley) XX/XX/XX
 */

#include <ctype.h>
#include "rogue.h"

#define	NUM_OPTS	(sizeof optlist / sizeof (OPTION))

/*
 * description of an option and what to do with it
 */
struct optstruct {
    char	*o_name;	/* option name */
    char	*o_prompt;	/* prompt for interactive entry */
    void	*o_opt;		/* pointer to thing to set */
    void	(*o_putfunc)(void *);	/* function to print value */
    int		(*o_getfunc)(void *, WINDOW *);	/* function to get value interactively */
};

typedef struct optstruct	OPTION;


/* The save filename is a filesystem byte sequence.  Passing it through
 * rogue_unctrl() would corrupt UTF-8 components of HOME and ROGUEOPTS.
 * Preserve all bytes (except the terminating NUL, which cannot occur in
 * the input) and reject overlength paths rather than silently truncating.
 * The historic leading '~' expands to the current HOME directory. */
int
rogue_set_save_path(const char *path, size_t length)
{
    const char *suffix = path;
    size_t prefix_length = 0;
    if (length != 0 && *suffix == '~') {
        prefix_length = strlen(home);
        suffix++;
        length--;
        while (length != 0 && *suffix == '/') {
            suffix++;
            length--;
        }
    }
    if (prefix_length > sizeof file_name - 1 ||
        length > sizeof file_name - 1 - prefix_length)
        return -1;
    if (prefix_length != 0)
        memcpy(file_name, home, prefix_length);
    if (length != 0)
        memcpy(file_name + prefix_length, suffix, length);
    file_name[prefix_length + length] = '\0';
    return 0;
}

/* A long save path must not wrap an 80-column options screen into the map. */
static void
show_option_value(WINDOW *win, const char *value)
{
    int row, col;
    size_t n;
    getyx(win, row, col);
    (void)row;
    if (col >= ROGUE_COLS - 1)
        return;
    for (n = 0; value[n] != '\0' && n < (size_t)(ROGUE_COLS - 1 - col); n++)
        waddch(win, (unsigned char)value[n]);
}

static OPTION optlist[] = {
    {"terse",	 "Terse output: ",
		 &terse,	put_bool,	get_bool	},
    {"flush",	 "Flush typeahead during battle: ",
		 &fight_flush,	put_bool,	get_bool	},
    {"jump",	 "Show position only at end of run: ",
		 &jump,		put_bool,	get_bool	},
    {"step",	"Do inventories one line at a time: ",
		&slow_invent,	put_bool,	get_bool	},
    {"askme",	"Ask me about unidentified things: ",
		&askme,		put_bool,	get_bool	},
    {"name",	 "Name: ",
		 whoami,	put_str,	get_str		},
    {"fruit",	 "Fruit: ",
		 fruit,		put_str,	get_str		},
    {"file",	 "Save file: ",
		 file_name,	put_str,	get_str		}
};

/*
 * print and then set options from the terminal
 */
int
option(void)
{
    register OPTION	*op;
    register int	retval;

    wclear(hw);
    touchwin(hw);
    /*
     * Display current values of options
     */
    for (op = optlist; op < &optlist[NUM_OPTS]; op++)
    {
	waddstr(hw, op->o_prompt);
	(*op->o_putfunc)(op->o_opt);
	waddch(hw, '\n');
    }
    /*
     * Set values
     */
    wmove(hw, 0, 0);
    for (op = optlist; op < &optlist[NUM_OPTS]; op++)
    {
	waddstr(hw, op->o_prompt);
	if ((retval = (*op->o_getfunc)(op->o_opt, hw))) {
	    if (retval == QUIT)
		break;
	    else if (op > optlist) {	/* MINUS */
		wmove(hw, (op - optlist) - 1, 0);
		op -= 2;
	    }
	    else	/* trying to back up beyond the top */
	    {
		putchar('\007');
		wmove(hw, 0, 0);
		op--;
	    }
	}
    }
    /*
     * Switch back to original screen
     */
    mvwaddstr(hw, ROGUE_LINES-1, 0, "--Press space to continue--");
    draw(hw);
    wait_for(' ');
    clearok(cw, TRUE);
    touchwin(cw);
    after = FALSE;
    return 0;
}

/*
 * put out a boolean
 */
void
put_bool(void *value)
{
    bool *b = value;
    waddstr(hw, *b ? "True" : "False");
}

/*
 * put out a string
 */
void
put_str(void *value)
{
    char *str = value;
    if (str == file_name)
        show_option_value(hw, str);
    else
        waddstr(hw, str);
}

/*
 * allow changing a boolean option and print it out
 */
int
get_bool(void *value, WINDOW *win)
{
    bool *bp = value;
    register int oy, ox;
    register bool op_bad;

    op_bad = TRUE;
    getyx(win, oy, ox);
    waddstr(win, *bp ? "True" : "False");
    while(op_bad)
    {
	wmove(win, oy, ox);
	draw(win);
	switch (readchar())
	{
	    case 't':
	    case 'T':
		*bp = TRUE;
		op_bad = FALSE;
		break;
	    case 'f':
	    case 'F':
		*bp = FALSE;
		op_bad = FALSE;
		break;
	    case '\n':
	    case '\r':
		op_bad = FALSE;
		break;
	    case '\033':
	    case '\007':
		return QUIT;
	    case '-':
		return MINUS;
	    default:
		mvwaddstr(win, oy, ox + 10, "(T or F)");
	}
    }
    wmove(win, oy, ox);
    wclrtoeol(win);
    waddstr(win, *bp ? "True" : "False");
    waddch(win, '\n');
    return NORM;
}

/*
 * set a string option
 */
int
get_str(void *value, WINDOW *win)
{
    char *opt = value;
    register char *sp;
    register int c;
    register int oy, ox;
    char buf[80];

    draw(win);
    getyx(win, oy, ox);
    /*
     * loop reading in the string, and put it in a temporary buffer
     */
    for (sp = buf;
	(c = readchar()) != '\n' && c != '\r' && c != '\033' && c != '\007';
	wclrtoeol(win), draw(win))
    {
	if (c == -1)
	    continue;
	else if (c == rogue_erase_char())	/* process erase character */
	{
	    if (sp > buf)
	    {
		register int i;

		sp--;
		for (i = strlen(rogue_unctrl(*sp)); i; i--)
		    waddch(win, '\b');
	    }
	    continue;
	}
	else if (c == rogue_kill_char())	/* process kill character */
	{
	    sp = buf;
	    wmove(win, oy, ox);
	    continue;
	}
	else if (sp == buf) {
	    if (c == '-')
		break;
	    else if (c == '~' && opt != file_name)
	    {
                size_t n = strlen(home);
                if (n >= sizeof buf) n = sizeof buf - 1;
                memcpy(buf, home, n);
                buf[n] = '\0';
                waddstr(win, buf);
                sp += n;
		continue;
	    }
	}
	if (sp < buf + sizeof buf - 1)
	{
	    *sp++ = c;
	    waddstr(win, rogue_unctrl(c));
	}
    }
    *sp = '\0';
    if (sp > buf) { /* only change option if something has been typed */
        if (opt == file_name) {
            if (rogue_set_save_path(buf, strlen(buf)) != 0)
                /* Keep the old filename and let the user edit it again. */
                waddstr(win, " (save filename too long)");
        } else {
            rogue_copy_option(opt, opt == fruit ? sizeof fruit : sizeof whoami,
                              buf, strlen(buf));
        }
    }
    wmove(win, oy, ox);
    if (opt == file_name)
        show_option_value(win, opt);
    else
        waddstr(win, opt);
    waddch(win, '\n');
    draw(win);
    if (c == '-')
	return MINUS;
    else if (c == '\033' || c == '\007')
	return QUIT;
    else
	return NORM;
}

/*
 * parse options from string, usually taken from the environment.
 * the string is a series of comma seperated values, with booleans
 * being stated as "name" (true) or "noname" (false), and strings
 * being "name=....", with the string being defined up to a comma
 * or the end of the entire option string.
 */
int
parse_opts(char *str)
{
    const char *token = str;
    while (*token) {
        const char *end, *equal;
        size_t name_len, value_len;
        OPTION *op;

        while (*token == ',' || isspace((unsigned char)*token))
            token++;
        if (*token == '\0')
            break;
        for (end = token; *end != '\0' && *end != ','; end++)
            ;
        equal = memchr(token, '=', (size_t)(end - token));
        name_len = (size_t)((equal != NULL ? equal : end) - token);
        while (name_len && isspace((unsigned char)token[name_len - 1]))
            name_len--;
        for (op = optlist; op < &optlist[NUM_OPTS]; op++) {
            size_t op_len = strlen(op->o_name);
            if (op->o_putfunc == put_bool) {
                if (name_len == op_len && !strncmp(token, op->o_name, op_len))
                    *(bool *)op->o_opt = TRUE;
                else if (name_len == op_len + 2 && !strncmp(token, "no", 2)
                         && !strncmp(token + 2, op->o_name, op_len))
                    *(bool *)op->o_opt = FALSE;
                continue;
            }
            if (equal == NULL || name_len != op_len || strncmp(token, op->o_name, op_len))
                continue;
            {
                char *dest = op->o_opt;
                size_t cap = dest == fruit ? sizeof fruit : sizeof whoami;
                const char *value = equal + 1;
                char expanded[sizeof file_name];
                size_t prefix = 0;
                value_len = (size_t)(end - value);
                if (dest == file_name) {
                    if (rogue_set_save_path(value, value_len) != 0)
                        return -1;
                    break;
                }
                if (value_len && *value == '~') {
                    prefix = strlen(home);
                    if (prefix >= sizeof expanded)
                        prefix = sizeof expanded - 1;
                    memcpy(expanded, home, prefix);
                    value++;
                    value_len--;
                    while (value_len && *value == '/') {
                        value++;
                        value_len--;
                    }
                }
                expanded[prefix] = '\0';
                rogue_copy_option(expanded + prefix, sizeof expanded - prefix, value, value_len);
                rogue_copy_option(dest, cap, expanded, strlen(expanded));
            }
            break;
        }
        token = end;
    }
    return 0;
}

/* Convert input bytes to the historical printable ^X representation, with
   an explicit destination bound. Both environment and interactive options
   pass through this single helper. */
void
rogue_copy_option(char *dest, size_t capacity, const char *source, size_t len)
{
    size_t i, used = 0;
    if (capacity == 0)
        return;
    for (i = 0; i < len; i++) {
        const char *printable = rogue_unctrl((unsigned char)source[i]);
        size_t n = strlen(printable);
        if (n > capacity - 1 - used)
            break;
        memcpy(dest + used, printable, n);
        used += n;
    }
    dest[used] = '\0';
}
