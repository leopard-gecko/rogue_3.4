/*
 * File for the fun ends
 * Death or a total win
 *
 * @(#)rip.c	X.XX (Berkeley) XX/XX/XX
 */

#include <time.h>
#include <signal.h>
#include <ctype.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <limits.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "rogue.h"

static const char *const rip[] = {
"                       __________",
"                      /          \\",
"                     /    REST    \\",
"                    /      IN      \\",
"                   /     PEACE      \\",
"                  /                  \\",
"                  |                  |",
"                  |                  |",
"                  |   killed by a    |",
"                  |                  |",
"                  |       1980       |",
"                 *|     *  *  *      | *",
"         ________)/\\\\_//(\\/(/\\)/\\//\\/|_)_______",
    0
};


/*
 * death:
 *	Do something really fun when he dies
 */
int
death(char monst)
{
    const char *const *dp = rip;
    char *killer;
    register struct tm *lt;
    time_t date;
    char buf[80];

    time(&date);
    lt = localtime(&date);
    clear();
    move(8, 0);
    while (*dp)
	printw("%s\n", *dp++);
    /* The tombstone is only 21 columns wide. Keep long option names
       inside the drawing; score files still keep the full name. */
    {
        char stone_name[20];
        size_t len = strlen(whoami);
        if (len >= sizeof stone_name) len = sizeof stone_name - 1;
        memcpy(stone_name, whoami, len);
        stone_name[len] = '\0';
        mvaddstr(14, 28-((strlen(stone_name)+1)/2), stone_name);
    }
    purse -= purse/10;
    sprintf(buf, "%d Au", purse);
    mvaddstr(15, 28-((strlen(buf)+1)/2), buf);
    killer = killname(monst);
    mvaddstr(17, 28-((strlen(killer)+1)/2), killer);
    if (*killer == 'a' || *killer == 'e' || *killer == 'i' ||
	*killer == 'o' || *killer == 'u')
	    mvaddch(16, 33, 'n');
    /* The original 1980 lettering was patched only in its last two digits.
       That would display 1926 in 2026. Replace all four year digits. */
    snprintf(prbuf, sizeof prbuf, "%04d", lt != NULL ? lt->tm_year + 1900 : 1980);
    mvaddstr(18, 26, prbuf);
    /* Keep the tombstone visible on the curses screen until Return.
       endwin() switches modern terminals out of their alternate screen. */
    score_wait_on_screen();
    endwin();
    score(purse, 0, monst);
    exit(0);
}

/*
 * Wait on the curses screen *before* leaving it.  On macOS Terminal and
 * other alternate-screen terminals, stdout prompts after endwin() would
 * appear over the shell's scrollback instead of the tombstone.
 */
void
score_wait_on_screen(void)
{
    static const char prompt[] = "[Press return to continue]";

    wmove(stdscr, ROGUE_LINES - 1, 0);
    wclrtoeol(stdscr);
    waddstr(stdscr, prompt);
    draw(stdscr);
    wait_for('\n');
}

/*
 * Versioned, fixed-layout scores.  Unlike the historical encrypted struct,
 * this representation is independent of pointer size, padding and byte order.
 * The 1981 gameplay scoring rules (top ten, strict greater-than insertion,
 * no entries after using wizard mode) are unchanged.
 *
 * All entries are parsed before updating.  A damaged or unknown version is
 * never overwritten.  The per-user file is advisory-locked for the entire
 * read/modify/write.  The per-user score path is ~/.rogue_roll.
 */
#define SCORE_MAGIC "ROGUE34-SCORES-1\n"
#define SCORE_COUNT 10
#define SCORE_MAX_BYTES 4096

struct score_entry {
    int amount;
    int flags;
    int depth;
    unsigned char monster;
    char name[80];
};

static void
score_printable_name(char *dst, size_t cap, const char *src)
{
    size_t i;
    for (i = 0; i + 1 < cap && src[i] != '\0'; i++) {
        unsigned char c = (unsigned char)src[i];
        dst[i] = (c >= 32 && c <= 126) ? (char)c : '?';
    }
    dst[i] = '\0';
}

