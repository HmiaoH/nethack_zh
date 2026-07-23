/* NetHack 5.0	steed.c	$NHDT-Date: 1781973068 2026/06/20 16:31:08 $  $NHDT-Branch: NetHack-5.0 $:$NHDT-Revision: 1.132 $ */
/* Copyright (c) Kevin Hugo, 1998-1999. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

/* Monsters that might be ridden */
static NEARDATA const char steeds[] = { S_QUADRUPED, S_UNICORN, S_ANGEL,
                                        S_CENTAUR,   S_DRAGON,  S_JABBERWOCK,
                                        '\0' };

staticfn boolean landing_spot(coord *, int, int);
staticfn void maybewakesteed(struct monst *);

/* caller has decided that hero can't reach something while mounted */
void
rider_cant_reach(void)
{
#ifdef ZHLANG
    You("aren't skilled enough to reach from %s.", y_monnam(u.usteed));
#else
    You("aren't skilled enough to reach from %s.", y_monnam(u.usteed));
#endif
}

/*** Putting the saddle on ***/

/* Can this monster wear a saddle? */
boolean
can_saddle(struct monst *mtmp)
{
    struct permonst *ptr = mtmp->data;

    return (strchr(steeds, ptr->mlet) && (ptr->msize >= MZ_MEDIUM)
            && (!humanoid(ptr) || ptr->mlet == S_CENTAUR) && !amorphous(ptr)
            && !noncorporeal(ptr) && !is_whirly(ptr) && !unsolid(ptr));
}

int
use_saddle(struct obj *otmp)
{
    struct monst *mtmp;
    struct permonst *ptr;
    int chance;

    if (!u_handsy())
        return ECMD_OK;

    /* Select an animal */
    if (u.uswallow || Underwater || !getdir((char *) 0)) {
#ifdef ZHLANG
        pline1(Never_mind);
#else
        pline1(Never_mind);
#endif
        return ECMD_CANCEL;
    }
    if (!u.dx && !u.dy) {
#ifdef ZHLANG
        pline("Saddle yourself?  Very funny...");
#else
        pline("Saddle yourself?  Very funny...");
#endif
        return ECMD_OK;
    }
    if (!isok(u.ux + u.dx, u.uy + u.dy)
        || !(mtmp = m_at(u.ux + u.dx, u.uy + u.dy)) || !canspotmon(mtmp)) {
#ifdef ZHLANG
        pline("I see nobody there.");
#else
        pline("I see nobody there.");
#endif
        return ECMD_TIME;
    }

    /* Is this a valid monster? */
    if ((mtmp->misc_worn_check & W_SADDLE) != 0L
        || which_armor(mtmp, W_SADDLE)) {
#ifdef ZHLANG
        pline("%s doesn't need another one.", Monnam(mtmp));
#else
        pline("%s doesn't need another one.", Monnam(mtmp));
#endif
        return ECMD_TIME;
    }
    ptr = mtmp->data;
    if (touch_petrifies(ptr) && !uarmg && !Stone_resistance) {
        char kbuf[BUFSZ];

#ifdef ZHLANG
        You("touch %s.", mon_nam(mtmp));
#else
        You("touch %s.", mon_nam(mtmp));
#endif
        if (!(poly_when_stoned(gy.youmonst.data) && polymon(PM_STONE_GOLEM))) {
            Sprintf(kbuf, "attempting to saddle %s",
                    an(pmname(mtmp->data, Mgender(mtmp))));
            instapetrify(kbuf);
        }
    }
    if (ptr == &mons[PM_AMOROUS_DEMON]) {
#ifdef ZHLANG
        pline("Shame on you!");
#else
        pline("Shame on you!");
#endif
        exercise(A_WIS, FALSE);
        return ECMD_TIME;
    }
    if (mtmp->isminion || mtmp->isshk || mtmp->ispriest || mtmp->isgd
        || mtmp->iswiz) {
#ifdef ZHLANG
        pline("I think %s would mind.", mon_nam(mtmp));
#else
        pline("I think %s would mind.", mon_nam(mtmp));
#endif
        return ECMD_TIME;
    }
    if (!can_saddle(mtmp)) {
#ifdef ZHLANG
        You_cant("saddle such a creature.");
#else
        You_cant("saddle such a creature.");
#endif
        return ECMD_TIME;
    }

    /* Calculate your chance */
    chance = ACURR(A_DEX) + ACURR(A_CHA) / 2 + 2 * mtmp->mtame;
    chance += u.ulevel * (mtmp->mtame ? 20 : 5);
    if (!mtmp->mtame)
        chance -= 10 * mtmp->m_lev;
    if (Role_if(PM_KNIGHT))
        chance += 20;
    switch (P_SKILL(P_RIDING)) {
    case P_ISRESTRICTED:
    case P_UNSKILLED:
    default:
        chance -= 20;
        break;
    case P_BASIC:
        break;
    case P_SKILLED:
        chance += 15;
        break;
    case P_EXPERT:
        chance += 30;
        break;
    }
    if (Confusion || Fumbling || Glib)
        chance -= 20;
    else if (uarmg && objdescr_is(uarmg, "riding gloves"))
        /* Bonus for wearing "riding" (but not fumbling) gloves */
        chance += 10;
    else if (uarmf && objdescr_is(uarmf, "riding boots"))
        /* ... or for "riding boots" */
        chance += 10;
    if (otmp->cursed)
        chance -= 50;

    /* [intended] steed becomes alert if possible */
    maybewakesteed(mtmp);

    /* Make the attempt */
    if (rn2(100) < chance) {
#ifdef ZHLANG
        You("put the saddle on %s.", mon_nam(mtmp));
#else
        You("put the saddle on %s.", mon_nam(mtmp));
#endif
        if (otmp->owornmask)
            remove_worn_item(otmp, FALSE);
        freeinv(otmp);
        /* !can_saddle(mtmp) already eliminated above */
        put_saddle_on_mon(otmp, mtmp);
    } else
#ifdef ZHLANG
        pline("%s resists!", Monnam(mtmp));
#else
        pline("%s resists!", Monnam(mtmp));
#endif
    return ECMD_TIME;
}

