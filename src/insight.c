/* NetHack 5.0	insight.c	$NHDT-Date: 1781973051 2026/06/20 16:30:51 $  $NHDT-Branch: NetHack-5.0 $:$NHDT-Revision: 1.139 $ */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

/*
 * Enlightenment and Conduct+Achievements and Vanquished+Extinct+Geno'd
 * and stethoscope/probing feedback.
 *
 * Most code used to reside in cmd.c, presumably because ^X was originally
 * a wizard mode command and the majority of those are in that file.
 * Some came from end.c where it is used during end of game disclosure.
 * And some came from priest.c that had once been in pline.c.
 */

#include "hack.h"

staticfn void enlght_out(const char *);
staticfn void enlght_line(const char *, const char *, const char *,
                          const char *);
staticfn char *enlght_combatinc(const char *, int, int, char *);
staticfn void enlght_halfdmg(int, int);
staticfn boolean walking_on_water(void);
staticfn boolean cause_known(int);
staticfn char *attrval(int, int, char *);
staticfn char *fmt_elapsed_time(char *, int);
staticfn char *N_times(long, char *) NONNULL NONNULLARG2;
staticfn void background_enlightenment(int, int);
staticfn void basics_enlightenment(int, int);
staticfn void characteristics_enlightenment(int, int);
staticfn void one_characteristic(int, int, int);
staticfn void status_enlightenment(int, int);
staticfn void weapon_insight(int);
staticfn void attributes_enlightenment(int, int);
staticfn void show_achievements(int);
staticfn int QSORTCALLBACK vanqsort_cmp(const genericptr, const genericptr);
staticfn int num_extinct(void);
staticfn int num_gone(int, int *);
staticfn char *size_str(int);
staticfn void item_resistance_message(int, const char *, int);

extern const char *const hu_stat[];  /* hunger status from eat.c */
extern const char *const enc_stat[]; /* encumbrance status from botl.c */

static const char You_[] = "You ", are[] = "are ", were[] = "were ",
                  have[] = "have ", had[] = "had ", can[] = "can ",
                  could[] = "could ", have_been[] = "have been ";
#ifndef ZHLANG
static const char have_never[] = "have never ", never[] = "never ";
#endif

/* for livelogging: */
struct ll_achieve_msg {
    long llflag;
    const char *msg;
};
/* ordered per 'enum achievements' in you.h */
/* take care to keep them in sync! */
static struct ll_achieve_msg achieve_msg [] = {
    { 0, "" }, /* actual achievements are numbered from 1 */
    { LL_ACHIEVE, "acquired the Bell of Opening" },
    { LL_ACHIEVE, "entered Gehennom" },
    { LL_ACHIEVE, "acquired the Candelabrum of Invocation" },
    { LL_ACHIEVE, "acquired the Book of the Dead" },
    { LL_ACHIEVE, "performed the invocation" },
    { LL_ACHIEVE, "acquired The Amulet of Yendor" },
    { LL_ACHIEVE, "entered the Elemental Planes" },
    { LL_ACHIEVE, "entered the Astral Plane" },
    { LL_ACHIEVE, "ascended" },
    /* if the type of item isn't discovered yet, disclosing the event
       via #chronicle would be a spoiler (particularly for gray stone);
       the ID'd name for the type of item will be appended to the next
       two messages, for display via livelog and/or dumplog */
    { LL_ACHIEVE | LL_SPOILER, "acquired the Mines' End" }, /* " luckstone" */
    { LL_ACHIEVE | LL_SPOILER, "acquired the Sokoban" }, /* " <item>" */
    { LL_ACHIEVE | LL_UMONST, "killed Medusa" },
     /* these two are not logged */
    { 0, "hero was always blond, no, blind" },
    { 0, "hero never wore armor" },
     /* */
    { LL_MINORAC | LL_DUMP, "entered the Gnomish Mines" },
    { LL_ACHIEVE, "reached Mine Town" }, /* probably minor, but dnh logs it */
    { LL_MINORAC, "entered a shop" },
    { LL_MINORAC, "entered a temple" },
    { LL_ACHIEVE, "consulted the Oracle" }, /* minor, but rare enough */
    { LL_MINORAC | LL_DUMP, "read a Discworld novel" }, /* even more so */
    { LL_ACHIEVE, "entered Sokoban" }, /* keep as major for turn comparison
                                        * with completed sokoban */
    { LL_ACHIEVE, "entered the Bigroom" },
    /* The following 8 are for advancing through the ranks
       and messages differ by role so are created on the fly;
       rank 0 (Xp 1 and 2) isn't an achievement */
    { LL_MINORAC | LL_DUMP, "" }, /* Xp 3 */
    { LL_MINORAC | LL_DUMP, "" }, /* Xp 6 */
    { LL_MINORAC | LL_DUMP, "" }, /* Xp 10 */
    { LL_ACHIEVE, "" }, /* Xp 14, so able to attempt the quest */
    { LL_ACHIEVE, "" }, /* Xp 18 */
    { LL_ACHIEVE, "" }, /* Xp 22 */
    { LL_ACHIEVE, "" }, /* Xp 26 */
    { LL_ACHIEVE, "" }, /* Xp 30 */
    { LL_MINORAC, "learned castle drawbridge's tune" }, /* achievement #31 */
    { 0, "" } /* keep this one at the end */
};

/* macros to simplify output of enlightenment messages; also used by
   conduct and achievements */
#define enl_msg(prefix, present, past, suffix, ps) \
    enlght_line((prefix), final ? (past) : (present), (suffix), (ps))
#define you_are(attr, ps) enl_msg(You_, are, were, (attr), (ps))
#define you_have(attr, ps) enl_msg(You_, have, had, (attr), (ps))
#define you_can(attr, ps) enl_msg(You_, can, could, (attr), (ps))
#define you_have_been(goodthing) \
    enl_msg(You_, have_been, were, (goodthing), "")
#define you_have_never(badthing) \
    enl_msg(You_, have_never, never, (badthing), "")
#define you_have_X(something) \
    enl_msg(You_, have, (const char *) "", (something), "")

staticfn void
enlght_out(const char *buf)
{
    if (ge.en_via_menu) {
        add_menu_str(ge.en_win, buf);
    } else
        putstr(ge.en_win, 0, buf);
}

#ifdef ZHLANG
/* ---------------------------------------------------------------------
 * Chinese helpers for the enlightenment / conduct output.
 *
 * The English output is assembled from fragments by enlght_line(); its
 * skeleton ("You are ", "Your strength is ", ...) doesn't survive
 * translation, so the Chinese version builds a whole sentence instead:
 * a subject, an optional Chinese verb, the already translated predicate
 * and an optional Chinese parenthetical, ending with a full stop.
 * All of these helpers are no-ops for the English build.
 * --------------------------------------------------------------------- */
staticfn const char *
zh_trim(const char *s)
{
    if (!s)
        return "";
    while (*s == ' ')
        ++s;
    return s;
}

/* English auxiliary/verb fragment used by enlght_line() -> Chinese */
staticfn const char *
zh_enl_verb(const char *middle)
{
    static const struct {
        const char *en, *zh;
    } verbs[] = {
        { "are ", "" },        { "were ", "曾" },
        { "have ", "有" },     { "had ", "曾有" },
        { "is ", "是" },       { "was ", "曾是" },
        { "is", "是" },        { "was", "曾是" },
        { "can ", "能" },      { "could ", "能" },
        { "have been ", "一直" }, { "have never ", "从未" },
        { "never ", "从未" },  { "take", "受到" },
        { "took", "受到" },    { "entered ", "进入了" },
        { "will be", "将会被" }, { "would have been", "本会被" },
        { 0, 0 }
    };
    int i;

    for (i = 0; verbs[i].en; ++i)
        if (!strcmp(middle, verbs[i].en))
            return verbs[i].zh;
    if ((uchar) *zh_trim(middle) & 0x80) /* already Chinese */
        return zh_trim(middle);
    return ""; /* unknown English fragment: leave it to the predicate */
}

/* small English words/phrases that show up inside the sentences */
staticfn const char *
zh_word(const char *en)
{
    static const struct {
        const char *en, *zh;
    } words[] = {
        { "arm", "手臂" },          { "eye", "眼睛" },
        { "face", "脸" },           { "finger", "手指" },
        { "fingers", "手指" },      { "fingertip", "指尖" },
        { "foot", "脚" },           { "hand", "手" },
        { "head", "头" },           { "leg", "腿" },
        { "legs", "腿" },           { "neck", "脖子" },
        { "spine", "脊背" },        { "toe", "脚趾" },
        { "hair", "头发" },         { "blood", "血" },
        { "lung", "肺" },           { "nose", "鼻子" },
        { "stomach", "胃" },        { "left", "左" },
        { "right", "右" },          { "more", "更" },
        { "less", "不那么" },       { "extremely", "极其" },
        { "very", "非常" },         { "slightly", "略微" },
        { "moderately", "中等" },   { "not possible", "无法" },
        { "permanently", "永久性" }, { "innately", "先天" },
        { "deliberately", "故意" }, { "temporarily", "暂时" },
        { "normally", "通常" },     { "orcs", "兽人" },
        { "elves", "精灵" },        { "demons", "恶魔" },
        { "humans", "人类" },       { "humans and elves", "人类和精灵" },
        { "certain monsters", "某些怪物" },
        { "on", "开启" },           { "off", "关闭" },
        { "all types", "所有类型" }, { "plus thrown", "外加投掷出的物品" },
        { "best", "最佳" },         { "worst", "最差" },
        { "no", "没有" },           { "both", "两者" },
        { "none", "无" },           { "two weapons", "双持武器" },
        { "to hit", "命中" },       { "damage", "伤害" },
        { "defense", "防御" },      { "small", "少量" },
        { "moderate", "中等" },     { "large", "大量" },
        { "huge", "巨大" },         { "bonus", "加成" },
        { "penalty", "惩罚" },      { "physical", "物理" },
        { "spell", "法术" },        { "half", "一半" },
        { "reduced", "减少的" },    { "swallowed", "吞下" },
        { "engulfed", "包住" },     { "holding", "抓住" },
        { "held by", "被…抓住" },   { "saved", "被拯救" },
        { 0, 0 }
    };
    int i;

    for (i = 0; words[i].en; ++i)
        if (!strcmp(en, words[i].en))
            return words[i].zh;
    return en;
}

/* attribute names (attrname[] in attrib.c stays English) */
staticfn const char *
zh_attr_name(int attrindx)
{
    static const char *const names[6] = {
        "力量", "智力", "感知", "敏捷", "体质", "魅力"
    };

    if (attrindx >= 0 && attrindx < 6)
        return names[attrindx];
    return "属性";
}

/* race names (races[] in role.c stays English) */
staticfn const char *
zh_race_word(const char *en)
{
    static const struct {
        const char *en, *zh;
    } race_names[] = {
        { "human", "人类" }, { "elf", "精灵" }, { "dwarf", "矮人" },
        { "gnome", "侏儒" }, { "orc", "兽人" }, { 0, 0 }
    };
    int i;

    for (i = 0; race_names[i].en; ++i)
        if (!strcmpi(en, race_names[i].en))
            return race_names[i].zh;
    return en;
}

staticfn const char *
zh_gender_word(int gendnum)
{
    static const char *const genders_zh[] = {
        "男性", "女性", "中性", "群体"
    };

    if (gendnum >= 0 && gendnum < (int) SIZE(genders_zh))
        return genders_zh[gendnum];
    return "";
}

/* encumbrance status words (enc_stat[] in botl.c stays English) */
staticfn const char *
zh_enc_status(int cap)
{
    static const char *const states[] = {
        "", "负重", "紧张", "吃力", "超负荷", "压倒"
    };

    if (cap >= 0 && cap < (int) SIZE(states))
        return states[cap];
    return "";
}

/* skill level names (skill_level_name() in weapon.c stays English) */
staticfn const char *
zh_skill_level(int skill)
{
    switch (P_SKILL(skill)) {
    case P_UNSKILLED:
        return "未入门";
    case P_BASIC:
        return "基本";
    case P_SKILLED:
        return "熟练";
    case P_EXPERT:
        return "专家";
    case P_MASTER:
        return "大师";
    case P_GRAND_MASTER:
        return "宗师";
    default:
        return "受限";
    }
}

/* format a complete Chinese line through the enlightenment window */
DISABLE_WARNING_FORMAT_NONLITERAL
staticfn void
enlght_zh(const char *fmt, ...)
{
    char buf[BUFSZ];
    va_list ap;

    va_start(ap, fmt);
    (void) vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    enlght_out(buf);
}
RESTORE_WARNING_FORMAT_NONLITERAL
#endif /* ZHLANG */

staticfn void
enlght_line(
    const char *start,
    const char *middle,
    const char *end,
    const char *ps)
{
#ifdef ZHLANG
    char buf[BUFSZ];
    const char *head = zh_trim(start), *verb = zh_enl_verb(middle),
               *body = zh_trim(end), *tail = zh_trim(ps);

    /* English subject fragments get a Chinese subject */
    if (!strcmp(start, You_))
        head = "你";
    else if (!strcmp(start, "It "))
        head = "现在";
    else if (!strcmp(start, "There "))
        head = "当前";
    Snprintf(buf, sizeof buf, "%s%s%s%s。", head, verb, body, tail);
    enlght_out(buf);
#else
#ifndef NO_ENLGHT_CONTRACTIONS
    static const struct contrctn {
        const char *twowords, *contrctn;
    } contra[] = {
        { " are not ", " aren't " },
        { " were not ", " weren't " },
        { " have not ", " haven't " },
        { " had not ", " hadn't " },
        { " can not ", " can't " },
        { " could not ", " couldn't " },
    };
    int i;
#endif
    char buf[BUFSZ];

    Sprintf(buf, " %s%s%s%s.", start, middle, end, ps);
#ifndef NO_ENLGHT_CONTRACTIONS
    if (strstri(buf, " not ")) { /* TODO: switch to libc strstr() */
        for (i = 0; i < SIZE(contra); ++i)
            (void) strsubst(buf, contra[i].twowords, contra[i].contrctn);
    }
#endif
    enlght_out(buf);
#endif /* ZHLANG */
}

/* format increased chance to hit or damage or defense (Protection) */
staticfn char *
enlght_combatinc(
    const char *inctyp, /* "to hit" or "damage" or "defense" */
    int incamt,         /* amount of increment (negative if decrement) */
    int final,          /* ENL_{GAMEINPROGRESS,GAMEOVERALIVE,GAMEOVERDEAD} */
    char *outbuf)
{
    const char *modif, *bonus;
    boolean invrt;
    int absamt;

    absamt = abs(incamt);
    /* Protection amount is typically larger than damage or to-hit;
       reduce magnitude by a third in order to stretch modifier ranges
       (small:1..5, moderate:6..10, large:11..19, huge:20+) */
    if (!strcmp(inctyp, "defense"))
        absamt = (absamt * 2) / 3;

    if (absamt <= 3)
        modif = "small";
    else if (absamt <= 6)
        modif = "moderate";
    else if (absamt <= 12)
        modif = "large";
    else
        modif = "huge";

    modif = !incamt ? "no" : an(modif); /* ("no" case shouldn't happen) */
    bonus = (incamt >= 0) ? "bonus" : "penalty";
    /* "bonus <foo>" (to hit) vs "<bar> bonus" (damage, defense) */
    invrt = strcmp(inctyp, "to hit") ? TRUE : FALSE;

#ifdef ZHLANG
    {
        /* the English fragments above are translated here; the caller
           supplies a Chinese inctyp ("伤害"/"防御"/"命中") */
        const char *modif_zh = (absamt <= 3) ? "少量"
                              : (absamt <= 6) ? "中等"
                                : (absamt <= 12) ? "大量" : "巨大";
        const char *bonus_zh = (incamt >= 0) ? "加成" : "惩罚";

        if (!incamt)
            modif_zh = "没有";
        Sprintf(outbuf, "%s%s%s", modif_zh, zh_word(inctyp), bonus_zh);
        if (final || wizard)
            Sprintf(eos(outbuf), "（%s%d）", (incamt > 0) ? "+" : "", incamt);
    }
#else
    Sprintf(outbuf, "%s %s %s", modif, invrt ? inctyp : bonus,
            invrt ? bonus : inctyp);
    if (final || wizard)
        Sprintf(eos(outbuf), " (%s%d)", (incamt > 0) ? "+" : "", incamt);
#endif
    nhUse(modif);
    nhUse(bonus);
    nhUse(invrt);

    return outbuf;
}

/* report half physical or half spell damage */
staticfn void
enlght_halfdmg(int category, int final)
{
    const char *category_name;
#ifndef ZHLANG
    char buf[BUFSZ];
#endif

    switch (category) {
    case HALF_PHDAM:
        category_name = "physical";
        break;
    case HALF_SPDAM:
        category_name = "spell";
        break;
    default:
        category_name = "unknown";
        break;
    }
#ifdef ZHLANG
    enlght_zh("你受到%s%s伤害%s。", (final || wizard) ? "一半" : "减少的",
              zh_word(category_name), zh_trim(from_what(category)));
#else
    Sprintf(buf, TR(" %s %s damage", " %s %s 伤害"), (final || wizard) ? "half" : "reduced",
            category_name);
    enl_msg(You_, TR("take", "受到"), "took", buf, from_what(category));
#endif
}

/* is hero actively using water walking capability on water (or lava)? */
staticfn boolean
walking_on_water(void)
{
    if (u.uinwater || Levitation || Flying)
        return FALSE;
    return (boolean) (Wwalking && is_pool_or_lava(u.ux, u.uy));
}

/* describe u.utraptype; used by status_enlightenment() and self_lookat() */
char *
trap_predicament(char *outbuf, int final, boolean wizxtra)
{
    struct trap *t;

    /* caller has verified u.utrap */
    *outbuf = '\0';
    switch (u.utraptype) {
    case TT_BURIEDBALL:
        Strcpy(outbuf, TR("tethered to something buried", "被拴在埋在地下的东西上"));
        break;
    case TT_LAVA:
        Sprintf(outbuf, TR("sinking into %s", "正陷入%s"), final ? TR("lava", "岩浆") : hliquid(TR("lava", "岩浆")));
        break;
    case TT_INFLOOR:
        Sprintf(outbuf, TR("stuck in %s", "卡在%s里"), the(surface(u.ux, u.uy)));
        break;
    default: /* TT_BEARTRAP, TT_PIT, or TT_WEB */
        Strcpy(outbuf, TR("trapped", "被困住"));
        if ((t = t_at(u.ux, u.uy)) != 0) /* should never be null */
            Sprintf(eos(outbuf), " in %s", an(trapname(t->ttyp, FALSE)));
        break;
    }
    if (wizxtra) { /* give extra information for wizard mode enlightenment */
        /* curly braces: u.utrap is an escape attempt counter rather than a
           turn timer so use different ornamentation than usual parentheses */
        Sprintf(eos(outbuf), " {%u}", u.utrap);
    }
    return outbuf;
}

/* check whether hero is wearing something that player definitely knows
   confers the target property; item must have been seen and its type
   discovered but it doesn't necessarily have to be fully identified */
staticfn boolean
cause_known(
    int propindx) /* index of a property which can be conveyed by worn item */
{
    struct obj *o;
    long mask = W_ARMOR | W_AMUL | W_RING | W_TOOL;

    /* simpler than from_what()/what_gives(); we don't attempt to
       handle artifacts and we deliberately ignore wielded items */
    for (o = gi.invent; o; o = o->nobj) {
        if (!(o->owornmask & mask))
            continue;
        if ((int) objects[o->otyp].oc_oprop == propindx
            && objects[o->otyp].oc_name_known && o->dknown)
            return TRUE;
    }
    return FALSE;
}

/* format a characteristic value, accommodating Strength's strangeness */
staticfn char *
attrval(
    int attrindx,
    int attrvalue,
    char resultbuf[]) /* should be at least [7] to hold "18/100\0" */
{
    if (attrindx != A_STR || attrvalue <= 18)
        Sprintf(resultbuf, "%d", attrvalue);
    else if (attrvalue > STR18(100)) /* 19 to 25 */
        Sprintf(resultbuf, "%d", attrvalue - 100);
    else /* simplify "18/\**" to be "18/100" */
        Sprintf(resultbuf, "18/%02d", attrvalue - 18);
    return resultbuf;
}

/* format urealtime.realtime as
      " D days, H hours, M minutes and S seconds"
   with any fields having a value of 0 omitted:
      0-00:00:20 => " 20 seconds"
      0-00:15:05 => " 15 minutes and 5 seconds"
      0-00:16:00 => " 16 minutes"
      0-01:15:10 => " 1 hour, 15 minutes and 10 seconds"
      0-02:00:01 => " 2 hours and 1 second"
      3-00:25:40 => " 3 days, 25 minutes and 40 seconds"
   (note: for a list of more than two entries, nethack usually includes the
   [style-wise] optional comma before "and" but in this instance it does not)
 */
staticfn char *
fmt_elapsed_time(char *outbuf, int final)
{
    int fieldcnt;
    long edays, ehours, eminutes, eseconds;
    /* for a game that's over, reallydone() has updated urealtime.realtime
       to its final value before calling us during end of game disclosure;
       for a game that's still in progress, it holds the amount of elapsed
       game time from previous sessions up through most recent save/restore
       (or up through latest level change when 'checkpoint' is On);
       '.start_timing' has a non-zero value even if '.realtime' is 0 */
    long etim = urealtime.realtime;

    if (!final)
        etim += timet_delta(getnow(), urealtime.start_timing);
    /* we could use localtime() to convert the value into a 'struct tm'
       to get date and time fields but this is simple and straightforward */
    eseconds = etim % 60L, etim /= 60L;
    eminutes = etim % 60L, etim /= 60L;
    ehours = etim % 24L;
    edays = etim / 24L;
    fieldcnt = !!edays + !!ehours + !!eminutes + !!eseconds;

    Strcpy(outbuf, fieldcnt ? "" : TR(" none", " 无")); /* 'none' should never happen */
    if (edays) {
        Sprintf(eos(outbuf), TR(" %ld day%s", " %ld 天%s"), edays, plur(edays));
        if (fieldcnt > 1) /* hours and/or minutes and/or seconds to follow */
            Strcat(outbuf, (fieldcnt == 2) ? TR(" and", " 和") : ",");
        --fieldcnt; /* edays has been processed */
    }
    if (ehours) {
        Sprintf(eos(outbuf), TR(" %ld hour%s", " %ld 小时%s"), ehours, plur(ehours));
        if (fieldcnt > 1) /* minutes and/or seconds to follow */
            Strcat(outbuf, (fieldcnt == 2) ? TR(" and", " 和") : ",");
        --fieldcnt; /* ehours has been processed */
    }
    if (eminutes) {
        Sprintf(eos(outbuf), TR(" %ld minute%s", " %ld 分钟%s"), eminutes, plur(eminutes));
        if (fieldcnt > 1) /* seconds to follow */
            Strcat(outbuf, TR(" and", " 和"));
        /* eminutes has been processed but no need to decrement fieldcnt */
    }
    if (eseconds)
        Sprintf(eos(outbuf), TR(" %ld second%s", " %ld 秒%s"), eseconds, plur(eseconds));
    return outbuf;
}

