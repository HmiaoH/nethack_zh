/* NetHack 5.0	utf8map.c	*/
/* Copyright (c) Michael Allison, 2021. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "utf8map.h"

/*
 * UTF-8 helpers, used by the tty port (and the Lua interface) to work
 * with display columns instead of bytes.  Kept outside of the
 * ENHANCED_SYMBOLS conditional because localized text needs them even
 * when the enhanced (UTF-8) symbols are not in use.
 */

/* Is 'cp' a wide (two terminal columns) character?
   Covers CJK ideographs, Hangul, kana and full-width forms. */
static boolean
u8_wide(uint32 cp)
{
    if (cp < 0x1100) /* fast path for Latin-1 and the like */
        return FALSE;
    if (cp <= 0x115F)                       /* Hangul Jamo */
        return TRUE;
    if (cp >= 0x2E80 && cp <= 0x303E)       /* CJK radicals, punctuation */
        return TRUE;
    if (cp >= 0x3041 && cp <= 0x33FF)       /* kana, CJK symbols */
        return TRUE;
    if (cp >= 0x3400 && cp <= 0x4DBF)       /* CJK ext A */
        return TRUE;
    if (cp >= 0x4E00 && cp <= 0x9FFF)       /* CJK unified ideographs */
        return TRUE;
    if (cp >= 0xA000 && cp <= 0xA4CF)       /* Yi */
        return TRUE;
    if (cp >= 0xA960 && cp <= 0xA97F)       /* Hangul Jamo ext A */
        return TRUE;
    if (cp >= 0xAC00 && cp <= 0xD7A3)       /* Hangul syllables */
        return TRUE;
    if (cp >= 0xF900 && cp <= 0xFAFF)       /* CJK compat ideographs */
        return TRUE;
    if (cp >= 0xFE10 && cp <= 0xFE19)       /* vertical forms */
        return TRUE;
    if (cp >= 0xFE30 && cp <= 0xFE6F)       /* CJK compat forms */
        return TRUE;
    if (cp >= 0xFF00 && cp <= 0xFF60)       /* full-width forms */
        return TRUE;
    if (cp >= 0xFFE0 && cp <= 0xFFE6)       /* full-width signs */
        return TRUE;
    if (cp >= 0x1B000 && cp <= 0x1B001)     /* kana supplement */
        return TRUE;
    if (cp >= 0x1F200 && cp <= 0x1F251)     /* enclosed ideographs */
        return TRUE;
    if (cp >= 0x20000 && cp <= 0x3FFFD)     /* CJK ext B and beyond */
        return TRUE;
    return FALSE;
}

/* Zero-width code points: combining marks, zero-width space/joiners
   and the byte order mark. */
static boolean
u8_zero_width(uint32 cp)
{
    if (cp >= 0x0300 && cp <= 0x036F)       /* combining diacritics */
        return TRUE;
    if (cp >= 0x1AB0 && cp <= 0x1AFF)       /* combining ext */
        return TRUE;
    if (cp >= 0x1DC0 && cp <= 0x1DFF)       /* combining supplement */
        return TRUE;
    if (cp >= 0x20D0 && cp <= 0x20FF)       /* combining for symbols */
        return TRUE;
    if (cp >= 0xFE00 && cp <= 0xFE0F)       /* variation selectors */
        return TRUE;
    if (cp >= 0xFE20 && cp <= 0xFE2F)       /* combining half marks */
        return TRUE;
    return (boolean) (cp == 0x200B || cp == 0x200C || cp == 0x200D
                      || cp == 0xFEFF);
}

/* Decode the UTF-8 sequence at 'str'.  Returns its length in bytes;
   *cp is set to the code point (or to the raw byte for invalid input). */
static int
u8_decode(const char *str, uint32 *cp)
{
    const unsigned char *p = (const unsigned char *) str;
    int len = u8_seqlen(str), i;
    uint32 val;

    if (len <= 1) {
        *cp = (uint32) *p;
        return len;
    }
    val = (uint32) (p[0] & ((len == 2) ? 0x1F
                            : (len == 3) ? 0x0F : 0x07));
    for (i = 1; i < len; ++i)
        val = (val << 6) | (uint32) (p[i] & 0x3F);
    *cp = val;
    return len;
}