static int
score_decode_name(const char *hex, char *name)
{
    size_t i, n = strlen(hex);
    if (n == 0 || n % 2 != 0 || n > 158)
        return -1;
    for (i = 0; i < n / 2; i++) {
        unsigned int v;
        char pair[3] = {hex[2*i], hex[2*i+1], 0};
        if (!isxdigit((unsigned char)pair[0]) ||
            !isxdigit((unsigned char)pair[1]) ||
            sscanf(pair, "%2x", &v) != 1 || v < 32 || v > 126)
            return -1;
        name[i] = (char)v;
    }
    name[n/2] = '\0';
    return 0;
}

static void
score_encode_name(const char *name, char hex[160])
{
    static const char digits[] = "0123456789abcdef";
    size_t i;
    for (i = 0; name[i] != '\0' && i < 79; i++) {
        unsigned char c = (unsigned char)name[i];
        hex[2*i] = digits[c >> 4];
        hex[2*i + 1] = digits[c & 15];
    }
    hex[2*i] = '\0';
}

/* Return 0 for a valid file (including a new empty file), -1 on corruption. */
static int
score_load(FILE *fp, struct score_entry entries[SCORE_COUNT], off_t size)
{
    char line[512];
    int n = 0;
    memset(entries, 0, sizeof(*entries) * SCORE_COUNT);
    if (size == 0)
        return 0;
    if (size > SCORE_MAX_BYTES || !fgets(line, sizeof line, fp) ||
        strcmp(line, SCORE_MAGIC) != 0)
        return -1;
    while (fgets(line, sizeof line, fp)) {
        int amount, flags, depth, used = 0;
        unsigned int monster;
        char hex[160];
        struct score_entry *e;
        if (n >= SCORE_COUNT ||
            sscanf(line, "%d %d %d %u %159s %n",
                   &amount, &flags, &depth, &monster, hex, &used) != 5 ||
            used == 0 || line[used] != '\0' || amount <= 0 ||
            flags < 0 || flags > 2 || depth < 0 || depth > 1000000 ||
            monster > 255)
            return -1;
        e = &entries[n];
        if (score_decode_name(hex, e->name) != 0)
            return -1;
        e->amount = amount;
        e->flags = flags;
        e->depth = depth;
        e->monster = (unsigned char)monster;
        n++;
    }
    return ferror(fp) ? -1 : 0;
}

static int
score_store(FILE *fp, struct score_entry entries[SCORE_COUNT])
{
    int i, fd = fileno(fp);
    if (fflush(fp) != 0 || ftruncate(fd, 0) != 0 || fseek(fp, 0, SEEK_SET) != 0)
        return -1;
    if (fputs(SCORE_MAGIC, fp) == EOF)
        return -1;
    for (i = 0; i < SCORE_COUNT; i++) {
        char hex[160];
        if (entries[i].amount <= 0)
            continue;
        score_encode_name(entries[i].name, hex);
        if (fprintf(fp, "%d %d %d %u %s\n",
                    entries[i].amount, entries[i].flags, entries[i].depth,
                    (unsigned int)entries[i].monster, hex) < 0)
            return -1;
    }
    if (fflush(fp) != 0 || fsync(fd) != 0)
        return -1;
    return 0;
}