/* "once" vs "twice" vs "17 times", used in several places */
staticfn char *
N_times(long n, char *outbuf)
{
    switch (n) {
    case 0:
    default:
        Sprintf(outbuf, TR("%ld times", "%ld 次"), n);
        break;
    case 1:
        Strcpy(outbuf, TR("once", "一次"));
        break;
    case 2:
        Strcpy(outbuf, TR("twice", "两次"));
        break;
    case 3:
        Strcpy(outbuf, TR("thrice", "三次"));
        break;
    }
    return outbuf;
}

void
enlightenment(
    int mode,  /* BASICENLIGHTENMENT | MAGICENLIGHTENMENT (| both) */
    int final) /* ENL_GAMEINPROGRESS:0, ENL_GAMEOVERALIVE, ENL_GAMEOVERDEAD */
{
    char buf[BUFSZ], tmpbuf[BUFSZ];

    ge.en_win = create_nhwindow(NHW_MENU);
    ge.en_via_menu = !final;
    if (ge.en_via_menu)
        start_menu(ge.en_win, MENU_BEHAVE_STANDARD);

    Strcpy(tmpbuf, svp.plname);
    *tmpbuf = highc(*tmpbuf); /* same adjustment as bottom line */
    /* as in background_enlightenment, when poly'd we need to use the saved
       gender in u.mfemale rather than the current you-as-monster gender */
    Snprintf(buf, sizeof(buf), TR("%s the %s's attributes:", "%s（%s）的属性："), tmpbuf,
             ((Upolyd ? u.mfemale : flags.female) && gu.urole.name.f)
                ? gu.urole.name.f
                : gu.urole.name.m);

    /* title */
    enlght_out(buf); /* "Conan the Archeologist's attributes:" */
    /* background and characteristics; ^X or end-of-game disclosure */
    if (mode & BASICENLIGHTENMENT) {
        /* role, race, alignment, deities, dungeon level, time, experience */
        background_enlightenment(mode, final);
        /* hit points, energy points, armor class, gold */
        basics_enlightenment(mode, final);
        /* strength, dexterity, &c */
        characteristics_enlightenment(mode, final);
    }
    /* expanded status line information, including things which aren't
       included there due to space considerations;
       shown for both basic and magic enlightenment */
    status_enlightenment(mode, final);
    /* remaining attributes; shown for potion,&c or wizard mode and
       explore mode ^X or end of game disclosure */
    if (mode & MAGICENLIGHTENMENT) {
        /* intrinsics and other traditional enlightenment feedback */
        attributes_enlightenment(mode, final);
    }

    enlght_out(""); /* separator */
    enlght_out(TR("Miscellaneous:", "杂项："));
    /* reminder to player and/or information for dumplog */
    if ((mode & BASICENLIGHTENMENT) != 0 && (wizard || discover || final)) {
        if (wizard || discover) {
            Sprintf(buf, TR("running in %s mode", "正以%s模式运行"),
                    wizard ? TR("debug", "调试") : TR("explore", "探索"));
            you_are(buf, "");
        }

        if (!flags.bones) {
            /* mention not saving bones iff hero just died */
            Sprintf(buf, TR("disabled loading%s of bones levels", "已禁用遗骨关卡的加载%s"),
                    (final == ENL_GAMEOVERDEAD) ? TR(" and storing", "以及保存") : "");
            you_have_X(buf);
        } else if (!u.uroleplay.numbones) {
            enl_msg(You_, TR("haven't encountered", "未遇到过"), TR("didn't encounter", "未遇到过"),
                    TR(" any bones levels", "任何遗骨关卡"), "");
        } else {
            Sprintf(buf, TR("encountered %ld bones level%s", "遇到过 %ld 个遗骨关卡%s"),
                    u.uroleplay.numbones, plur(u.uroleplay.numbones));
            you_have_X(buf);
        }
    }
    (void) fmt_elapsed_time(buf, final);
    enl_msg(TR("Total elapsed playing time ", "总游戏时长"), "is", TR("was", "曾是"), buf, "");

    if (!ge.en_via_menu) {
        display_nhwindow(ge.en_win, TRUE);
    } else {
        menu_item *selected = 0;

        end_menu(ge.en_win, (char *) 0);
        if (select_menu(ge.en_win, PICK_NONE, &selected) > 0)
            free((genericptr_t) selected);
        ge.en_via_menu = FALSE;
    }
    destroy_nhwindow(ge.en_win);
    ge.en_win = WIN_ERR;
}

/*ARGSUSED*/
/* display role, race, alignment and such to en_win */
staticfn void
background_enlightenment(int unused_mode UNUSED, int final)
{
    const char *role_titl, *rank_titl;
    int innategend, difgend, difalgn;
    char buf[BUFSZ], tmpbuf[BUFSZ];

    /* note that if poly'd, we need to use u.mfemale instead of flags.female
       to access hero's saved gender-as-human/elf/&c rather than current */
    innategend = (Upolyd ? u.mfemale : flags.female) ? 1 : 0;
    role_titl = (innategend && gu.urole.name.f) ? gu.urole.name.f
                                                : gu.urole.name.m;
    rank_titl = rank_of(u.ulevel, Role_switch, innategend);

    enlght_out(""); /* separator after title */
    enlght_out(TR("Background:", "背景："));

    /* if polymorphed, report current shape before underlying role;
       will be repeated as first status: "you are transformed" and also
       among various attributes: "you are in beast form" (after being
       told about lycanthropy) or "you are polymorphed into <a foo>"
       (with countdown timer appended for wizard mode); we really want
       the player to know he's not a samurai at the moment... */
    if (Upolyd) {
        struct permonst *uasmon = gy.youmonst.data;
        boolean altphrasing = vampshifted(&gy.youmonst);
#ifndef ZHLANG
        char anbuf[20]; /* includes trailing space; [4] suffices */
#endif

        tmpbuf[0] = '\0';
        /* here we always use current gender, not saved role gender */
        if (!is_male(uasmon) && !is_female(uasmon) && !is_neuter(uasmon))
            Sprintf(tmpbuf, "%s ", genders[flags.female ? 1 : 0].adj);
        if (altphrasing)
            Sprintf(eos(tmpbuf), "%s in ",
                    pmname(&mons[gy.youmonst.cham],
                           flags.female ? FEMALE : MALE));
#ifdef ZHLANG
        /* "你目前处于<怪物>形态。" */
        if (altphrasing)
            Sprintf(buf, "目前处于%s，%s形态",
                    pmname(&mons[gy.youmonst.cham],
                           flags.female ? FEMALE : MALE),
                    pmname(uasmon, flags.female ? FEMALE : MALE));
        else
            Sprintf(buf, "目前处于%s形态",
                    pmname(uasmon, flags.female ? FEMALE : MALE));
        you_are(buf, "");
#else
        Snprintf(buf, sizeof(buf), TR("%s%s%s%s form", "%s%s%s%s形态"),
                 !final ? "currently " : "",
                 altphrasing ? just_an(anbuf, tmpbuf) : "in ",
                 tmpbuf, pmname(uasmon, flags.female ? FEMALE : MALE));
        you_are(buf, "");
#endif
    }

    /* report role; omit gender if it's redundant (eg, "female priestess") */
    tmpbuf[0] = '\0';
    if (!gu.urole.name.f
        && ((gu.urole.allow & ROLE_GENDMASK) == (ROLE_MALE | ROLE_FEMALE)
            || innategend != flags.initgend))
#ifdef ZHLANG
        Sprintf(tmpbuf, "%s", zh_gender_word(innategend));
#else
        Sprintf(tmpbuf, "%s ", genders[innategend].adj);
#endif
    buf[0] = '\0';
    if (!strcmpi(rank_titl, role_titl)) {
        /* omit role when rank title matches it */
#ifdef ZHLANG
        Sprintf(eos(buf), "%s，%d 级%s%s", rank_titl, u.ulevel, tmpbuf,
                zh_race_word(gu.urace.noun));
#else
        Sprintf(eos(buf), TR("%s, level %d %s%s", "%s，%d 级%s%s"), an(rank_titl), u.ulevel,
                tmpbuf, gu.urace.noun);
#endif
    } else {
#ifdef ZHLANG
        Sprintf(eos(buf), "%s，%d 级的%s%s%s", rank_titl, u.ulevel, tmpbuf,
                zh_race_word(gu.urace.adj), role_titl);
#else
        Sprintf(eos(buf), TR("%s, a level %d %s%s %s", "%s，%d 级的%s%s%s"), an(rank_titl), u.ulevel,
                tmpbuf, gu.urace.adj, role_titl);
#endif
    }
#ifdef ZHLANG
    /* the "actually" wording is about a former shape (see above) */
    enlght_zh("你%s是%s。", Upolyd ? "实际上" : "", buf);
#else
    if (Upolyd)
        Strcpy(buf, TR("actually ", "实际上是")); /* "You are actually a ..." */
    you_are(buf, "");
#endif

    /* report alignment (bypass you_are() in order to omit ending period);
       adverb is used to distinguish between temporary change (helm of opp.
       alignment), permanent change (one-time conversion), and original */
#ifdef ZHLANG
    {
        const char *adv = (u.ualign.type != u.ualignbase[A_CURRENT])
                            ? (!final ? "目前" : "暂时")
                            : (u.ualign.type != u.ualignbase[A_ORIGINAL])
                              ? (!final ? "现在" : "后来")
                              : (!u.uconduct.gnostic && svm.moves > 1000L)
                                ? "名义上" : "";

        Sprintf(buf, "你%s%s，%s为%s执行任务，与以下神祇敌对：",
                final ? "曾是" : "是", align_str(u.ualign.type), adv,
                u_gname());
        if (u.ualign.type != A_LAWFUL)
            Sprintf(eos(buf), " %s（%s）和", align_gname(A_LAWFUL),
                    align_str(A_LAWFUL));
        if (u.ualign.type != A_NEUTRAL)
            Sprintf(eos(buf), " %s（%s）%s", align_gname(A_NEUTRAL),
                    align_str(A_NEUTRAL),
                    (u.ualign.type != A_CHAOTIC) ? "以及" : "");
        if (u.ualign.type != A_CHAOTIC)
            Sprintf(eos(buf), " %s（%s）", align_gname(A_CHAOTIC),
                    align_str(A_CHAOTIC));
        Strcat(buf, "。");
        enlght_out(buf);
    }
#else
    Sprintf(buf, TR(" %s%s%s, %son a mission for %s", " %s%s%s，%s为%s执行任务"),
            You_, !final ? are : were,
            align_str(u.ualign.type),
            /* helm of opposite alignment (might hide conversion) */
            (u.ualign.type != u.ualignbase[A_CURRENT])
               /* what's the past tense of "currently"? if we used "formerly"
                  it would sound like a reference to the original alignment */
               ? (!final ? "currently " : "temporarily ")
               /* permanent conversion */
               : (u.ualign.type != u.ualignbase[A_ORIGINAL])
                  /* and what's the past tense of "now"? certainly not "then"
                     in a context like this...; "belatedly" == weren't that
                     way sooner (in other words, didn't start that way) */
                  ? (!final ? "now " : "belatedly ")
                  /* atheist (ignored in very early game) */
                  : (!u.uconduct.gnostic && svm.moves > 1000L)
                     ? "nominally "
                     /* lastly, normal case */
                     : "",
            u_gname());
    enlght_out(buf);
    /* show the rest of this game's pantheon (finishes previous sentence)
       [appending "also Moloch" at the end would allow for straightforward
       trailing "and" on all three aligned entries but looks too verbose] */
    Sprintf(buf, TR(" who %s opposed by", "，%s与以下神祇敌对："), !final ? "is" : "was");
    if (u.ualign.type != A_LAWFUL)
        Sprintf(eos(buf), TR(" %s (%s) and", " %s（%s）和"), align_gname(A_LAWFUL),
                align_str(A_LAWFUL));
    if (u.ualign.type != A_NEUTRAL)
        Sprintf(eos(buf), " %s (%s)%s", align_gname(A_NEUTRAL),
                align_str(A_NEUTRAL),
                (u.ualign.type != A_CHAOTIC) ? " and" : "");
    if (u.ualign.type != A_CHAOTIC)
        Sprintf(eos(buf), " %s (%s)", align_gname(A_CHAOTIC),
                align_str(A_CHAOTIC));
    Strcat(buf, "."); /* terminate sentence */
    enlght_out(buf);
#endif

    /* show original alignment,gender,race,role if any have been changed;
       giving separate message for temporary alignment change bypasses need
       for tricky phrasing otherwise necessitated by possibility of having
       helm of opposite alignment mask a permanent alignment conversion */
    difgend = (innategend != flags.initgend);
    difalgn = (((u.ualign.type != u.ualignbase[A_CURRENT]) ? 1 : 0)
               + ((u.ualignbase[A_CURRENT] != u.ualignbase[A_ORIGINAL])
                  ? 2 : 0));
    if (difalgn & 1) { /* have temporary alignment so report permanent one */
        Sprintf(buf, TR("actually %s", "实际上是%s"), align_str(u.ualignbase[A_CURRENT]));
        you_are(buf, "");
        difalgn &= ~1; /* suppress helm from "started out <foo>" message */
    }
    if (difgend || difalgn) { /* sex change or perm align change or both */
#ifdef ZHLANG
        Sprintf(buf, " 你最初是%s%s%s。",
                difgend ? zh_gender_word(flags.initgend) : "",
                (difgend && difalgn) ? " 和 " : "",
                difalgn ? align_str(u.ualignbase[A_ORIGINAL]) : "");
        enlght_out(buf);
#else
        Sprintf(buf, TR(" You started out %s%s%s.", " 你最初是%s%s%s。"),
                difgend ? genders[flags.initgend].adj : "",
                (difgend && difalgn) ? " and " : "",
                difalgn ? align_str(u.ualignbase[A_ORIGINAL]) : "");
        enlght_out(buf);
#endif
    }

    /* "You are left-handed." won't work well if polymorphed into something
       without hands; use "You are normally left-handed." in that situation */
#ifdef ZHLANG
    enlght_zh("你是%s%s撇子。",
              !strcmp(body_part(HANDED), "handed") ? "" : "通常",
              URIGHTY ? "右" : "左");
#else
    Sprintf(buf, TR("%s%s-handed", "%s%s撇子"),
            !strcmp(body_part(HANDED), "handed") ? "" : "normally ",
            URIGHTY ? "right" : "left");
    you_are(buf, "");
#endif

    /* As of 3.6.2: dungeon level, so that ^X really has all status info as
       claimed by the comment below; this reveals more information than
       the basic status display, but that's one of the purposes of ^X;
       similar information is revealed by #overview; the "You died in
       <location>" given by really_done() is more rudimentary than this */
    *buf = *tmpbuf = '\0';
    if (In_endgame(&u.uz)) {
        int egdepth = observable_depth(&u.uz);

        (void) endgamelevelname(tmpbuf, egdepth);
#ifdef ZHLANG
        Snprintf(buf, sizeof buf, "在终局中，位于%s", tmpbuf);
#else
        Snprintf(buf, sizeof(buf), TR("in the endgame, on the %s%s", "在终局中，位于%s%s"),
                 !strncmp(tmpbuf, "Plane", 5) ? "Elemental " : "", tmpbuf);
#endif
    } else if (Is_knox(&u.uz)) {
        /* this gives away the fact that the knox branch is only 1 level */
#ifdef ZHLANG
        Sprintf(buf, "位于%s", dname_zh(svd.dungeons[u.uz.dnum].dname));
#else
        Sprintf(buf, TR("on the %s level", "位于%s层"), svd.dungeons[u.uz.dnum].dname);
#endif
        /* TODO? maybe phrase it differently when actually inside the fort,
           if we're able to determine that (not trivial) */
    } else {
        char dgnbuf[QBUFSZ];

        Strcpy(dgnbuf, svd.dungeons[u.uz.dnum].dname);
        if (!strncmpi(dgnbuf, "The ", 4))
            *dgnbuf = lowc(*dgnbuf);
        Sprintf(tmpbuf, TR("level %d", "第 %d 层"),
                In_quest(&u.uz) ? dunlev(&u.uz) : depth(&u.uz));
        /* TODO? maybe extend this bit to include various other automatic
           annotations from the dungeon overview code */
        if (Is_rogue_level(&u.uz))
            Strcat(tmpbuf, TR(", a primitive area", "，一片原始区域"));
        else if (Is_bigroom(&u.uz) && !Blind)
            Strcat(tmpbuf, TR(", a very big room", "，一个极大的房间"));
#ifdef ZHLANG
        /* dname_zh() expects the exact (capitalized) dungeon name; dgnbuf
           has been lowercased above for the English wording */
        Snprintf(buf, sizeof buf, "在%s，%s",
                 dname_zh(svd.dungeons[u.uz.dnum].dname), tmpbuf);
#else
        Snprintf(buf, sizeof(buf), "in %s, on %s", dgnbuf, tmpbuf);
#endif
    }
    you_are(buf, "");

    /* this is shown even if the 'time' option is off */
    if (svm.moves == 1L) {
#ifdef ZHLANG
        enlght_zh("你刚刚开始你的冒险。");
#else
        you_have(TR("just started your adventure", "刚刚开始你的冒险"), "");
#endif
    } else {
        /* 'turns' grates on the nerves in this context... */
#ifdef ZHLANG
        enlght_zh("你在 %ld 回合前进入了地牢。", svm.moves);
#else
        Sprintf(buf, TR("the dungeon %ld turn%s ago", "地牢 %ld 回合%s前"),
                svm.moves, plur(svm.moves));
        /* same phrasing for current and final: "entered" is unconditional */
        enlght_line(You_, "entered ", buf, "");
#endif
    }

    /* for gameover, these have been obtained in really_done() so that they
       won't vary if user leaves a disclosure prompt or --More-- unanswered
       long enough for the dynamic value to change between then and now */
    if (final ? iflags.at_midnight : midnight()) {
        enl_msg("It ", "is ", TR("was ", "曾是"), TR("the midnight hour", "午夜时分"), "");
    } else if (final ? iflags.at_night : night()) {
        enl_msg("It ", "is ", TR("was ", "曾是"), TR("nighttime", "夜晚"), "");
    }
    /* other environmental factors */
    if (flags.moonphase == FULL_MOON || flags.moonphase == NEW_MOON) {
        /* [This had "tonight" but has been changed to "in effect".
           There is a similar issue to Friday the 13th--it's the value
           at the start of the current session but that session might
           have dragged on for an arbitrary amount of time.  We want to
           report the values that currently affect play--or affected
           play when game ended--rather than actual outside situation.] */
        Sprintf(buf, TR("a %s moon in effect%s", "%s月当空%s"),
                (flags.moonphase == FULL_MOON) ? TR("full", "满")
                : (flags.moonphase == NEW_MOON) ? TR("new", "新")
                  /* showing these would probably just lead to confusion
                     since they have no effect on game play... */
                  : (flags.moonphase < FULL_MOON) ? TR("first quarter", "上弦")
                    : TR("last quarter", "下弦"),
                /* we don't have access to 'how' here--aside from survived
                   vs died--so settle for general platitude */
                final ? TR(" when your adventure ended", "（在你冒险结束时）") : "");
        enl_msg(TR("There ", "当前"), "is ", TR("was ", "曾是"), buf, "");
    }
    if (flags.friday13) {
        /* let player know that friday13 penalty is/was in effect;
           we don't say "it is/was Friday the 13th" because that was at
           the start of the session and it might be past midnight (or
           days later if the game has been paused without save/restore),
           so phrase this similar to the start up message */
        Sprintf(buf, TR(" Bad things %s on Friday the 13th.", " 13号星期五，坏事%s。"),
                !final ? TR("can happen", "可能发生")
                : (final == ENL_GAMEOVERALIVE) ? TR("could have happened", "本可能发生")
                  /* there's no may to tell whether -1 Luck made a
                     difference but hero has died... */
                  : TR("happened", "发生了"));
        enlght_out(buf);
    }

    if (!Upolyd) {
        int ulvl = (int) u.ulevel;
        /* [flags.showexp currently does not matter; should it?] */

        /* experience level is already shown above */
        Sprintf(buf, TR("%-1ld experience point%s", "%-1ld 点经验值%s"), u.uexp, plur(u.uexp));
        /* TODO?
         *  Remove wizard-mode restriction since patient players can
         *  determine the numbers needed without resorting to spoilers
         *  (even before this started being disclosed for 'final';
         *  just enable 'showexp' and look at normal status lines
         *  after drinking gain level potions or eating wraith corpses
         *  or being level-drained by vampires).
         */
        if (ulvl < 30 && (final || wizard)) {
            long nxtlvl = newuexp(ulvl), delta = nxtlvl - u.uexp;

#ifdef ZHLANG
            Sprintf(eos(buf), "，还差 %ld 点经验值升到 %d 级", delta, ulvl + 1);
#else
            Sprintf(eos(buf), TR(", %ld %s%sneeded %s level %d", "，还差 %ld %s%s%s%d 级"),
                    delta, (u.uexp > 0) ? "more " : "",
                    /* present tense=="needed", past tense=="were needed" */
                    !final ? "" : (delta == 1L) ? "was " : "were ",
                    /* "for": grammatically iffy but less likely to wrap */
                    (ulvl < 18) ? "to attain" : "for", (ulvl + 1));
#endif
        }
        you_have(buf, "");
    }
#ifdef SCORE_ON_BOTL
    if (flags.showscore) {
        /* describes what's shown on status line, which is an approximation;
           only show it here if player has the 'showscore' option enabled */
        Sprintf(buf, "%ld%s", botl_score(),
                !final ? "" : TR(" before end-of-game adjustments", "（结算前的数值）"));
        enl_msg(TR("Your score ", "你的得分"), "is ", TR("was ", "曾是"), buf, "");
    }
#endif
}

/* hit points, energy points, armor class -- essential information which
   doesn't fit very well in other categories */