/* Number of bytes in the UTF-8 sequence at the start of 'str'.
   Never returns more than 4 and always returns at least 1 for a
   non-empty string, so that a malformed byte cannot cause a caller
   to loop forever.  Returns 0 at the terminating NUL. */
int
u8_seqlen(const char *str)
{
    const unsigned char *p = (const unsigned char *) str;
    int len, i;

    if (!p || !*p)
        return 0;
    if (*p < 0x80)
        return 1;
    if ((*p & 0xE0) == 0xC0)
        len = 2;
    else if ((*p & 0xF0) == 0xE0)
        len = 3;
    else if ((*p & 0xF8) == 0xF0)
        len = 4;
    else
        return 1; /* stray continuation byte or invalid lead byte */
    for (i = 1; i < len; ++i)
        if (p[i] == '\0' || (p[i] & 0xC0) != 0x80)
            return 1; /* truncated or malformed sequence */
    return len;
}

/* Display width of a UTF-8 string, in terminal columns. */
int
u8_width(const char *str)
{
    const unsigned char *p = (const unsigned char *) str;
    int width = 0;

    if (!p)
        return 0;
    while (*p) {
        int len;
        uint32 cp;

        if (*p < 0x80) {
            len = 1;
            cp = (uint32) *p;
        } else {
            len = u8_decode((const char *) p, &cp);
        }
        if (!u8_zero_width(cp))
            width += u8_wide(cp) ? 2 : 1;
        p += len;
    }
    return width;
}

#ifdef ENHANCED_SYMBOLS

extern const struct symparse loadsyms[];
extern struct enum_dump monsdump[];
extern struct enum_dump objdump[];
extern glyph_map glyphmap[MAX_GLYPH];
extern const char *const known_handling[];        /* symbols.c */

/* hexdd[] is defined in decl.c */

int
unicode_val(const char *cp)
{
    const char *dp;
    int cval = 0, dcount;

    if (cp && *cp) {
        cval = dcount = 0;
        if ((*cp == 'U' || *cp == 'u')
            && cp[1] == '+' && cp[2] && (dp = strchr(hexdd, cp[2])) != 0) {
            cp += 2; /* move past the 'U' and '+' */
            do {
                cval = (cval * 16) + ((int) (dp - hexdd) / 2);
            } while (*++cp && (dp = strchr(hexdd, *cp)) != 0 && ++dcount < 7);
        }
    }
    return cval;
}

int
set_map_u(glyph_map *gmap, uint32 utf32ch, const uint8 *utf8str)
{
    glyph_map *tmpgm = gmap;

    if (!tmpgm || !utf32ch)
        return 0;

    if (gmap->u == 0) {
        gmap->u =
            (struct unicode_representation *) alloc(sizeof *gmap->u);
        gmap->u->utf8str = 0;
    }
    if (gmap->u->utf8str != 0) {
        free(gmap->u->utf8str);
        gmap->u->utf8str = 0;
    }
    gmap->u->utf8str = (uint8 *) dupstr((const char *) utf8str);
    gmap->u->utf32ch = utf32ch;
    return 1;
}

void
free_all_glyphmap_u(void)
{
    int glyph;
    int x, y;

    for (glyph = 0; glyph < MAX_GLYPH; ++glyph) {
        if (glyphmap[glyph].u) {
            if (glyphmap[glyph].u->utf8str) {
                free(glyphmap[glyph].u->utf8str);
                glyphmap[glyph].u->utf8str = 0;
            }
            free(glyphmap[glyph].u);
            glyphmap[glyph].u = 0;
        }
    }
    /* Prevent use after free from gg.gbuf */
    for (y = 0; y < ROWNO; ++y) {
        for (x = 0; x < COLNO; ++x) {
            gg.gbuf[y][x].glyphinfo.gm.u = NULL;
        }
    }
}

/* helper routine if a window port wants to embed any UTF-8 sequences
   for the glyph representation in the string in place of the \GNNNNNNNN
   reference */
