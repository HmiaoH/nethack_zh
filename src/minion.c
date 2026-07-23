/* NetHack 5.0	minion.c	$NHDT-Date: 1781973054 2026/06/20 16:30:54 $  $NHDT-Branch: NetHack-5.0 $:$NHDT-Revision: 1.88 $ */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/*-Copyright (c) Robert Patrick Rankin, 2008. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

/* used to pick among the four basic elementals without worrying whether
   they've been reordered (difficulty reassessment?) or any new ones have
   been introduced (hybrid types added to 'E'-class?) */
static const int elementals[4] = {
    PM_AIR_ELEMENTAL, PM_FIRE_ELEMENTAL,
    PM_EARTH_ELEMENTAL, PM_WATER_ELEMENTAL
};

void
newemin(struct monst *mtmp)
{
    if (!mtmp->mextra)
        mtmp->mextra = newmextra();
    if (!EMIN(mtmp)) {
        EMIN(mtmp) = (struct emin *) alloc(sizeof(struct emin));
        (void) memset((genericptr_t) EMIN(mtmp), 0, sizeof(struct emin));
        EMIN(mtmp)->parentmid = mtmp->m_id;
    }
}

void
free_emin(struct monst *mtmp)
{
    if (mtmp->mextra && EMIN(mtmp)) {
        free((genericptr_t) EMIN(mtmp));
        EMIN(mtmp) = (struct emin *) 0;
    }
    mtmp->isminion = 0;
}

/* count the number of monsters on the level */
int
monster_census(boolean spotted) /* seen|sensed vs all */
{
    struct monst *mtmp;
    int count = 0;

    for (mtmp = fmon; mtmp; mtmp = mtmp->nmon) {
        if (DEADMONSTER(mtmp) || PARKEDMONSTER(mtmp))
            continue;
        if (spotted && !canspotmon(mtmp))
            continue;
        ++count;
    }
    return count;
}