/*ARGSUSED*/
staticfn void
basics_enlightenment(int mode UNUSED, int final)
{
#ifndef ZHLANG
    static char Power[] = "energy points (spell power)";
#endif
    char buf[BUFSZ];
    int pw = u.uen, hp = (Upolyd ? u.mh : u.uhp),
        pwmax = u.uenmax, hpmax = (Upolyd ? u.mhmax : u.uhpmax);

    enlght_out(""); /* separator after background */
    enlght_out(TR("Basics:", "基础："));

    if (hp < 0)
        hp = 0;
    /* "1 out of 1" rather than "all" if max is only 1; should never happen */
    if (hp == hpmax && hpmax > 1)
        Sprintf(buf, TR("all %d hit points", "全部 %d 点生命值"), hpmax);
    else
        Sprintf(buf, TR("%d out of %d hit point%s", "%d/%d 点生命值%s"), hp, hpmax, plur(hpmax));
    you_have(buf, "");

    /* low max energy is feasible, so handle couple of extra special cases */
#ifdef ZHLANG
    if (pwmax == 0)
        enlght_zh("你没有能量点。");
    else if (pw == pwmax)
        enlght_zh("你拥有全部 %d 点能量点。", pwmax);
    else
        enlght_zh("你拥有 %d/%d 点能量点。", pw, pwmax);
#else
    if (pwmax == 0 || (pw == pwmax && pwmax == 2)) /* both: not "all 2" */
        Sprintf(buf, "%s %s", !pwmax ? "no" : "both", Power);
    else if (pw == pwmax && pwmax > 2)
        Sprintf(buf, TR("all %d %s", "全部 %d %s"), pwmax, Power);
    else
        Sprintf(buf, TR("%d out of %d %s", "%d/%d %s"), pw, pwmax, Power);
    you_have(buf, "");
#endif

    if (Upolyd) {
        switch (mons[u.umonnum].mlevel) {
        case 0:
            /* status line currently being explained shows "HD:0" */
            Strcpy(buf, TR("0 hit dice (actually 1/2)", "0 生命骰（实际为 1/2）"));
            break;
        case 1:
            Strcpy(buf, TR("1 hit die", "1 生命骰"));
            break;
        default:
            Sprintf(buf, TR("%d hit dice", "%d 生命骰"), mons[u.umonnum].mlevel);
            break;
        }
        you_have(buf, "");
    }

    find_ac(); /* enforces AC_MAX cap */
    Sprintf(buf, "%d", u.uac);
    if (abs(u.uac) == AC_MAX)
        Sprintf(eos(buf), TR(", the %s possible", "，已是可能的%s值"),
                (u.uac < 0) ? TR("best", "最佳") : TR("worst", "最差"));
    enl_msg(TR("Your armor class ", "你的防御等级"), "is ", TR("was ", "曾是"), buf, "");

    /* gold; similar to doprgold (#showgold) but without shop billing info;
       includes container contents, unlike status line but like doprgold */
    {
        long umoney = money_cnt(gi.invent), hmoney = hidden_gold(final);

#ifdef ZHLANG
        if (!umoney)
            Strcpy(buf, " 你的钱袋是空的");
        else
            Sprintf(buf, " 你的钱袋里装着 %ld %s", umoney, currency(umoney));
        /* wallet line is followed by a continuation when gold is stashed */
        Strcat(buf, !hmoney ? "。" : !umoney ? "，但" : "，其中");
        enlght_out(buf);
        if (hmoney) {
            Sprintf(buf, "%ld %s 存放在你的背包里", hmoney,
                    umoney ? "更多" : currency(hmoney));
            enlght_zh("你还有%s。", buf);
        }
#else
        if (!umoney) {
            Sprintf(buf, TR(" Your wallet %s empty", " 你的钱袋%s空的"), !final ? "is" : "was");
        } else {
            Sprintf(buf, TR(" Your wallet contain%s %ld %s", " 你的钱袋里装%s %ld %s"), !final ? "s" : "ed",
                    umoney, currency(umoney));
        }
        /* terminate the wallet line if appropriate, otherwise add an
           introduction to subsequent continuation; output now either way */
        Strcat(buf, !hmoney ? "." : !umoney ? ", but" : ", and");
        enlght_out(buf);

        /* put contained gold on its own line to avoid excessive width; it's
           phrased as a continuation of the wallet line so not capitalized */
        if (hmoney) {
            Sprintf(buf, TR("%ld %s stashed away in your pack", "%ld %s 存放在你的背包里"),
                    hmoney, umoney ? "more" : currency(hmoney));
            enl_msg(TR("you ", "你"), "have ", "had ", buf, "");
        }
#endif
    }

#ifdef ZHLANG
    if (flags.pickup) {
        char ocl[MAXOCLASSES + 1];

        Strcpy(buf, "开启");
        if (costly_spot(u.ux, u.uy)) {
            /* being in a shop inhibits autopickup, even 'pickup_thrown' */
            Strcat(buf, "，但在商店内会暂时禁用");
        } else {
            oc_to_str(flags.pickup_types, ocl);
            if (*ocl)
                Sprintf(eos(buf), "，拾取范围：%s", ocl);
            else
                Strcat(buf, "，拾取所有类型");
            if (flags.pickup_thrown && *ocl)
                Strcat(buf, "，外加投掷出的物品");
            if (ga.apelist)
                Strcat(buf, "，但有例外");
        }
    } else
        Strcpy(buf, "关闭");
    enlght_zh("自动拾取：%s。", buf);
#else
    if (flags.pickup) {
        char ocl[MAXOCLASSES + 1];

        Strcpy(buf, "on");
        if (costly_spot(u.ux, u.uy)) {
            /* being in a shop inhibits autopickup, even 'pickup_thrown' */
            Strcat(buf, TR(", but temporarily disabled while inside the shop", "，但在商店内会暂时禁用"));
        } else {
            oc_to_str(flags.pickup_types, ocl);
            Sprintf(eos(buf), TR(" for %s%s%s", "，范围为%s%s%s"), *ocl ? "'" : "",
                    *ocl ? ocl : "all types", *ocl ? "'" : "");
            if (flags.pickup_thrown && *ocl)
                Strcat(buf, TR(" plus thrown", "，外加投掷出的物品")); /* show when not 'all types' */
            if (ga.apelist)
                Strcat(buf, TR(", with exceptions", "，但有例外"));
        }
    } else
        Strcpy(buf, TR("off", "关闭"));
    enl_msg(TR("Autopickup ", "自动拾取"), "is ", "was ", buf, "");
#endif
}

/* characteristics: expanded version of bottom line strength, dexterity, &c */
staticfn void
characteristics_enlightenment(int mode, int final)
{
    char buf[BUFSZ];

    enlght_out("");
    Sprintf(buf, TR("%sCharacteristics:", "%s属性："),
            !final ? "" : TR("Final ", "最终"));
    enlght_out(buf);

    /* bottom line order */
    one_characteristic(mode, final, A_STR); /* strength */
    one_characteristic(mode, final, A_DEX); /* dexterity */
    one_characteristic(mode, final, A_CON); /* constitution */
    one_characteristic(mode, final, A_INT); /* intelligence */
    one_characteristic(mode, final, A_WIS); /* wisdom */
    one_characteristic(mode, final, A_CHA); /* charisma */
}

/* display one attribute value for characteristics_enlightenment() */
staticfn void
one_characteristic(int mode, int final, int attrindx)
{
    extern const char *const attrname[]; /* attrib.c */
    boolean hide_innate_value = FALSE, interesting_alimit;
    int acurrent, abase, apeak, alimit;
    const char *paren_pfx;
    char subjbuf[BUFSZ], valubuf[BUFSZ], valstring[32];

    /* being polymorphed or wearing certain cursed items prevents
       hero from reliably tracking changes to characteristics so
       we don't show base & peak values then; when the items aren't
       cursed, hero could take them off to check underlying values
       and we show those in such case so that player doesn't need
       to actually resort to doing that */
    if (Upolyd) {
        hide_innate_value = TRUE;
    } else if (Fixed_abil) {
        if (stuck_ring(uleft, RIN_SUSTAIN_ABILITY)
            || stuck_ring(uright, RIN_SUSTAIN_ABILITY))
            hide_innate_value = TRUE;
    }
    switch (attrindx) {
    case A_STR:
        if (uarmg && uarmg->otyp == GAUNTLETS_OF_POWER && uarmg->cursed)
            hide_innate_value = TRUE;
        break;
    case A_DEX:
        break;
    case A_CON:
        if (u_wield_art(ART_OGRESMASHER) && uwep->cursed)
            hide_innate_value = TRUE;
        break;
    case A_INT:
        if (uarmh && uarmh->otyp == DUNCE_CAP && uarmh->cursed)
            hide_innate_value = TRUE;
        break;
    case A_WIS:
        if (uarmh && uarmh->otyp == DUNCE_CAP && uarmh->cursed)
            hide_innate_value = TRUE;
        break;
    case A_CHA:
        break;
    default:
        return; /* impossible */
    };
    /* note: final disclosure includes MAGICENLIGHTENTMENT */
    if ((mode & MAGICENLIGHTENMENT) && !Upolyd)
        hide_innate_value = FALSE;

    acurrent = ACURR(attrindx);
    (void) attrval(attrindx, acurrent, valubuf); /* Sprintf(valubuf,"%d",) */
#ifdef ZHLANG
    Sprintf(subjbuf, "你的%s", zh_attr_name(attrindx));

    if (!hide_innate_value) {
        /* show abase, amax, and/or attrmax if acurr doesn't match abase
           (a magic bonus or penalty is in effect) or abase doesn't match
           amax (some points have been lost to poison or exercise abuse
           and are restorable) or attrmax is different from normal human
           (while game is in progress; trying to reduce dependency on
           spoilers to keep track of such stuff) or attrmax was different
           from abase (at end of game; this attribute wasn't maxed out) */
        abase = ABASE(attrindx);
        apeak = AMAX(attrindx);
        alimit = ATTRMAX(attrindx);
        /* criterium for whether the limit is interesting varies */
        interesting_alimit =
            final ? TRUE /* was originally `(abase != alimit)' */
                  : (alimit != (attrindx != A_STR ? 18 : STR18(100)));
        paren_pfx = final ? "（" : "（当前；";
        if (acurrent != abase) {
            Sprintf(eos(valubuf), "%s基础值：%s", paren_pfx,
                    attrval(attrindx, abase, valstring));
            paren_pfx = "，";
        }
        if (abase != apeak) {
            Sprintf(eos(valubuf), "%s峰值：%s", paren_pfx,
                    attrval(attrindx, apeak, valstring));
            paren_pfx = "，";
        }
        if (interesting_alimit) {
            Sprintf(eos(valubuf), "%s%s上限：%s", paren_pfx,
                    /* more verbose if exceeding 'limit' due to magic bonus */
                    (acurrent > alimit) ? "先天" : "",
                    attrval(attrindx, alimit, valstring));
        }
        if (acurrent != abase || abase != apeak || interesting_alimit)
            Strcat(valubuf, "）");
    }
    enlght_zh("%s是%s。", subjbuf, valubuf);
#else
    Sprintf(subjbuf, TR("Your %s ", "你的%s"), attrname[attrindx]);

    if (!hide_innate_value) {
        /* show abase, amax, and/or attrmax if acurr doesn't match abase
           (a magic bonus or penalty is in effect) or abase doesn't match
           amax (some points have been lost to poison or exercise abuse
           and are restorable) or attrmax is different from normal human
           (while game is in progress; trying to reduce dependency on
           spoilers to keep track of such stuff) or attrmax was different
           from abase (at end of game; this attribute wasn't maxed out) */
        abase = ABASE(attrindx);
        apeak = AMAX(attrindx);
        alimit = ATTRMAX(attrindx);
        /* criterium for whether the limit is interesting varies */
        interesting_alimit =
            final ? TRUE /* was originally `(abase != alimit)' */
                  : (alimit != (attrindx != A_STR ? 18 : STR18(100)));
        paren_pfx = final ? " (" : " (current; ";
        if (acurrent != abase) {
            Sprintf(eos(valubuf), TR("%sbase:%s", "%s基础值：%s"), paren_pfx,
                    attrval(attrindx, abase, valstring));
            paren_pfx = ", ";
        }
        if (abase != apeak) {
            Sprintf(eos(valubuf), TR("%speak:%s", "%s峰值：%s"), paren_pfx,
                    attrval(attrindx, apeak, valstring));
            paren_pfx = ", ";
        }
        if (interesting_alimit) {
            Sprintf(eos(valubuf), TR("%s%slimit:%s", "%s%s上限：%s"), paren_pfx,
                    /* more verbose if exceeding 'limit' due to magic bonus */
                    (acurrent > alimit) ? "innate " : "",
                    attrval(attrindx, alimit, valstring));
            /* paren_pfx = ", "; */
        }
        if (acurrent != abase || abase != apeak || interesting_alimit)
            Strcat(valubuf, ")");
    }
    enl_msg(subjbuf, "is ", "was ", valubuf, "");
#endif
}

/* status: selected obvious capabilities, assorted troubles */
staticfn void
status_enlightenment(int mode, int final)
{
    boolean magic = (mode & MAGICENLIGHTENMENT) ? TRUE : FALSE;
    int cap;
    char buf[BUFSZ], youtoo[BUFSZ], heldmon[BUFSZ];
    boolean Riding = (u.usteed
                      /* if hero dies while dismounting, u.usteed will still
                         be set; we want to ignore steed in that situation */
                      && !(final == ENL_GAMEOVERDEAD
                           && !strcmp(svk.killer.name, "riding accident")));
    const char *steedname = (!Riding ? (char *) 0
                      : x_monnam(u.usteed,
                                 u.usteed->mtame ? ARTICLE_YOUR : ARTICLE_THE,
                                 (char *) 0,
                                 (SUPPRESS_SADDLE | SUPPRESS_HALLUCINATION),
                                 FALSE));

    /*\
     * Status (many are abbreviated on bottom line; others are or
     *     should be discernible to the hero hence to the player)
    \*/
    enlght_out(""); /* separator after title or characteristics */
    enlght_out(final ? TR("Final Status:", "最终状态：") : TR("Status:", "状态："));

#ifdef ZHLANG
    Strcpy(youtoo, "你");
#else
    Strcpy(youtoo, You_);
#endif
    /* not a traditional status but inherently obvious to player; more
       detail given below (attributes section) for magic enlightenment */
    if (Upolyd) {
#ifdef ZHLANG
        Sprintf(buf, "已变形");
        if (ugenocided()) {
            const char *inside = udeadinside();

            Sprintf(eos(buf), "，内心感到%s",
                    !strcmp(inside, "dead") ? "死寂" /* NOTRANS */
                    : !strcmp(inside, "condemned") ? "被诅咒" : "空虚"); /* NOTRANS */
        }
        you_are(buf, "");
#else
        Strcpy(buf, TR("transformed", "已变形"));
        if (ugenocided())
            Sprintf(eos(buf), TR(" and %s %s inside", "，内心%s%s"),
                    final ? "felt" : "feel", udeadinside());
        you_are(buf, "");
#endif
    }
    /* not a trouble, but we want to display riding status before maybe
       reporting steed as trapped or hero stuck to cursed saddle */
    if (Riding) {
        Sprintf(buf, TR("riding %s", "正骑着%s"), steedname);
        you_are(buf, "");
        Sprintf(eos(youtoo), TR("and %s ", "与%s"), steedname);
    }
    /* other movement situations that hero should always know */
    if (Levitation) {
        if (Lev_at_will && magic)
            you_are(TR("levitating, at will", "可随意漂浮"), "");
        else
            enl_msg(youtoo, are, were, TR("levitating", "正在漂浮"), from_what(LEVITATION));
    } else if (Flying) { /* can only fly when not levitating */
        enl_msg(youtoo, are, were, TR("flying", "正在飞行"), from_what(FLYING));
    }
    if (Underwater) {
        you_are(TR("underwater", "在水下"), "");
    } else if (u.uinwater) {
        you_are(Swimming ? TR("swimming", "正在游泳") : TR("in water", "在水中"),
                from_what(SWIMMING));
    } else if (walking_on_water()) {
        /* show active Wwalking here, potential Wwalking elsewhere */
        Sprintf(buf, TR("walking on %s", "在%s上行走"),
                is_pool(u.ux, u.uy) ? TR("water", "水面")
                : is_lava(u.ux, u.uy) ? TR("lava", "岩浆")
                  : surface(u.ux, u.uy)); /* catchall; shouldn't happen */
        you_are(buf, from_what(WWALKING));
    }
    if (Upolyd && (u.uundetected || U_AP_TYPE != M_AP_NOTHING))
        youhiding(TRUE, final);

    /* internal troubles, mostly in the order that prayer ranks them */
    if (Stoned) {
        if (final && (Stoned & I_SPECIAL))
            enlght_out(TR(" You turned into stone.", " 你变成了石头。"));
        else
            you_are(TR("turning to stone", "正在石化"), "");
    }
    if (Slimed) {
        if (final && (Slimed & I_SPECIAL))
            enlght_out(TR(" You turned into slime.", " 你变成了粘液。"));
        else
            you_are(TR("turning into slime", "正在变成粘液"), "");
    }
    if (Strangled) {
        if (u.uburied) {
            you_are(TR("buried", "被埋住"), "");
        } else {
            if (final && (Strangled & I_SPECIAL)) {
                enlght_out(TR(" You died from strangulation.", " 你死于窒息。"));
            } else {
                Strcpy(buf, TR("being strangled", "正被勒住"));
                if (wizard)
                    Sprintf(eos(buf), " (%ld)", (Strangled & TIMEOUT));
                you_are(buf, from_what(STRANGLED));
            }
        }
    }
    if (Sick) {
        /* the two types of sickness are lumped together; hero can be
           afflicted by both but there is only one timeout; botl status
           puts TermIll before FoodPois and death due to timeout reports
           terminal illness if both are in effect, so do the same here */
        if (final && (Sick & I_SPECIAL)) {
#ifdef ZHLANG
            enlght_zh("你死于%s。",
                      (u.usick_type & SICK_NONVOMITABLE) ? "绝症"
                                                         : "食物中毒");
#else
            Sprintf(buf, TR(" %sdied from %s.", " %s死于%s。"), You_, /* has trailing space */
                    (u.usick_type & SICK_NONVOMITABLE)
                    ? "terminal illness" : "food poisoning");
            enlght_out(buf);
#endif
        } else {
            /* unlike death due to sickness, report the two cases separately
               because it is possible to cure one without curing the other */
            if (u.usick_type & SICK_NONVOMITABLE)
                you_are(TR("terminally sick from illness", "因疾病而病危"), "");
            if (u.usick_type & SICK_VOMITABLE)
                you_are(TR("terminally sick from food poisoning", "因食物中毒而病危"), "");
        }
    }
    if (Vomiting)
        you_are(TR("nauseated", "感到恶心"), "");
    if (Stunned)
        you_are(TR("stunned", "感到眩晕"), "");
    if (Confusion)
        you_are(TR("confused", "处于混乱状态"), "");
    if (Hallucination)
        you_are(TR("hallucinating", "产生幻觉"), "");
    if (Blind) {
        /* check the reasons in same order as from_what() */
        Sprintf(buf, TR("%s blind", "%s失明"),
                (HBlinded & FROMOUTSIDE) != 0L ? TR("permanently", "永久性")
                : (HBlinded & FROMFORM) ? TR("innately", "先天")
                  /* better phrasing desperately wanted... */
                  : Blindfolded_only ? TR("deliberately", "故意")
                    /* timed, possibly combined with blindfold */
                    : TR("temporarily", "暂时"));
        if (wizard && (HBlinded == BlindedTimeout && !Blindfolded))
            Sprintf(eos(buf), " (%ld)", BlindedTimeout);
        /* !haseyes: avoid "you are innately blind innately" */
        you_are(buf, !haseyes(gy.youmonst.data) ? "" : from_what(BLINDED));
    }
    if (Deaf)
        you_are(TR("deaf", "耳聋"), from_what(DEAF));

    /* external troubles, more or less */
    if (Punished) {
        if (uball) {
            Sprintf(buf, TR("chained to %s", "被%s锁住"), ansimpleoname(uball));
        } else {
            impossible("Punished without uball?");
            Strcpy(buf, TR("punished", "受罚"));
        }
        you_are(buf, "");
    }
    if (u.utrap) {
        char predicament[BUFSZ];
        boolean anchored = (u.utraptype == TT_BURIEDBALL);

        (void) trap_predicament(predicament, final, wizard);
#ifdef ZHLANG
        if (u.usteed) { /* not `Riding' here */
            enlght_zh("%s%s。", anchored ? "你与坐骑都处于" : "坐骑处于",
                      predicament);
        } else
            you_are(predicament, "");
#else
        if (u.usteed) { /* not `Riding' here */
            Sprintf(buf, "%s%s ", anchored ? "you and " : "", steedname);
            *buf = highc(*buf);
            enl_msg(buf, (anchored ? TR("are ", "都处于") : "is "),
                    (anchored ? "were " : "was "), predicament, "");
        } else
            you_are(predicament, "");
#endif
    } /* (u.utrap) */
    heldmon[0] = '\0'; /* lint suppression */
    if (u.ustuck) { /* includes u.uswallow */
        Strcpy(heldmon, a_monnam(u.ustuck));
        if (!strcmp(heldmon, "it")
            && (!has_mgivenname(u.ustuck)
                || strcmp(MGIVENNAME(u.ustuck), "it") != 0))
            Strcpy(heldmon, TR("an unseen creature", "一个看不见的生物"));
    }
    if (u.uswallow) {
        assert(u.ustuck != NULL); /* implied by u.uswallow */
#ifdef ZHLANG
        Snprintf(buf, sizeof buf, "被%s%s",
                 heldmon, digests(u.ustuck->data) ? "吞下" : "包住");
        if (dmgtype(u.ustuck->data, AD_DGST)) {
            if (final && !u.uswldtim)
                Strcat(buf, "，最终被完全消化");
            else
                Sprintf(eos(buf), "，正被消化");
        }
        if (wizard)
            Sprintf(eos(buf), " (%u)", u.uswldtim);
        you_are(buf, "");
#else
        Snprintf(buf, sizeof buf, "%s by %s",
                digests(u.ustuck->data) ? "swallowed" : "engulfed",
                heldmon);
        if (dmgtype(u.ustuck->data, AD_DGST)) {
            /* if final, death via digestion can be deduced by u.uswallow
               still being True and u.uswldtim having been decremented to 0 */
            if (final && !u.uswldtim)
                Strcat(buf, TR(" and got totally digested", "，最终被完全消化"));
            else
                Sprintf(eos(buf), TR(" and %s being digested", "，%s正被消化"),
                        final ? "were" : "are");
        }
        if (wizard)
            Sprintf(eos(buf), " (%u)", u.uswldtim);
        you_are(buf, "");
#endif
    } else if (u.ustuck) {
        boolean ustick = (Upolyd && sticks(gy.youmonst.data));
        int dx = u.ustuck->mx - u.ux, dy = u.ustuck->my - u.uy;

#ifdef ZHLANG
        Snprintf(buf, sizeof buf, "%s%s（%s）",
                 ustick ? "抓住" : "被…抓住：",
                 heldmon, dxdy_to_dist_descr(dx, dy, TRUE));
        you_are(buf, "");
#else
        Snprintf(buf, sizeof buf, "%s %s (%s)",
                 ustick ? "holding" : "held by",
                 heldmon, dxdy_to_dist_descr(dx, dy, TRUE));
        you_are(buf, "");
#endif
    }
    if (Riding) {
        struct obj *saddle = which_armor(u.usteed, W_SADDLE);

        if (saddle && saddle->cursed) {
            Sprintf(buf, TR("stuck to %s %s", "被粘在%s%s上"), s_suffix(steedname),
                    simpleonames(saddle));
            you_are(buf, "");
        }
    }
    if (Wounded_legs) {
        /* EWounded_legs is used to track left/right/both rather than some
           form of extrinsic impairment; HWounded_legs is used for timeout;
           both apply to steed instead of hero when mounted */
        long whichleg = (EWounded_legs & BOTH_SIDES);
#ifdef ZHLANG
        /* "你的左腿受伤了。" / "坐骑的左腿受伤了。" */
        {
            const char *lr = (whichleg == LEFT_SIDE) ? "左"
                             : (whichleg == RIGHT_SIDE) ? "右" : "";
            char legbuf[BUFSZ];

            if (whichleg == BOTH_SIDES)
                Strcpy(legbuf, "双腿");
            else
                Sprintf(legbuf, "%s腿", lr);
            if (u.usteed) { /* not `Riding' here */
                if (wizard && steedname)
                    enlght_zh("%s的%s受伤了。", steedname, legbuf);
            } else {
                enlght_zh("你的%s受伤了。", legbuf);
            }
        }
#else
        const char *bp = u.usteed ? mbodypart(u.usteed, LEG) : body_part(LEG),
            *article = "a ", /* precedes "wounded", so never "an " */
            *leftright = "";

        if (whichleg == BOTH_SIDES)
            bp = makeplural(bp), article = "";
        else
            leftright = (whichleg == LEFT_SIDE) ? "left " : "right ";

        Sprintf(buf, TR("%swounded %s%s", "%s受伤的%s%s"), article, leftright, bp);

        /* when mounted, Wounded_legs applies to steed rather than to
           hero; we only report steed's wounded legs in wizard mode */
        if (u.usteed) { /* not `Riding' here */
            if (wizard && steedname) {
                char steednambuf[BUFSZ];

                Strcpy(steednambuf, steedname);
                *steednambuf = highc(*steednambuf);
                enl_msg(steednambuf, TR(" has ", "身上有"), " had ", buf, "");
            }
        } else {
            you_have(buf, "");
        }
#endif
    }
#ifdef ZHLANG
    if (Glib) {
        if (wizard)
            enlght_zh("%s湿滑（%ld）。",
                      uarmg ? "你的手套" : "你的手指", (Glib & TIMEOUT));
        else
            enlght_zh("%s湿滑。", uarmg ? "你的手套" : "你的手指");
    }
#else
    if (Glib) {
        Sprintf(buf, TR("slippery %s", "湿滑的%s"), fingers_or_gloves(TRUE));
        if (wizard)
            Sprintf(eos(buf), " (%ld)", (Glib & TIMEOUT));
        you_have(buf, "");
    }
#endif
    if (Fumbling) {
        if (magic || cause_known(FUMBLING)) {
#ifdef ZHLANG
            enlght_zh("你手脚不灵便%s。", zh_trim(from_what(FUMBLING)));
#else
            enl_msg(You_, TR("fumble", "失手"), "fumbled", "", from_what(FUMBLING));
#endif
        }
    }
    if (Sleepy) {
        if (magic || cause_known(SLEEPY)) {
#ifdef ZHLANG
            enlght_zh("你不由自主地睡着了%s。", zh_trim(from_what(SLEEPY)));
#else
            Strcpy(buf, from_what(SLEEPY));
            if (wizard)
                Sprintf(eos(buf), " (%ld)", (HSleepy & TIMEOUT));
            enl_msg(TR("You ", "你"), "fall", "fell", " asleep uncontrollably", buf);
#endif
        }
    }
    /* hunger/nutrition */
    if (Hunger) {
        if (magic || cause_known(HUNGER))
#ifdef ZHLANG
            enlght_zh("你很快感到饥饿%s。", zh_trim(from_what(HUNGER)));
#else
            enl_msg(You_, TR("hunger", "饥饿"), "hungered", " rapidly",
                    from_what(HUNGER));
#endif
    }
#ifdef ZHLANG
    {
        const char *hunger_zh;

        switch (u.uhs) {
        case SATIATED:
            hunger_zh = "吃得很饱";
            break;
        case WEAK:
            hunger_zh = "因严重饥饿而虚弱";
            break;
        case FAINTING:
        case FAINTED:
            hunger_zh = "因饥饿过度而昏厥";
            break;
        case STARVED:
            hunger_zh = "快要饿死了";
            break;
        case HUNGRY:
            hunger_zh = "饥饿";
            break;
        default:
            hunger_zh = "不饿";
            break;
        }
        Sprintf(buf, "你%s", hunger_zh);
        if (wizard)
            Sprintf(eos(buf), " <%d>", u.uhunger);
        enlght_zh("%s。", buf);
    }
#else
    Strcpy(buf, hu_stat[u.uhs]); /* hunger status; omitted if "normal" */
    mungspaces(buf);             /* strip trailing spaces */
    /* status line doesn't show hunger when state is "not hungry", we do;
       needed for wizard mode's reveal of u.uhunger but add it for everyone */
    if (!*buf)
        Strcpy(buf, TR("not hungry", "不饿"));
    if (*buf) { /* (since "not hungry" was added, this will always be True) */
        *buf = lowc(*buf); /* override capitalization */
        if (!strcmp(buf, "weak"))
            Strcat(buf, TR(" from severe hunger", "（因严重饥饿）"));
        else if (!strncmp(buf, "faint", 5)) /* fainting, fainted */
            Strcat(buf, TR(" due to starvation", "（因饥饿过度）"));
        if (wizard)
            Sprintf(eos(buf), " <%d>", u.uhunger);
        you_are(buf, "");
    }
#endif
    /* encumbrance */
#ifdef ZHLANG
    if ((cap = near_capacity()) > UNENCUMBERED) {
        const char *move = (cap == SLT_ENCUMBER) ? "略微减慢"
                           : (cap == MOD_ENCUMBER) ? "中等减慢"
                             : (cap == HVY_ENCUMBER) ? "大大减慢"
                               : (cap == EXT_ENCUMBER) ? "极度减慢"
                                 : "无法移动";

        if (wizard)
            Sprintf(buf, "你%s（负重 %d），移动速度%s。",
                    zh_enc_status(cap), inv_weight(), move);
        else
            Sprintf(buf, "你%s，移动速度%s。", zh_enc_status(cap), move);
        enlght_zh("%s", buf);
    } else {
        /* last resort entry, guarantees Status section is non-empty */
        if (wizard)
            enlght_zh("你负担正常（负重 %d）。", inv_weight());
        else
            enlght_zh("你负担正常。");
    }
#else
    if ((cap = near_capacity()) > UNENCUMBERED) {
        const char *adj = "?_?"; /* (should always get overridden) */

        Strcpy(buf, enc_stat[cap]);
        *buf = lowc(*buf);
        switch (cap) {
        case SLT_ENCUMBER:
            adj = "slightly";
            break; /* burdened */
        case MOD_ENCUMBER:
            adj = "moderately";
            break; /* stressed */
        case HVY_ENCUMBER:
            adj = "very";
            break; /* strained */
        case EXT_ENCUMBER:
            adj = "extremely";
            break; /* overtaxed */
        case OVERLOADED:
            adj = "not possible";
            break;
        }
        if (wizard)
            Sprintf(eos(buf), " <%d>", inv_weight());
        Sprintf(eos(buf), TR("; movement %s %s%s", "；移动速度%s %s%s"), !final ? "is" : "was", adj,
                (cap < OVERLOADED) ? " slowed" : "");
        you_are(buf, "");
    } else {
        /* last resort entry, guarantees Status section is non-empty
           (no longer needed for that purpose since weapon status added;
           still useful though) */
        Strcpy(buf, TR("unencumbered", "负担正常"));
        if (wizard)
            Sprintf(eos(buf), " <%d>", inv_weight());
        you_are(buf, "");
    }
#endif
    /* current weapon(s) and corresponding skill level(s) */
    weapon_insight(final);
    /* unlike ring of increase accuracy's effect, the monk's suit penalty
       is too blatant to be restricted to magical enlightenment */
    if (iflags.tux_penalty && !Upolyd) {
        (void) enlght_combatinc(TR("to hit", "命中"), -gu.urole.spelarmr, final,
                                buf);
        /* if from_what() ever gets extended from wizard mode to normal
           play, it could be adapted to handle this */
#ifdef ZHLANG
        Sprintf(eos(buf), "，因为你的%s", simpleonames(uarm));
        enlght_zh("你%s。", buf);
#else
        Sprintf(eos(buf), TR(" due to your %s", "，因为你的%s"), suit_simple_name(uarm));
        you_have(buf, "");
#endif
    }
    /* report 'nudity' */
    if (!uarm && !uarmu && !uarmc && !uarms && !uarmg && !uarmf && !uarmh) {
#ifdef ZHLANG
        enlght_zh("你未穿戴任何护甲。");
#else
        if (u.uroleplay.nudist)
            enl_msg(You_, "do", "did", " not wear any armor", "");
        else
            you_are(TR("not wearing any armor", "没有穿戴任何护甲"), "");
#endif
    }
}

