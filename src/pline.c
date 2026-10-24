/* NetHack 5.0	pline.c	$NHDT-Date: 1781973061 2026/06/20 16:31:01 $  $NHDT-Branch: NetHack-5.0 $:$NHDT-Revision: 1.150 $ */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/*-Copyright (c) Robert Patrick Rankin, 2018. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

#define BIGBUFSZ (5 * BUFSZ) /* big enough to format a 4*BUFSZ string (from
                              * config file parsing) with modest decoration;
                              * result will then be truncated to BUFSZ-1 */

staticfn void putmesg(const char *);
staticfn char *You_buf(int);
staticfn void execplinehandler(const char *);
#ifdef USER_SOUNDS
extern void maybe_play_sound(const char *);
#endif
#ifdef DUMPLOG_CORE

/* keep the most recent DUMPLOG_MSG_COUNT messages */
void
dumplogmsg(const char *line)
{
    /*
     * TODO:
     *  This essentially duplicates message history, which is
     *  currently implemented in an interface-specific manner.
     *  The core should take responsibility for that and have
     *  this share it.
     */
    unsigned indx = gs.saved_pline_index; /* next slot to use */
    char *oldest = gs.saved_plines[indx]; /* current content of that slot */

    if (!strncmp(line, "Unknown command", 15))
        return;
    if (oldest && strlen(oldest) >= strlen(line)) {
        /* this buffer will gradually shrink until the 'else' is needed;
           there's no pressing need to track allocation size instead */
        Strcpy(oldest, line);
    } else {
        if (oldest)
            free((genericptr_t) oldest);
        gs.saved_plines[indx] = dupstr(line);
    }
    gs.saved_pline_index = (indx + 1) % DUMPLOG_MSG_COUNT;
}

/* called during save (unlike the interface-specific message history,
   this data isn't saved and restored); end-of-game releases saved_plines[]
   while writing its contents to the final dump log */
void
dumplogfreemessages(void)
{
    unsigned i;

    for (i = 0; i < DUMPLOG_MSG_COUNT; ++i)
        if (gs.saved_plines[i])
            free((genericptr_t) gs.saved_plines[i]), gs.saved_plines[i] = 0;
    gs.saved_pline_index = 0;
}
#endif

#ifdef ZHLANG
/*
 * Chinese typography for text that is about to be shown.  Messages are
 * assembled from pieces shared with the English game ("%s.", "?" appended
 * to a prompt, an adjective followed by " "), so the Chinese result ends
 * up with half-width punctuation after Chinese text and stray spaces
 * between Chinese words.  These helpers fix that at the output layer only;
 * English text is never touched because every rule needs a Chinese
 * character next to it.
 */

/* length of the UTF-8 sequence starting at 's' */
#define zh_u8len(s) \
    ((((uchar) *(s)) & 0x80) == 0 ? 1                                    \
     : (((uchar) *(s)) & 0xE0) == 0xC0 ? 2                               \
       : (((uchar) *(s)) & 0xF0) == 0xE0 ? 3 : 4)

/* the code point at 's' (0 for a malformed or ASCII sequence) */
staticfn unsigned
zh_codepoint(const char *s)
{
    const uchar *b = (const uchar *) s;

    if ((b[0] & 0xF0) == 0xE0 && (b[1] & 0xC0) == 0x80
        && (b[2] & 0xC0) == 0x80)
        return ((b[0] & 0x0Fu) << 12) | ((b[1] & 0x3Fu) << 6) | (b[2] & 0x3Fu);
    return 0;
}

/* CJK ideograph */
#define zh_is_han(cp) ((cp) >= 0x4E00 && (cp) <= 0x9FFF)
/* text that full-width punctuation may follow: ideographs and the
   closing marks ）」』”】 */
#define zh_is_zhtail(cp) \
    (zh_is_han(cp) || (cp) == 0xFF09 || (cp) == 0x300D || (cp) == 0x300F \
     || (cp) == 0x201D || (cp) == 0x3011)

/* start of the character that ends just before 'p' (p > base) */
staticfn const char *
zh_prevchar(const char *base, const char *p)
{
    const char *q = p - 1;

    while (q > base && (((uchar) *q) & 0xC0) == 0x80)
        --q;
    return q;
}