/* mon summons a monster */
int
msummon(struct monst *mon)
{
    struct permonst *ptr;
    int dtype = NON_PM, cnt = 0, result = 0, census;
    boolean xlight;
    aligntyp atyp;
    struct monst *mtmp;

    if (mon) {
        ptr = mon->data;

        if (u_wield_art(ART_DEMONBANE) && is_demon(ptr)) {
            if (canseemon(mon))
#ifdef ZHLANG
                pline("%s困惑了一会儿。", Monnam(mon));
#else
                pline("%s looks puzzled for a moment.", Monnam(mon));
#endif
            return 0;
        }

        atyp = mon->ispriest ? EPRI(mon)->shralign
               : mon->isminion ? EMIN(mon)->min_align
                 : (ptr->maligntyp == A_NONE) ? A_NONE
                   : sgn(ptr->maligntyp);
    } else {
        ptr = &mons[PM_WIZARD_OF_YENDOR];
        atyp = (ptr->maligntyp == A_NONE) ? A_NONE : sgn(ptr->maligntyp);
    }

    if (is_dprince(ptr) || (ptr == &mons[PM_WIZARD_OF_YENDOR])) {
        dtype = (!rn2(20)) ? dprince(atyp) : (!rn2(4)) ? dlord(atyp)
                                                       : ndemon(atyp);
        cnt = ((dtype != NON_PM)
               && !rn2(4) && is_ndemon(&mons[dtype])) ? 2 : 1;
    } else if (is_dlord(ptr)) {
        dtype = (!rn2(50)) ? dprince(atyp) : (!rn2(20)) ? dlord(atyp)
                                                        : ndemon(atyp);
        cnt = ((dtype != NON_PM)
               && !rn2(4) && is_ndemon(&mons[dtype])) ? 2 : 1;
    } else if (ptr == &mons[PM_BONE_DEVIL]) {
        dtype = PM_SKELETON;
        cnt = 1;
    } else if (is_ndemon(ptr)) {
        dtype = (!rn2(20)) ? dlord(atyp) : (!rn2(6)) ? ndemon(atyp)
                                                     : monsndx(ptr);
        cnt = 1;
    } else if (is_lminion(mon)) {
        dtype = (is_lord(ptr) && !rn2(20))
                    ? llord()
                    : (is_lord(ptr) || !rn2(6)) ? lminion() : monsndx(ptr);
        cnt = ((dtype != NON_PM)
               && !rn2(4) && !is_lord(&mons[dtype])) ? 2 : 1;
    } else if (ptr == &mons[PM_ANGEL]) {
        /* non-lawful angels can also summon */
        if (!rn2(6)) {
            switch (atyp) { /* see summon_minion */
            case A_NEUTRAL:
                dtype = ROLL_FROM(elementals);
                break;
            case A_CHAOTIC:
            case A_NONE:
                dtype = ndemon(atyp);
                break;
            }
        } else {
            dtype = PM_ANGEL;
        }
        cnt = ((dtype != NON_PM)
               && !rn2(4) && !is_lord(&mons[dtype])) ? 2 : 1;
    }

    if (dtype == NON_PM)
        return 0;

    /* sanity checks */
    if (cnt > 1 && (mons[dtype].geno & G_UNIQ) != 0)
        cnt = 1;
    /*
     * If this daemon is unique and being re-summoned (the only way we
     * could get this far with an extinct dtype), try another.
     */
    if ((svm.mvitals[dtype].mvflags & G_GONE) != 0) {
        dtype = ndemon(atyp);
        if (dtype == NON_PM)
            return 0;
    }

    /* some candidates can generate a group of monsters, so simple
       count of non-null makemon() result is not sufficient */
    census = monster_census(FALSE);
    xlight = FALSE;

    while (cnt > 0) {
        mtmp = makemon(&mons[dtype], u.ux, u.uy, MM_EMIN|MM_NOMSG);
        if (mtmp) {
            result++;
            /* an angel's alignment should match the summoner */
            if (dtype == PM_ANGEL) {
                mtmp->isminion = 1;
                EMIN(mtmp)->min_align = atyp;
                /* renegade if same alignment but not peaceful
                   or peaceful but different alignment */
                EMIN(mtmp)->renegade =
                    (atyp != u.ualign.type) ^ !mtmp->mpeaceful;
            }

            if (mtmp->data->mlet == S_ANGEL && !Blind) {
                /* for any 'A', 'cloud of smoke' will be 'flash of light';
                   if more than one monster is being created, that message
                   might be skipped for this monster but show 'mtmp' anyway */
                show_transient_light((struct obj *) 0, mtmp->mx, mtmp->my);
                xlight = TRUE;
                /* we don't do this for 'burst of flame' (fire elemental)
                   because those monsters become their own light source */
            }

            if (cnt == 1 && canseemon(mtmp)) {
                const char *cloud = 0,
                           *what = msummon_environ(mtmp->data, &cloud);

#ifdef ZHLANG
                pline("%s出现在一阵%s%s中！", Amonnam(mtmp),
                      cloud, what);
#else
                pline("%s appears in a %s of %s!", Amonnam(mtmp),
                      cloud, what);
#endif
            }
        }
        cnt--;
    }

    if (xlight) {
        /* Note: if we forced --More-- here, the 'A's would be visible for
           long enough to be seen, but like with clairvoyance, some players
           would be annoyed at the disruption of having to acknowledge it */
        transient_light_cleanup();
    }

    /* how many monsters exist now compared to before? */
    if (result)
        result = monster_census(FALSE) - census;

    return result;
}