/* extracted from status_enlightenment() to reduce clutter there */
staticfn void
weapon_insight(int final)
{
#ifdef ZHLANG
    int wtype;

    nhUse(final);

    /* report being weaponless; distinguish whether gloves are worn */
    if (!uwep) {
        if (uarmg)
            enlght_zh("你戴着%s，双手空着。", gloves_simple_name(uarmg));
        else if (humanoid(gy.youmonst.data))
            enlght_zh("你徒手。");
        else
            enlght_zh("你没有挥舞任何武器。");
    } else if (u.twoweap) {
        enlght_zh("你同时挥舞着两件武器。");
    } else {
        enlght_zh("你挥舞着%s。", an(simpleonames(uwep)));
    }

    /* skill with current weapon */
    wtype = weapon_type(uwep);
    if (wtype != P_NONE && (!uwep || !is_ammo(uwep))) {
        int sklvl = P_SKILL(wtype);
        boolean hav = (sklvl != P_UNSKILLED && sklvl != P_SKILLED);
        const char *skname = skill_name(wtype);

        if (!strcmp(skname, "two weapons"))
            skname = "双持武器";
        else if (!strcmp(skname, "bare handed combat"))
            skname = "徒手战斗";

        if (sklvl == P_ISRESTRICTED)
            enlght_zh("你无法使用%s。", skname);
        else if (hav)
            enlght_zh("你的%s技能为%s。", skname, zh_skill_level(wtype));
        else
            enlght_zh("你使用%s的技能为%s。", skname, zh_skill_level(wtype));
        if (can_advance(wtype, FALSE))
            enlght_zh("你可以提升%s技能。", skname);

        if (u.twoweap) {
            int wtype2 = weapon_type(uswapwep),
                twoskl = P_SKILL(P_TWO_WEAPON_COMBAT);
            const char *twozh = (twoskl == P_ISRESTRICTED)
                                    ? "受限"
                                    : zh_skill_level(P_TWO_WEAPON_COMBAT);

            enlght_zh("你的双持武器技能为%s。", twozh);
            if (wtype2 != P_NONE && wtype2 != wtype) {
                const char *skname2 = skill_name(wtype2);

                if (!strcmp(skname2, "two weapons"))
                    skname2 = "双持武器";
                else if (!strcmp(skname2, "bare handed combat"))
                    skname2 = "徒手战斗";
                enlght_zh("你的%s技能为%s%s。", skname2,
                          zh_skill_level(wtype2),
                          (P_SKILL(wtype2) > twoskl) ? "，但受双持限制" : "");
            }
            if (can_advance(wtype, FALSE) || can_advance(P_TWO_WEAPON_COMBAT, FALSE)
                || (wtype2 != P_NONE && can_advance(wtype2, FALSE)))
                enlght_zh("你可以提升相关武器技能。");
        }
    }
#else
    char buf[BUFSZ];
    int wtype;

    /* report being weaponless; distinguish whether gloves are worn
       [perhaps mention silver ring(s) when not wearing gloves?] */
    if (!uwep) {
        you_are(empty_handed(), "");

    /* two-weaponing implies hands and
       a weapon or wep-tool (not other odd stuff) in each hand */
    } else if (u.twoweap) {
        you_are(TR("wielding two weapons at once", "同时挥舞两件武器"), "");

    /* report most weapons by their skill class (so a katana will be
       described as a long sword, for instance; mattock, hook, and aklys
       are exceptions), or wielded non-weapon item by its object class */
    } else {
        const char *what = weapon_descr(uwep);

        /* [what about other silver items?] */
        if (uwep->otyp == SHIELD_OF_REFLECTION)
            what = shield_simple_name(uwep); /* silver|smooth shield */
        else if (is_wet_towel(uwep))
            what = /* (uwep->spe < 3) ? "moist towel" : */ "wet towel";

        if (!strcmpi(what, "armor") || !strcmpi(what, "food")
            || !strcmpi(what, "venom"))
            Sprintf(buf, TR("wielding some %s", "挥舞着%s"), what);
        else
            /* [maybe include known blessed?] */
            Sprintf(buf, TR("wielding %s", "挥舞着%s"),
                    (uwep->quan == 1L) ? an(what) : makeplural(what));
        you_are(buf, "");
    }

    /*
     * Skill with current weapon.  Might help players who've never
     * noticed #enhance or decided that it was pointless.
     */
    if ((wtype = weapon_type(uwep)) != P_NONE && (!uwep || !is_ammo(uwep))) {
        char sklvlbuf[20];
        int sklvl = P_SKILL(wtype);
        boolean hav = (sklvl != P_UNSKILLED && sklvl != P_SKILLED);

        if (sklvl == P_ISRESTRICTED)
            Strcpy(sklvlbuf, "no");
        else
            (void) lcase(skill_level_name(wtype, sklvlbuf));
        /* "you have no/basic/expert/master/grand-master skill with <skill>"
           or "you are unskilled/skilled in <skill>" */
        Sprintf(buf, "%s %s %s", sklvlbuf,
                hav ? "skill with" : "in", skill_name(wtype));

        if (!u.twoweap) {
            if (can_advance(wtype, FALSE))
                Sprintf(eos(buf), TR(" and %s that", "，并且%s"),
                        !final ? "can enhance" : "could have enhanced");
            if (hav)
                you_have(buf, "");
            else
                you_are(buf, "");

        } else { /* two-weapon */
            static const char also_[] = "also ";
            char pfx[QBUFSZ], sfx[QBUFSZ],
                sknambuf2[20], sklvlbuf2[20], twobuf[20];
            const char *also = "", *also2 = "", *also3 = (char *) 0,
                       *verb_present, *verb_past;
            int wtype2 = weapon_type(uswapwep),
                sklvl2 = P_SKILL(wtype2),
                twoskl = P_SKILL(P_TWO_WEAPON_COMBAT);
            boolean a1, a2, ab,
                    hav2 = (sklvl2 != P_UNSKILLED && sklvl2 != P_SKILLED);

            /* normally hero must have access to two-weapon skill in
               order to initiate u.twoweap, but not if polymorphed into
               a form which has multiple weapon attacks, so we need to
               avoid getting bitten by unexpected skill value */
            if (twoskl == P_ISRESTRICTED) {
                twoskl = P_UNSKILLED;
                /* restricted is the same as unskilled as far as bonus
                   or penalty goes, and it isn't ordinarily seen so
                   skill_level_name() returns "Unknown" for it */
                Strcpy(twobuf, TR("restricted", "受限"));
            } else {
                (void) lcase(skill_level_name(P_TWO_WEAPON_COMBAT, twobuf));
            }

            /* keep buf[] from above in case skill levels match */
            pfx[0] = sfx[0] = '\0';
            if (twoskl < sklvl) {
                /* twoskil won't be restricted so sklvl is at least basic */
                Sprintf(pfx, TR("Your skill in %s ", "你的%s技能"), skill_name(wtype));
                Sprintf(sfx, TR(" limited by being %s with two weapons", " 受限于双持武器时的%s水平"), twobuf);
                also = also_;
            } else if (twoskl > sklvl) {
                /* sklvl might be restricted */
                Strcpy(pfx, TR("Your two weapon skill ", "你的双持武器技能"));
                Strcpy(sfx, TR(" limited by ", " 受限于"));
                if (sklvl > P_ISRESTRICTED)
                    Sprintf(eos(sfx), TR("being %s", "等级为%s"), sklvlbuf);
                else
                    Sprintf(eos(sfx), TR("having no skill", "毫无技能"));
                Sprintf(eos(sfx), TR(" with %s", "，武器为%s"), skill_name(wtype));
                also2 = also_;
            } else {
                Strcat(buf, TR(" and two weapons", "以及双持武器"));
                also3 = also_;
            }
            if (*pfx)
                enl_msg(pfx, "is", "was", sfx, "");
            else if (hav)
                you_have(buf, "");
            else
                you_are(buf, "");

            /* skip comparison between secondary and two-weapons if it is
               identical to the comparison between primary and twoweap */
            if (wtype2 != wtype) {
                Strcpy(sknambuf2, skill_name(wtype2));
                (void) lcase(skill_level_name(wtype2, sklvlbuf2));
                verb_present = "is", verb_past = "was";
                pfx[0] = sfx[0] = buf[0] = '\0';
                if (twoskl < sklvl2) {
                    /* twoskil is at least unskilled, sklvl2 at least basic */
                    Sprintf(pfx, TR("Your skill in %s ", "你的%s技能"), sknambuf2);
                    Sprintf(sfx, TR(" %slimited by being %s with two weapons", " %s受限于双持武器时的%s水平"),
                            also, twobuf);
                } else if (twoskl > sklvl2) {
                    /* sklvl2 might be restricted */
                    Strcpy(pfx, TR("Your two weapon skill ", "你的双持武器技能"));
                    Sprintf(sfx, TR(" %slimited by ", " %s受限于"), also2);
                    if (sklvl2 > P_ISRESTRICTED)
                        Sprintf(eos(sfx), TR("being %s", "等级为%s"), sklvlbuf2);
                    else
                        Strcat(eos(sfx), TR("having no skill", "毫无技能"));
                    Sprintf(eos(sfx), TR(" with %s", "，武器为%s"), sknambuf2);
                } else {
                    /* equal; two-weapon is at least unskilled, so sklvl2 is
                       too; "you [also] have basic/expert/master/grand-master
                       skill with <skill>" or "you [also] are unskilled/
                       skilled in <skill> */
                    Sprintf(buf, "%s %s %s", sklvlbuf2,
                            hav2 ? "skill with" : "in", sknambuf2);
                    Strcat(buf, TR(" and two weapons", "以及双持武器"));
                    if (also3) {
                        Strcpy(pfx, TR("You also ", "你还"));
                        Snprintf(sfx, sizeof(sfx), " %s", buf), buf[0] = '\0';
                        verb_present = hav2 ? "have" : "are";
                        verb_past = hav2 ? "had" : "were";
                    }
                }
                if (*pfx)
                    enl_msg(pfx, verb_present, verb_past, sfx, "");
                else if (hav2)
                    you_have(buf, "");
                else
                    you_are(buf, "");
            } /* wtype2 != wtype */

            /* if training and available skill credits already allow
               #enhance for any of primary, secondary, or two-weapon,
               tell the player; avoid attempting figure out whether
               spending skill credits enhancing one might make either
               or both of the others become ineligible for enhancement */
            a1 = can_advance(wtype, FALSE);
            a2 = (wtype2 != wtype) ? can_advance(wtype2, FALSE) : FALSE;
            ab = can_advance(P_TWO_WEAPON_COMBAT, FALSE);
            if (a1 || a2 || ab) {
                static const char also_wik_[] = " and also with ";

                /* for just one, the conditionals yield
                   1) "skill with <that one>"; for more than one:
                   2) "skills with <primary> and also with <secondary>" or
                   3) "skills with <primary> and also with two-weapons" or
                   4) "skills with <secondary> and also with two-weapons" or
                   5) "skills with <primary>, <secondary>, and two-weapons"
                   (no 'also's or extra 'with's for case 5); when primary
                   and secondary use the same skill, only cases 1 and 3 are
                   possible because 'a2' gets forced to False above */
                Sprintf(sfx, TR(" skill%s with %s%s%s%s%s", " 技能%s：%s%s%s%s%s"),
                        ((int) a1 + (int) a2 + (int) ab > 1) ? "s" : "",
                        a1 ? skill_name(wtype) : "",
                        ((a1 && a2 && ab) ? ", "
                         : (a1 && (a2 || ab)) ? also_wik_ : ""),
                        a2 ? skill_name(wtype2) : "",
                        ((a1 && a2 && ab) ? ", and "
                         : (a2 && ab) ? also_wik_ : ""),
                        ab ? "two weapons" : "");
                enl_msg(You_, TR("can enhance", "可以提升"), "could have enhanced", sfx, "");
            }
        } /* two-weapon */
    } /* skill applies */
#endif /* ZHLANG */
}

staticfn void
item_resistance_message(
    int adtyp,
    const char *prot_message,
    int final)
{
    int protection = u_adtyp_resistance_obj(adtyp);

#ifdef ZHLANG
    if (protection) {
        boolean somewhat = protection < 99;
        static const struct {
            int adtyp;
            const char *dmg;
        } dmgtypes[] = {
            { AD_FIRE, "火焰" }, { AD_COLD, "寒冷" }, { AD_DISN, "瓦解" },
            { AD_ELEC, "电击" }, { AD_ACID, "酸蚀" }, { 0, (char *) 0 }
        };
        const char *dmg = "伤害";
        int i;

        nhUse(prot_message);
        nhUse(final);
        for (i = 0; dmgtypes[i].dmg; ++i)
            if (dmgtypes[i].adtyp == adtyp)
                dmg = dmgtypes[i].dmg;
        enlght_zh("你的物品%s保护你免受%s伤害%s。",
                  somewhat ? "在一定程度上" : "",
                  dmg, zh_trim(item_what(adtyp)));
    }
#else
    if (protection) {
        boolean somewhat = protection < 99;

        enl_msg(TR("Your items ", "你的物品"),
                somewhat ? "are somewhat" : "are",
                somewhat ? "were somewhat" : "were",
                prot_message, item_what(adtyp));
    }
#endif
}