/* remove a single space that sits between two ideographs ("未诅咒的 食物
   口粮") or between an ideograph and an enchantment ("的 +3"); works in
   place since the text only gets shorter */
void
zh_squeeze_spaces(char *buf)
{
    char *in = buf, *out = buf;

    while (*in) {
        if (*in == ' ' && in > buf && in[1] != ' '
            && zh_is_han(zh_codepoint(zh_prevchar(buf, in)))
            && (zh_is_han(zh_codepoint(in + 1))
                /* "未诅咒的 +3 小盾" -> "未诅咒的+3 小盾", matching the
                   erosion words that are written without a space */
                || ((in[1] == '+' || in[1] == '-')
                    && in[2] >= '0' && in[2] <= '9'))) {
            ++in;
            continue;
        }
        *out++ = *in++;
    }
    *out = '\0';
}

/* full-width punctuation after Chinese text plus zh_squeeze_spaces();
   returns a rotating static buffer of BUFSZ */
const char *
zh_tidy(const char *src)
{
    static char bufs[3][BUFSZ];
    static int bufidx = 0;
    char *out = bufs[bufidx];
    const char *p = src;
    size_t olen = 0;

    bufidx = (bufidx + 1) % SIZE(bufs);
    if (!src)
        return strcpy(out, "");
    while (*p) {
        const char *rep = 0;
        int skip = 1, n = zh_u8len(p);

        if (p > src && strchr("?!,:;.", *p)
            && zh_is_zhtail(zh_codepoint(zh_prevchar(src, p)))) {
            switch (*p) {
            case '?': rep = "？"; break;
            case '!': rep = "！"; break;
            case ',': rep = "，"; break;
            case ':': rep = "："; break;
            case ';': rep = "；"; break;
            case '.':
                if (!strncmp(p, "...", 3)) {
                    rep = "……", skip = 3;
                } else if (!p[1] || p[1] == ' ' || p[1] == '"'
                           || zh_codepoint(p + 1)) {
                    rep = "。";
                }
                break;
            }
        }
        if (rep) {
            n = (int) strlen(rep);
            if (olen + n >= BUFSZ - 1)
                break;
            (void) memcpy(out + olen, rep, (size_t) n);
            olen += n, p += skip;
        } else {
            if (olen + n >= BUFSZ - 1)
                break; /* never split a character */
            (void) memcpy(out + olen, p, (size_t) n);
            olen += n, p += n;
        }
    }
    out[olen] = '\0';
    zh_squeeze_spaces(out);
    return out;
}
#endif /* ZHLANG */

/* keeps windowprocs usage out of pline() */
staticfn void
putmesg(const char *line)
{
    int attr = ATR_NONE;

    if (iflags.debug_prevent_pline)
        return;

    if ((gp.pline_flags & URGENT_MESSAGE) != 0
        && (windowprocs.wincap2 & WC2_URGENT_MESG) != 0)
        attr |= ATR_URGENT;
    if ((gp.pline_flags & SUPPRESS_HISTORY) != 0
        && (windowprocs.wincap2 & WC2_SUPPRESS_HIST) != 0)
        attr |= ATR_NOHISTORY;
    putstr(WIN_MESSAGE, attr, line);
    SoundSpeak(line);
}

/* set the direction where next message happens */
void
set_msg_dir(int dir)
{
    dirtocoord(&a11y.msg_loc, dir);
    a11y.msg_loc.x += u.ux;
    a11y.msg_loc.y += u.uy;
}

/* set the coordinate where next message happens */
void
set_msg_xy(coordxy x, coordxy y)
{
    a11y.msg_loc.x = x;
    a11y.msg_loc.y = y;
}

staticfn void vpline(const char *, va_list);

DISABLE_WARNING_FORMAT_NONLITERAL

void
pline(const char *line, ...)
{
    va_list the_args;

    va_start(the_args, line);
    vpline(line, the_args);
    va_end(the_args);
}

void
pline_dir(int dir, const char *line, ...)
{
    va_list the_args;

    set_msg_dir(dir);

    va_start(the_args, line);
    vpline(line, the_args);
    va_end(the_args);
}

void
pline_xy(coordxy x, coordxy y, const char *line, ...)
{
    va_list the_args;

    set_msg_xy(x, y);

    va_start(the_args, line);
    vpline(line, the_args);
    va_end(the_args);
}