void
summon_minion(aligntyp alignment, boolean talk)
{
    struct monst *mon;
    int mnum;

    switch ((int) alignment) {
    case A_LAWFUL:
        mnum = lminion();
        break;
    case A_NEUTRAL:
        mnum = ROLL_FROM(elementals);
        break;
    case A_CHAOTIC:
    case A_NONE:
        mnum = ndemon(alignment);
        break;
    default:
        impossible("unaligned player?");
        mnum = ndemon(A_NONE);
        break;
    }
    if (mnum == NON_PM) {
        mon = 0;
    } else if (mnum == PM_ANGEL) {
        mon = makemon(&mons[mnum], u.ux, u.uy, MM_EMIN|MM_NOMSG);
        if (mon) {
            mon->isminion = 1;
            EMIN(mon)->min_align = alignment;
            EMIN(mon)->renegade = FALSE;
        }
    } else if (mnum != PM_SHOPKEEPER && mnum != PM_GUARD
               && mnum != PM_ALIGNED_CLERIC && mnum != PM_HIGH_CLERIC) {
        /* This was mons[mnum].pxlth == 0 but is this restriction
           appropriate or necessary now that the structures are separate? */
        mon = makemon(&mons[mnum], u.ux, u.uy, MM_EMIN|MM_NOMSG);
        if (mon) {
            mon->isminion = 1;
            EMIN(mon)->min_align = alignment;
            EMIN(mon)->renegade = FALSE;
        }
    } else {
        mon = makemon(&mons[mnum], u.ux, u.uy, MM_NOMSG);
    }
    if (mon) {
        if (talk) {
            if (!Deaf)
#ifdef ZHLANG
                pline_The("%s的声音响起：", align_gname(alignment));
#else
                pline_The("voice of %s booms:", align_gname(alignment));
#endif
            else
#ifdef ZHLANG
                You_feel("%s洪亮的声音：",
                         s_suffix(align_gname(alignment)));
#else
                You_feel("%s booming voice:",
                         s_suffix(align_gname(alignment)));
#endif
            SetVoice(mon, 0, 80, 0);
#ifdef ZHLANG
            verbalize("你必须为你的轻率付出代价！");
#else
            verbalize("Thou shalt pay for thine indiscretion!");
#endif
            if (canspotmon(mon))
#ifdef ZHLANG
                pline("%s出现在你面前。", Amonnam(mon));
#else
                pline("%s appears before you.", Amonnam(mon));
#endif
            mon->mstrategy &= ~STRAT_APPEARMSG;
        }
        mon->mpeaceful = FALSE;
        /* don't call set_malign(); player was naughty */
    }
}

#define Athome (Inhell && (mtmp->cham == NON_PM))