/* attributes: intrinsics and the like, other non-obvious capabilities */
staticfn void
attributes_enlightenment(
    int unused_mode UNUSED,
    int final)
{
#ifndef ZHLANG
    static NEARDATA const char
        if_surroundings_permitted[] = " if surroundings permitted";
#endif
    int ltmp, armpro, warnspecies;
    char buf[BUFSZ];

    /*\
     *  Attributes
    \*/
    enlght_out("");
    enlght_out(final ? TR("Final Attributes:", "最终属性：") : TR("Attributes:", "属性："));

    if (u.uevent.uhand_of_elbereth) {
        static const char *const hofe_titles[3] = {
#ifdef ZHLANG
            "艾尔贝瑞斯之手", "平衡的使者", "阿廖克的荣耀"
#else
            "the Hand of Elbereth", "the Envoy of Balance",
            "the Glory of Arioch"
#endif
        };
        you_are(hofe_titles[u.uevent.uhand_of_elbereth - 1], "");
    }

#ifdef ZHLANG
    /* 你的信仰程度与阵营 */
    Sprintf(buf, "%s", piousness(TRUE, align_str(u.ualign.type)));
    if (u.ualign.record >= 0)
        you_are(buf, "");
    else
        enlght_zh("你%s。", buf);
#else
    Sprintf(buf, "%s", piousness(TRUE, "aligned"));
    if (u.ualign.record >= 0)
        you_are(buf, "");
    else
        you_have(buf, "");
#endif

    if (wizard) {
        Sprintf(buf, " %d", u.ualign.record);
        enl_msg(TR("Your alignment ", "你的阵营"), "is", TR("was", "曾是"), buf, "");
    }

    /*** Resistances to troubles ***/
    if (Invulnerable)
        you_are(TR("invulnerable", "刀枪不入"), from_what(INVULNERABLE));
    if (Antimagic)
        you_are(TR("magic-protected", "受魔法保护"), from_what(ANTIMAGIC));
    if (Fire_resistance)
        you_are(TR("fire resistant", "具有火焰抗性"), from_what(FIRE_RES));
    item_resistance_message(AD_FIRE, " protected from fire", final);
    if (Cold_resistance)
        you_are(TR("cold resistant", "具有寒冷抗性"), from_what(COLD_RES));
    item_resistance_message(AD_COLD, " protected from cold", final);
    if (Sleep_resistance)
        you_are(TR("sleep resistant", "具有睡眠抗性"), from_what(SLEEP_RES));
    if (Disint_resistance)
        you_are(TR("disintegration resistant", "具有瓦解抗性"),
                from_what(DISINT_RES));
    item_resistance_message(AD_DISN, " protected from disintegration", final);
    if (Shock_resistance)
        you_are(TR("shock resistant", "具有电击抗性"), from_what(SHOCK_RES));
    item_resistance_message(AD_ELEC, " protected from electric shocks",
                            final);
    if (Poison_resistance)
        you_are(TR("poison resistant", "具有毒素抗性"), from_what(POISON_RES));
    if (Acid_resistance) {
#ifdef ZHLANG
        Sprintf(buf, "%s具有酸蚀抗性",
                temp_resist(ACID_RES) ? "暂时" : "");
#else
        Sprintf(buf, "%.20s%.30s",
                temp_resist(ACID_RES) ? "temporarily " : "",
                "acid resistant");
#endif
        you_are(buf, from_what(ACID_RES));
    }
    item_resistance_message(AD_ACID, " protected from acid", final);
    if (Drain_resistance)
        you_are(TR("level-drain resistant", "具有等级吸取抗性"),
                from_what(DRAIN_RES));
    if (Sick_resistance)
        you_are(TR("immune to sickness", "对疾病免疫"), from_what(SICK_RES));
    if (Stone_resistance) {
#ifdef ZHLANG
        Sprintf(buf, "%s具有石化抗性",
                temp_resist(STONE_RES) ? "暂时" : "");
#else
        Sprintf(buf, "%.20s%.30s",
                temp_resist(STONE_RES) ? "temporarily " : "",
                "petrification resistant");
#endif
        you_are(buf, from_what(STONE_RES));
    }
    if (Halluc_resistance)
#ifdef ZHLANG
        enlght_zh("你对幻觉有抗性%s。", zh_trim(from_what(HALLUC_RES)));
#else
        enl_msg(You_, TR("resist", "抵抗"), "resisted", " hallucinations",
                from_what(HALLUC_RES));
#endif
    if (u.uedibility)
        you_can(TR("recognize detrimental food", "辨别有害食物"), "");

    /*** Vision and senses ***/
    if ((HBlinded || EBlinded) && BBlinded) /* blind w/ blindness blocked */
        you_can(TR("see", "看见"), from_what(-BLINDED)); /* Eyes of the Overworld */
    if (Blnd_resist && !Blind) /* skip if no eyes or blindfolded */
        you_are(TR("not subject to light-induced blindness", "不受光线致盲影响"),
                from_what(BLND_RES));
    if (See_invisible) {
        if (!Blind)
#ifdef ZHLANG
            enlght_zh("你能看见隐形的东西%s。", zh_trim(from_what(SEE_INVIS)));
#else
            enl_msg(You_, TR("see", "看见"), "saw", " invisible", from_what(SEE_INVIS));
#endif
        else if (!PermaBlind)
#ifdef ZHLANG
            enlght_zh("只要不失明，你就能看见隐形的东西。");
#else
            enl_msg(You_, TR("will see", "将会看见"), "would have seen",
                    " invisible when not blind", "");
#endif
        else
#ifdef ZHLANG
            enlght_zh("若未失明，你本可看见隐形的东西。");
#else
            enl_msg(You_, TR("would see", "本会看见"), "would have seen",
                    " invisible if not blind", "");
#endif
    }
    if (Blind_telepat)
        you_are(TR("telepathic", "具有心灵感应"), from_what(TELEPAT));
    if (Warning)
        you_are(TR("warned", "获得警示"), from_what(WARNING));
    if (Warn_of_mon && svc.context.warntype.obj) {
        Sprintf(buf, TR("aware of the presence of %s", "能感知%s的存在"),
                (svc.context.warntype.obj & M2_ORC) ? TR("orcs", "兽人")
                : (svc.context.warntype.obj & M2_ELF) ? TR("elves", "精灵")
                  : (svc.context.warntype.obj & M2_DEMON) ? TR("demons", "恶魔")
                    : something);
        you_are(buf, from_what(WARN_OF_MON));
    }
    if (Warn_of_mon && svc.context.warntype.polyd) {
        Sprintf(buf, TR("aware of the presence of %s", "能感知%s的存在"),
                ((svc.context.warntype.polyd & (M2_HUMAN | M2_ELF))
                 == (M2_HUMAN | M2_ELF)) ? TR("humans and elves", "人类和精灵")
                    : (svc.context.warntype.polyd & M2_HUMAN) ? TR("humans", "人类")
                      : (svc.context.warntype.polyd & M2_ELF) ? TR("elves", "精灵")
                        : (svc.context.warntype.polyd & M2_ORC) ? TR("orcs", "兽人")
                          : (svc.context.warntype.polyd & M2_DEMON) ? TR("demons", "恶魔")
                            : TR("certain monsters", "某些怪物"));
        you_are(buf, "");
    }
    warnspecies =  svc.context.warntype.speciesidx;
    if (Warn_of_mon && ismnum(warnspecies)) {
        Sprintf(buf, TR("aware of the presence of %s", "能感知%s的存在"),
                makeplural(mons[warnspecies].pmnames[NEUTRAL]));
        you_are(buf, from_what(WARN_OF_MON));
    }
    if (Undead_warning)
        you_are(TR("warned of undead", "对亡灵有预警"), from_what(WARN_UNDEAD));
    if (Searching)
        you_have(TR("automatic searching", "具有自动搜索能力"),
                 from_what(SEARCHING));
    if (Clairvoyant) {
        you_are(TR("clairvoyant", "拥有千里眼"), from_what(CLAIRVOYANT));
    } else if ((HClairvoyant || EClairvoyant) && BClairvoyant) {
        Strcpy(buf, from_what(-CLAIRVOYANT));
        (void) strsubst(buf, " because of ", " if not for ");
#ifdef ZHLANG
        enlght_zh("你本可以拥有千里眼%s。", zh_trim(buf));
#else
        enl_msg(You_, TR("could be", "本可成为"), "could have been", " clairvoyant", buf);
#endif
    }
    if (Infravision)
        you_have(TR("infravision", "具有红外视觉"), from_what(INFRAVISION));
    if (Detect_monsters) {
        Strcpy(buf, TR("sensing the presence of monsters", "感知怪物的存在"));
        if (wizard) {
            long detectmon_timeout = (HDetect_monsters & TIMEOUT);

            if (detectmon_timeout)
                Sprintf(eos(buf), " (%ld)", detectmon_timeout);
        }
        you_are(buf, "");
    }
    if (u.umconf) { /* 'u.umconf' is a counter rather than a timeout */
#ifdef ZHLANG
        buf[0] = '\0';
        if (wizard && !final) {
            if (u.umconf == 1)
                Strcpy(buf, "（仅限下次击中）");
            else /* u.umconf > 1 */
                Sprintf(buf, "（接下来 %u 次击中）", u.umconf);
        }
        enlght_zh("你击中怪物时会迷惑它们%s。", buf);
#else
        Strcpy(buf, TR(" monsters when hitting them", " 怪物，只要击中它们"));
        if (wizard && !final) {
            if (u.umconf == 1)
                Strcat(buf, TR(" (next hit only)", "（仅限下次击中）"));
            else /* u.umconf > 1 */
                Sprintf(eos(buf), TR(" (next %u hits)", "（接下来 %u 次击中）"), u.umconf);
        }
        enl_msg(You_, TR("will confuse", "会迷惑"), "would have confused", buf, "");
#endif
    }

    /*** Appearance and behavior ***/
    if (Adornment) {
        int adorn = 0;

        if (uleft && uleft->otyp == RIN_ADORNMENT)
            adorn += uleft->spe;
        if (uright && uright->otyp == RIN_ADORNMENT)
            adorn += uright->spe;
        /* the sum might be 0 (+0 ring or two which negate each other);
           that yields "you are charismatic" (which isn't pointless
           because it potentially impacts seduction attacks) */
        Sprintf(buf, TR("%scharismatic", "%s有魅力"),
                (adorn > 0) ? TR("more ", "更")
                            : (adorn < 0) ? TR("less ", "不那么") : "");
        you_are(buf, from_what(ADORNED));
    }
    if (Invisible)
        you_are(TR("invisible", "隐形"), from_what(INVIS));
    else if (Invis)
        you_are(TR("invisible to others", "对他人隐形"), from_what(INVIS));
    /* ordinarily "visible" is redundant; this is a special case for
       the situation when invisibility would be an expected attribute */
    else if ((HInvis || EInvis) && BInvis)
        you_are(TR("visible", "可见"), from_what(-INVIS));
    if (Displaced)
        you_are(TR("displaced", "位置错位"), from_what(DISPLACED));
    if (Stealth) {
        you_are(TR("stealthy", "行动无声"), from_what(STEALTH));
    } else if (BStealth && (HStealth || EStealth)) {
        Sprintf(buf, TR(" stealthy%s", " 行动无声%s"),
                (BStealth == FROMOUTSIDE) ? TR(" if not mounted", "（不骑乘时）") : "");
        enl_msg(You_, TR("would be", "本会"), TR("would have been", "本会已经"), buf, "");
    }
    if (Aggravate_monster)
#ifdef ZHLANG
        enlght_zh("你会激怒怪物%s。", zh_trim(from_what(AGGRAVATE_MONSTER)));
#else
        enl_msg(TR("You aggravate", "你会激怒"), "", "d", " monsters",
                from_what(AGGRAVATE_MONSTER));
#endif
    if (Conflict)
#ifdef ZHLANG
        enlght_zh("你会引发冲突%s。", zh_trim(from_what(CONFLICT)));
#else
        enl_msg(TR("You cause", "你会引发"), "", "d", " conflict", from_what(CONFLICT));
#endif

    /*** Transportation ***/
    if (Jumping)
        you_can(TR("jump", "跳跃"), from_what(JUMPING));
    if (Teleportation)
        you_can(TR("teleport", "传送"), from_what(TELEPORT));
    if (Teleport_control)
        you_have(TR("teleport control", "传送控制"), from_what(TELEPORT_CONTROL));
    /* actively levitating handled earlier as a status condition */
    if (BLevitation) { /* levitation is blocked */
        long save_BLev = BLevitation;

        BLevitation = 0L;
        if (Levitation) {
            /* either trapped in the floor or inside solid rock
               (or both if chained to buried iron ball and have
               moved one step into solid rock somehow) */
            boolean trapped = (save_BLev & I_SPECIAL) != 0L,
                    terrain = (save_BLev & FROMOUTSIDE) != 0L;

#ifdef ZHLANG
            if (trapped && terrain)
                enlght_zh("若没有被困住且周围环境允许，你本可漂浮。");
            else if (trapped)
                enlght_zh("若没有被困住，你本可漂浮。");
            else if (terrain)
                enlght_zh("若周围环境允许，你本可漂浮。");
            else
                enlght_zh("你本可漂浮。");
#else
            Sprintf(buf, "%s%s%s",
                    trapped ? " if not trapped" : "",
                    (trapped && terrain) ? " and" : "",
                    terrain ? if_surroundings_permitted : "");
            enl_msg(You_, TR("would levitate", "本可漂浮"), "would have levitated", buf, "");
#endif
        }
        BLevitation = save_BLev;
    }
    /* actively flying handled earlier as a status condition */
    if (BFlying) { /* flight is blocked */
        long save_BFly = BFlying;

        BFlying = 0L;
        if (Flying) {
#ifdef ZHLANG
            if (Levitation)
                enlght_zh("若你没在漂浮，你本可飞行。");
            else if (save_BFly == I_SPECIAL)
                enlght_zh("若没有被困住，你本可飞行。");
            else if (save_BFly == FROMOUTSIDE)
                enlght_zh("若周围环境允许，你本可飞行。");
            else
                enlght_zh("若情况允许，你本可飞行。");
#else
            enl_msg(You_, TR("would fly", "本可飞行"), "would have flown",
                    /* wording quibble: for past tense, "hadn't been"
                       would sound better than "weren't" (and
                       "had permitted" better than "permitted"), but
                       "weren't" and "permitted" are adequate so the
                       extra complexity to handle that isn't worth it */
                    Levitation
                       ? " if you weren't levitating"
                       : (save_BFly == I_SPECIAL)
                          /* this is an oversimplification; being trapped
                             might also be blocking levitation so flight
                             would still be blocked after escaping trap */
                          ? " if you weren't trapped"
                          : (save_BFly == FROMOUTSIDE)
                             ? if_surroundings_permitted
                             /* two or more of levitation, surroundings,
                                and being trapped in the floor */
                             : " if circumstances permitted",
                    "");
#endif
        }
        BFlying = save_BFly;
    }
    /* including this might bring attention to the fact that ceiling
       clinging has inconsistencies... */
    if (is_clinger(gy.youmonst.data)) {
        boolean has_lid = has_ceiling(&u.uz);

        if (has_lid && !u.uinwater) {
            you_can(TR("cling to the ceiling", "紧贴天花板"), "");
        } else {
#ifdef ZHLANG
            if (!has_lid && u.uinwater)
                enlght_zh("若天花板存在且你不在水中，你本可紧贴天花板。");
            else if (!has_lid)
                enlght_zh("若天花板存在，你本可紧贴天花板。");
            else if (Underwater)
                enlght_zh("若你未在水下，你本可紧贴天花板。");
            else
                enlght_zh("若你不在水中，你本可紧贴天花板。");
#else
            Sprintf(buf, TR(" to the ceiling if %s%s%s", " 天花板上，如果%s%s%s"),
                    !has_lid ? "there was one" : "",
                    (!has_lid && u.uinwater) ? " and " : "",
                    u.uinwater ? (Underwater ? "you weren't underwater"
                                  : "you weren't in the water") : "");
            /* past tense is applicable for death while Unchanging */
            enl_msg(You_, TR("could cling", "本可紧贴"), "could have clung", buf, "");
#endif
        }
    }
    /* actively walking on water handled earlier as a status condition */
    if (Wwalking && !walking_on_water())
        you_can(TR("walk on water", "在水面行走"), from_what(WWALKING));
    /* actively swimming (in water but not under it) handled earlier */
    if (Swimming && (Underwater || !u.uinwater))
        you_can(TR("swim", "游泳"), from_what(SWIMMING));
    if (Breathless)
        you_can(TR("survive without air", "无需空气而存活"), from_what(MAGICAL_BREATHING));
    else if (Amphibious)
        you_can(TR("breathe water", "在水下呼吸"), from_what(MAGICAL_BREATHING));
    if (Passes_walls)
        you_can(TR("walk through walls", "穿墙行走"), from_what(PASSES_WALLS));

    /*** Physical attributes ***/
    if (Regeneration)
#ifdef ZHLANG
        enlght_zh("你会再生%s。", zh_trim(from_what(REGENERATION)));
#else
        enl_msg(TR("You regenerate", "你会再生"), "", "d", "", from_what(REGENERATION));
#endif
    if (Slow_digestion)
#ifdef ZHLANG
        enlght_zh("你消化得更慢%s。", zh_trim(from_what(SLOW_DIGESTION)));
#else
        you_have(TR("slower digestion", "消化更慢"), from_what(SLOW_DIGESTION));
#endif
    if (u.uhitinc) {
        (void) enlght_combatinc(TR("to hit", "命中"), u.uhitinc, final, buf);
#ifdef ZHLANG
        if (iflags.tux_penalty && !Upolyd)
            Sprintf(eos(buf), "（%s护甲惩罚）",
                    (u.uhitinc < 0) ? "增加"
                    : (u.uhitinc < 4 * gu.urole.spelarmr / 5)
                      ? "部分抵消"
                      : (u.uhitinc < gu.urole.spelarmr) ? "几乎抵消"
                        : "克服");
        enlght_zh("你%s。", buf);
#else
        if (iflags.tux_penalty && !Upolyd)
            Sprintf(eos(buf), TR(" %s your suit's penalty", " %s护甲惩罚"),
                    (u.uhitinc < 0) ? "increasing"
                    : (u.uhitinc < 4 * gu.urole.spelarmr / 5)
                      ? "partly offsetting"
                      : (u.uhitinc < gu.urole.spelarmr) ? "nearly offsetting"
                        : "overcoming");
        you_have(buf, "");
#endif
    }
    if (u.udaminc)
        you_have(enlght_combatinc(TR("damage", "伤害"), u.udaminc, final, buf), "");
    if (u.uspellprot || Protection) {
        int prot = 0;

        if (uleft && uleft->otyp == RIN_PROTECTION)
            prot += uleft->spe;
        if (uright && uright->otyp == RIN_PROTECTION)
            prot += uright->spe;
        if (uamul && uamul->otyp == AMULET_OF_GUARDING)
            prot += 2;
        if (HProtection & INTRINSIC)
            prot += u.ublessed;
        prot += u.uspellprot;
        if (prot)
            you_have(enlght_combatinc(TR("defense", "防御"), prot, final, buf), "");
    }
    if ((armpro = magic_negation(&gy.youmonst)) > 0) {
        /* magic cancellation factor, conferred by worn armor */
#ifdef ZHLANG
        if (armpro > 3)
            armpro = 3;
        enlght_zh("你的魔法抵御为%s。",
                  (armpro == 1) ? "有防护"
                  : (armpro == 2) ? "受守护" : "受保护");
#else
        static const char *const mc_types[] = {
            "" /*ordinary*/, "warded", "guarded", "protected",
        };
        /* sanity check */
        if (armpro >= SIZE(mc_types))
            armpro = SIZE(mc_types) - 1;
        you_are(mc_types[armpro], "");
#endif
    }
    if (Half_physical_damage)
        enlght_halfdmg(HALF_PHDAM, final);
    if (Half_spell_damage)
        enlght_halfdmg(HALF_SPDAM, final);
    if (Half_gas_damage)
#ifdef ZHLANG
        enlght_zh("你受到的毒气伤害减少了。");
#else
        enl_msg(You_, TR("take", "受到"), "took", " reduced poison gas damage", "");
#endif
    if (spellid(0) > NO_SPELL) { /* skip if no spells are known yet */
        /* greatly simplified edition of percent_success(spell.c)--may need
           to be suppressed if oversimplification leads to player confusion */
        char cast_adj[QBUFSZ];
        boolean suit = uarm && is_metallic(uarm),
                robe = uarmc && uarmc->otyp == ROBE;

        *cast_adj = '\0';
        if (suit) /* omit "wearing" to shorten the text */
            Sprintf(cast_adj, TR(" impaired by metallic armor%s", "因金属护甲而受损%s"),
                    robe ? TR(", mitigated by your robe", "，长袍减轻了影响") : "");
        else if (robe)
            Strcpy(cast_adj, TR(" enhanced by wearing a robe", "因穿着长袍而增强"));

        if (*cast_adj)
            enl_msg(TR("Your spell casting ", "你的施法能力"), "is", TR("was", "曾是"), cast_adj, "");
    }
    /* polymorph and other shape change */
    if (Protection_from_shape_changers)
        you_are(TR("protected from shape changers", "免受变形者伤害"),
                from_what(PROT_FROM_SHAPE_CHANGERS));
    if (Unchanging) {
        const char *what = 0;

        if (!Upolyd) /* Upolyd handled below after current form */
            you_can(TR("not change from your current form", "保持当前形态不变"),
                    from_what(UNCHANGING));
        /* blocked shape changes */
        if (Polymorph)
            what = !final ? "polymorph" : "have polymorphed";
        else if (ismnum(u.ulycn))
            what = !final ? "change shape" : "have changed shape";
        if (what) {
#ifdef ZHLANG
            nhUse(what);
            enlght_zh("若不是被锁定在当前形态，你本会定期变形。");
#else
            Sprintf(buf, TR("would %s periodically", "会定期%s"), what);
            /* omit from_what(UNCHANGING); too verbose */
            enl_msg(You_, buf, buf, TR(" if not locked into your current form", "，前提是未被锁定在当前形态"),
                    "");
#endif
        }
    } else if (Polymorph) {
        you_are(TR("polymorphing periodically", "定期变形"), from_what(POLYMORPH));
    }
    if (Polymorph_control)
        you_have(TR("polymorph control", "变形控制"), from_what(POLYMORPH_CONTROL));
    if (Upolyd && u.umonnum != u.ulycn
        /* if we've died from turning into slime, we're polymorphed
           right now but don't want to list it as a temporary attribute
           [we need a more reliable way to detect this situation] */
        && !(final == ENL_GAMEOVERDEAD
             && u.umonnum == PM_GREEN_SLIME && !Unchanging)) {
        /* foreign shape (except were-form which is handled below) */
        if (!vampshifted(&gy.youmonst))
            Sprintf(buf, TR("polymorphed into %s", "已变形为%s"),
                    an(pmname(gy.youmonst.data,
                              flags.female ? FEMALE : MALE)));
        else
            Sprintf(buf, TR("polymorphed into %s in %s form", "已变形为%s，形态为%s"),
                    an(pmname(&mons[gy.youmonst.cham],
                              flags.female ? FEMALE : MALE)),
                    pmname(gy.youmonst.data, flags.female ? FEMALE : MALE));
        if (wizard)
            Sprintf(eos(buf), " (%d)", u.mtimedone);
        you_are(buf, "");
    }
    if (lays_eggs(gy.youmonst.data) && flags.female) /* Upolyd */
        you_can(TR("lay eggs", "产卵"), "");
    if (ismnum(u.ulycn)) {
        /* "you are a werecreature [in beast form]" */
        Strcpy(buf, an(pmname(&mons[u.ulycn],
               flags.female ? FEMALE : MALE)));
        if (u.umonnum == u.ulycn) {
            Strcat(buf, TR(" in beast form", "，处于野兽形态"));
            if (wizard)
                Sprintf(eos(buf), " (%d)", u.mtimedone);
        }
        you_are(buf, "");
    }
    if (Unchanging && Upolyd) /* !Upolyd handled above */
        you_can(TR("not change from your current form", "保持当前形态不变"), from_what(UNCHANGING));
    if (Hate_silver)
        you_are(TR("harmed by silver", "会被银伤害"), "");
    /* movement and non-armor-based protection */
    if (Fast)
        you_are(Very_fast ? TR("very fast", "速度极快") : TR("fast", "速度很快"),
                from_what(FAST));
    if (Reflecting)
        you_have(TR("reflection", "具有反射能力"), from_what(REFLECTING));
    if (Free_action)
        you_have(TR("free action", "自由行动"), from_what(FREE_ACTION));
    if (Fixed_abil)
        you_have(TR("fixed abilities", "固定的属性"), from_what(FIXED_ABIL));
    if (Lifesaved)
#ifdef ZHLANG
        enlght_zh("你的生命将被拯救。");
#else
        enl_msg(TR("Your life ", "你的生命"), "will be", "would have been", " saved", "");
#endif

    /*** Miscellany ***/
#ifdef ZHLANG
    if (Luck) {
        ltmp = abs((int) Luck);

        if (Luck < 0)
            Sprintf(buf, "%s不幸运",
                    ltmp >= 10 ? "极其" : ltmp >= 5 ? "非常" : "有些");
        else
            Sprintf(buf, "%s幸运",
                    ltmp >= 10 ? "极其" : ltmp >= 5 ? "非常" : "");
        if (wizard)
            Sprintf(eos(buf), "（%d）", Luck);
        you_are(buf, "");
    } else if (wizard)
        enlght_zh("你的运气是零。");
    if (u.moreluck > 0)
        you_have(TR("extra luck", "额外幸运"), "");
    else if (u.moreluck < 0)
        enlght_zh("你的运气减少了。");
    if (carrying(LUCKSTONE) || stone_luck(TRUE)) {
        ltmp = stone_luck(FALSE);
        if (ltmp <= 0)
            enlght_zh("坏运气不会随时间消退。");
        if (ltmp >= 0)
            enlght_zh("好运气不会随时间消退。");
    }
#else
    if (Luck) {
        ltmp = abs((int) Luck);
        Sprintf(buf, TR("%s%slucky", "%s%s幸运"),
                ltmp >= 10 ? "extremely " : ltmp >= 5 ? "very " : "",
                Luck < 0 ? "un" : "");
        if (wizard)
            Sprintf(eos(buf), " (%d)", Luck);
        you_are(buf, "");
    } else if (wizard)
        enl_msg(TR("Your luck ", "你的运气"), "is", "was", " zero", "");
    if (u.moreluck > 0)
        you_have(TR("extra luck", "额外幸运"), "");
    else if (u.moreluck < 0)
        you_have(TR("reduced luck", "幸运减少"), "");
    if (carrying(LUCKSTONE) || stone_luck(TRUE)) {
        ltmp = stone_luck(FALSE);
        if (ltmp <= 0)
            enl_msg(TR("Bad luck ", "坏运气"), "does", "did", " not time out for you", "");
        if (ltmp >= 0)
            enl_msg(TR("Good luck ", "好运气"), "does", "did", " not time out for you", "");
    }
#endif

    if (u.ugangr) {
#ifdef ZHLANG
        Sprintf(buf, "%s%s",
                u.ugangr > 6 ? "极其" : u.ugangr > 3 ? "非常" : "",
                "对你发怒");
        if (wizard)
            Sprintf(eos(buf), "（%d）", u.ugangr);
        enlght_zh("%s%s。", u_gname(), buf);
#else
        Sprintf(buf, TR(" %sangry with you", " %s对你发怒"),
                u.ugangr > 6 ? "extremely " : u.ugangr > 3 ? "very " : "");
        if (wizard)
            Sprintf(eos(buf), " (%d)", u.ugangr);
        enl_msg(u_gname(), " is", " was", buf, "");
#endif
    } else {
        /*
         * We need to suppress this when the game is over, because death
         * can change the value calculated by can_pray(), potentially
         * resulting in a false claim that you could have prayed safely.
         */
        if (!final) {
#if 0
            /* "can [not] safely pray" vs "could [not] have safely prayed" */
            Sprintf(buf, TR("%s%ssafely pray%s", "%s%s安全地祈祷%s"), can_pray(FALSE) ? "" : TR("not ", "不"),
                    final ? TR("have ", "已经") : "", final ? "ed" : "");
#else
#ifdef ZHLANG
            if (can_pray(FALSE))
                Strcpy(buf, "能安全地祈祷");
            else
                Strcpy(buf, "不能安全地祈祷");
            if (wizard)
                Sprintf(eos(buf), "（%d）", u.ublesscnt);
            enlght_zh("你%s。", buf);
#else
            Sprintf(buf, TR("%ssafely pray", "%s安全地祈祷"), can_pray(FALSE) ? "" : "not ");
            if (wizard)
                Sprintf(eos(buf), " (%d)", u.ublesscnt);
            you_can(buf, "");
#endif
#endif
        }
    }

#ifdef DEBUG
    /* named fruit debugging (doesn't really belong here...); to enable,
       include 'fruit' in DEBUGFILES list (even though it isn't a file...) */
    if (wizard && explicitdebug("fruit")) {
        struct fruit *f;

        reorder_fruit(TRUE); /* sort by fruit index, from low to high;
                              * this modifies the gf.ffruit chain, so could
                              * possibly mask or even introduce a problem,
                              * but it does useful sanity checking */
        for (f = gf.ffruit; f; f = f->nextf) {
            Sprintf(buf, TR("Fruit #%d ", "水果 #%d "), f->fid);
            enl_msg(buf, "is ", TR("was ", "曾是"), f->fname, "");
        }
        enl_msg(TR("The current fruit ", "当前水果"), "is ", TR("was ", "曾是"), svp.pl_fruit, "");
        Sprintf(buf, "%d", flags.made_fruit);
        enl_msg(TR("The made fruit flag ", "已生成水果标志"), "is ", TR("was ", "曾是"), buf, "");
    }
#endif

    {
#ifdef ZHLANG
        if (final < 2) { /* still in progress, or quit/escaped/ascended */
            if (!u.umortality)
                enlght_zh(final ? "你活下来了。" : "你还活着。");
            else {
                char nbuf[40];

                enlght_zh("你曾被杀%s，但活了下来。",
                          N_times((long) u.umortality, nbuf));
            }
        } else if (u.umortality <= 1) {
            enlght_zh("你已死亡。");
        } else {
            enlght_zh("你已死亡（第 %ld 次！）。", u.umortality);
        }
#else
        const char *p;

        buf[0] = '\0';
        if (final < 2) { /* still in progress, or quit/escaped/ascended */
            p = "survived after being killed ";
            if (!u.umortality)
                p = !final ? (char *) 0 : "survived";
            else
                (void) N_times((long) u.umortality, buf);
        } else { /* game ended in character's death */
            p = "are dead";
            switch (u.umortality) {
            case 0:
                impossible("dead without dying?");
                FALLTHROUGH;
                /* FALLTHRU */
            case 1:
                break; /* just "are dead" */
            default:
                Sprintf(buf, TR(" (%d%s time!)", "（第 %d%s 次！）"), u.umortality,
                        ordin(u.umortality));
                break;
            }
        }
        if (p)
            enl_msg(You_, TR("have been killed ", "曾被杀"), p, buf, "");
#endif
    }
}

