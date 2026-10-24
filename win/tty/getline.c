/* NetHack 5.0	getline.c	$NHDT-Date: 1781973100 2026/06/20 16:31:40 $  $NHDT-Branch: NetHack-5.0 $:$NHDT-Revision: 1.71 $ */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/*-Copyright (c) Michael Allison, 2006. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

#ifdef TTY_GRAPHICS

#if !defined(MAC68K)
#define NEWAUTOCOMP
#endif

#include "wintty.h"
#include "func_tab.h"
#include "utf8map.h"

char morc = 0; /* tell the outside world what char you chose */
static boolean suppress_history;
static boolean ext_cmd_getlin_hook(char *);

typedef boolean (*getlin_hook_proc)(char *);

static void hooked_tty_getlin(const char *, char *, getlin_hook_proc);
extern int extcmd_via_menu(void); /* cmd.c */

extern char erase_char, kill_char; /* from appropriate tty.c file */

#ifdef ZHLANG
/* Number of bytes in the UTF-8 sequence introduced by lead byte 'c'
   (1 when 'c' is not a valid lead byte). */
static int
u8_lead_bytes(unsigned char c)
{
    if (c >= 0xC2 && c <= 0xDF)
        return 2;
    if (c >= 0xE0 && c <= 0xEF)
        return 3;
    if (c >= 0xF0 && c <= 0xF4)
        return 4;
    return 1;
}

/* Display width of the (multi-byte) character that ends at 'end'; the
   start of that character is stored in *start. */
static int
u8_prevchar(const char *base, char *end, char **start)
{
    char *p = end - 1;
    char tmp[8];
    int n;

    while (p > base && ((unsigned char) *p & 0xC0) == 0x80)
        --p;
    n = (int) (end - p);
    if (n > (int) sizeof tmp - 1)
        n = (int) sizeof tmp - 1;
    (void) memcpy(tmp, p, (size_t) n);
    tmp[n] = '\0';
    *start = p;
    return u8_width(tmp);
}

/* Move the top-line cursor back over 'backcols' columns, blank the next
   'clearcols' columns and return the cursor to where it started. */
static void
u8_erase_span(int backcols, int clearcols)
{
    int k;

    for (k = 0; k < backcols; ++k)
        putsyms("\b");
    for (k = 0; k < clearcols; ++k)
        putsyms(" ");
    for (k = 0; k < clearcols; ++k)
        putsyms("\b");
}
#endif /* ZHLANG */

/*
 * Read a line closed with '\n' into the array char bufp[BUFSZ].
 * (The '\n' is not stored. The string is closed with a '\0'.)
 * Reading can be interrupted by an escape ('\033').  If there is already
 * some text, it is removed and prompting continues as if from the start.
 * However, if there is no text yet (or anymore) then "\033" is returned.
 */
void
tty_getlin(const char *query, char *bufp)
{
    suppress_history = FALSE;
    hooked_tty_getlin(query, bufp, (getlin_hook_proc) 0);
}