int
score(int amount, int flags, char monst)
{
    static const char *reason[] = {"killed", "quit", "A total winner"};
    struct score_entry top_ten[SCORE_COUNT];
    struct stat st;
    int fd = -1, i, j, valid = 1;
    int writing = (flags != -1 && !waswizard && amount > 0);
    int exists = 1;
    FILE *fp = NULL;
    const char *path = rogue_score_path();

    if (path == NULL) {
        fprintf(stderr, "Rogue 3.4: score file path is too long.\n");
        return -1;
    }

    /* A read-only -s does not create or rewrite a score file. */
    fd = open(path, writing ? (O_RDWR | O_CREAT | O_NOFOLLOW) : (O_RDONLY | O_NOFOLLOW), 0600);
    if (fd < 0 && errno == ENOENT && !writing)
        exists = 0;
    else if (fd < 0)
        valid = 0;
    if (valid && exists) {
        /* Check size only *after* acquiring the lock; another score writer
           may have changed the file while we waited for the lock. */
        if (flock(fd, writing ? LOCK_EX : LOCK_SH) != 0 ||
            fstat(fd, &st) != 0 || !S_ISREG(st.st_mode))
            valid = 0;
        else {
            fp = fdopen(fd, writing ? "r+" : "r");
            if (fp == NULL || score_load(fp, top_ten, st.st_size) != 0)
                valid = 0;
        }
    } else if (valid) {
        memset(top_ten, 0, sizeof top_ten);
    }
    if (valid && writing && fp) {
        for (i = 0; i < SCORE_COUNT; i++)
            if (amount > top_ten[i].amount)
                break;
        if (i < SCORE_COUNT) {
            for (j = SCORE_COUNT - 1; j > i; j--)
                top_ten[j] = top_ten[j-1];
            top_ten[i].amount = amount;
            top_ten[i].flags = flags;
            top_ten[i].depth = flags == 2 ? max_level : level;
            top_ten[i].monster = (unsigned char)monst;
            score_printable_name(top_ten[i].name, sizeof top_ten[i].name, whoami);
            if (top_ten[i].name[0] == '\0')
                strcpy(top_ten[i].name, "Unknown");
            if (score_store(fp, top_ten) != 0)
                valid = 0;
        }
    }
    /* Finish all file access before waiting for the player's input. */
    if (fp) fclose(fp);
    else if (fd >= 0) close(fd);

    /* All callers have already ended curses (or have never started it for
       -s).  Score reporting only writes to the ordinary terminal screen. */
    if (!valid) {
        fprintf(stderr, "\nCannot read or update the Rogue 3.4 scores at %s.\n"
                "The file was left unchanged if its format is unknown.\n", path);
        return -1;
    }
    printf("\nTop Ten Adventurers:\nRank\tScore\tName\n");
    for (i = 0; i < SCORE_COUNT; i++) {
        const struct score_entry *e = &top_ten[i];
        const char *killer;
        if (e->amount <= 0)
            continue;
        printf("%d\t%d\t%s: %s on level %d", i+1, e->amount,
               e->name, reason[e->flags], e->depth);
        if (e->flags == 0) {
            killer = killname((char)e->monster);
            printf(" by a%s %s", strchr("aeiou", *killer) ? "n" : "", killer);
        }
        printf(".\n");
    }
    fflush(stdout);
    return 0;
}

int
total_winner(void)
{
    register struct linked_list *item;
    register struct object *obj;
    register int worth;

    for (item = pack; item != NULL; item = next(item))
    {
	obj = (struct object *) ldata(item);
	switch (obj->o_type)
	{
	    when FOOD:
		purse += 2;
		goto price_weapon; /* Preserve the historical FOOD fallthrough. */
	    case WEAPON:
	    price_weapon:
		switch (obj->o_which)
		{
		    when MACE: worth = 8;
		    when SWORD: worth = 15;
		    when BOW: worth = 75;
		    when ARROW: worth = 1;
		    when DAGGER: worth = 2;
		    when ROCK: worth = 1;
		    when TWOSWORD: worth = 30;
		    when SLING: worth = 1;
		    when DART: worth = 1;
		    when CROSSBOW: worth = 15;
		    when BOLT: worth = 1;
		    when SPEAR: worth = 2;
		    otherwise: worth = 0;
		}
		purse += worth * (1 + (10 * (obj->o_dplus + obj->o_hplus)));
	    when ARMOR:
		switch (obj->o_which)
		{
		    when LEATHER: worth = 5;
		    when RING_MAIL: worth = 30;
		    when STUDDED_LEATHER: worth = 15;
		    when SCALE_MAIL: worth = 3;
		    when CHAIN_MAIL: worth = 75;
		    when SPLINT_MAIL: worth = 80;
		    when BANDED_MAIL: worth = 90;
		    when PLATE_MAIL: worth = 400;
		    otherwise: worth = 0;
		}
		purse += worth * (1 + (10 * (a_class[obj->o_which] - obj->o_ac)));
	    when POTION:
	    case SCROLL:
		purse += rnd(100) + 50;
	    when AMULET:
		purse += 1000;
	}
    }
    wclear(cw);
    wmove(cw, ROGUE_LINES-1, 0);
    draw(cw);
    endwin();
    printf("Congratulations, you have made it to the light of day!\n");
    printf("You have joined the elite ranks of those who have escaped\n");
    printf("the Dungeons of Doom alive.  You journey home and sell all your\n");
    printf("loot at a great profit and are admitted to the fighters guild.\n");
    printf("[Press return to continue]");
    fflush(stdout);
    wait_for('\n');
    score(purse, 2, 0);
    exit(0);
}

char *
killname(char monst)
{
    if (monst >= 'A' && monst <= 'Z')
	return monsters[monst-'A'].m_name;
    else if (monst == 'a')
	return "arrow";
    else
	return "dart";
}