/* ^X command */
int
doattributes(void)
{
    int mode = BASICENLIGHTENMENT;

    /* show more--as if final disclosure--for wizard and explore modes */
    if (wizard || discover)
        mode |= MAGICENLIGHTENMENT;

    enlightenment(mode, ENL_GAMEINPROGRESS);
    return ECMD_OK;
}

void
youhiding(boolean via_enlghtmt, /* enlightenment line vs topl message */
          int msgflag)          /* for variant message phrasing */
{
    char *bp, buf[BUFSZ];

    Strcpy(buf, TR("hiding", "躲藏"));
    if (U_AP_TYPE != M_AP_NOTHING) {
        /* mimic; hero is only able to mimic a strange object or gold
           or hallucinatory alternative to gold, so we skip the details
           for the hypothetical furniture and monster cases */
        bp = eos(strcpy(buf, "mimicking"));
        if (U_AP_TYPE == M_AP_OBJECT) {
            Sprintf(bp, " %s", an(simple_typename(gy.youmonst.mappearance)));
        } else if (U_AP_TYPE == M_AP_FURNITURE) {
            Strcpy(bp, TR(" something", " 某个东西"));
        } else if (U_AP_TYPE == M_AP_MONSTER) {
            Strcpy(bp, TR(" someone", " 某个身影"));
        } else {
            ; /* something unexpected; leave 'buf' as-is */
        }
    } else if (u.uundetected) {
        bp = eos(buf); /* points past "hiding" */
        if (gy.youmonst.data->mlet == S_EEL) {
            if (is_pool(u.ux, u.uy))
                Sprintf(bp, TR(" in the %s", " 在%s中"), waterbody_name(u.ux, u.uy));
        } else if (hides_under(gy.youmonst.data)) {
            struct obj *o = svl.level.objects[u.ux][u.uy];

            if (o)
                Sprintf(bp, TR(" underneath %s", " 在%s下面"), ansimpleoname(o));
        } else if (is_clinger(gy.youmonst.data) || Flying) {
            /* Flying: 'lurker above' hides on ceiling but doesn't cling */
            Sprintf(bp, TR(" on the %s", " 在%s上"), ceiling(u.ux, u.uy));
        } else {
            /* on floor; is_hider() but otherwise not special: 'trapper' */
            if (u.utrap && u.utraptype == TT_PIT) {
                struct trap *t = t_at(u.ux, u.uy);

                Sprintf(bp, TR(" in a %spit", " 在%s坑里"),
                        (t && t->ttyp == SPIKED_PIT) ? TR("spiked ", "带刺") : "");
            } else
                Sprintf(bp, TR(" on the %s", " 在%s上"), surface(u.ux, u.uy));
        }
    } else {
        ; /* shouldn't happen; will result in generic "you are hiding" */
    }

    if (via_enlghtmt) {
        int final = msgflag; /* 'final' is used by you_are() macro */

        you_are(buf, "");
    } else {
        /* for dohide(), when player uses '#monster' command */
        You(TR("are %s %s.", "%s%s。"), msgflag ? TR("already", "已经") : TR("now", "现在"), buf);
    }
}

/* #conduct command [KMH]; shares enlightenment's tense handling */
int
doconduct(void)
{
    show_conduct(ENL_GAMEINPROGRESS);
    return ECMD_OK;
}

/* display conducts; for doconduct(), also disclose() and dump_everything() */
void
show_conduct(int final)
{
    char buf[BUFSZ], bufN[40];
    int ngenocided;

    /* Create the conduct window */
    ge.en_win = create_nhwindow(NHW_MENU);
    putstr(ge.en_win, 0, TR("Voluntary challenges:", "自愿挑战："));

    /* rerolling; "You <this or that>" is about the character, rerolling
       is about the player so phrase it differently;
       also, always use past tense since the chance to do something with it
       is gone by time player can issue #conduct command or see disclosure */
    if (!u.uroleplay.reroll)
        Strcpy(buf, TR(" Character rerolling was not enabled.", " 未启用角色重投功能。"));
    else if (!u.uroleplay.numrerolls)
        Strcpy(buf, TR(" Your character was not rerolled.", " 你的角色未被重投。"));
    else
        Sprintf(buf, TR(" Your character was rerolled %s.", " 你的角色被重投了 %s。"),
                N_times(u.uroleplay.numrerolls, bufN));
    enlght_out(buf);

    if (u.uroleplay.blind)
#ifdef ZHLANG
        enlght_zh("你生来就失明。");
#else
        you_have_been(TR("blind from birth", "天生失明"));
#endif
    if (u.uroleplay.deaf)
#ifdef ZHLANG
        enlght_zh("你生来就耳聋。");
#else
        you_have_been(TR("deaf from birth", "天生耳聋"));
#endif
    /* note: we don't report "you are without possessions" unless the
       game started with the pauper option set */
    if (u.uroleplay.pauper)
#ifdef ZHLANG
        enlght_zh(gi.invent ? "你开局时没有任何财产。" : "你没有携带任何财产。");
#else
        enl_msg(You_, gi.invent ? TR("started", "开局时") : "are", "started out",
                " without possessions", "");
#endif
    /* nudist is far more than a subset of possessionless, and a much
       more impressive accomplishment, but showing "started out without
       possessions" before "faithfully nudist" looks more logical */
    if (u.uroleplay.nudist)
        you_have_been(TR("faithfully nudist", "忠实的裸体主义者"));

    if (!u.uconduct.food)
#ifdef ZHLANG
        enlght_zh("你一直没有进食。");
#else
        enl_msg(You_, TR("have gone", "一直"), "went", " without food", "");
#endif
        /* but beverages are okay */
    else if (!u.uconduct.unvegan)
        you_have_X(TR("followed a strict vegan diet", "一直严格奉行纯素食"));
    else if (!u.uconduct.unvegetarian)
        you_have_been(TR("vegetarian", "素食者"));

    if (!u.uconduct.gnostic)
        you_have_been(TR("an atheist", "无神论者"));

    if (!u.uconduct.weaphit) {
#ifdef ZHLANG
        enlght_zh("你从未用武器击中过任何东西。");
#else
        you_have_never("hit with a wielded weapon");
#endif
    } else if (wizard) {
        Sprintf(buf, TR("hit with a wielded weapon %ld time%s", "用持有的武器击中过 %ld 次%s"),
                u.uconduct.weaphit, plur(u.uconduct.weaphit));
        you_have_X(buf);
    }
    if (!u.uconduct.killer)
        you_have_been(TR("a pacifist", "和平主义者"));

    if (!u.uconduct.literate) {
        you_have_been(TR("illiterate", "文盲"));
    } else if (wizard) {
        Sprintf(buf, TR("read items or engraved %ld time%s", "阅读物品或刻写过 %ld 次%s"), u.uconduct.literate,
                plur(u.uconduct.literate));
        you_have_X(buf);
    }

    if (!u.uconduct.pets)
#ifdef ZHLANG
        enlght_zh("你从未养过宠物。");
#else
        you_have_never("had a pet");
#endif

    ngenocided = num_genocides();
    if (ngenocided == 0) {
#ifdef ZHLANG
        enlght_zh("你从未灭绝过任何怪物。");
#else
        you_have_never("genocided any monsters");
#endif
    } else {
        Sprintf(buf, TR("genocided %d type%s of monster%s", "灭绝了 %d 种怪物%s%s"), ngenocided,
                plur(ngenocided), plur(ngenocided));
        you_have_X(buf);
    }

    if (!u.uconduct.polypiles) {
#ifdef ZHLANG
        enlght_zh("你从未变形过物品。");
#else
        you_have_never("polymorphed an object");
#endif
    } else if (wizard) {
        Sprintf(buf, TR("polymorphed %ld item%s", "变形过 %ld 件物品%s"), u.uconduct.polypiles,
                plur(u.uconduct.polypiles));
        you_have_X(buf);
    }

    if (!u.uconduct.polyselfs) {
#ifdef ZHLANG
        enlght_zh("你从未改变过形态。");
#else
        you_have_never("changed form");
#endif
    } else if (wizard) {
        Sprintf(buf, TR("changed form %ld time%s", "改变过 %ld 次形态%s"), u.uconduct.polyselfs,
                plur(u.uconduct.polyselfs));
        you_have_X(buf);
    }

    if (!u.uconduct.wishes) {
        you_have_X(TR("used no wishes", "没有许过愿"));
    } else {
#ifdef ZHLANG
        Sprintf(buf, TR("used %ld wish%s", "许过 %ld 个愿望%s"),
                u.uconduct.wishes, "");
#else
        Sprintf(buf, TR("used %ld wish%s", "许过 %ld 个愿望%s"), u.uconduct.wishes,
                (u.uconduct.wishes > 1L) ? "es" : "");
#endif
        if (u.uconduct.wisharti) {
            /* if wisharti == wishes
             *  1 wish (for an artifact)
             *  2 wishes (both for artifacts)
             *  N wishes (all for artifacts)
             * else (N is at least 2 in order to get here; M < N)
             *  N wishes (1 for an artifact)
             *  N wishes (M for artifacts)
             */
#ifdef ZHLANG
            if (u.uconduct.wisharti == u.uconduct.wishes)
                Sprintf(eos(buf), "（%s",
                        (u.uconduct.wisharti > 2L)
                            ? "全部"
                            : (u.uconduct.wisharti == 2L) ? "两者" : "");
            else
                Sprintf(eos(buf), "（其中 %ld 个", u.uconduct.wisharti);
            Sprintf(eos(buf), "为了神器）");
#else
            if (u.uconduct.wisharti == u.uconduct.wishes)
                Sprintf(eos(buf), " (%s",
                        (u.uconduct.wisharti > 2L) ? "all "
                          : (u.uconduct.wisharti == 2L) ? "both " : "");
            else
                Sprintf(eos(buf), " (%ld ", u.uconduct.wisharti);

            Sprintf(eos(buf), TR("for %s)", "为了%s）"),
                    (u.uconduct.wisharti == 1L) ? "an artifact"
                                                : "artifacts");
#endif
        }
        you_have_X(buf);

        if (!u.uconduct.wisharti)
#ifdef ZHLANG
            enlght_zh("你未曾为了任何神器许愿。");
#else
            enl_msg(You_, TR("have not wished", "未曾许愿"), "did not wish",
                    " for any artifacts", "");
#endif
    }

    /* only report Sokoban conduct if the Sokoban branch has been entered */
    if (sokoban_in_play()) {
#ifdef ZHLANG
        if (!u.uconduct.sokocheat) {
            enlght_zh("你遵守了索科班的所有特殊规则。");
        } else {
            enlght_zh("你违反了索科班特殊规则 %s。",
                      N_times(u.uconduct.sokocheat, bufN));
        }
#else
        const char *presentverb = "have violated", *pastverb = "violated";

        if (!u.uconduct.sokocheat) {
            presentverb = "have not violated";
            pastverb = "did not violate";
            Strcpy(buf, TR(" any of the special Sokoban rules", " 索科班的任何特殊规则"));
        } else {
            Strcpy(buf, TR(" the special Sokoban rules ", " 索科班特殊规则"));
            Strcat(buf, N_times(u.uconduct.sokocheat, bufN));
        }
        enl_msg(You_, presentverb, pastverb, buf, "");
#endif
    }

    show_achievements(final);

    /* Pop up the window and wait for a key */
    display_nhwindow(ge.en_win, TRUE);
    destroy_nhwindow(ge.en_win);
    ge.en_win = WIN_ERR;
}

/*
 *      Achievements (see 'enum achievements' in you.h).
 */

staticfn void
show_achievements(
    int final) /* 'final' is used "behind the curtain" by enl_foo() macros */
{
    int i, achidx, absidx, acnt;
    char title[QBUFSZ], buf[QBUFSZ];
    winid awin = WIN_ERR;

    /* unfortunately we can't show the achievements (at least not all of
       them) while the game is in progress because it would give away the
       ID of luckstone (at Mine's End) and of real Amulet of Yendor */
    if (!final && !wizard)
        return;

    /* first, figure whether any achievements have been accomplished
       so that we don't show the header for them if the resulting list
       below it would be empty */
    if ((acnt = count_achievements()) == 0)
        return;

    if (ge.en_win != WIN_ERR) {
        awin = ge.en_win; /* end of game disclosure window */
        putstr(awin, 0, "");
    } else {
        awin = create_nhwindow(NHW_MENU);
    }
    Sprintf(title, TR("Achievement%s:", "成就%s："), plur(acnt));
    putstr(awin, 0, title);

    /* display achievements in the order in which they were recorded;
       lone exception is to defer the Amulet if we just ascended;
       it warrants alternate wording when given away during ascension,
       but the Amulet achievement is always attained before entering
       endgame and the alternate wording looks strange if shown before
       "reached endgame" and "reached Astral" */
    if (remove_achievement(ACH_UWIN)) { /* UWIN == Ascended! */
        /* for ascension, force it to be last and Amulet next to last
           by taking them out and then adding them back */
        if (remove_achievement(ACH_AMUL)) /* should always be True here */
            record_achievement(ACH_AMUL);
        record_achievement(ACH_UWIN);
    }
    for (i = 0; i < acnt; ++i) {
        achidx = u.uachieved[i];
        absidx = abs(achidx);

        switch (absidx) {
        case ACH_BLND:
#ifdef ZHLANG
            enlght_zh("你在看不见的情况下探索过。");
#else
            enl_msg(You_, TR("are exploring", "正在探索"), "explored",
                    " without being able to see", "");
#endif
            break;
        case ACH_NUDE:
#ifdef ZHLANG
            enlght_zh("你一直未穿戴任何护甲。");
#else
            enl_msg(You_, TR("have gone", "一直"), "went", " without any armor", "");
#endif
            break;
        case ACH_MINE:
            you_have_X(TR("entered the Gnomish Mines", "进入过侏儒矿坑"));
            break;
        case ACH_TOWN:
            you_have_X(TR("entered Minetown", "进入过矿镇"));
            break;
        case ACH_SHOP:
            you_have_X(TR("entered a shop", "进入过商店"));
            break;
        case ACH_TMPL:
            you_have_X(TR("entered a temple", "进入过神庙"));
            break;
        case ACH_ORCL:
            you_have_X(TR("consulted the Oracle of Delphi", "咨询过德尔斐神谕者"));
            break;
        case ACH_NOVL:
            you_have_X(TR("read from a Discworld novel", "读过碟形世界小说"));
            break;
        case ACH_SOKO:
            you_have_X(TR("entered Sokoban", "进入过索科班"));
            break;
        case ACH_SOKO_PRIZE: /* hard to reach guaranteed bag or amulet */
            you_have_X(TR("completed Sokoban", "完成了索科班"));
            break;
        case ACH_MINE_PRIZE: /* hidden guaranteed luckstone */
            you_have_X(TR("completed the Gnomish Mines", "完成了侏儒矿坑"));
            break;
        case ACH_BGRM:
            you_have_X(TR("entered the Big Room", "进入过大房间"));
            break;
        case ACH_MEDU:
            you_have_X(TR("defeated Medusa", "击败了美杜莎"));
            break;
        case ACH_TUNE:
#ifdef ZHLANG
            you_have_X("学会了开关城堡吊桥的曲调");
#else
            you_have_X(
                "learned the tune to open and close the Castle's drawbridge");
#endif
            break;
        case ACH_BELL:
            /* alternate phrasing for present vs past and also for
               possessing the item vs once held it */
#ifdef ZHLANG
            enlght_zh(u.uhave.bell ? "你拥有开启之铃。" : "你曾持有过开启之铃。");
#else
            enl_msg(You_,
                    u.uhave.bell ? "have" : "have handled",
                    u.uhave.bell ? "had" : "handled",
                    " the Bell of Opening", "");
#endif
            break;
        case ACH_HELL:
#ifdef ZHLANG
            enlght_zh("你进入了盖亨农。");
#else
            enl_msg(You_, TR("have ", "已经"), "", "entered Gehennom", "");
#endif
            break;
        case ACH_CNDL:
#ifdef ZHLANG
            enlght_zh(u.uhave.menorah ? "你拥有召唤烛台。" : "你曾持有过召唤烛台。");
#else
            enl_msg(You_,
                    u.uhave.menorah ? "have" : "have handled",
                    u.uhave.menorah ? "had" : "handled",
                    " the Candelabrum of Invocation", "");
#endif
            break;
        case ACH_BOOK:
#ifdef ZHLANG
            enlght_zh(u.uhave.book ? "你拥有死亡之书。" : "你曾持有过死亡之书。");
#else
            enl_msg(You_,
                    u.uhave.book ? "have" : "have handled",
                    u.uhave.book ? "had" : "handled",
                    " the Book of the Dead", "");
#endif
            break;
        case ACH_INVK:
            you_have_X(TR("gained access to Moloch's Sanctum", "得以进入摩洛克圣所"));
            break;
        case ACH_AMUL:
            /* alternate wording for ascended (always past tense) since
               hero had it until #offer forced it to be relinquished */
#ifdef ZHLANG
            if (u.uevent.ascended)
                enlght_zh("你交出了厄运护身符。");
            else if (u.uhave.amulet)
                enlght_zh("你拥有厄运护身符。");
            else
                enlght_zh("你曾持有过厄运护身符。");
#else
            enl_msg(You_,
                    u.uhave.amulet ? "have" : "have obtained",
                    u.uevent.ascended ? "delivered"
                     : u.uhave.amulet ? "had" : "had obtained",
                    " the Amulet of Yendor", "");
#endif
            break;

        /* reaching Astral makes feedback about reaching the Planes
           be redundant and ascending makes both be redundant, but
           we display all that apply */
        case ACH_ENDG:
            you_have_X(TR("reached the Elemental Planes", "到达了元素位面"));
            break;
        case ACH_ASTR:
            you_have_X(TR("reached the Astral Plane", "到达了星界"));
            break;
        case ACH_UWIN:
            /* the ultimate achievement... */
            enlght_out(TR(" You ascended!", " 你飞升了！"));
            break;

        /* rank 0 is the starting condition, not an achievement; 8 is Xp 30 */
        case ACH_RNK1: case ACH_RNK2: case ACH_RNK3: case ACH_RNK4:
        case ACH_RNK5: case ACH_RNK6: case ACH_RNK7: case ACH_RNK8:
            Sprintf(buf, TR("attained the rank of %s", "获得了%s的称号"),
                    rank_of(rank_to_xlev(absidx - (ACH_RNK1 - 1)),
                            Role_switch, (achidx < 0) ? TRUE : FALSE));
            you_have_X(buf);
            break;

        default:
            Sprintf(buf, TR(" [Unexpected achievement #%d.]", " 【意外成就 #%d。】"), achidx);
            enlght_out(buf);
            break;
        } /* switch */
    } /* for */

    if (awin != ge.en_win) {
        display_nhwindow(awin, TRUE);
        destroy_nhwindow(awin);
    }
}