void
put_saddle_on_mon(struct obj *saddle, struct monst *mtmp)
{
    if (!can_saddle(mtmp) || which_armor(mtmp, W_SADDLE)) {
        if (saddle)
            impossible("put_saddle_on_mon: saddle obj could get orphaned");
        return;
    }
    if (!saddle) {
        if ((saddle = mksobj(SADDLE, TRUE, FALSE)) != 0) {
            fully_identify_obj(saddle);
            /* mpickobj can later override identification if out-of-view */
        } else {
            return;
        }
    }
    if (mpickobj(mtmp, saddle))
        panic("merged saddle?");
    mtmp->misc_worn_check |= W_SADDLE;
    saddle->owornmask = W_SADDLE;
    saddle->leashmon = mtmp->m_id;
    update_mon_extrinsics(mtmp, saddle, TRUE, FALSE);
}

/*** Riding the monster ***/

/* Can we ride this monster?  Caller should also check can_saddle() */
boolean
can_ride(struct monst *mtmp)
{
    return (mtmp->mtame && humanoid(gy.youmonst.data)
            && !verysmall(gy.youmonst.data) && !bigmonst(gy.youmonst.data)
            && (!Underwater || is_swimmer(mtmp->data)));
}

/* the #ride command */
int
doride(void)
{
    boolean forcemount = FALSE;

    if (u.usteed) {
        dismount_steed(DISMOUNT_BYCHOICE);
    } else if (getdir((char *) 0) && isok(u.ux + u.dx, u.uy + u.dy)) {
        if (wizard && y_n("Force the mount to succeed?") == 'y')
            forcemount = TRUE;
        return (mount_steed(m_at(u.ux + u.dx, u.uy + u.dy), forcemount)
                ? ECMD_TIME : ECMD_OK);
    } else {
        return ECMD_CANCEL;
    }
    return ECMD_TIME;
}