void
pline_mon(struct monst *mtmp, const char *line, ...)
{
    va_list the_args;

    if (mtmp == &gy.youmonst)
        set_msg_xy(0, 0);
    else
        set_msg_xy(mtmp->mx, mtmp->my);

    va_start(the_args, line);
    vpline(line, the_args);
    va_end(the_args);
}

staticfn void
vpline(const char *line, va_list the_args)
{
    static int in_pline = 0;
    char pbuf[BIGBUFSZ]; /* will get chopped down to BUFSZ-1 if longer */
    int ln;
    int msgtyp;
    boolean no_repeat;
    coord a11y_mesgxy;

    a11y_mesgxy = a11y.msg_loc; /* save a11y.msg_loc before reseting it */
    /* always reset a11y.msg_loc whether we end up using it or not */
    a11y.msg_loc.x = a11y.msg_loc.y = 0;

    if (!line || !*line)
        return;
#ifdef HANGUPHANDLING
    if (program_state.done_hup)
        return;
#endif
    if (program_state.wizkit_wishing)
        return;

    /* when accessiblemsg is set and a11y.msg_loc is nonzero, use the latter
       to insert a location prefix in front of current message */
    if (a11y.accessiblemsg && isok(a11y_mesgxy.x, a11y_mesgxy.y)) {
        char *tmp, *dirstr, dirstrbuf[QBUFSZ];

        dirstr = coord_desc(a11y_mesgxy.x, a11y_mesgxy.y, dirstrbuf,
                            ((iflags.getpos_coords == GPCOORDS_NONE)
                             ? GPCOORDS_COMFULL : iflags.getpos_coords));
        tmp = (char *) alloc(strlen(line) + sizeof ": " + strlen(dirstr));
        Strcpy(tmp, dirstr);
        Strcat(tmp, ": ");
        Strcat(tmp, line);
        vpline(tmp, the_args);
        free((genericptr_t) tmp);
        return;
    }

    if (!strchr(line, '%')) {
        /* format does not specify any substitutions; use it as-is */
        ln = (int) strlen(line);
    } else if (line[0] == '%' && line[1] == 's' && !line[2]) {
        /* "%s" => single string; skip format and use its first argument;
           unlike with the format, it is irrelevant whether the argument
           contains any percent signs */
        line = va_arg(the_args, const char *); /*VA_NEXT(line,const char *);*/
        ln = (int) strlen(line);
    } else {
        /* perform printf() formatting */
        ln = vsnprintf(pbuf, sizeof pbuf, line, the_args);
        line = pbuf;
        /* note: 'ln' is number of characters attempted, not necessarily
           strlen(line); that matters for the overflow check; if we avoid
           the extremely-too-long panic then 'ln' will be actual length */
    }
    if (ln > (int) sizeof pbuf - 1) /* extremely too long */
        panic("pline attempting to print %d characters!", ln);

    if (ln > BUFSZ - 1) {
        /* too long but modestly so; allow but truncate, preserving final
           3 chars: "___ extremely long text" -> "___ extremely l...ext"
           (this may be suboptimal if overflow is less than 3) */
#ifdef ZHLANG
        /*
         * Chinese messages are multi-byte UTF-8, so the cut points must
         * fall on character boundaries or the tty would receive a broken
         * sequence.  Keep the same shape as the English version:
         * head, "...", then the last three *characters*.
         */
        {
            int avail = BUFSZ - 1; /* room for text, not counting NUL */
            int headlen, taillen = 0, cnt = 0;
            const char *tail = line + ln;

            while (tail > line && cnt < 3) {
                --tail;
                /* step back over continuation bytes to the char start */
                while (tail > line
                       && ((unsigned char) *tail & 0xC0) == 0x80)
                    --tail;
                ++cnt;
            }
            taillen = (int) (line + ln - tail);
            headlen = avail - 3 - taillen;
            if (headlen < 0)
                headlen = 0;
            /* don't cut in the middle of a character in the head either */
            while (headlen > 0
                   && ((unsigned char) line[headlen] & 0xC0) == 0x80)
                --headlen;
            (void) memmove(pbuf, line, headlen); /* may be the same buffer */
            pbuf[headlen] = pbuf[headlen + 1] = pbuf[headlen + 2] = '.';
            (void) memmove(pbuf + headlen + 3, tail, taillen);
            pbuf[headlen + 3 + taillen] = '\0';
        }
#else
        if (line != pbuf) /* no '%' was present or format was just "%s" */
            (void) strncpy(pbuf, line, BUFSZ - 1); /* caveat: unterminated */
        pbuf[BUFSZ - 1 - 6] = pbuf[BUFSZ - 1 - 5] = pbuf[BUFSZ - 1 - 4] = '.';
        /* avoid strncpy; buffers could overlap if excess is small */
        pbuf[BUFSZ - 1 - 3] = line[ln - 3];
        pbuf[BUFSZ - 1 - 2] = line[ln - 2];
        pbuf[BUFSZ - 1 - 1] = line[ln - 1];
        pbuf[BUFSZ - 1] = '\0';
#endif
        line = pbuf;
    }
    msgtyp = MSGTYP_NORMAL;
#ifdef ZHLANG
    line = zh_tidy(line); /* Chinese punctuation and spacing */
#endif

#ifdef DUMPLOG_CORE
    /* We hook here early to have options-agnostic output.
     * Unfortunately, that means Norep() isn't honored (general issue) and
     * that short lines aren't combined into one longer one (tty behavior).
     */
    if ((gp.pline_flags & SUPPRESS_HISTORY) == 0)
        dumplogmsg(line);
#endif
    /* use raw_print() if we're called too early (or perhaps too late
       during shutdown) or if we're being called recursively (probably
       via debugpline() in the interface code) */
    if (in_pline++ || !iflags.window_inited) {
        /* [we should probably be using raw_printf("\n%s", line) here] */
        raw_print(line);
        iflags.last_msg = PLNMSG_UNKNOWN;
        goto pline_done;
    }

    no_repeat = (gp.pline_flags & PLINE_NOREPEAT) ? TRUE : FALSE;
    if ((gp.pline_flags & OVERRIDE_MSGTYPE) == 0) {
        msgtyp = msgtype_type(line, no_repeat);
#ifdef USER_SOUNDS
        if (msgtyp == MSGTYP_NORMAL || msgtyp == MSGTYP_NOSHOW)
            maybe_play_sound(line);
#endif
        if ((gp.pline_flags & URGENT_MESSAGE) == 0
            && (msgtyp == MSGTYP_NOSHOW
                || (msgtyp == MSGTYP_NOREP && !strcmp(line, gp.prevmsg))))
            /* FIXME: we need a way to tell our caller that this message
             * was suppressed so that caller doesn't set iflags.last_msg
             * for something that hasn't been shown, otherwise a subsequent
             * message which uses alternate wording based on that would be
             * doing so out of context and probably end up seeming silly.
             * (Not an issue for no-repeat but matters for no-show.)
             */
            goto pline_done;
    }

    if (gv.vision_full_recalc) {
        int tmp_in_pline = in_pline;

        in_pline = 0;
        vision_recalc(0);
        in_pline = tmp_in_pline;
    }
    if (u.ux)
        flush_screen((gp.pline_flags & NO_CURS_ON_U) ? 0 : 1); /* %% */

    putmesg(line);

    execplinehandler(line);

    /* this gets cleared after every pline message */
    iflags.last_msg = PLNMSG_UNKNOWN;
    (void) strncpy(gp.prevmsg, line, BUFSZ), gp.prevmsg[BUFSZ - 1] = '\0';
    if (msgtyp == MSGTYP_STOP)
        display_nhwindow(WIN_MESSAGE, TRUE); /* --more-- */
 pline_done:
#ifdef SND_SPEECH
    /* clear the SPEECH flag so caller never has to */
    gp.pline_flags &= ~PLINE_SPEECH;
#endif
    --in_pline;
}