/* record an achievement (add at end of list unless already present) */
void
record_achievement(schar achidx)
{
    int i, absidx;
    int repeat_achievement = 0;

    absidx = abs(achidx);
    /* valid achievements range from 1 to N_ACH-1; however, ranks can be
       stored as the complement (ie, negative) to track gender */
    if ((achidx < 1 && (absidx < ACH_RNK1 || absidx > ACH_RNK8))
        || achidx >= N_ACH) {
        impossible("Achievement #%d is out of range.", achidx);
        return;
    }

    /* the list has an extra slot so there is always at least one 0 at
       its end (more than one unless all N_ACH-1 possible achievements
       have been recorded); find first empty slot or achievement #achidx;
       an attempt to duplicate an achievement can happen if any of Bell,
       Candelabrum, Book, or Amulet is dropped then picked up again */
    for (i = 0; u.uachieved[i]; ++i)
        if (abs(u.uachieved[i]) == absidx) {
            repeat_achievement = 1;
            break;
        }

    /*
     * We do the sound for an achievement, even if it has already been
     * achieved before. Some players might have set up level-based
     * theme music or something. We do let the sound interface know
     * that it's not the original achievement though.
     */
    SoundAchievement(achidx, 0, repeat_achievement);

    if (repeat_achievement)
        return; /* already recorded, don't duplicate it */
    u.uachieved[i] = achidx;

    /* avoid livelog for achievements recorded during final disclosure:
       nudist and blind-from-birth; also ascension which is suppressed
       by this gets logged separately in really_done() */
    if (program_state.gameover)
        return;

    if (absidx >= ACH_RNK1 && absidx <= ACH_RNK8) {
        livelog_printf(achieve_msg[absidx].llflag,
                       TR("attained the rank of %s (level %d)", "晋升为%s（%d级）"),
                       rank_of(rank_to_xlev(absidx - (ACH_RNK1 - 1)),
                               Role_switch, (achidx < 0) ? TRUE : FALSE),
                       u.ulevel);
    } else if (achidx == ACH_SOKO_PRIZE
               || achidx == ACH_MINE_PRIZE) {
        /* need to supply extra information for these two */
        short otyp = ((achidx == ACH_SOKO_PRIZE)
                      ? svc.context.achieveo.soko_prize_otyp
                      : svc.context.achieveo.mines_prize_otyp);

        /* note: OBJ_NAME() works here because both "bag of holding" and
           "amulet of reflection" are fully named in their objects[] entry
           but that's not true in the general case */
        livelog_printf(achieve_msg[achidx].llflag, "%s %s",
                       achieve_msg[achidx].msg, OBJ_NAME(objects[otyp]));
    } else {
        livelog_printf(achieve_msg[absidx].llflag, "%s",
                       achieve_msg[absidx].msg);
    }
}

/* discard a recorded achievement; return True if removed, False otherwise */
boolean
remove_achievement(schar achidx)
{
    int i;

    for (i = 0; u.uachieved[i]; ++i)
        if (abs(u.uachieved[i]) == abs(achidx))
            break; /* stop when found */
    if (!u.uachieved[i]) /* not found */
        return FALSE;
    /* list is 0 terminated so any beyond the removed one move up a slot */
    do {
        u.uachieved[i] = u.uachieved[i + 1];
    } while (u.uachieved[++i]);
    return TRUE;
}

/* used to decide whether there are any achievements to display */
int
count_achievements(void)
{
    int i, acnt = 0;

    for (i = 0; u.uachieved[i]; ++i)
        ++acnt;
    return acnt;
}

/* convert a rank index to an achievement number; encode it when female
   in order to subsequently report gender-specific ranks accurately */
schar
achieve_rank(int rank) /* 1..8 */
{
    schar achidx = (schar) ((rank - 1) + ACH_RNK1);

    if (flags.female)
        achidx = -achidx;
    return achidx;
}

/* return True if sokoban branch has been entered, False otherwise */
boolean
sokoban_in_play(void)
{
    int achidx;

    /* TODO? move this to dungeon.c and test furthest level reached of the
       sokoban branch instead of relying on the entered-sokoban achievement */

    for (achidx = 0; u.uachieved[achidx]; ++achidx)
        if (u.uachieved[achidx] == ACH_SOKO)
            return TRUE;
    return FALSE;
}

/* #chronicle command */
int
do_gamelog(void)
{
#ifdef CHRONICLE
    if (gg.gamelog) {
        show_gamelog(ENL_GAMEINPROGRESS);
    } else {
#ifdef ZHLANG
        pline("没有编年事件记录。");
#else
        pline("No chronicled events.");
#endif
    }
#else
#ifdef ZHLANG
    pline("编年史功能在编译时已被关闭。");
#else
    pline("Chronicle was turned off during compile-time.");
#endif
#endif /* !CHRONICLE */
    return ECMD_OK;
}

/* 'major' events for dumplog; inclusion or exclusion here may need tuning */
#define LL_majors (0L \
                   | LL_WISH            \
                   | LL_ACHIEVE         \
                   | LL_UMONST          \
                   | LL_DIVINEGIFT      \
                   | LL_LIFESAVE        \
                   | LL_ARTIFACT        \
                   | LL_GENOCIDE        \
                   | LL_DUMP) /* explicitly for dumplog */
#define majorevent(llmsg) (((llmsg)->flags & LL_majors) != 0)
#define spoilerevent(llmsg) (((llmsg)->flags & LL_SPOILER) != 0)

/* #chronicle details */
void
show_gamelog(int final)
{
#ifdef CHRONICLE
    struct gamelog_line *llmsg;
    winid win;
    char buf[BUFSZ];
    int eventcnt = 0;

    win = create_nhwindow(NHW_TEXT);
    Sprintf(buf, TR("%s events:", "%s事件："), final ? TR("Major", "重大") : TR("Logged", "已记录"));
    putstr(win, 0, buf);
    for (llmsg = gg.gamelog; llmsg; llmsg = llmsg->next) {
        if (final && !majorevent(llmsg))
            continue;
        if (!final && !wizard && spoilerevent(llmsg))
            continue;
        if (!eventcnt++)
            putstr(win, 0, TR(" Turn", " 回合"));
        Snprintf(buf, sizeof buf, "%5ld: %s", llmsg->turn, llmsg->text);
        putstr(win, 0, buf);
    }
    /* since start of game is logged as a major event, 'eventcnt' should
       never end up as 0; for 'final', end of game is a major event too */
    if (!eventcnt)
        putstr(win, 0, TR(" none", " 无"));

    display_nhwindow(win, TRUE);
    destroy_nhwindow(win);
#else
    nhUse(final);
#endif /* !CHRONICLE */
    return;
}

/*
 *      Vanquished monsters.
 */

/* the two uppercase choices are implemented but suppressed from menu.
   also used in options.c */
const char *const vanqorders[NUM_VANQ_ORDER_MODES][3] = {
    { "t", "traditional: by monster level",
           "traditional: by monster level, by internal monster index" },
    { "d", "by monster difficulty rating",
           "by monster difficulty rating, by internal monster index" },
    { "a", "alphabetically, unique monsters separate",
           "alphabetically, first unique monsters, then others" },
    { "A", "alphabetically, unique monsters intermixed",
           "alphabetically, unique monsters and others intermixed" },
    { "C", "by monster class, high to low level in class",
           "by monster class, high to low level within class" },
    { "c", "by monster class, low to high level in class",
           "by monster class, low to high level within class" },
    { "n", "by count, high to low",
           "by count, high to low, by internal index within tied count" },
    { "z", "by count, low to high",
           "by count, low to high, by internal index within tied count" },
};

staticfn int QSORTCALLBACK
vanqsort_cmp(
    const genericptr vptr1,
    const genericptr vptr2)
{
    int indx1 = *(short *) vptr1, indx2 = *(short *) vptr2,
        mlev1, mlev2, mstr1, mstr2, uniq1, uniq2, died1, died2, res;
    const char *name1, *name2, *punct;
    schar mcls1, mcls2;

    switch (flags.vanq_sortmode) {
    default:
    case VANQ_MLVL_MNDX:
        /* sort by monster level */
        mlev1 = mons[indx1].mlevel;
        mlev2 = mons[indx2].mlevel;
        res = mlev2 - mlev1; /* mlevel high to low */
        break;
    case VANQ_MSTR_MNDX:
        /* sort by monster toughness */
        mstr1 = mons[indx1].difficulty;
        mstr2 = mons[indx2].difficulty;
        res = mstr2 - mstr1; /* monstr high to low */
        break;
    case VANQ_ALPHA_SEP:
        uniq1 = ((mons[indx1].geno & G_UNIQ) && indx1 != PM_HIGH_CLERIC);
        uniq2 = ((mons[indx2].geno & G_UNIQ) && indx2 != PM_HIGH_CLERIC);
        if (uniq1 ^ uniq2) { /* one or other uniq, but not both */
            res = uniq2 - uniq1;
            break;
        } /* else both unique or neither unique */
        FALLTHROUGH;
        /*FALLTHRU*/
    case VANQ_ALPHA_MIX:
        name1 = mons[indx1].pmnames[NEUTRAL];
        name2 = mons[indx2].pmnames[NEUTRAL];
        res = strcmpi(name1, name2); /* caseblind alpha, low to high */
        break;
    case VANQ_MCLS_HTOL:
    case VANQ_MCLS_LTOH:
        /* mons[].mlet is a small integer, 1..N, of type plain char;
           if 'char' happens to be unsigned, (mlet1 - mlet2) would yield
           an inappropriate result when mlet2 is greater than mlet1,
           so force our copies (mcls1, mcls2) to be signed */
        mcls1 = (schar) mons[indx1].mlet;
        mcls2 = (schar) mons[indx2].mlet;
        /* S_ANT through S_ZRUTY correspond to lowercase monster classes,
           S_ANGEL through S_ZOMBIE correspond to uppercase, and various
           punctuation characters are used for classes beyond those */
        if (mcls1 > S_ZOMBIE && mcls2 > S_ZOMBIE) {
            /* force a specific order to the punctuation classes that's
               different from the internal order;
               internal order is ok if neither or just one is punctuation
               since letters have lower values so come out before punct */
            static const char punctclasses[] = {
                S_LIZARD, S_EEL, S_GOLEM, S_GHOST, S_DEMON, S_HUMAN, '\0'
            };

            if ((punct = strchr(punctclasses, mcls1)) != 0)
                mcls1 = (schar) (S_ZOMBIE + 1 + (int) (punct - punctclasses));
            if ((punct = strchr(punctclasses, mcls2)) != 0)
                mcls2 = (schar) (S_ZOMBIE + 1 + (int) (punct - punctclasses));
        }
        res = mcls1 - mcls2; /* class */
        if (res == 0) {
            /* Riders are in the same class as major demons, yielding res==0
               above when both mcls1 and mcls2 are either Riders or demons or
               one of each; force Riders to be sorted before demons */
            res = is_rider(&mons[indx2]) - is_rider(&mons[indx1]);
            /* res -1 => #1 is a Rider, #2 isn't;
                    0 => both Riders or neither;
                   +1 => #2 is a Rider, #1 isn't */
            if (res)
                break;
            mlev1 = mons[indx1].mlevel;
            mlev2 = mons[indx2].mlevel;
            res = mlev1 - mlev2; /* mlevel low to high */
            if (flags.vanq_sortmode == VANQ_MCLS_HTOL)
                res = -res; /* mlevel high to low */
        }
        break;
    case VANQ_COUNT_H_L:
    case VANQ_COUNT_L_H:
        died1 = svm.mvitals[indx1].died;
        died2 = svm.mvitals[indx2].died;
        res = died2 - died1; /* dead count high to low */
        if (flags.vanq_sortmode == VANQ_COUNT_L_H)
            res = -res; /* dead count low to high */
        break;
    }
    /* tiebreaker: internal mons[] index */
    if (res == 0)
        res = indx1 - indx2; /* mndx low to high */
    return res;
}

/* returns -1 if cancelled via ESC */
int
set_vanq_order(boolean for_vanq)
{
    winid tmpwin;
    menu_item *selected;
    anything any;
    char buf[BUFSZ];
    const char *desc;
    int i, n, choice,
        clr = NO_COLOR;

    tmpwin = create_nhwindow(NHW_MENU);
    start_menu(tmpwin, MENU_BEHAVE_STANDARD);
    any = cg.zeroany; /* zero out all bits */
    for (i = 0; i < SIZE(vanqorders); i++) {
        if (i == VANQ_ALPHA_MIX || i == VANQ_MCLS_HTOL) /* skip these */
            continue;
        /* suppress some orderings if this menu if for 'm #genocided' */
        if (!for_vanq && (i == VANQ_COUNT_H_L || i == VANQ_COUNT_L_H))
            continue;
        desc = vanqorders[i][2];
        /* unique monsters can't be genocided so "alpha, unique separate"
           and "alpha, unique intermixed" are confusing descriptions when
           this menu is for #genocided rather than for #vanquished */
        if (!for_vanq && i == VANQ_ALPHA_SEP)
            desc = "alphabetically";
        any.a_int = i + 1;
        add_menu(tmpwin, &nul_glyphinfo, &any, *vanqorders[i][0], 0,
                 ATR_NONE, clr, desc,
                 (i == flags.vanq_sortmode) ? MENU_ITEMFLAGS_SELECTED
                                            : MENU_ITEMFLAGS_NONE);
    }
    Sprintf(buf, TR("Sort order for %s", "%s的排序方式"),
            for_vanq ? TR("vanquished monster counts (also genocided types)", "击杀怪物统计（含灭绝种类）")
                     : TR("genocided monster types (also vanquished counts)", "灭绝怪物种类（含击杀统计）"));
    end_menu(tmpwin, buf);

    n = select_menu(tmpwin, PICK_ONE, &selected);
    destroy_nhwindow(tmpwin);
    if (n > 0) {
        choice = selected[0].item.a_int - 1;
        /* skip preselected entry if we have more than one item chosen */
        if (n > 1 && choice == flags.vanq_sortmode)
            choice = selected[1].item.a_int - 1;
        free((genericptr_t) selected);
        flags.vanq_sortmode = choice;
    }
    return (n < 0) ? -1 : flags.vanq_sortmode;
}

/* #vanquished command */
int
dovanquished(void)
{
    list_vanquished(iflags.menu_requested ? 'A' : 'y', FALSE);
    iflags.menu_requested = FALSE;
    return ECMD_OK;
}

/* high priests aren't unique but are flagged as such to simplify something */
#define UniqCritterIndx(mndx) \
    ((mons[mndx].geno & G_UNIQ) != 0 && mndx != PM_HIGH_CLERIC)

#define done_stopprint program_state.stopprint

/* used for #vanquished and end of game disclosure and end of game dumplog */
void
list_vanquished(char defquery, boolean ask)
{
    int i;
    int pfx, nkilled;
    unsigned ntypes, ni;
    long total_killed = 0L;
    winid klwin;
    short mindx[NUMMONS];
    char c, buf[BUFSZ], buftoo[BUFSZ];
    /* 'A' is only supplied by 'm #vanquished'; 'd' is only supplied by
       dump_everything() when writing dumplog, so won't happen if built
       without '#define DUMPLOG' but there's no need for conditionals here */
    boolean force_sort = (defquery == 'A'),
            dumping = (defquery == 'd');

    /* normally we don't ask about sort order for the vanquished list unless
       it contains at least two entries; however, if player has used explicit
       'm #vanquished', choose order no matter what it contains so far */
    if (force_sort) { /* iflags.menu_requested via dovanquished() */
        /* choose value for vanq_sortmode via menu; ESC cancels choosing
           sort order but continues with vanquishd monsters display */
        (void) set_vanq_order(TRUE);
    }
    if (dumping || force_sort) {
        /* switch from 'A' or 'd' to 'y'; 'ask' is already False for the
           cases that might supply 'A' or 'd' */
        defquery = 'y';
        ask = FALSE; /* redundant */
    }

    /* get totals first */
    ntypes = 0;
    for (i = LOW_PM; i < NUMMONS; i++) {
        if ((nkilled = (int) svm.mvitals[i].died) == 0)
            continue;
        mindx[ntypes++] = i;
        total_killed += (long) nkilled;
    }

    /* vanquished creatures list;
     * includes all dead monsters, not just those killed by the player
     */
    if (ntypes != 0) {
        char mlet, prev_mlet = 0; /* used as small integer, not character */
        boolean class_header, uniq_header, Rider,
                was_uniq = FALSE, special_hdr = FALSE;

        if (ask) {
            char allow_yn[10];

            if (ntypes > 1) {
                Strcpy(allow_yn, ynaqchars);
            } else {
                Strcpy(allow_yn, ynqchars); /* don't include 'a', but */
                Strcat(allow_yn, "\033a");  /* allow user to answer 'a' */
                if (defquery == 'a') /* potential default from 'disclose' */
                    defquery = 'y';
            }
            c = yn_function(TR("Do you want an account of creatures vanquished?", "需要查看消灭的怪物记录吗？"),
                            allow_yn, defquery, TRUE);
        } else {
            c = defquery;
        }

        if (c == 'q')
            done_stopprint++;
        if (c == 'y' || c == 'a') {
            if (c == 'a' && ntypes > 1) { /* ask user to choose sort order */
                /* choose value for vanq_sortmode via menu; ESC cancels list
                   of vanquished monsters but does not set 'done_stopprint' */
                if (set_vanq_order(TRUE) < 0)
                    return;
            }
            uniq_header = (flags.vanq_sortmode == VANQ_ALPHA_SEP);
            class_header = ((flags.vanq_sortmode == VANQ_MCLS_LTOH
                             || flags.vanq_sortmode == VANQ_MCLS_HTOL)
                            && ntypes > 1);

            klwin = create_nhwindow(NHW_MENU);
            putstr(klwin, 0, TR("Vanquished creatures:", "已消灭的怪物："));
            if (!dumping)
                putstr(klwin, 0, "");

            qsort((genericptr_t) mindx, ntypes, sizeof *mindx, vanqsort_cmp);
            for (ni = 0; ni < ntypes; ni++) {
                i = mindx[ni];
                nkilled = svm.mvitals[i].died;
                Rider = is_rider(&mons[i]);
                mlet = mons[i].mlet;
                if (class_header
                    && (mlet != prev_mlet || (special_hdr && !Rider))) {
                    if (!Rider) {
                        Strcpy(buf, def_monsyms[(int) mlet].explain);
                        special_hdr = FALSE;
                    } else {
                        Strcpy(buf, TR("Rider", "天启骑士"));
                        special_hdr = TRUE;
                    }
                    /* 'ask' implies final disclosure, where highlighting
                       of various header lines is suppressed */
                    putstr(klwin, ask ? ATR_NONE : iflags.menu_headings.attr,
                           upstart(buf));
                    prev_mlet = mlet;
                }
                if (UniqCritterIndx(i)) {
                    Sprintf(buf, "%s%s",
                            !type_is_pname(&mons[i]) ? TR("the ", "这个 ") : "",
                            mons[i].pmnames[NEUTRAL]);
                    if (nkilled > 1)
                        Sprintf(eos(buf), " (%s)",
                                N_times((long) nkilled, buftoo));
                    was_uniq = TRUE;
                } else {
                    if (uniq_header && was_uniq) {
                        putstr(klwin, 0, "");
                        was_uniq = FALSE;
                    }
                    /* trolls or undead might have come back,
                       but we don't keep track of that */
                    if (nkilled == 1)
                        Strcpy(buf, an(mons[i].pmnames[NEUTRAL]));
                    else
                        Sprintf(buf, "%3d %s", nkilled,
                                makeplural(mons[i].pmnames[NEUTRAL]));
                }
                /* number of leading spaces to match 3 digit prefix */
                pfx = !strncmpi(buf, "the ", 4) ? 0
                      : !strncmpi(buf, "an ", 3) ? 1
                        : !strncmpi(buf, "a ", 2) ? 2
                          : !digit(buf[2]) ? 4 : 0;
                if (class_header)
                    ++pfx;
                Snprintf(buftoo, sizeof buftoo, "%*s%s", pfx, "", buf);
                putstr(klwin, 0, buftoo);
            }
            /*
             * if (Hallucination)
             *     putstr(klwin, 0, "and a partridge in a pear tree");
             */
            if (ntypes > 1) {
                if (!dumping)
                    putstr(klwin, 0, "");
                Sprintf(buf, TR("%ld creatures vanquished.", "共消灭 %ld 只怪物。"), total_killed);
                putstr(klwin, 0, buf);
            }
            display_nhwindow(klwin, TRUE);
            destroy_nhwindow(klwin);
        }

    /*
     * For end-of-game disclosure, we're only called when some monsters
     * were vanquished and won't reach these 'else-if's.
     *
     * If no monsters have been vanquished, we're either called for game
     * still in progress, so use present tense via pline(), or for dumplog
     * which needs putstr() and past tense.
     */
    } else if (!program_state.gameover) {
        /* #vanquished rather than final disclosure, so pline() is ok */
#ifdef ZHLANG
        pline("没有怪物被消灭。");
#else
        pline("No creatures have been vanquished.");
#endif
#ifdef DUMPLOG
    } else if (dumping) {
        putstr(0, 0, TR("No creatures were vanquished.", "没有怪物被消灭。")); /* not pline() */
#endif
    }
}

/* number of monster species which have been genocided */
int
num_genocides(void)
{
    int i, n = 0;

    for (i = LOW_PM; i < NUMMONS; ++i) {
        if (svm.mvitals[i].mvflags & G_GENOD) {
            ++n;
            if (UniqCritterIndx(i))
                impossible("unique creature '%d: %s' genocided?",
                           i, mons[i].pmnames[NEUTRAL]);
        }
    }
    return n;
}