/* Start riding, with the given monster */
boolean
mount_steed(
    struct monst *mtmp, /* The animal */
    boolean force)      /* Quietly force this animal */
{
    struct obj *otmp;
    char buf[BUFSZ];
    struct permonst *ptr;

    /* Sanity checks */
    if (u.usteed) {
#ifdef ZHLANG
        You("are already riding %s.", mon_nam(u.usteed));
#else
        You("are already riding %s.", mon_nam(u.usteed));
#endif
        return (FALSE);
    }

    /* Is the player in the right form? */
    if (Hallucination && !force) {
#ifdef ZHLANG
        pline("Maybe you should find a designated driver.");
#else
        pline("Maybe you should find a designated driver.");
#endif
        return (FALSE);
    }
    /* While riding, Wounded_legs refers to the steed's
     * legs, not the hero's legs.
     * That opens up a potential abuse where the player
     * can mount a steed, then dismount immediately to
     * heal leg damage, because leg damage is always
     * healed upon dismount (Wounded_legs context switch).
     * By preventing a hero with Wounded_legs from
     * mounting a steed, the potential for abuse is
     * reduced.  However, dismounting still immediately
     * heals the steed's wounded legs.  [In 3.4.3 and
     * earlier, that unintentionally made the hero's
     * temporary 1 point Dex loss become permanent.]
     */
    if (Wounded_legs) {
        char qbuf[QBUFSZ];

        legs_in_no_shape("riding", FALSE);
        Sprintf(qbuf, "Heal your leg%s?",
                ((HWounded_legs & BOTH_SIDES) == BOTH_SIDES) ? "s" : "");
        if (force && wizard && y_n(qbuf) == 'y')
            heal_legs(0);
        else
            return (FALSE);
    }

    if (Upolyd && (!humanoid(gy.youmonst.data)
                   || verysmall(gy.youmonst.data)
                   || bigmonst(gy.youmonst.data)
                   || slithy(gy.youmonst.data))) {
#ifdef ZHLANG
        You("won't fit on a saddle.");
#else
        You("won't fit on a saddle.");
#endif
        return (FALSE);
    }
    if (!force && (near_capacity() > SLT_ENCUMBER)) {
#ifdef ZHLANG
        You_cant("do that while carrying so much stuff.");
#else
        You_cant("do that while carrying so much stuff.");
#endif
        return (FALSE);
    }

    /* Can the player reach and see the monster? */
    if (!mtmp || (!force && ((Blind && !Blind_telepat) || mtmp->mundetected
                             || M_AP_TYPE(mtmp) == M_AP_FURNITURE
                             || M_AP_TYPE(mtmp) == M_AP_OBJECT))) {
#ifdef ZHLANG
        pline("I see nobody there.");
#else
        pline("I see nobody there.");
#endif
        return (FALSE);
    }
    if (mtmp->data == &mons[PM_LONG_WORM]
        && (u.ux + u.dx != mtmp->mx || u.uy + u.dy != mtmp->my)) {
        /* As of 3.6.2:  test_move(below) is used to check for trying to mount
           diagonally into or out of a doorway or through a tight squeeze;
           attempting to mount a tail segment when hero was not adjacent
           to worm's head could trigger an impossible() in worm_cross()
           called from test_move(), so handle not-on-head before that */
#ifdef ZHLANG
        You("couldn't ride %s, let alone its tail.", a_monnam(mtmp));
#else
        You("couldn't ride %s, let alone its tail.", a_monnam(mtmp));
#endif
        return FALSE;
    }
    if (u.uswallow || u.ustuck || u.utrap || Punished
        || !test_move(u.ux, u.uy, mtmp->mx - u.ux, mtmp->my - u.uy,
                      TEST_MOVE)) {
        if (Punished || !(u.uswallow || u.ustuck || u.utrap))
#ifdef ZHLANG
            You("are unable to swing your %s over.", body_part(LEG));
#else
            You("are unable to swing your %s over.", body_part(LEG));
#endif
        else
#ifdef ZHLANG
            You("are stuck here for now.");
#else
            You("are stuck here for now.");
#endif
        return (FALSE);
    }

    /* Is this a valid monster? */
    otmp = which_armor(mtmp, W_SADDLE);
    if (!otmp) {
#ifdef ZHLANG
        pline("%s is not saddled.", Monnam(mtmp));
#else
        pline("%s is not saddled.", Monnam(mtmp));
#endif
        return (FALSE);
    }

    ptr = mtmp->data;
    if (touch_petrifies(ptr) && !Stone_resistance) {
        char kbuf[BUFSZ];

#ifdef ZHLANG
        You("touch %s.", mon_nam(mtmp));
#else
        You("touch %s.", mon_nam(mtmp));
#endif
        Sprintf(kbuf, "attempting to ride %s",
                an(pmname(mtmp->data, Mgender(mtmp))));
        instapetrify(kbuf);
    }
    if (!mtmp->mtame || mtmp->isminion) {
#ifdef ZHLANG
        pline("I think %s would mind.", mon_nam(mtmp));
#else
        pline("I think %s would mind.", mon_nam(mtmp));
#endif
        return (FALSE);
    }
    if (mtmp->mtrapped) {
        struct trap *t = t_at(mtmp->mx, mtmp->my);

#ifdef ZHLANG
        You_cant("mount %s while %s's trapped in %s.", mon_nam(mtmp),
                 mhe(mtmp), an(trapname(t->ttyp, FALSE)));
#else
        You_cant("mount %s while %s's trapped in %s.", mon_nam(mtmp),
                 mhe(mtmp), an(trapname(t->ttyp, FALSE)));
#endif
        return (FALSE);
    }

    if (!force && !Role_if(PM_KNIGHT) && !(--mtmp->mtame)) {
        /* no longer tame */
        newsym(mtmp->mx, mtmp->my);
#ifdef ZHLANG
        pline("%s resists%s!", Monnam(mtmp),
              mtmp->mleashed ? " and its leash comes off" : "");
#else
        pline("%s resists%s!", Monnam(mtmp),
              mtmp->mleashed ? " and its leash comes off" : "");
#endif
        if (mtmp->mleashed)
            m_unleash(mtmp, FALSE);
        return (FALSE);
    }
    if (!force && Underwater && !is_swimmer(ptr)) {
#ifdef ZHLANG
        You_cant("ride that creature while under %s.",
                 hliquid("water"));
#else
        You_cant("ride that creature while under %s.",
                 hliquid("water"));
#endif
        return (FALSE);
    }
    if (!can_saddle(mtmp) || !can_ride(mtmp)) {
#ifdef ZHLANG
        You_cant("ride such a creature.");
#else
        You_cant("ride such a creature.");
#endif
        return FALSE;
    }

    /* Is the player impaired? */
    if (!force && !is_floater(ptr) && !is_flyer(ptr) && Levitation
        && !Lev_at_will) {
#ifdef ZHLANG
        You("cannot reach %s.", mon_nam(mtmp));
#else
        You("cannot reach %s.", mon_nam(mtmp));
#endif
        return (FALSE);
    }
    if (!force && uarm && is_metallic(uarm) && greatest_erosion(uarm)) {
#ifdef ZHLANG
        Your("%s armor is too stiff to be able to mount %s.",
             uarm->oeroded ? "rusty" : "corroded", mon_nam(mtmp));
#else
        Your("%s armor is too stiff to be able to mount %s.",
             uarm->oeroded ? "rusty" : "corroded", mon_nam(mtmp));
#endif
        return (FALSE);
    }
    if (!force
        && (Confusion || Fumbling || Glib || Wounded_legs || otmp->cursed
            || otmp->greased
            || (u.ulevel + mtmp->mtame < rnd(MAXULEV / 2 + 5)))) {
        if (Levitation) {
#ifdef ZHLANG
            pline("%s slips away from you.", Monnam(mtmp));
#else
            pline("%s slips away from you.", Monnam(mtmp));
#endif
            return FALSE;
        }
#ifdef ZHLANG
        You("slip while trying to get on %s.", mon_nam(mtmp));
#else
        You("slip while trying to get on %s.", mon_nam(mtmp));
#endif

        Sprintf(buf, "slipped while mounting %s",
                /* "a saddled mumak" or "a saddled pony called Dobbin" */
                x_monnam(mtmp, ARTICLE_A, (char *) 0,
                         SUPPRESS_IT | SUPPRESS_INVISIBLE
                             | SUPPRESS_HALLUCINATION,
                         TRUE));
        losehp(Maybe_Half_Phys(rn1(5, 10)), buf, NO_KILLER_PREFIX);
        return (FALSE);
    }

    /* Success */
    maybewakesteed(mtmp);
    if (!force) {
        if (Levitation && !is_floater(ptr) && !is_flyer(ptr))
            /* Must have Lev_at_will at this point */
#ifdef ZHLANG
            pline("%s magically floats up!", Monnam(mtmp));
#else
            pline("%s magically floats up!", Monnam(mtmp));
#endif
#ifdef ZHLANG
        You("mount %s.", mon_nam(mtmp));
#else
        You("mount %s.", mon_nam(mtmp));
#endif
        if (Flying)
#ifdef ZHLANG
            You("and %s take flight together.", mon_nam(mtmp));
#else
            You("and %s take flight together.", mon_nam(mtmp));
#endif
    }
    /* setuwep handles polearms differently when you're mounted */
    if (uwep && is_pole(uwep))
        gu.unweapon = FALSE;
    u.usteed = mtmp;
    {
        boolean was_stealthy = Stealth != 0;

        steed_vs_stealth();
        if (was_stealthy && !Stealth)
#ifdef ZHLANG
            You("aren't stealthy anymore.");
#else
            You("aren't stealthy anymore.");
#endif
    }
    remove_monster(mtmp->mx, mtmp->my);
    teleds(mtmp->mx, mtmp->my, TELEDS_ALLOW_DRAG);
    disp.botl = TRUE;
    return TRUE;
}