/* returns 1 if it won't attack. */
int
demon_talk(struct monst *mtmp)
{
    long cash, demand, offer;

    if (u_wield_art(ART_EXCALIBUR) || u_wield_art(ART_DEMONBANE)) {
        if (canspotmon(mtmp))
#ifdef ZHLANG
            pline("%s看起来很愤怒。", Amonnam(mtmp));
#else
            pline("%s looks very angry.", Amonnam(mtmp));
#endif
        else
#ifdef ZHLANG
            You_feel("紧张气氛在积聚。");
#else
            You_feel("tension building.");
#endif
        mtmp->mpeaceful = mtmp->mtame = 0;
        set_malign(mtmp);
        newsym(mtmp->mx, mtmp->my);
        return 0;
    }

    if (is_fainted()) {
        reset_faint(); /* if fainted - wake up */
    } else {
        stop_occupation();
        if (gm.multi > 0) {
            nomul(0);
            unmul((char *) 0);
        }
    }

    /* Slight advantage given. */
    if (is_dprince(mtmp->data) && mtmp->minvis) {
        boolean wasunseen = !canspotmon(mtmp);

        mtmp->minvis = mtmp->perminvis = 0;
        if (wasunseen && canspotmon(mtmp)) {
#ifdef ZHLANG
            pline("%s出现在你面前。", Amonnam(mtmp));
#else
            pline("%s appears before you.", Amonnam(mtmp));
#endif
            mtmp->mstrategy &= ~STRAT_APPEARMSG;
        }
        newsym(mtmp->mx, mtmp->my);
    }
    if (gy.youmonst.data->mlet == S_DEMON) { /* Won't blackmail their own. */
        if (!Deaf)
#ifdef ZHLANG
            pline("%s说：\"狩猎愉快，%s。\"", Amonnam(mtmp),
                  flags.female ? "姐妹" : "兄弟");
#else
            pline("%s says, \"Good hunting, %s.\"", Amonnam(mtmp),
                  flags.female ? "Sister" : "Brother");
#endif
        else if (canseemon(mtmp))
#ifdef ZHLANG
            pline("%s说了些什么。", Amonnam(mtmp),
                  says());
#else
            pline("%s %s something.", Amonnam(mtmp),
                  says());
#endif
        if (!tele_restrict(mtmp))
            (void) rloc(mtmp, RLOC_MSG);
        return 1;
    }
    cash = money_cnt(gi.invent);
    demand = (cash * (rnd(80) + 20 * Athome))
           / (100 * (1 + (sgn(u.ualign.type) == sgn(mtmp->data->maligntyp))));

    if (!demand || gm.multi < 0) { /* you have no gold or can't move */
        mtmp->mpeaceful = 0;
        set_malign(mtmp);
        return 0;
    } else {
        /* make sure that the demand is unmeetable if the monster
           has the Amulet, preventing monster from being satisfied
           and removed from the game (along with said Amulet...) */
        /* [actually the Amulet is safe; it would be dropped when
           mongone() gets rid of the monster; force combat anyway;
           also make it unmeetable if the player is Deaf, to simplify
           handling that case as player-won't-pay] */
        if (mon_has_amulet(mtmp) || Deaf)
            /* 125: 5*25 in case hero has maximum possible charisma */
            demand = cash + (long) rn1(1000, 125);

        if (!Deaf)
#ifdef ZHLANG
            pline("%s要求%ld%s以换取安全通行。",
                  Amonnam(mtmp), demand, currency(demand));
#else
            pline("%s demands %ld %s for safe passage.",
                  Amonnam(mtmp), demand, currency(demand));
#endif
        else if (canseemon(mtmp))
#ifdef ZHLANG
            pline("%s似乎在要求什么东西。", Amonnam(mtmp));
#else
            pline("%s seems to be demanding something.", Amonnam(mtmp));
#endif
        offer = 0L;
        if (!Deaf &&
            ((offer = bribe(mtmp, "How much will you offer?")) >= demand)) {
#ifdef ZHLANG
            pline("%s消失了，嘲笑着懦弱的凡人。",
                  Amonnam(mtmp));
#else
            pline("%s vanishes, laughing about cowardly mortals.",
                  Amonnam(mtmp));
#endif
        } else if (offer > 0L
                   && (long) rnd(5 * ACURR(A_CHA)) > (demand - offer)) {
#ifdef ZHLANG
            pline("%s凶狠地瞪了你一眼，然后消失了。",
                  Amonnam(mtmp));
#else
            pline("%s scowls at you menacingly, then vanishes.",
                  Amonnam(mtmp));
#endif
        } else {
#ifdef ZHLANG
            pline("%s生气了……", Amonnam(mtmp));
#else
            pline("%s gets angry...", Amonnam(mtmp));
#endif
            mtmp->mpeaceful = 0;
            set_malign(mtmp);
            return 0;
        }
    }
    /* if 'mtmp' is unrecognizable due to hero's hallucination,
       #chronicle will reveal its true identity -- just live with that;
       also, avoid random hallucinatory currency() units */
    livelog_printf(LL_UMONST, "bribed %s with %ld %s for safe passage",
                   x_monnam(mtmp, ARTICLE_A, (char *) 0, EXACT_NAME, FALSE),
                   offer, (offer == 1L) ? "zorkmid" : "zorkmids");
    mongone(mtmp);
    return 1;
}