RESTORE_WARNING_FORMAT_NONLITERAL

/* pline() variant which can override MSGTYPE handling or suppress
   message history (tty interface uses pline() to issue prompts and
   they shouldn't be blockable via MSGTYPE=hide) */
void
custompline(unsigned pflags, const char *line, ...)
{
    va_list the_args;

    va_start(the_args, line);
    gp.pline_flags = pflags;
    vpline(line, the_args);
    gp.pline_flags = 0;
    va_end(the_args);
}

/* if player has dismissed --More-- with ESC to suppress further messages
   until next input request, tell the interface that it should override that
   and re-enable them; equivalent to custompline(URGENT_MESSAGE, line, ...)
   but slightly simpler to use */
void
urgent_pline(const char *line, ...)
{
    va_list the_args;

    va_start(the_args, line);
    gp.pline_flags = URGENT_MESSAGE;
    vpline(line, the_args);
    gp.pline_flags = 0;
    va_end(the_args);
}

void
Norep(const char *line, ...)
{
    va_list the_args;

    va_start(the_args, line);
    gp.pline_flags = PLINE_NOREPEAT;
    vpline(line, the_args);
    gp.pline_flags = 0;
    va_end(the_args);
}

staticfn char *
You_buf(int siz)
{
    if (siz > gy.you_buf_siz) {
        if (gy.you_buf)
            free((genericptr_t) gy.you_buf);
        gy.you_buf_siz = siz + 10;
        gy.you_buf = (char *) alloc((unsigned) gy.you_buf_siz);
    }
    return gy.you_buf;
}