/* You and your steed have moved */
void
exercise_steed(void)
{
    if (!u.usteed)
        return;

    /* It takes many turns of riding to exercise skill */
    if (++u.urideturns >= 100) {
        u.urideturns = 0;
        use_skill(P_RIDING, 1);
    }
    return;
}

/* The player kicks or whips the steed */
void
kick_steed(void)
{
    char He[BUFSZ]; /* monverbself() appends to the "He"/"She"/"It" value */
    if (!u.usteed)
        return;

    /* [ALI] Various effects of kicking sleeping/paralyzed steeds */
    if (helpless(u.usteed)) {
        /* We assume a message has just been output of the form
         * "You kick <steed>."
         */
        Strcpy(He, mhe(u.usteed));
        *He = highc(*He);
        if ((u.usteed->mcanmove || u.usteed->mfrozen) && !rn2(2)) {
            if (u.usteed->mcanmove)
                u.usteed->msleeping = 0;
            else if (u.usteed->mfrozen > 2)
                u.usteed->mfrozen -= 2;
            else {
                u.usteed->mfrozen = 0;
                u.usteed->mcanmove = 1;
            }
            if (helpless(u.usteed))
#ifdef ZHLANG
                pline("%s stirs.", He);
#else
                pline("%s stirs.", He);
#endif
            else
                /* if hallucinating, might yield "He rouses herself" or
                   "She rouses himself" */
#ifdef ZHLANG
                pline("%s!", monverbself(u.usteed, He, "rouse", (char *) 0));
#else
                pline("%s!", monverbself(u.usteed, He, "rouse", (char *) 0));
#endif
        } else
#ifdef ZHLANG
            pline("%s does not respond.", He);
#else
            pline("%s does not respond.", He);
#endif
        return;
    }

    /* Make the steed less tame and check if it resists */
    if (u.usteed->mtame)
        u.usteed->mtame--;
    if (!u.usteed->mtame && u.usteed->mleashed)
        m_unleash(u.usteed, TRUE);
    if (!u.usteed->mtame
        || (u.ulevel + u.usteed->mtame < rnd(MAXULEV / 2 + 5))) {
        newsym(u.usteed->mx, u.usteed->my);
        dismount_steed(DISMOUNT_THROWN);
        return;
    }

#ifdef ZHLANG
    pline("%s gallops!", Monnam(u.usteed));
#else
    pline("%s gallops!", Monnam(u.usteed));
#endif
    u.ugallop += rn1(20, 30);
    return;
}

/*
 * Try to find a dismount point adjacent to the steed's location.
 * If all else fails, try enexto().  Use enexto() as a last resort because
 * enexto() chooses its point randomly, possibly even outside the
 * room's walls, which is not what we want.
 * Adapted from mail daemon code.
 */