static void
hooked_tty_getlin(
    const char *query,
    char *bufp,
    getlin_hook_proc hook)
{
    char *obufp = bufp;
    int c;
    int pushback = -1; /* a byte read while completing a UTF-8 sequence */
    struct WinDesc *cw = wins[WIN_MESSAGE];
    boolean doprev = FALSE;

    if (ttyDisplay->toplin == TOPLINE_NEED_MORE && !(cw->flags & WIN_STOP))
        more();
    cw->flags &= ~WIN_STOP;
    ttyDisplay->toplin = TOPLINE_SPECIAL_PROMPT;
    ttyDisplay->inread++;

    /*
     * Issue the prompt.
     *
     * custompline() will call vpline() which calls flush_screen() which
     * calls bot(). The core now disables bot() processing while inside
     * getlin, so the screen won't be modified during whatever this prompt
     * is for.
     */
    custompline(OVERRIDE_MSGTYPE | SUPPRESS_HISTORY, "%s ", query);

#ifdef EDIT_GETLIN
    /* bufp is input/output; treat current contents (presumed to be from
       previous getlin()) as default input */
    addtopl(obufp);
    bufp = eos(obufp);
#else
    /* !EDIT_GETLIN: bufp is output only; init it to empty */
    *bufp = '\0';
#endif

    for (;;) {
        (void) fflush(stdout);
        Strcat(strcat(strcpy(gt.toplines, query), " "), obufp);
        term_curs_set(1);
        c = (pushback >= 0) ? pushback : pgetchar();
        pushback = -1;
        term_curs_set(0);
        if (c == '\033' || c == EOF) {
            if (c == EOF)
                iflags.term_gone = 1;
            if (c == '\033' && obufp[0] != '\0') {
                obufp[0] = '\0';
                bufp = obufp;
                tty_clear_nhwindow(WIN_MESSAGE);
                cw->maxcol = cw->maxrow;
                addtopl(query);
                addtopl(" ");
                addtopl(obufp);
            } else {
                obufp[0] = '\033';
                obufp[1] = '\0';
                break;
            }
        }
        if (ttyDisplay->intr) {
            ttyDisplay->intr--;
            *bufp = 0;
        }
        if (c == C('p')) { /* ctrl-P, doesn't honor rebinding #prevmsg cmd */
            int sav = ttyDisplay->inread;

            ttyDisplay->inread = 0;
            if (iflags.prevmsg_window == 's'
                || (iflags.prevmsg_window == 'c' && !doprev)) {
                /* msg_window:single, or msg_window:combination while it's
                   behaving like msg_window:single */
                if (!doprev)
                    (void) tty_doprev_message(); /* need two initially */
                (void) tty_doprev_message();
                ttyDisplay->inread = sav;
                doprev = TRUE;
                continue;
            } else {
                /* msg_window:full or reverse, or msg_window:combination while
                   it's behaving like msg_window:full */
                (void) tty_doprev_message();
                ttyDisplay->inread = sav;
                doprev = FALSE;
                tty_clear_nhwindow(WIN_MESSAGE);
                cw->maxcol = cw->maxrow;
                addtopl(query);
                addtopl(" ");
                *bufp = 0;
                addtopl(obufp);
            }
        } else if (doprev) {
            tty_clear_nhwindow(WIN_MESSAGE);
            cw->maxcol = cw->maxrow;
            doprev = FALSE;
            addtopl(query);
            addtopl(" ");
            *bufp = 0;
            addtopl(obufp);
        }
        if (c == erase_char || c == '\b') {
            if (bufp != obufp) {
#ifdef ZHLANG
                char *prev;
                int w, tailw;

                /* delete a whole UTF-8 character and move the cursor back
                   by its display width, not by its byte count */
                w = u8_prevchar(obufp, bufp, &prev);
                tailw = u8_width(bufp);
                if (w < 1)
                    w = 1;
                u8_erase_span(w, w + tailw);
                bufp = prev;
                *bufp = '\0';
#else
#ifdef NEWAUTOCOMP
                char *i;

#endif /* NEWAUTOCOMP */
                bufp--;
#ifndef NEWAUTOCOMP
                putsyms("\b \b"); /* putsym converts \b */
#else                             /* NEWAUTOCOMP */
                putsyms("\b");
                for (i = bufp; *i; ++i)
                    putsyms(" ");
                for (; i > bufp; --i)
                    putsyms("\b");
                *bufp = 0;
#endif                            /* NEWAUTOCOMP */
#endif /* ZHLANG */
            } else
                tty_nhbell();
        } else if (c == '\n' || c == '\r') {
#ifndef NEWAUTOCOMP
            *bufp = 0;
#endif /* not NEWAUTOCOMP */
            break;
        } else if (' ' <= (unsigned char) c && c != '\177'
                   /* avoid isprint() - some people don't have it
                      ' ' is not always a printing char */
                   && (bufp - obufp < BUFSZ - 1 && bufp - obufp < COLNO)) {
#ifdef NEWAUTOCOMP
            char *i = eos(bufp);

#endif /* NEWAUTOCOMP */
#ifdef ZHLANG
            {
                char seq[8];
                int got = 0, want, k;
                unsigned char uc = (unsigned char) c;

                /* Read (and then echo) a complete UTF-8 sequence at once.
                   Echoing one byte at a time would place cursor-control
                   escape sequences between the bytes of a character. */
                want = u8_lead_bytes(uc);
                if ((BUFSZ - 1 - (int) (bufp - obufp)) < want
                    || (COLNO - (int) (bufp - obufp)) < want)
                    want = 1;
                seq[got++] = (char) c;
                while (got < want) {
                    int c2 = (pushback >= 0) ? pushback : pgetchar();

                    pushback = -1;
                    if (c2 == EOF || (unsigned char) c2 < 0x80
                        || (unsigned char) c2 > 0xBF) {
                        pushback = c2; /* not part of this character */
                        break;
                    }
                    seq[got++] = (char) c2;
                }
                for (k = 0; k < got; ++k)
                    bufp[k] = seq[k];
                bufp += got;
                *bufp = '\0';
                putsyms(bufp - got);
#ifdef NEWAUTOCOMP
                if (got > 1)
                    i = bufp; /* command completion is ASCII only */
#endif
            }
#else
            *bufp = c;
            bufp[1] = 0;
            putsyms(bufp);
            bufp++;
#endif
            if (hook && (*hook)(obufp)) {
                putsyms(bufp);
#ifndef NEWAUTOCOMP
                bufp = eos(bufp);
#else  /* NEWAUTOCOMP */
                /* pointer and cursor left where they were */
                for (i = bufp; *i; ++i)
                    putsyms("\b");
            } else if (i > bufp) {
                char *s = i;

                /* erase rest of prior guess */
                for (; i > bufp; --i)
                    putsyms(" ");
                for (; s > bufp; --s)
                    putsyms("\b");
#endif /* NEWAUTOCOMP */
            }
        } else if (c == kill_char || c == '\177') { /* Robert Viduya */
            /* this test last - @ might be the kill_char */
#ifdef ZHLANG
            {
                int totalw = u8_width(obufp);
                int tailw = u8_width(bufp);
                int backw = totalw - tailw;

                if (backw < 0)
                    backw = 0;
                u8_erase_span(backw, totalw);
                bufp = obufp;
                *bufp = '\0';
            }
#else
#ifndef NEWAUTOCOMP
            while (bufp != obufp) {
                bufp--;
                putsyms("\b \b");
            }
#else  /* NEWAUTOCOMP */
            for (; *bufp; ++bufp)
                putsyms(" ");
            for (; bufp != obufp; --bufp)
                putsyms("\b \b");
            *bufp = 0;
#endif /* NEWAUTOCOMP */
#endif /* ZHLANG */
        } else
            tty_nhbell();
    }
    ttyDisplay->toplin = TOPLINE_NON_EMPTY;
    ttyDisplay->inread--;
    clear_nhwindow(WIN_MESSAGE); /* clean up after ourselves */

    if (suppress_history) {
        /* prevent next message from pushing current query+answer into
           tty message history */
        *gt.toplines = '\0';
#ifdef DUMPLOG_CORE
    } else {
        /* needed because we've bypassed pline() */
        dumplogmsg(gt.toplines);
#endif
    }
}