void
free_youbuf(void)
{
    if (gy.you_buf)
        free((genericptr_t) gy.you_buf), gy.you_buf = (char *) 0;
    gy.you_buf_siz = 0;
}

/* `prefix' may be a string literal or a computed pointer; size the buffer
   for the actual prefix length (C strings never need more room). */
#define YouPrefix(pointer, prefix, text) \
    Strcpy((pointer = You_buf((int) (strlen(text) + strlen(prefix) + 1))), \
           prefix)

#define YouMessage(pointer, prefix, text) \
    strcat((YouPrefix(pointer, prefix, text), pointer), text)

#ifdef ZHLANG
/*
 * Chinese messages put the subject in front of the translated text
 * ("你挥舞着…"), so the You() family supplies a Chinese prefix instead of
 * the former empty string.  Translations which already spell out the
 * subject must not get a second one, hence the "already starts with 你"
 * test.  Untranslated (ASCII) text keeps the old behaviour of getting no
 * prefix, so an English fallback sentence is not mangled.
 */
/* does 'text' begin with any of the NUL-terminated strings in 'words'? */
staticfn boolean
zh_text_starts_with(const char *text, const char *const *words)
{
    int i;

    for (i = 0; words[i]; ++i)
        if (!strncmp(text, words[i], strlen(words[i])))
            return TRUE;
    return FALSE;
}

/*
 * Choose the Chinese subject prefix for the You() family.
 *
 *  full  is the normal prefix ("你感到", "你不能", ...);
 *  shortf is just the subject ("你"), used when the translated text
 *        already starts with the verb that 'full' would supply, so that
 *        "你感到" + "感到内疚" doesn't come out as "你感到感到内疚";
 *  verbs selects the synonym list for that verb ('F' feel, 'C' can't,
 *        'H' hear, 'S' see, 0 none).
 */
staticfn const char *
zh_you_prefix(const char *text, const char *full, const char *shortf,
              int verbs)
{
    static const char *const feel_verbs[] = {
        "感到", "感觉", "觉得", "觉着", 0
    };
    static const char *const cant_verbs[] = {
        "不能", "无法", "不会", "没法", "不可以", 0
    };
    static const char *const hear_verbs[] = {
        "听到", "听见", "听闻", "耳闻", 0
    };
    static const char *const see_verbs[] = {
        "看见", "看到", "见到", "注意到", "察觉", "感觉到", "感到", 0
    };

    if (!text || !*text)
        return "";
    /* the subject is already spelled out in the translation */
    if (!strncmp(text, "你", sizeof "你" - 1))
        return "";
    /* the verb is already there: keep just the subject */
    switch (verbs) {
    case 'F':
        if (zh_text_starts_with(text, feel_verbs))
            return shortf;
        break;
    case 'C':
        if (zh_text_starts_with(text, cant_verbs))
            return shortf;
        break;
    case 'H':
        if (zh_text_starts_with(text, hear_verbs))
            return shortf;
        break;
    case 'S':
        if (zh_text_starts_with(text, see_verbs))
            return shortf;
        break;
    default:
        break;
    }
    if (!((unsigned char) *text & 0x80) && *text != '%')
        return ""; /* plain untranslated English: keep the old output */
    return full;
}
#else
#define zh_you_prefix(text, full, shortf, verbs) (full)
#endif