staticfn boolean
landing_spot(
    coord *spot, /* landing position (we fill it in) */
    int reason,
    int forceit)
{
    coord cc, try[8]; /* 8: the 8 spots adjacent to the hero's spot */
    int i, j, best_j, clockwise_j, counterclk_j,
        n, viable, distance, min_distance = -1;
    coordxy x, y;
    boolean found, impaird, kn_trap, boulder;
    struct trap *t;

    (void) memset((genericptr_t) try, 0, sizeof try);
    n = 0;
    j = xytodir(u.dx, u.dy);
    if (reason == DISMOUNT_KNOCKED && j != DIR_ERR) {
        /* we'll check preferred location first; if viable it'll be picked */
        best_j = j;
        try[0].x = u.dx, try[0].y = u.dy;
        /* the two next best locations are checked second and third */
        i = rn2(2);
        clockwise_j = DIR_RIGHT(j); /* (j + 1) % 8 */
        dirtocoord(&cc, clockwise_j);
        try[1 + i].x = cc.x, try[1 + i].y = cc.y; /* [1] or [2] */
        counterclk_j = DIR_LEFT(j); /* (j + 8 - 1) % 8 */
        dirtocoord(&cc, counterclk_j);
        try[2 - i].x = cc.x, try[2 - i].y = cc.y; /* [2] or [1] */
        n = 3;
        debugpline3("knock from saddle: best %s, next %s or %s",
                    directionname(best_j),
                    directionname(clockwise_j), directionname(counterclk_j));
    } else {
        best_j = clockwise_j = counterclk_j = -1;
    }
    for (j = 0; j < N_DIRS; ++j) {
        /* fortunately NODIAG() handling isn't needed for DISMOUNT_KNOCKED
           because hero can only ride when humanoid */
        if (j == best_j || j == clockwise_j || j == counterclk_j)
            continue;
        /* j==0 is W, j==1 NW, j==2 N, j==3 NE, ..., around to j==7 SW;
           so odd j values are diagonal directions here */
        if (reason == DISMOUNT_POLY && NODIAG(u.umonnum) && (j % 1) != 0)
            continue;
        dirtocoord(&cc, j);
        try[n++] = cc;
    }

    /*
     * Up to three passes;
     * i==0: voluntary dismount without impairment avoids known traps and
     *       boulders;
     * i==1: voluntary dismount with impairment or knocked out of saddle
     *       avoids boulders but allows known traps;
     * i==2: other, allow traps and boulders.
     *
     * Fallback to i==1 if nothing appropriate was found for i==0 and
     * to i==2 as last resort.
     */
    impaird = (Stunned || Confusion || Fumbling);
    viable = 0;
    found = FALSE;
    for (i = (reason == DISMOUNT_BYCHOICE && !impaird) ? 0
             : ((reason == DISMOUNT_BYCHOICE && impaird)
                || reason == DISMOUNT_KNOCKED) ? 1
               : 2;
         i <= 2 && !found; ++i) {
        for (j = 0; j < n; ++j) {
            x = u.ux + try[j].x;
            y = u.uy + try[j].y;
            if (!isok(x, y) || u_at(x, y)) /* [note: u_at() can't happen] */
                continue;

            if (accessible(x, y) && !MON_AT(x, y)
                && test_move(u.ux, u.uy, x - u.ux, y - u.uy, TEST_MOVE)) {
                ++viable;
                distance = distu(x, y);
                if (min_distance < 0 /* no viable candidate yet */
                    /* or better than pending candidate (note: orthogonal
                       spots are distance 1 and diagonal ones distance 2;
                       treating one as better than the other is arbitrary
                       and not wanted for DISMOUNT_KNOCKED) */
                    || ((best_j == -1) ? (distance < min_distance) : (j < 3))
                    /* or equally good, maybe substitute this one */
                    || (distance == min_distance && !rn2(viable))) {
                    /* traps avoided on pass 0; boulders avoided on 0 and 1 */
                    kn_trap = i == 0 && ((t = t_at(x, y)) != 0 && t->tseen
                                         && t->ttyp != VIBRATING_SQUARE);
                    boulder = i <= 1 && (sobj_at(BOULDER, x, y)
                                         && !throws_rocks(gy.youmonst.data));
                    if (!kn_trap && !boulder) {
                        spot->x = x;
                        spot->y = y;
                        min_distance = distance;
                        found = TRUE;
                        if (best_j != -1 && j < 3)
                            /* since best_j is first candidate (j==0), j==1
                               and j==2 can only get here when best_j was
                               not viable; 50:50 chance for clockwise_j to
                               come before counterclk_j so each has same
                               chance to be next after best_j */
                            break;
                    }
                }
            }
        }
    }

    /* If we didn't find a good spot and forceit is on, try enexto(). */
    if (forceit && !found)
        found = enexto(spot, u.ux, u.uy, gy.youmonst.data);

    return found;
}