void
xwaitforspace(const char *s) /* chars allowed besides return */
{
    int c, x = ttyDisplay ? (int) ttyDisplay->dismiss_more : '\n';

    morc = 0;
    while (
#ifdef HANGUPHANDLING
        !program_state.done_hup &&
#endif
        (c = tty_nhgetch()) != EOF) {
        if (c == '\n' || c == '\r')
            break;

        if (iflags.cbreak) {
            if (c == '\033') {
                if (ttyDisplay)
                    ttyDisplay->dismiss_more = 1;
                morc = '\033';
                break;
            }
            if ((s && strchr(s, c)) || c == x || (x == '\n' && c == '\r')) {
                morc = (char) c;
                break;
            }
            tty_nhbell();
        }
    }
}

/*
 * Implement extended command completion by using this hook into
 * tty_getlin.  Check the characters already typed, if they uniquely
 * identify an extended command, expand the string to the whole
 * command.
 *
 * Return TRUE if we've extended the string at base.  Otherwise return FALSE.
 * Assumptions:
 *
 *      + we don't change the characters that are already in base
 *      + base has enough room to hold our string
 */
static boolean
ext_cmd_getlin_hook(char *base)
{
    int *ecmatches;
    int nmatches = extcmds_match(base, ECM_NOFLAGS, &ecmatches);

    if (nmatches == 1) {
        struct ext_func_tab *ec = extcmds_getentry(ecmatches[0]);

        Strcpy(base, ec->ef_txt);
        return TRUE;
    }

    return FALSE; /* didn't match anything */
}

/*
 * Read in an extended command, doing command line completion.  We
 * stop when we have found enough characters to make a unique command.
 */
int
tty_get_ext_cmd(void)
{
    char buf[BUFSZ];
    int nmatches;
    int *ecmatches = 0;
    boolean (*no_hook)(char *base) = (boolean (*)(char *)) 0;
    char extcmd_char[2];

    if (iflags.extmenu)
        return extcmd_via_menu();

    suppress_history = TRUE;
    /* maybe a runtime option?
     * hooked_tty_getlin("#", buf,
     *                   (flags.cmd_comp && !gi.in_doagain)
     *                      ? ext_cmd_getlin_hook
     *                      : (getlin_hook_proc) 0);
     */
    extcmd_char[0] = extcmd_initiator(), extcmd_char[1] = '\0';
    buf[0] = '\0';
    hooked_tty_getlin(extcmd_char, buf,
                      !gi.in_doagain ? ext_cmd_getlin_hook : no_hook);
    (void) mungspaces(buf);

    nmatches = (buf[0] == '\0' || buf[0] == '\033') ? -1
              : extcmds_match(buf, ECM_IGNOREAC | ECM_EXACTMATCH, &ecmatches);
    if (nmatches != 1) {
        if (nmatches != -1)
            pline("%s%.60s: unknown extended command.",
                  visctrl(extcmd_char[0]), buf);
        return -1;
    }

    return ecmatches[0];
}

#endif /* TTY_GRAPHICS */

/*getline.c*/