long
bribe(struct monst *mtmp, const char *prompt)
{
    char buf[BUFSZ] = DUMMY;
    long offer;
    long umoney = money_cnt(gi.invent);

    getlin(prompt, buf);
    if (sscanf(buf, "%ld", &offer) != 1)
        offer = 0L;

    /*Michael Paddon -- fix for negative offer to monster*/
    /*JAR880815 - */
    if (offer < 0L) {
#ifdef ZHLANG
        You("想少给%s钱，但搞砸了。", mon_nam(mtmp));
#else
        You("try to shortchange %s, but fumble.", mon_nam(mtmp));
#endif
        return 0L;
    } else if (offer == 0L) {
#ifdef ZHLANG
        You("拒绝了。");
#else
        You("refuse.");
#endif
        return 0L;
    } else if (offer >= umoney) {
#ifdef ZHLANG
        You("把你所有的金币都给了%s。", mon_nam(mtmp));
#else
        You("give %s all your gold.", mon_nam(mtmp));
#endif
        offer = umoney;
    } else {
#ifdef ZHLANG
        You("给了%s %ld %s。", mon_nam(mtmp), offer, currency(offer));
#else
        You("give %s %ld %s.", mon_nam(mtmp), offer, currency(offer));
#endif
    }
    (void) money2mon(mtmp, offer);
    disp.botl = TRUE;
    return offer;
}

int
dprince(aligntyp atyp)
{
    int tryct, pm;

    for (tryct = !In_endgame(&u.uz) ? 20 : 0; tryct > 0; --tryct) {
        pm = rn1(PM_DEMOGORGON + 1 - PM_ORCUS, PM_ORCUS);
        if (!(svm.mvitals[pm].mvflags & G_GONE)
            && (atyp == A_NONE || sgn(mons[pm].maligntyp) == sgn(atyp)))
            return pm;
    }
    return dlord(atyp); /* approximate */
}

int
dlord(aligntyp atyp)
{
    int tryct, pm;

    for (tryct = !In_endgame(&u.uz) ? 20 : 0; tryct > 0; --tryct) {
        pm = rn1(PM_YEENOGHU + 1 - PM_JUIBLEX, PM_JUIBLEX);
        if (!(svm.mvitals[pm].mvflags & G_GONE)
            && (atyp == A_NONE || sgn(mons[pm].maligntyp) == sgn(atyp)))
            return pm;
    }
    return ndemon(atyp); /* approximate */
}

/* create lawful (good) lord */
int
llord(void)
{
    if (!(svm.mvitals[PM_ARCHON].mvflags & G_GONE))
        return PM_ARCHON;

    return lminion(); /* approximate */
}

int
lminion(void)
{
    int tryct;
    struct permonst *ptr;

    for (tryct = 0; tryct < 20; tryct++) {
        ptr = mkclass(S_ANGEL, 0);
        if (ptr && !is_lord(ptr))
            return monsndx(ptr);
    }

    return NON_PM;
}

int
ndemon(aligntyp atyp) /* A_NONE is used for 'any alignment' */
{
    struct permonst *ptr;

    /*
     * 3.6.2:  [fixed #H2204, 22-Dec-2010, eight years later...]
     * pick a correctly aligned demon in one try.  This used to
     * use mkclass() to choose a random demon type and keep trying
     * (up to 20 times) until it got one with the desired alignment.
     * mkclass_aligned() skips wrongly aligned potential candidates.
     * [The only neutral demons are djinni and mail daemon and
     * mkclass() won't pick them, but call it anyway in case either
     * aspect of that changes someday.]
     */
#if 0
    if (atyp == A_NEUTRAL)
        return NON_PM;
#endif
    ptr = mkclass_aligned(S_DEMON, 0, atyp);
    return (ptr && is_ndemon(ptr)) ? monsndx(ptr) : NON_PM;
}

/* guardian angel has been affected by conflict so is abandoning hero */
void
lose_guardian_angel(
    struct monst *mon) /* if Null, angel hasn't been created yet */
{
    coord mm;
    int i;

    if (mon) {
        if (canspotmon(mon)) {
            if (!Deaf) {
#ifdef ZHLANG
                pline("%s斥责你，说道：", Monnam(mon));
#else
                pline("%s rebukes you, saying:", Monnam(mon));
#endif
                SetVoice(mon, 0, 80, 0);
#ifdef ZHLANG
                verbalize("既然你渴望冲突，那就再多来点吧！");
#else
                verbalize("Since you desire conflict, have some more!");
#endif
            } else {
#ifdef ZHLANG
                pline("%s消失了！", Monnam(mon));
#else
                pline("%s vanishes!", Monnam(mon));
#endif
            }
        }
        mongone(mon);
    }
    /* create 2 to 4 hostile angels to replace the lost guardian */
    for (i = rn1(3, 2); i > 0; --i) {
        mm.x = u.ux;
        mm.y = u.uy;
        if (enexto(&mm, mm.x, mm.y, &mons[PM_ANGEL]))
            (void) mk_roamer(&mons[PM_ANGEL], u.ualign.type, mm.x, mm.y,
                             FALSE);
    }
}