/* Stop riding the current steed */
void
dismount_steed(
    int reason) /* Player was thrown off etc. */
{
    struct monst *mtmp;
    struct obj *otmp;
    const char *verb;
    coord cc, steedcc;
    unsigned save_utrap = u.utrap;
    boolean ulev, ufly,
            repair_leg_damage = (Wounded_legs != 0L),
            have_spot = landing_spot(&cc, reason, 0);

    mtmp = u.usteed; /* make a copy of steed pointer */
    /* Sanity check */
    if (!mtmp) /* Just return silently */
        return;
    u.usteed = 0; /* affects Fly test; could hypothetically affect Lev;
                   * also affects u_locomotion() */
    ufly = Flying ? TRUE : FALSE;
    ulev = Levitation ? TRUE : FALSE;
    verb = u_locomotion("fall"); /* only used for _FELL and _KNOCKED */
    u.usteed = mtmp;

    /* Check the reason for dismounting */
    otmp = which_armor(mtmp, W_SADDLE);
    switch (reason) {
    case DISMOUNT_THROWN:
        verb = "are thrown";
        FALLTHROUGH;
        /*FALLTHRU*/
    case DISMOUNT_KNOCKED:
    case DISMOUNT_FELL:
#ifdef ZHLANG
        You("%s off of %s!", verb, mon_nam(mtmp));
#else
        You("%s off of %s!", verb, mon_nam(mtmp));
#endif
        if (!have_spot)
            have_spot = landing_spot(&cc, reason, 1);
        if (!ulev && !ufly) {
            losehp(Maybe_Half_Phys(rn1(10, 10)), "riding accident",
                   KILLED_BY_AN);
            set_wounded_legs(BOTH_SIDES, (int) HWounded_legs + rn1(5, 5));
            repair_leg_damage = FALSE;
        }
        break;
    case DISMOUNT_POLY:
#ifdef ZHLANG
        You("can no longer ride %s.", mon_nam(u.usteed));
#else
        You("can no longer ride %s.", mon_nam(u.usteed));
#endif
        if (!have_spot)
            have_spot = landing_spot(&cc, reason, 1);
        break;
    case DISMOUNT_ENGULFED:
        /* caller displays message */
        break;
    case DISMOUNT_BONES:
        /* hero has just died... */
        break;
    case DISMOUNT_GENERIC:
        /* no messages, just make it so */
        break;
    case DISMOUNT_BYCHOICE:
    default:
        if (otmp && otmp->cursed) {
#ifdef ZHLANG
            You("can't.  The saddle %s cursed.",
                otmp->bknown ? "is" : "seems to be");
#else
            You("can't.  The saddle %s cursed.",
                otmp->bknown ? "is" : "seems to be");
#endif
            otmp->bknown = 1; /* ok to skip set_bknown() here */
            return;
        }
        if (!have_spot) {
#ifdef ZHLANG
            You("can't.  There isn't anywhere for you to stand.");
#else
            You("can't.  There isn't anywhere for you to stand.");
#endif
            return;
        }
        if (!has_mgivenname(mtmp)) {
#ifdef ZHLANG
            pline("You've been through the dungeon on %s with no name.",
                  an(pmname(mtmp->data, Mgender(mtmp))));
#else
            pline("You've been through the dungeon on %s with no name.",
                  an(pmname(mtmp->data, Mgender(mtmp))));
#endif
            if (Hallucination)
#ifdef ZHLANG
                pline("It felt good to get out of the rain.");
#else
                pline("It felt good to get out of the rain.");
#endif
        } else
#ifdef ZHLANG
            You("dismount %s.", mon_nam(mtmp));
#else
            You("dismount %s.", mon_nam(mtmp));
#endif
    }
    /* While riding, Wounded_legs refers to the steed's legs;
       after dismounting, it reverts to the hero's legs. */
    if (repair_leg_damage)
        heal_legs(1);

    /* Release the steed */
    u.usteed = (struct monst *) NULL;
    u.ugallop = 0L;
    {
        boolean was_stealthy = Stealth != 0;

        steed_vs_stealth();
        if (Stealth && !was_stealthy)
#ifdef ZHLANG
            You("seem less noisy now.");
#else
            You("seem less noisy now.");
#endif
    }

    if (u.utraptype == TT_BEARTRAP
        || u.utraptype == TT_PIT
        || u.utraptype == TT_WEB) {
        mtmp->mtrapped = 1;
    }

    /*
     * rloc(), rloc_to(), and monkilled()->mondead()->m_detach() all
     * expect mtmp to be on the map or else have mtmp->mx be 0, but
     * setting the latter to 0 here would interfere with dropping
     * the saddle.  Prior to 3.6.2, being off the map didn't matter.
     *
     * place_monster() expects mtmp to be alive and not be u.usteed.
     *
     * Unfortunately, <u.ux,u.uy> (former steed's implicit location)
     * might now be occupied by an engulfer, so we can't just put mtmp
     * at that spot.  An engulfer's previous spot will be unoccupied
     * but we don't know where that was and even if we did, it might
     * be hostile terrain.
     */
    steedcc.x = u.ux, steedcc.y = u.uy;
    if (m_at(u.ux, u.uy)) {
        /* hero's spot has a monster in it; hero must have been plucked
           from saddle as engulfer moved into his spot--other dismounts
           shouldn't run into this situation; find nearest viable spot */
        if (!enexto(&steedcc, u.ux, u.uy, mtmp->data)
            /* no spot? must have been engulfed by a lurker-above over
               water or lava; try requesting a location for a flyer */
            && !enexto(&steedcc, u.ux, u.uy, &mons[PM_BAT]))
            /* still no spot; last resort is any spot within bounds */
            (void) enexto(&steedcc, u.ux, u.uy, &mons[PM_GHOST]);
    }

    if (!DEADMONSTER(mtmp)) {
        gi.in_steed_dismounting++;
        place_monster(mtmp, steedcc.x, steedcc.y);
        gi.in_steed_dismounting--;

        /* if for bones, there's no reason to place the hero;
           we want to make room for potential ghost, so move steed */
        if (reason == DISMOUNT_BONES) {
            /* move the steed to an adjacent square */
            if (enexto(&cc, u.ux, u.uy, mtmp->data))
                rloc_to(mtmp, cc.x, cc.y);
            else /* evidently no room nearby; move steed elsewhere */
                (void) rloc(mtmp, RLOC_ERR | RLOC_NOMSG);
            return;
        }

        /* Set hero's and/or steed's positions.  Usually try moving the
           hero first.  Note: for DISMOUNT_ENGULFED, caller hasn't set
           u.uswallow yet but has set u.ustuck. */
        if (!u.uswallow && !u.ustuck && have_spot) {
            struct permonst *mdat = mtmp->data;

            /* The steed may drop into water/lava */
            if (grounded(mdat)) {
                if (is_pool(u.ux, u.uy)) {
                    if (!Underwater)
#ifdef ZHLANG
                        pline("%s falls into the %s!", Monnam(mtmp),
                              surface(u.ux, u.uy));
#else
                        pline("%s falls into the %s!", Monnam(mtmp),
                              surface(u.ux, u.uy));
#endif
                    if (!cant_drown(mdat)) {
                        killed(mtmp);
                        adjalign(-1);
                    }
                } else if (is_lava(u.ux, u.uy)) {
#ifdef ZHLANG
                    pline("%s is pulled into the %s!", Monnam(mtmp),
                          hliquid("lava"));
#else
                    pline("%s is pulled into the %s!", Monnam(mtmp),
                          hliquid("lava"));
#endif
                    if (!likes_lava(mdat)) {
                        killed(mtmp);
                        adjalign(-1);
                    }
                }
            }
            /* Steed dismounting consists of two steps: being moved to another
             * square, and descending to the floor.  We have functions to do
             * each of these activities, but they're normally called
             * individually and include an attempt to look at or pick up the
             * objects on the floor:
             * teleds() --> spoteffects() --> pickup()
             * float_down() --> pickup()
             * We use this kludge to make sure there is only one such attempt.
             *
             * Clearly this is not the best way to do it.  A full fix would
             * involve having these functions not call pickup() at all,
             * instead calling them first and calling pickup() afterwards.
             * But it would take a lot of work to keep this change from
             * having any unforeseen side effects (for instance, you would
             * no longer be able to walk onto a square with a hole, and
             * autopickup before falling into the hole).
             */
            /* [ALI] No need to move the player if the steed died. */
            if (!DEADMONSTER(mtmp)) {
                /* Keep steed here, move the player to cc;
                 * teleds() clears u.utrap
                 */
                gi.in_steed_dismounting = TRUE;
                teleds(cc.x, cc.y, TELEDS_ALLOW_DRAG);
                if (sobj_at(BOULDER, cc.x, cc.y))
                    sokoban_guilt();
                gi.in_steed_dismounting = FALSE;

                /* Put your steed in your trap */
                if (save_utrap)
                    (void) mintrap(mtmp, NO_TRAP_FLAGS);
            }

        /* Couldn't move hero... try moving the steed. */
        } else if (enexto(&cc, u.ux, u.uy, mtmp->data)) {
            /* Keep player here, move the steed to cc */
            rloc_to(mtmp, cc.x, cc.y);
            /* Player stays put */

        /* Otherwise, steed goes bye-bye. */
        } else {
#if 1       /* original there's-no-room handling */
            if (reason == DISMOUNT_BYCHOICE) {
                /* [un]#ride: hero gets credit/blame for killing steed */
                killed(mtmp);
                adjalign(-1);
            } else {
                /* other dismount: kill former steed with no penalty;
                   damage type is just "neither AD_DGST nor -AD_RBRE" */
                monkilled(mtmp, "", -AD_PHYS);
            }
#else
            /* Can't use this [yet?] because it violates monmove()'s
             * assumption that a moving monster (engulfer) can't cause
             * another monster (steed) to be removed from the fmon list.
             * That other monster (steed) might be cached as the next one
             * to move.
             */
            /* migrate back to this level if hero leaves and returns
               or to next level if it is happening in the endgame */
            mdrop_special_objs(mtmp);
            deal_with_overcrowding(mtmp);
#endif
        }
    } /* !DEADMONST(mtmp) */

    /* usually return the hero to the surface */
    if (reason != DISMOUNT_ENGULFED && reason != DISMOUNT_BONES) {
        gi.in_steed_dismounting = TRUE;
        (void) float_down(0L, W_SADDLE);
        gi.in_steed_dismounting = FALSE;
        disp.botl = TRUE;
        encumber_msg();
        gv.vision_full_recalc = 1;
    } else
        disp.botl = TRUE;
    /* polearms behave differently when not mounted */
    if (uwep && is_pole(uwep))
        gu.unweapon = TRUE;
    return;
}