void
You(const char *line, ...)
{
    va_list the_args;
    char *tmp;

    va_start(the_args, line);
#ifdef ZHLANG
    vpline(YouMessage(tmp, zh_you_prefix(line, "你", "你", 0), line),
           the_args);
#else
    vpline(YouMessage(tmp, "You ", line), the_args);
#endif
    va_end(the_args);
}

void
Your(const char *line, ...)
{
    va_list the_args;
    char *tmp;

    va_start(the_args, line);
#ifdef ZHLANG
    vpline(YouMessage(tmp, zh_you_prefix(line, "你的", "你", 0), line),
           the_args);
#else
    vpline(YouMessage(tmp, "Your ", line), the_args);
#endif
    va_end(the_args);
}

void
You_feel(const char *line, ...)
{
    va_list the_args;
    char *tmp;

    va_start(the_args, line);
#ifdef ZHLANG
    if (Unaware)
        YouPrefix(tmp, zh_you_prefix(line, "你梦见自己感到", "你梦见自己",
                                     'F'),
                      line);
    else
        YouPrefix(tmp, zh_you_prefix(line, "你感到", "你", 'F'), line);
#else
    if (Unaware)
        YouPrefix(tmp, "You dream that you feel ", line);
    else
        YouPrefix(tmp, "You feel ", line);
#endif
    vpline(strcat(tmp, line), the_args);
    va_end(the_args);
}

void
You_cant(const char *line, ...)
{
    va_list the_args;
    char *tmp;

    va_start(the_args, line);
#ifdef ZHLANG
    vpline(YouMessage(tmp, zh_you_prefix(line, "你不能", "你", 'C'), line),
           the_args);
#else
    vpline(YouMessage(tmp, "You can't ", line), the_args);
#endif
    va_end(the_args);
}

void
pline_The(const char *line, ...)
{
    va_list the_args;
    char *tmp;

    va_start(the_args, line);
#ifdef ZHLANG
    vpline(YouMessage(tmp, "", line), the_args);
#else
    vpline(YouMessage(tmp, "The ", line), the_args);
#endif
    va_end(the_args);
}

void
There(const char *line, ...)
{
    va_list the_args;
    char *tmp;

    va_start(the_args, line);
#ifdef ZHLANG
    vpline(YouMessage(tmp, "", line), the_args);
#else
    vpline(YouMessage(tmp, "There ", line), the_args);
#endif
    va_end(the_args);
}

void
You_hear(const char *line, ...)
{
    va_list the_args;
    char *tmp;

    if ((Deaf && !Unaware) || !flags.acoustics)
        return;
    va_start(the_args, line);
#ifdef ZHLANG
    if (Underwater)
        YouPrefix(tmp, zh_you_prefix(line, "你勉强听到", "你勉强", 'H'),
                      line);
    else if (Unaware)
        YouPrefix(tmp, zh_you_prefix(line, "你梦见自己听到", "你梦见自己",
                                     'H'),
                      line);
    else
        YouPrefix(tmp, zh_you_prefix(line, "你听到", "你", 'H'), line);
#else
    if (Underwater)
        YouPrefix(tmp, "You barely hear ", line);
    else if (Unaware)
        YouPrefix(tmp, "You dream that you hear ", line);
    else
        YouPrefix(tmp, "You hear ", line);  /* Deaf-aware */
#endif
    vpline(strcat(tmp, line), the_args);
    va_end(the_args);
}

void
You_see(const char *line, ...)
{
    va_list the_args;
    char *tmp;

    va_start(the_args, line);
#ifdef ZHLANG
    if (Unaware)
        YouPrefix(tmp, zh_you_prefix(line, "你梦见自己看见", "你梦见自己",
                                     'S'),
                      line);
    else if (Blind) /* caller should have caught this... */
        YouPrefix(tmp, zh_you_prefix(line, "你感觉到", "你", 'S'), line);
    else
        YouPrefix(tmp, zh_you_prefix(line, "你看见", "你", 'S'), line);
#else
    if (Unaware)
        YouPrefix(tmp, "You dream that you see ", line);
    else if (Blind) /* caller should have caught this... */
        YouPrefix(tmp, "You sense ", line);
    else
        YouPrefix(tmp, "You see ", line);
#endif
    vpline(strcat(tmp, line), the_args);
    va_end(the_args);
}