/* just entered the Astral Plane; receive tame guardian angel if worthy */
void
gain_guardian_angel(void)
{
    struct monst *mtmp;
    struct obj *otmp;
    coord mm;

    Hear_again(); /* attempt to cure any deafness now (divine
                     message will be heard even if that fails) */
    if (Conflict) {
       if (!Deaf)
#ifdef ZHLANG
            pline("一个声音响起：");
#else
            pline("A voice booms:");
#endif
        else
#ifdef ZHLANG
            You_feel("一个洪亮的声音：");
#else
            You_feel("a booming voice:");
#endif
        SetVoice((struct monst *) 0, 0, 80, voice_deity);
#ifdef ZHLANG
        verbalize("你对冲突的渴望将被满足！");
#else
        verbalize("Thy desire for conflict shall be fulfilled!");
#endif
        /* send in some hostile angels instead */
        lose_guardian_angel((struct monst *) 0);
    } else if (u.ualign.record > 8) { /* fervent */
        if (!Deaf)
#ifdef ZHLANG
            pline("一个声音低语道：");
#else
            pline("A voice whispers:");
#endif
        else
#ifdef ZHLANG
            You_feel("一个温柔的声音：");
#else
            You_feel("a soft voice:");
#endif
        SetVoice((struct monst *) 0, 0, 80, voice_deity);
#ifdef ZHLANG
        verbalize("你一直值得我的眷顾！");
#else
        verbalize("Thou hast been worthy of me!");
#endif
        mm.x = u.ux;
        mm.y = u.uy;
        if (enexto(&mm, mm.x, mm.y, &mons[PM_ANGEL])
            && (mtmp = mk_roamer(&mons[PM_ANGEL], u.ualign.type, mm.x, mm.y,
                                 TRUE)) != 0) {
            mtmp->mstrategy &= ~STRAT_APPEARMSG;
            /* guardian angel -- the one case mtame doesn't imply an
             * edog structure, so we don't want to call tamedog().
             * [Note: this predates mon->mextra which allows a monster
             * to have both emin and edog at the same time.]
             */
            /* Too nasty for the game to unexpectedly break petless conduct on
             * the final level of the game. The angel will still appear, but
             * won't be tamed. */
            if (u.uconduct.pets) {
                mtmp->mtame = 10;
                u.uconduct.pets++;
            }
            /* for 'hilite_pet'; after making tame, before next message */
            newsym(mtmp->mx, mtmp->my);
            if (!Blind)
#ifdef ZHLANG
                pline("一位天使出现在你附近。");
#else
                pline("An angel appears near you.");
#endif
            else
#ifdef ZHLANG
                You_feel("一位友善的天使在你附近出现。");
#else
                You_feel("the presence of a friendly angel near you.");
#endif
            /* make him strong enough vs. endgame foes */
            mtmp->m_lev = rn1(8, 15);
            mtmp->mhp = mtmp->mhpmax =
                d((int) mtmp->m_lev, 10) + 30 + rnd(30);
            if ((otmp = select_hwep(mtmp)) == 0) {
                otmp = mksobj(SILVER_SABER, FALSE, FALSE);
                if (mpickobj(mtmp, otmp))
                    panic("merged weapon?");
            }
            bless(otmp);
            if (otmp->spe < 4)
                otmp->spe += rnd(4);
            if ((otmp = which_armor(mtmp, W_ARMS)) == 0
                || otmp->otyp != SHIELD_OF_REFLECTION) {
                (void) mongets(mtmp, AMULET_OF_REFLECTION);
                m_dowear(mtmp, TRUE);
            }
        }
    }
}

/*minion.c*/