/* when attempting to saddle or mount a sleeping steed, try to wake it up
   (for the saddling case, it won't be u.usteed yet) */
staticfn void
maybewakesteed(struct monst *steed)
{
    int frozen = (int) steed->mfrozen;
    boolean wasimmobile = helpless(steed);

    steed->msleeping = 0;
    if (frozen) {
        frozen = (frozen + 1) / 2; /* half */
        /* might break out of timed sleep or paralysis */
        if (!rn2(frozen)) {
            steed->mfrozen = 0;
            steed->mcanmove = 1;
        } else {
            /* didn't awake, but remaining duration is halved */
            steed->mfrozen = frozen;
        }
    }
    if (wasimmobile && !helpless(steed))
#ifdef ZHLANG
        pline("%s wakes up.", Monnam(steed));
#else
        pline("%s wakes up.", Monnam(steed));
#endif
    /* regardless of waking, terminate any meal in progress */
    finish_meating(steed);
}

/* steed has taken on a new shape */
void
poly_steed(
    struct monst *steed,
    struct permonst *oldshape)
{
    if (!can_saddle(steed) || !can_ride(steed)) {
        /* can't get here; newcham() -> mon_break_armor() -> m_lose_armor()
           removes saddle and/or forces hero to dismount, if applicable,
           before newcham() calls us */
        dismount_steed(DISMOUNT_FELL);
    } else {
        char buf[BUFSZ];

        Strcpy(buf, x_monnam(steed, ARTICLE_YOUR, (char *) 0,
                             SUPPRESS_SADDLE, FALSE));
        if (oldshape != steed->data)
            (void) strsubst(buf, "your ", "your new ");
#ifdef ZHLANG
        You("adjust yourself in the saddle on %s.", buf);
#else
        You("adjust yourself in the saddle on %s.", buf);
#endif

        /* riding blocks stealth unless hero+steed fly */
        steed_vs_stealth();
    }
}