/* return a count of the number of extinct species */
staticfn int
num_extinct(void)
{
    int i, n = 0;

    for (i = LOW_PM; i < NUMMONS; ++i) {
        if (UniqCritterIndx(i))
            continue;
        if ((svm.mvitals[i].mvflags & G_GONE) == G_EXTINCT)
            ++n;
    }
    return n;
}

/* collect both genocides and extinctions, skipping uniques */
staticfn int
num_gone(int mvflags, int *mindx)
{
    uchar mflg = (uchar) mvflags;
    int i, n = 0;

    (void) memset((genericptr_t) mindx, 0, NUMMONS * sizeof *mindx);

    for (i = LOW_PM; i < NUMMONS; ++i) {
        /* uniques can't be genocided but can become extinct;
           however, they're never reported as extinct, so skip them */
        if (UniqCritterIndx(i))
            continue;

        if ((svm.mvitals[i].mvflags & mflg) != 0)
            mindx[n++] = i;
    }
    return n;
}

/* show genocided and extinct monster types for final disclosure/dumplog
   or for the #genocided command */
void
list_genocided(char defquery, boolean ask)
{
    int i, mndx;
    int ngenocided, nextinct, ngone, mvflags, mindx[NUMMONS];
    char c;
    winid klwin;
    char buf[BUFSZ];
    boolean genoing, /* prompting for genocide or class genocide */
            dumping; /* for DUMPLOG; doesn't need to be conditional */
    boolean both = (program_state.gameover || wizard || discover);

    dumping = (defquery == 'd');
    genoing = (defquery == 'g');
    if (dumping || genoing)
        defquery = 'y';
    if (genoing)
        both = FALSE; /* genocides only, not extinctions */

    /* this goes through the whole monster list up to three times but will
       happen rarely and is simpler than a more general single pass check;
       extinctions are only revealed during end of game disclosure or when
       running in wizard or explore mode */
    ngenocided = num_genocides();
    nextinct = both ? num_extinct() : 0;
    mvflags = G_GENOD | (both ? G_EXTINCT : 0);
    ngone = num_gone(mvflags, mindx);

    /* genocided or extinct species list */
    if (ngone > 0) {
        Sprintf(buf, TR("Do you want a list of %sspecies%s%s?", "需要查看%s物种%s%s的列表吗？"),
                (nextinct && !ngenocided) ? TR("extinct ", "已绝迹的") : "",
                (ngenocided) ? TR(" genocided", "（已灭绝）") : "",
                (nextinct && ngenocided) ? TR(" and extinct", "与已绝迹的") : "");
        c = ask ? yn_function(buf, (ngone > 1) ? "ynaq" : "ynq\033a", /* NOTRANS */
                              defquery, TRUE)
                : defquery;
        if (c == 'q')
            done_stopprint++;
        if (c == 'y' || c == 'a') {
            int save_sortmode;
            char mlet, prev_mlet = 0;
            boolean class_header = FALSE;

            if (ngone > 1) {
                if (c == 'a') { /* ask player to choose sort order */
                    /* #genocided shares #vanquished's sort order */
                    if (set_vanq_order(FALSE) < 0)
                        return;
                }
                /* sort orderings count-high-to-low or count-low-to-high
                   don't make sense for genocides; if the preferred order
                   to set to either of those, use alphabetical instead;
                   note: the tie breaker for by-class is level-high-to-low
                   or level-low-to-high rather than count so is ok as-is */
                save_sortmode = flags.vanq_sortmode;
                if (flags.vanq_sortmode == VANQ_COUNT_H_L
                    || flags.vanq_sortmode == VANQ_COUNT_L_H)
                    flags.vanq_sortmode = VANQ_ALPHA_MIX;
                qsort((genericptr_t) mindx, ngone,
                      sizeof *mindx, vanqsort_cmp);
                class_header = (flags.vanq_sortmode == VANQ_MCLS_LTOH
                                || flags.vanq_sortmode == VANQ_MCLS_HTOL);
                flags.vanq_sortmode = save_sortmode;
            }

            klwin = create_nhwindow(NHW_MENU);
            Sprintf(buf, TR("%s%s species:", "%s%s物种："),
                    (ngenocided) ? TR("Genocided", "被灭绝") : TR("Extinct", "已绝迹"),
                    (nextinct && ngenocided) ? TR(" or extinct", "或已绝迹") : "");
            putstr(klwin, 0, buf);
            if (!dumping)
                putstr(klwin, 0, "");

            for (i = 0; i < ngone; ++i) {
                mndx = mindx[i];
                mlet = mons[mndx].mlet;
                if (class_header && mlet != prev_mlet) {
                    Strcpy(buf, def_monsyms[(int) mlet].explain);
                    /* 'ask' implies final disclosure, where highlighting
                       of various header lines is suppressed */
                    putstr(klwin, ask ? ATR_NONE : iflags.menu_headings.attr,
                           upstart(buf));
                    prev_mlet = mlet;
                }
                Sprintf(buf, " %s", makeplural(mons[mndx].pmnames[NEUTRAL]));
                /*
                 * "Extinct" is unfortunate terminology.  A species
                 * is marked extinct when its birth limit is reached,
                 * but there might be members of the species still
                 * alive, contradicting the meaning of the word.
                 *
                 * We only append "(extinct)" if the G_GENOD bit is
                 * clear.  During normal play, 'mndx' won't be in the
                 * collected list unless that bit is set.
                 */
                if ((svm.mvitals[mndx].mvflags & G_GONE) == G_EXTINCT)
                    Strcat(buf, TR(" (extinct)", "（已灭绝）"));
                putstr(klwin, 0, buf);
            }
            if (!dumping)
                putstr(klwin, 0, "");
            if (ngenocided > 0) {
                Sprintf(buf, TR("%d species genocided.", "%d 个物种已被灭绝。"), ngenocided);
                putstr(klwin, 0, buf);
            }
            if (nextinct > 0) {
                Sprintf(buf, TR("%d species extinct.", "%d 个物种已灭绝。"), nextinct);
                putstr(klwin, 0, buf);
            }

            display_nhwindow(klwin, TRUE);
            destroy_nhwindow(klwin);
        }

    /* See the comment for similar code near the end of list_vanquished(). */
    } else if (!program_state.gameover) {
        /* #genocided rather than final disclosure, so pline() is ok and
           extinction has been ignored */
#ifdef ZHLANG
        pline("没有种族被灭绝%s。", genoing ? "尚未" : "");
#else
        pline("No creatures have been genocided%s.", genoing ? " yet" : "");
#endif
#ifdef DUMPLOG
    } else if (dumping) { /* 'gameover' is True if we make it here */
        putstr(0, 0, TR("No species were genocided or became extinct.", "没有物种被灭绝或灭绝。"));
#endif
    }
}

/* M-g - #genocided command */
int
dogenocided(void)
{
    list_genocided(iflags.menu_requested ? 'a' : 'y', FALSE);
    return ECMD_OK;
}

DISABLE_WARNING_FORMAT_NONLITERAL

/* #wizborn extended command */
int
doborn(void)
{
    static const char fmt[] = "%4i %4i %c %-30s";
    int i;
    winid datawin = create_nhwindow(NHW_TEXT);
    char buf[BUFSZ];
    int nborn = 0, ndied = 0;

    putstr(datawin, 0, TR("died born", "死亡 出生"));
    for (i = LOW_PM; i < NUMMONS; i++)
        if (svm.mvitals[i].born || svm.mvitals[i].died
            || (svm.mvitals[i].mvflags & G_GONE) != 0) {
            Sprintf(buf, fmt,
                    svm.mvitals[i].died, svm.mvitals[i].born,
                    ((svm.mvitals[i].mvflags & G_GONE) == G_EXTINCT) ? 'E'
                    : ((svm.mvitals[i].mvflags & G_GONE) == G_GENOD) ? 'G'
                      : ((svm.mvitals[i].mvflags & G_GONE) != 0) ? 'X'
                        : ' ',
                    mons[i].pmnames[NEUTRAL]);
            putstr(datawin, 0, buf);
            nborn += svm.mvitals[i].born;
            ndied += svm.mvitals[i].died;
        }

    putstr(datawin, 0, "");
    Sprintf(buf, fmt, ndied, nborn, ' ', "");

    display_nhwindow(datawin, FALSE);
    destroy_nhwindow(datawin);

    return ECMD_OK;
}

RESTORE_WARNING_FORMAT_NONLITERAL

/*
 * align_str(), piousness(), mstatusline() and ustatusline() once resided
 * in pline.c, then got moved to priest.c just to be out of there.  They
 * fit better here.
 */

const char *
align_str(aligntyp alignment)
{
    switch ((int) alignment) {
    case A_CHAOTIC:
#ifdef ZHLANG
        return "混沌";
#else
        return "chaotic";
#endif
    case A_NEUTRAL:
#ifdef ZHLANG
        return "中立";
#else
        return "neutral";
#endif
    case A_LAWFUL:
#ifdef ZHLANG
        return "守序";
#else
        return "lawful";
#endif
    case A_NONE:
#ifdef ZHLANG
        return "无阵营";
#else
        return "unaligned";
#endif
    }
#ifdef ZHLANG
    return "未知";
#else
    return "unknown";
#endif
}

staticfn char *
size_str(int msize)
{
    static char outbuf[40];

    switch (msize) {
    case MZ_TINY:
        Strcpy(outbuf, TR("tiny", "极小"));
        break;
    case MZ_SMALL:
        Strcpy(outbuf, TR("small", "小"));
        break;
    case MZ_MEDIUM:
        Strcpy(outbuf, TR("medium", "中等"));
        break;
    case MZ_LARGE:
        Strcpy(outbuf, TR("large", "大"));
        break;
    case MZ_HUGE:
        Strcpy(outbuf, TR("huge", "巨大"));
        break;
    case MZ_GIGANTIC:
        Strcpy(outbuf, TR("gigantic", "庞大"));
        break;
    default:
        Sprintf(outbuf, TR("unknown size (%d)", "未知体型（%d）"), msize);
        break;
    }
    return outbuf;
}

/* used for self-probing */
char *
piousness(boolean showneg, const char *suffix)
{
    static char buf[32]; /* bigger than "insufficiently neutral" */
    const char *pio;

    /* note: piousness 20 matches MIN_QUEST_ALIGN (quest.h) */
#ifdef ZHLANG
    if (u.ualign.record >= 20)
        pio = "虔诚地信奉";
    else if (u.ualign.record > 13)
        pio = "虔敬地信奉";
    else if (u.ualign.record > 8)
        pio = "热忱地信奉";
    else if (u.ualign.record > 3)
        pio = "坚定地信奉";
    else if (u.ualign.record == 3)
        pio = "信奉";
    else if (u.ualign.record > 0)
        pio = "犹豫地信奉";
    else if (u.ualign.record == 0)
        pio = "名义上信奉";
    else if (!showneg)
        pio = "还不够信奉";
    else if (u.ualign.record >= -3)
        pio = "偏离了";
    else if (u.ualign.record >= -8)
        pio = "对…犯下了罪过：";
    else
        pio = "严重违背了";

    Sprintf(buf, "%s", pio);
    if (suffix && *suffix)
        Strcat(buf, suffix);
#else
    if (u.ualign.record >= 20)
        pio = "piously";
    else if (u.ualign.record > 13)
        pio = "devoutly";
    else if (u.ualign.record > 8)
        pio = "fervently";
    else if (u.ualign.record > 3)
        pio = "stridently";
    else if (u.ualign.record == 3)
        pio = "";
    else if (u.ualign.record > 0)
        pio = "haltingly";
    else if (u.ualign.record == 0)
        pio = "nominally";
    else if (!showneg)
        pio = "insufficiently";
    else if (u.ualign.record >= -3)
        pio = "strayed";
    else if (u.ualign.record >= -8)
        pio = "sinned";
    else
        pio = "transgressed";

    Sprintf(buf, "%s", pio);
    if (suffix && (!showneg || u.ualign.record >= 0)) {
        if (u.ualign.record != 3)
            Strcat(buf, " ");
        Strcat(buf, suffix);
    }
#endif
    return buf;
}

/* stethoscope or probing applied to monster -- one-line feedback */
void
mstatusline(struct monst *mtmp)
{
    aligntyp alignment = mon_aligntyp(mtmp);
    char info[BUFSZ], monnambuf[BUFSZ];

    info[0] = 0;
    if (mtmp->mtame) {
        Strcat(info, TR(", tame", "，驯服"));
        if (wizard) {
            Sprintf(eos(info), " (%d", mtmp->mtame);
            if (!mtmp->isminion)
                Sprintf(eos(info), TR("; hungry %ld; apport %d", "；饥饿 %ld；传送 %d"),
                        EDOG(mtmp)->hungrytime, EDOG(mtmp)->apport);
            Strcat(info, ")");
        }
    } else if (mtmp->mpeaceful)
        Strcat(info, TR(", peaceful", "，和平"));

    if (mtmp->data == &mons[PM_LONG_WORM]) {
        int segndx, nsegs = count_wsegs(mtmp);

        /* the worm code internals don't consider the head to be one of
           the worm's segments, but we count it as such when presenting
           worm feedback to the player */
        if (!nsegs) {
            Strcat(info, TR(", single segment", "，单节"));
        } else {
            ++nsegs; /* include head in the segment count */
            segndx = wseg_at(mtmp, gb.bhitpos.x, gb.bhitpos.y);
            Sprintf(eos(info), TR(", %d%s of %d segments", "，第 %d%s 节（共 %d 节）"),
                    segndx, ordin(segndx), nsegs);
        }
    }
    if (ismnum(mtmp->cham) && mtmp->data != &mons[mtmp->cham])
        /* don't reveal the innate form (chameleon, vampire, &c),
           just expose the fact that this current form isn't it */
        Strcat(info, TR(", shapechanger", "，变形者"));
    /* pets eating mimic corpses mimic while eating, so this comes first */
    if (mtmp->meating)
        Strcat(info, TR(", eating", "，进食中"));
    /* a stethoscope exposes mimic before getting here so this
       won't be relevant for it, but wand of probing doesn't */
    if (mtmp->mundetected || mtmp->m_ap_type
        || visible_region_at(gb.bhitpos.x, gb.bhitpos.y))
        mhidden_description(mtmp,
                       MHID_PREFIX | MHID_ARTICLE | MHID_ALTMON | MHID_REGION,
                            eos(info));
    if (mtmp->mcan)
        Strcat(info, TR(", cancelled", "，已被取消"));
    if (mtmp->mconf)
        Strcat(info, TR(", confused", "，混乱"));
    if (mtmp->mblinded || !mtmp->mcansee)
        Strcat(info, TR(", blind", "，失明"));
    if (mtmp->mstun)
        Strcat(info, TR(", stunned", "，眩晕"));
    if (mtmp->msleeping)
        Strcat(info, TR(", asleep", "，睡眠"));
#if 0 /* unfortunately mfrozen covers temporary sleep and being busy
       * (donning armor, for instance) as well as paralysis */
    else if (mtmp->mfrozen)
        Strcat(info, TR(", paralyzed", "，麻痹"));
#else
    else if (mtmp->mfrozen || !mtmp->mcanmove)
        Strcat(info, TR(", can't move", "，无法动弹"));
#endif
    /* [arbitrary reason why it isn't moving] */
    else if ((mtmp->mstrategy & STRAT_WAITMASK) != 0)
        Strcat(info, TR(", meditating", "，沉思"));
    if (mtmp->mflee)
        Strcat(info, TR(", scared", "，恐惧"));
    if (mtmp->mtrapped)
        Strcat(info, TR(", trapped", "，被困住"));
    if (mtmp->mspeed)
        Strcat(info, (mtmp->mspeed == MFAST) ? TR(", fast", "，快速")
                      : (mtmp->mspeed == MSLOW) ? TR(", slow", "，缓慢")
                         : TR(", [? speed]", "，[速度?]"));
    if (mtmp->minvis)
        Strcat(info, TR(", invisible", "，隐形"));
    if (mtmp == u.ustuck) {
        struct permonst *pm = u.ustuck->data;

        /* being swallowed/engulfed takes priority over sticks(youmonst);
           this used to have that backwards and checked sticks() first */
        Strcat(info, u.uswallow ? (digests(pm)
                                   ? TR(", digesting you", "，正在消化你")
                                   /* note: the "swallowing you" case won't
                                      happen because all animal engulfers
                                      either digest their victims (purple
                                      worm) or enfold them (trappers and
                                      lurkers above) */
                                   : (is_animal(pm) && !enfolds(pm))
                                     ? TR(", swallowing you", "，把你吞了下去")
                                     : TR(", engulfing you", "，正将你吞没"))
                     /* !u.uswallow; if both youmonst and ustuck are holders,
                        youmonst wins */
                     : (!sticks(gy.youmonst.data) ? TR(", holding you", "，抓住了你")
                                                 : TR(", held by you",
                                                      "，被你擒住")));
    }
    if (mtmp == u.usteed) {
        Strcat(info, TR(", carrying you", "，载着你"));
        if (Wounded_legs) {
            /* EWounded_legs is used to track left/right/both rather than
               some form of extrinsic impairment; HWounded_legs is used for
               timeout; both apply to steed instead of hero when mounted */
            long legs = (EWounded_legs & BOTH_SIDES);
            const char *what = mbodypart(mtmp, LEG);

            if (legs == BOTH_SIDES)
                what = makeplural(what);
            Sprintf(eos(info), TR(", injured %s", "，%s受伤"), what);
        }
    }
    if (mtmp->mleashed)
        Strcat(info, TR(", leashed", "，被拴着"));

    /* avoid "Status of the invisible newt ..., invisible" */
    /* and unlike a normal mon_nam, use "saddled" even if it has a name */
    Strcpy(monnambuf, x_monnam(mtmp, ARTICLE_YOUR, (char *) 0,
                               (SUPPRESS_IT | SUPPRESS_INVISIBLE), FALSE));

#ifdef ZHLANG
    pline("%s 的状态（%s，%s）：等级 %d  HP %d(%d)  AC %d%s。",
#else
    pline("Status of %s (%s, %s):  Level %d  HP %d(%d)  AC %d%s.",
#endif
          monnambuf, align_str(alignment), size_str(mtmp->data->msize),
          mtmp->m_lev, mtmp->mhp, mtmp->mhpmax, find_mac(mtmp), info);
}

/* stethoscope or probing applied to hero -- one-line feedback */
void
ustatusline(void)
{
    NhRegion *reg;
    char info[BUFSZ];
    size_t ln;

    info[0] = '\0';
    if (Sick) {
        Strcat(info, TR(", dying from", "，濒死于"));
        if (u.usick_type & SICK_VOMITABLE)
            Strcat(info, TR(" food poisoning", " 食物中毒"));
        if (u.usick_type & SICK_NONVOMITABLE) {
            if (u.usick_type & SICK_VOMITABLE)
                Strcat(info, TR(" and", " 和"));
            Strcat(info, TR(" illness", " 疾病"));
        }
    }
    if (Stoned)
        Strcat(info, TR(", solidifying", "，正在石化"));
    if (Slimed)
        Strcat(info, TR(", becoming slimy", "，正在变成粘液"));
    if (Strangled)
        Strcat(info, TR(", being strangled", "，正被勒住"));
    if (Vomiting)
        Strcat(info, TR(", nauseated", "，恶心")); /* !"nauseous" */
    if (Confusion)
        Strcat(info, TR(", confused", "，混乱"));
    if (Blind) {
        Strcat(info, TR(", blind", "，失明"));
        if (u.ucreamed) {
            if ((long) u.ucreamed < BlindedTimeout || Blindfolded
                || !haseyes(gy.youmonst.data))
                Strcat(info, TR(", cover", "，被"));
            Strcat(info, TR("ed by sticky goop", "黏液糊住"));
        } /* note: "goop" == "glop"; variation is intentional */
    }
    if (Stunned)
        Strcat(info, TR(", stunned", "，眩晕"));
    if (Wounded_legs && !u.usteed) {
        /* EWounded_legs is used to track left/right/both rather than some
           form of extrinsic impairment; HWounded_legs is used for timeout;
           both apply to steed instead of hero when mounted */
        long legs = (EWounded_legs & BOTH_SIDES);
        const char *what = body_part(LEG);

        if (legs == BOTH_SIDES)
            what = makeplural(what);
        /* when it's just one leg, ^X reports which, left or right;
           ustatusline() doesn't, in order to keep the output a bit shorter */
        Sprintf(eos(info), TR(", injured %s", "，%s受伤"), what);
    }
    if (Glib)
        Sprintf(eos(info), TR(", slippery %s", "，%s湿滑"), fingers_or_gloves(TRUE));
    if (u.utrap)
        Strcat(info, TR(", trapped", "，被困住"));
    if (Fast)
        Strcat(info, Very_fast ? TR(", very fast", "，速度极快") : TR(", fast", "，快速"));
    if (u.uundetected)
        Strcat(info, TR(", concealed", "，隐蔽"));
    else if (U_AP_TYPE != M_AP_NOTHING)
        Strcat(info, TR(", disguised", "，伪装"));
    if (Invis)
        Strcat(info, TR(", invisible", "，隐形"));
    if (u.ustuck) {
        if (u.uswallow)
            Strcat(info, digests(u.ustuck->data) ? TR(", being digested by ", "，正被消化，对方是")
                                                 : TR(", engulfed by ", "，被吞入，对方是"));
        else if (!sticks(gy.youmonst.data))
            Strcat(info, TR(", held by ", "，被擒住，对方是"));
        else
            Strcat(info, TR(", holding ", "，擒住"));
        /* FIXME? a_monnam() uses x_monnam() which has a special case that
           forces "the" instead of "a" when formatting u.ustuck while hero
           is swallowed; we don't really want that here but it isn't worth
           fiddling with just for self-probing while engulfed */
        Strcat(info, a_monnam(u.ustuck));
    }
    if (!u.uswallow
        && (reg = visible_region_at(u.ux, u.uy)) != 0
        && (ln = strlen(info)) < sizeof info)
        Snprintf(eos(info), sizeof info - ln, TR(", in a cloud of %s", "，身处%s云中"),
                 reg_damg(reg) ? TR("poison gas", "毒气") : TR("vapor", "雾气"));

#ifdef ZHLANG
    pline("%s 的状态（%s）：等级 %d  HP %d(%d)  AC %d%s。", svp.plname,
#else
    pline("Status of %s (%s):  Level %d  HP %d(%d)  AC %d%s.", svp.plname,
#endif
          piousness(FALSE, align_str(u.ualign.type)),
          Upolyd ? mons[u.umonnum].mlevel : u.ulevel, Upolyd ? u.mh : u.uhp,
          Upolyd ? u.mhmax : u.uhpmax, u.uac, info);
}

/* for 'onefile' processing where end of this file isn't necessarily the
   end of the source code seen by the compiler */
#undef enl_msg
#undef you_are
#undef you_have
#undef you_can
#undef you_have_been
#undef you_have_never
#undef you_have_X
#undef LL_majors
#undef majorevent
#undef spoilerevent
#undef UniqCritterIndx
#undef done_stopprint

/*insight.c*/