/* Print a message inside double-quotes.
 * The caller is responsible for checking deafness.
 * Gods can speak directly to you in spite of deafness.
 */
void
verbalize(const char *line, ...)
{
    va_list the_args;
    char *tmp;

    va_start(the_args, line);
    gp.pline_flags |= PLINE_VERBALIZE;
    tmp = You_buf((int) strlen(line) + sizeof "\"\"");
    Strcpy(tmp, "\"");
    Strcat(tmp, line);
    Strcat(tmp, "\"");
    vpline(tmp, the_args);
    gp.pline_flags &= ~PLINE_VERBALIZE;
    va_end(the_args);
}

#ifdef CHRONICLE

void
gamelog_add(long glflags, long gltime, const char *str)
{
    struct gamelog_line *tmp;
    struct gamelog_line *lst = gg.gamelog;

    tmp = (struct gamelog_line *) alloc(sizeof (struct gamelog_line));
    tmp->turn = gltime;
    tmp->flags = glflags;
    tmp->text = dupstr(str);
    tmp->next = NULL;
    while (lst && lst->next)
        lst = lst->next;
    if (!lst)
        gg.gamelog = tmp;
    else
        lst->next = tmp;
}

void
livelog_printf(long ll_type, const char *line, ...)
{
    char gamelogbuf[BUFSZ * 2];
    va_list the_args;

    va_start(the_args, line);
    (void) vsnprintf(gamelogbuf, sizeof gamelogbuf, line, the_args);
    va_end(the_args);

    gamelog_add(ll_type, svm.moves, gamelogbuf);
    strNsubst(gamelogbuf, "\t", "_", 0);
    livelog_add(ll_type, gamelogbuf);
}

#else

void
gamelog_add(
    long glflags UNUSED, long gltime UNUSED, const char *msg UNUSED)
{
    ; /* nothing here */
}

void
livelog_printf(
    long ll_type UNUSED, const char *line UNUSED, ...)
{
    ; /* nothing here */
}

#endif /* !CHRONICLE */

staticfn void vraw_printf(const char *, va_list);

void
raw_printf(const char *line, ...)
{
    va_list the_args;

    va_start(the_args, line);
    vraw_printf(line, the_args);
    va_end(the_args);
    if (!program_state.beyond_savefile_load)
        ge.early_raw_messages++;
}

DISABLE_WARNING_FORMAT_NONLITERAL

staticfn void
vraw_printf(const char *line, va_list the_args)
{
    char pbuf[BIGBUFSZ]; /* will be chopped down to BUFSZ-1 if longer */

    if (strchr(line, '%')) {
        (void) vsnprintf(pbuf, sizeof(pbuf), line, the_args);
        line = pbuf;
    }
    if ((int) strlen(line) > BUFSZ - 1) {
        if (line != pbuf)
            line = strncpy(pbuf, line, BUFSZ - 1);
        /* unlike pline, we don't futz around to keep last few chars */
        pbuf[BUFSZ - 1] = '\0'; /* terminate strncpy or truncate vsprintf */
    }
    raw_print(line);
    execplinehandler(line);
    if (!program_state.beyond_savefile_load)
        ge.early_raw_messages++;
}