/* decide whether hero's steed is able to move;
   doesn't check for holding traps--those affect the hero directly */
boolean
stucksteed(boolean checkfeeding)
{
    struct monst *steed = u.usteed;

    if (steed) {
        /* check whether steed can move */
        if (helpless(steed)) {
#ifdef ZHLANG
            pline("%s won't move!", YMonnam(steed));
#else
            pline("%s won't move!", YMonnam(steed));
#endif
            return TRUE;
        }
        /* optionally check whether steed is in the midst of a meal */
        if (checkfeeding && steed->meating) {
#ifdef ZHLANG
            pline("%s is still eating.", YMonnam(steed));
#else
            pline("%s is still eating.", YMonnam(steed));
#endif
            return TRUE;
        }
    }
    return FALSE;
}

void
place_monster(struct monst *mon, coordxy x, coordxy y)
{
    struct monst *othermon;
    const char *monnm, *othnm;
    char buf[QBUFSZ];

    buf[0] = '\0';
    /* normal map bounds are <1..COLNO-1,0..ROWNO-1> but sometimes
       vault guards (either living or dead) are parked at <0,0>;
       their mstate should have the MON_PARKED bit set (post-5.0.0) */
    if (!isok(x, y)
        && !(((mon->mstate & MON_PARKED) != 0) || PARKEDMONSTER(mon))) {
        describe_level(buf, 0);
        impossible("trying to place %s at <%d,%d> mstate:%lx on %s",
                   minimal_monnam(mon, TRUE), x, y, mon->mstate, buf);
        x = y = 0;
    }
    if ((mon == u.usteed && !gi.in_steed_dismounting)
        /* special case is for convoluted vault guard handling */
        || (DEADMONSTER(mon) && !PARKEDMONSTER(mon))) {
        describe_level(buf, 0);
        impossible("placing %s onto map, mstate:%lx, on %s?",
                   (mon == u.usteed) ? "steed" : "defunct monster",
                   mon->mstate, buf);
        return;
    }
    if ((othermon = svl.level.monsters[x][y]) != 0) {
        describe_level(buf, 0);
        monnm = minimal_monnam(mon, FALSE);
        othnm = (mon != othermon) ? minimal_monnam(othermon, TRUE) : "itself";
        impossible("placing %s over %s at <%d,%d>, mstates:%lx %lx on %s?",
                   monnm, othnm, x, y, othermon->mstate, mon->mstate, buf);
    }
    mon->mx = x, mon->my = y;
    svl.level.monsters[x][y] = mon;
    /* even though MON_FLOOR is not actually a bit currently
       (MON_FLOOR == 0) we want to preserve some of the other
       bits that may be set. We'll probably make MON_FLOOR an
       actual bit one day */

    mon->mstate &= ~(MON_OFFMAP | MON_DETACH | MON_LIMBO | MON_MIGRATING);

    /* We don't mess with these bits above:
       MON_BUBBLEMOVE | MON_ENDGAME_FREE | MON_ENDGAME_MIGR
       | MON_OBLITERATE | MON_STILL_ARRIVING | MON_PARKED
     */
}

/*steed.c*/