char *
mixed_to_utf8(char *buf, size_t bufsz, const char *str, int *retflags)
{
    char *put = buf;
    glyph_info glyphinfo = nul_glyphinfo;

    if (!str)
        return strcpy(buf, "");

    while (*str && put < (buf + bufsz) - 1) {
        if (*str == '\\') {
            int dcount, so, ggv;
            const char *save_str;

            save_str = str++;
            switch (*str) {
            case 'G': /* glyph value \GXXXXNNNN*/
                if ((dcount = decode_glyph(str + 1, &ggv))) {
                    str += (dcount + 1);
                    map_glyphinfo(0, 0, ggv, 0, &glyphinfo);
                    if (glyphinfo.gm.u && glyphinfo.gm.u->utf8str) {
                        uint8 *ucp = glyphinfo.gm.u->utf8str;

                        while (*ucp && put < (buf + bufsz) - 1)
                            *put++ = *ucp++;
                        if (retflags)
                            *retflags = 1;
                    } else {
                        so = glyphinfo.gm.sym.symidx;
                        *put++ = gs.showsyms[so];
                        if (retflags)
                            *retflags = 0;
                    }
                    /* 'str' is ready for the next loop iteration and
                        '*str' should not be copied at the end of this
                        iteration */
                    continue;
                } else {
                    /* possible forgery - leave it the way it is */
                    str = save_str;
                }
                break;
            case '\\':
                break;
            case '\0':
                /* String ended with '\\'.  This can happen when someone
                    names an object with a name ending with '\\', drops the
                    named object on the floor nearby and does a look at all
                    nearby objects. */
                /* brh - should we perhaps not allow things to have names
                    that contain '\\' */
                str = save_str;
                break;
            }
        }
        if (put < (buf + bufsz) - 1)
            *put++ = *str++;
    }
    *put = '\0';
    return buf;
}

int
add_custom_urep_entry(
    const char *customization_name,
    int glyphidx,
    uint32 utf32ch,
    const uint8 *utf8str,
    enum graphics_sets which_set)
{
    struct symset_customization *gdc
        = &gs.sym_customizations[which_set][custom_ureps];
    struct customization_detail *details, *newdetails = 0;


    if (!gdc->details) {
        gdc->customization_name = dupstr(customization_name);
        gdc->custtype = custom_ureps;
        gdc->details = 0;
        gdc->details_end = 0;
    }
    details = find_matching_customization(customization_name,
                                          custom_ureps, which_set); /* FIXME */
    if (details) {
        while (details) {
            if (details->content.urep.glyphidx == glyphidx) {
                if (details->content.urep.u.utf8str)
                    free(details->content.urep.u.utf8str);
                if (utf32ch) {
                    details->content.urep.u.utf8str =
                        (uint8 *) dupstr((const char *) utf8str);
                    details->content.urep.u.utf32ch = utf32ch;
                } else {
                    details->content.urep.u.utf8str = (uint8 *) 0;
                    details->content.urep.u.utf32ch = 0;
                }
                return 1;
            }
            details = details->next;
        }
    }
    /* create new details entry */
    newdetails = (struct customization_detail *) alloc(
                                        sizeof (struct customization_detail));
    newdetails->content.urep.glyphidx = glyphidx;
    if (utf8str && *utf8str) {
        newdetails->content.urep.u.utf8str =
            (uint8 *) dupstr((const char *) utf8str);
    } else {
        newdetails->content.urep.u.utf8str =
            (uint8 *) 0;
    }
    newdetails->content.urep.u.utf32ch = utf32ch;
    newdetails->next = (struct customization_detail *) 0;
    if (gdc->details == NULL) {
        gdc->details = newdetails;
    } else {
        gdc->details_end->next = newdetails;
    }
    gdc->details_end = newdetails;
    gdc->count++;
    return 1;
}
#endif /* ENHANCED_SYMBOLS */

void
reset_customsymbols(void)
{
#ifdef ENHANCED_SYMBOLS
    free_all_glyphmap_u();
    apply_customizations(gc.currentgraphics, do_custom_symbols);
#endif
}

/* utf8map.c */