void
impossible(const char *s, ...)
{
    va_list the_args;
    char pbuf[BIGBUFSZ]; /* will be chopped down to BUFSZ-1 if longer */
    char pbuf2[BUFSZ];

    va_start(the_args, s);
    if (program_state.in_impossible)
        panic("impossible called impossible");

    program_state.in_impossible = 1;
    (void) vsnprintf(pbuf, sizeof pbuf, s, the_args);
    va_end(the_args);
    pbuf[BUFSZ - 1] = '\0'; /* sanity */
    paniclog("impossible", pbuf);
    if (iflags.debug_fuzzer == fuzzer_impossible_panic)
        panic("%s", pbuf);

    gp.pline_flags = URGENT_MESSAGE;
    pline("%s", pbuf);
    gp.pline_flags = 0;

    if (program_state.in_sanity_check) {
        /* skip rest of multi-line feedback */
        program_state.in_impossible = 0;
        return;
    }

#ifdef ZHLANG
    Strcpy(pbuf2, "程序出错了！");
    if (program_state.something_worth_saving)
        Strcat(pbuf2, "（保存并重新加载可能可以解决此问题。）");
#else
    Strcpy(pbuf2, "Program in disorder!");
    if (program_state.something_worth_saving)
        Strcat(pbuf2, "  (Saving and reloading may fix this problem.)");
#endif
    pline("%s", pbuf2);
#ifdef ZHLANG
    pline("请向 %s 报告这些信息。", DEVTEAM_EMAIL);
#else
    pline("Please report these messages to %s.", DEVTEAM_EMAIL);
#endif
    if (sysopt.support) {
#ifdef ZHLANG
        pline("或者，联系本地支持：%s", sysopt.support);
#else
        pline("Alternatively, contact local support: %s", sysopt.support);
#endif
    }

#ifdef CRASHREPORT
    if (sysopt.crashreporturl) {
        boolean report = ('y' == yn_function(
#ifdef ZHLANG
                                              "立即报告？",
#else
                                              "Report now?",
#endif
                                              ynchars, 'n', FALSE));

        raw_print(""); /* prove to the user the character was accepted */
        if (report) {
            submit_web_report(1, "Impossible", pbuf);
        }
    }
#endif

    program_state.in_impossible = 0;
}

RESTORE_WARNING_FORMAT_NONLITERAL

static boolean use_pline_handler = TRUE;

staticfn void
execplinehandler(const char *line)
{
#if defined(UNIX) && (defined(POSIX_TYPES) || defined(__GNUC__))
    int f;
#endif
    const char *args[3];

    if (!use_pline_handler || !sysopt.msghandler)
        return;

#if defined(UNIX) && (defined(POSIX_TYPES) || defined(__GNUC__))
    f = fork();
    if (f == 0) { /* child */
        args[0] = sysopt.msghandler;
        args[1] = line;
        args[2] = NULL;
        (void) setgid(getgid());
        (void) setuid(getuid());
        (void) execv(args[0], (char *const *) args);
        perror((char *) 0);
        (void) fprintf(stderr, "Exec to message handler %s failed.\n", sysopt.msghandler);
        nh_terminate(EXIT_FAILURE);
    } else if (f > 0) {
        int status;

        waitpid(f, &status, 0);
    } else if (f == -1) {
        perror((char *) 0);
        use_pline_handler = FALSE;
#ifdef ZHLANG
        pline("%s", "创建消息处理程序进程失败。");
#else
        pline("%s", "Fork to message handler failed.");
#endif
    }
#elif defined(WIN32)
    {
        intptr_t ret;
        args[0] = sysopt.msghandler;
        args[1] = line;
        args[2] = NULL;
        ret = _spawnv(_P_NOWAIT, sysopt.msghandler, args);
        nhUse(ret);  /* -Wunused-but-set-variable */
    }
#else
    use_pline_handler = FALSE;
    nhUse(args);
    nhUse(line);
#endif
}

/* nhassert_failed is called when an nhassert's condition is false */
void
nhassert_failed(const char *expression, const char *filepath, int line)
{
    const char *filename, *p;

    /* Attempt to get filename from path.
       TODO: we really need a port provided function to return a filename
       from a path. */
    filename = filepath;
    if ((p = strrchr(filename, '/')) != 0)
        filename = p + 1;
    if ((p = strrchr(filename, '\\')) != 0)
        filename = p + 1;
#ifdef VMS
    /* usually "device:[directory]name"
       but might be "device:[root.][directory]name"
       and either "[directory]" or "[root.]" or both can be delimited
       by <> rather than by []; find the last of ']', '>', and ':'  */
    if ((p = strrchr(filename, ']')) != 0)
        filename = p + 1;
    if ((p = strrchr(filename, '>')) != 0)
        filename = p + 1;
    if ((p = strrchr(filename, ':')) != 0)
        filename = p + 1;
#endif

    impossible("nhassert(%s) failed in file '%s' at line %d",
               expression, filename, line);
}

/*pline.c*/
