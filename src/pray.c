/* NetHack 5.0	pray.c	$NHDT-Date: 1781973062 2026/06/20 16:31:02 $  $NHDT-Branch: NetHack-5.0 $:$NHDT-Revision: 1.253 $ */
/* Copyright (c) Benson I. Margulies, Mike Stephenson, Steve Linhart, 1989. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

staticfn int prayer_done(void);
staticfn void maybe_turn_mon_iter(struct monst *);
staticfn struct obj *worst_cursed_item(void);
staticfn int in_trouble(void);
staticfn void fix_curse_trouble(struct obj *, const char *);
staticfn void fix_worst_trouble(int);
staticfn void angrygods(aligntyp);
staticfn void at_your_feet(const char *);
staticfn void gcrownu(void);
staticfn void give_spell(void);
staticfn void pleased(aligntyp);
staticfn void godvoice(aligntyp, const char *);
staticfn void god_zaps_you(aligntyp);
staticfn void fry_by_god(aligntyp, boolean);
staticfn void gods_angry(aligntyp);
staticfn void gods_upset(aligntyp);
staticfn void consume_offering(struct obj *);
staticfn void offer_too_soon(aligntyp);
staticfn void offer_real_amulet(struct obj *, aligntyp); /* NORETURN */
staticfn void offer_negative_valued(boolean, aligntyp);
staticfn void offer_fake_amulet(struct obj *, boolean, aligntyp);
staticfn void offer_different_alignment_altar(struct obj *, aligntyp);
staticfn void sacrifice_your_race(struct obj *, boolean, aligntyp);
staticfn int bestow_artifact(uchar);
staticfn int sacrifice_value(struct obj *);
staticfn int eval_offering(struct obj *, aligntyp);
staticfn void offer_corpse(struct obj *, boolean, aligntyp);
staticfn boolean pray_revive(void);
staticfn boolean water_prayer(boolean);
staticfn boolean blocked_boulder(int, int);

/* simplify a few tests */
#define Cursed_obj(obj, typ) ((obj) && (obj)->otyp == (typ) && (obj)->cursed)

/*
 * Logic behind deities and altars and such:
 * + prayers are made to your god if not on an altar, and to the altar's god
 *   if you are on an altar
 * + If possible, your god answers all prayers, which is why bad things happen
 *   if you try to pray on another god's altar
 * + sacrifices work basically the same way, but the other god may decide to
 *   accept your allegiance, after which they are your god.  If rejected,
 *   your god takes over with your punishment.
 * + if you're in Gehennom, all messages come from Moloch
 */

/*
 *      Moloch, who dwells in Gehennom, is the "renegade" cruel god
 *      responsible for the theft of the Amulet from Marduk, the Creator.
 *      Moloch is unaligned.
 */
static const char *const Moloch = "Moloch";

static const char *const godvoices[] = {
    "booms out", "thunders", "rings out", "booms",
};

#define PIOUS 20
#define DEVOUT 14
#define FERVENT 9
#define STRIDENT 4

/*
 * The actual trouble priority is determined by the order of the
 * checks performed in in_trouble() rather than by these numeric
 * values, so keep that code and these values synchronized in
 * order to have the values be meaningful.
 */

#define TROUBLE_STONED 14
#define TROUBLE_SLIMED 13
#define TROUBLE_STRANGLED 12
#define TROUBLE_LAVA 11
#define TROUBLE_SICK 10
#define TROUBLE_STARVING 9
#define TROUBLE_REGION 8 /* stinking cloud */
#define TROUBLE_HIT 7
#define TROUBLE_LYCANTHROPE 6
#define TROUBLE_COLLAPSING 5
#define TROUBLE_STUCK_IN_WALL 4
#define TROUBLE_CURSED_LEVITATION 3
#define TROUBLE_UNUSEABLE_HANDS 2
#define TROUBLE_CURSED_BLINDFOLD 1

#define TROUBLE_PUNISHED (-1)
#define TROUBLE_FUMBLING (-2)
#define TROUBLE_CURSED_ITEMS (-3)
#define TROUBLE_SADDLE (-4)
#define TROUBLE_BLIND (-5)
#define TROUBLE_POISONED (-6)
#define TROUBLE_WOUNDED_LEGS (-7)
#define TROUBLE_HUNGRY (-8)
#define TROUBLE_STUNNED (-9)
#define TROUBLE_CONFUSED (-10)
#define TROUBLE_HALLUCINATION (-11)


#define ugod_is_angry() (u.ualign.record < 0)
#define on_altar() IS_ALTAR(levl[u.ux][u.uy].typ)
#define on_shrine() ((levl[u.ux][u.uy].altarmask & AM_SHRINE) != 0)
#define a_align(x, y) ((aligntyp) Amask2align(levl[x][y].altarmask & AM_MASK))

/* used by turn undead iteration function; always reinitialized
   before iterating that, so don't need to be globals */
static int turn_undead_range;
static int turn_undead_msg_cnt;

/* critically low hit points if hp <= 5 or hp <= maxhp/N for some N */
boolean
critically_low_hp(
    boolean only_if_injured) /* determines whether maxhp <= 5 matters */
{
    int divisor, hplim,
        curhp = Upolyd ? u.mh : u.uhp,
        maxhp = Upolyd ? u.mhmax : u.uhpmax;

    if (only_if_injured && !(curhp < maxhp))
        return FALSE;
    /* if maxhp is extremely high, use lower threshold for the division test
       (golden glow cuts off at 11+5*lvl, nurse interaction at 25*lvl; this
       ought to use monster hit dice--and a smaller multiplier--rather than
       ulevel when polymorphed, but polyself doesn't maintain that) */
    hplim = 15 * u.ulevel;
    if (maxhp > hplim)
        maxhp = hplim;
    /* 7 used to be the unconditional divisor */
    switch (xlev_to_rank(u.ulevel)) { /* maps 1..30 into 0..8 */
    case 0:
    case 1:
        divisor = 5;
        break; /* explvl 1 to 5 */
    case 2:
    case 3:
        divisor = 6;
        break; /* explvl 6 to 13 */
    case 4:
    case 5:
        divisor = 7;
        break; /* explvl 14 to 21 */
    case 6:
    case 7:
        divisor = 8;
        break; /* explvl 22 to 29 */
    default:
        divisor = 9;
        break; /* explvl 30+ */
    }
    /* 5 is a magic number in TROUBLE_HIT handling below */
    return (boolean) (curhp <= 5 || curhp * divisor <= maxhp);
}

/* return True if surrounded by impassible rock, regardless of the state
   of your own location (for example, inside a doorless closet) */
boolean
stuck_in_wall(void)
{
    int i, j, x, y, count = 0;

    if (Passes_walls)
        return FALSE;
    for (i = -1; i <= 1; i++) {
        x = u.ux + i;
        for (j = -1; j <= 1; j++) {
            if (!i && !j)
                continue;
            y = u.uy + j;
            if (!isok(x, y)
                || (IS_OBSTRUCTED(levl[x][y].typ)
                    && (levl[x][y].typ != SDOOR && levl[x][y].typ != SCORR))
                || (blocked_boulder(i, j) && !throws_rocks(gy.youmonst.data)))
                ++count;
        }
    }
    return (count == 8) ? TRUE : FALSE;
}

/*
 * Return 0 if nothing particular seems wrong, positive numbers for
 * serious trouble, and negative numbers for comparative annoyances.
 * This returns the worst problem. There may be others, and the gods
 * may fix more than one.
 *
 * This could get as bizarre as noting surrounding opponents, (or
 * hostile dogs), but that's really hard.
 *
 * We could force rehumanize of polyselfed people, but we can't tell
 * unintentional shape changes from the other kind. Oh well.
 * 3.4.2: make an exception if polymorphed into a form which lacks
 * hands; that's a case where the ramifications override this doubt.
 */
staticfn int
in_trouble(void)
{
    struct obj *otmp;
    int i;

    /*
     * major troubles
     */
    if (Stoned)
        return TROUBLE_STONED;
    if (Slimed)
        return TROUBLE_SLIMED;
    if (Strangled)
        return TROUBLE_STRANGLED;
    if (u.utrap && u.utraptype == TT_LAVA)
        return TROUBLE_LAVA;
    if (Sick)
        return TROUBLE_SICK;
    if (u.uhs >= WEAK)
        return TROUBLE_STARVING;
    if (region_danger())
        return TROUBLE_REGION;
    if ((!Upolyd || Unchanging) && critically_low_hp(FALSE))
        return TROUBLE_HIT;
    if (ismnum(u.ulycn))
        return TROUBLE_LYCANTHROPE;
    if (near_capacity() >= EXT_ENCUMBER && AMAX(A_STR) - ABASE(A_STR) > 3)
        return TROUBLE_COLLAPSING;
    if (stuck_in_wall())
        return TROUBLE_STUCK_IN_WALL;
    if (Cursed_obj(uarmf, LEVITATION_BOOTS)
        || stuck_ring(uleft, RIN_LEVITATION)
        || stuck_ring(uright, RIN_LEVITATION))
        return TROUBLE_CURSED_LEVITATION;
    if (nohands(gy.youmonst.data) || !freehand()) {
        /* for bag/box access [cf use_container()]...
           make sure it's a case that we know how to handle;
           otherwise "fix all troubles" would get stuck in a loop */
        if (welded(uwep))
            return TROUBLE_UNUSEABLE_HANDS;
        if (Upolyd && nohands(gy.youmonst.data)
            && (!Unchanging || ((otmp = unchanger()) != 0 && otmp->cursed)))
            return TROUBLE_UNUSEABLE_HANDS;
    }
    if (Blindfolded && ublindf->cursed)
        return TROUBLE_CURSED_BLINDFOLD;

    /*
     * minor troubles
     */
    if (Punished || (u.utrap && u.utraptype == TT_BURIEDBALL))
        return TROUBLE_PUNISHED;
    if (Cursed_obj(uarmg, GAUNTLETS_OF_FUMBLING)
        || Cursed_obj(uarmf, FUMBLE_BOOTS))
        return TROUBLE_FUMBLING;
    if (worst_cursed_item())
        return TROUBLE_CURSED_ITEMS;
    if (u.usteed) { /* can't voluntarily dismount from a cursed saddle */
        otmp = which_armor(u.usteed, W_SADDLE);
        if (Cursed_obj(otmp, SADDLE))
            return TROUBLE_SADDLE;
    }

    if (BlindedTimeout > 1L && !(HBlinded & ~TIMEOUT)
        && (!u.uswallow
            || !attacktype_fordmg(u.ustuck->data, AT_ENGL, AD_BLND)))
        return TROUBLE_BLIND;
    /* deafness isn't its own trouble; healing magic cures deafness
       when it cures blindness, so do the same with trouble repair */
    if ((HDeaf & TIMEOUT) > 1L)
        return TROUBLE_BLIND;

    for (i = 0; i < A_MAX; i++)
        if (ABASE(i) < AMAX(i))
            return TROUBLE_POISONED;
    if (Wounded_legs && !u.usteed)
        return TROUBLE_WOUNDED_LEGS;
    if (u.uhs >= HUNGRY)
        return TROUBLE_HUNGRY;
    if (HStun & TIMEOUT)
        return TROUBLE_STUNNED;
    if (HConfusion & TIMEOUT)
        return TROUBLE_CONFUSED;
    if (HHallucination & TIMEOUT)
        return TROUBLE_HALLUCINATION;
    return 0;
}

/* select an item for TROUBLE_CURSED_ITEMS */
staticfn struct obj *
worst_cursed_item(void)
{
    struct obj *otmp;

    /* if strained or worse, check for loadstone first */
    if (near_capacity() >= HVY_ENCUMBER) {
        for (otmp = gi.invent; otmp; otmp = otmp->nobj)
            if (Cursed_obj(otmp, LOADSTONE))
                return otmp;
    }
    /* weapon takes precedence if it is interfering
       with taking off a ring or putting on a shield */
    if (welded(uwep) && (uright || bimanual(uwep))) { /* weapon */
        otmp = uwep;
    /* gloves come next, due to rings */
    } else if (uarmg && uarmg->cursed) { /* gloves */
        otmp = uarmg;
    /* then shield due to two handed weapons and spells */
    } else if (uarms && uarms->cursed) { /* shield */
        otmp = uarms;
    /* then cloak due to body armor */
    } else if (uarmc && uarmc->cursed) { /* cloak */
        otmp = uarmc;
    } else if (uarm && uarm->cursed) { /* suit */
        otmp = uarm;
    /* if worn helmet of opposite alignment is making you an adherent
       of the current god, he/she/it won't uncurse that for you */
    } else if (uarmh && uarmh->cursed /* helmet */
               && uarmh->otyp != HELM_OF_OPPOSITE_ALIGNMENT) {
        otmp = uarmh;
    } else if (uarmf && uarmf->cursed) { /* boots */
        otmp = uarmf;
    } else if (uarmu && uarmu->cursed) { /* shirt */
        otmp = uarmu;
    } else if (uamul && uamul->cursed) { /* amulet */
        otmp = uamul;
    } else if (uleft && uleft->cursed) { /* left ring */
        otmp = uleft;
    } else if (uright && uright->cursed) { /* right ring */
        otmp = uright;
    } else if (ublindf && ublindf->cursed) { /* eyewear */
        otmp = ublindf; /* must be non-blinding lenses */
    /* if weapon wasn't handled above, do it now */
    } else if (welded(uwep)) { /* weapon */
        otmp = uwep;
    /* active secondary weapon even though it isn't welded */
    } else if (uswapwep && uswapwep->cursed && u.twoweap) {
        otmp = uswapwep;
    /* all worn items ought to be handled by now */
    } else {
        for (otmp = gi.invent; otmp; otmp = otmp->nobj) {
            if (!otmp->cursed)
                continue;
            if (otmp->otyp == LOADSTONE || confers_luck(otmp))
                break;
        }
    }
    return otmp;
}

staticfn void
fix_curse_trouble(struct obj *otmp, const char *what)
{
    if (!otmp) {
        impossible("fix_curse_trouble: nothing to uncurse.");
        return;
    }
    if (otmp == uarmg && Glib) {
        make_glib(0);
        Your(TR("%s are no longer slippery.", "%s不再滑溜了。"), gloves_simple_name(uarmg));
        if (!otmp->cursed)
            return;
    }
    if (!Blind || (otmp == ublindf && Blindfolded_only)) {
        pline("%s %s.",
                what ? what : (const char *) Yobjnam2(otmp, TR("softly glow", "柔和地发光")),
                hcolor(NH_AMBER));
        iflags.last_msg = PLNMSG_OBJ_GLOWS;
        otmp->bknown = !Hallucination; /* ok to skip set_bknown() */
    }
    uncurse(otmp);
    update_inventory();
}

staticfn void
fix_worst_trouble(int trouble)
{
    int i, maxhp;
    struct obj *otmp = 0;
    const char *what = (const char *) 0;
    static NEARDATA const char leftglow[] = "Your left ring softly glows",
                               rightglow[] = "Your right ring softly glows";

    switch (trouble) {
    case TROUBLE_STONED:
        make_stoned(0L, "You feel more limber.", 0, (char *) 0);
        break;
    case TROUBLE_SLIMED:
        make_slimed(0L, "The slime disappears.");
        break;
    case TROUBLE_STRANGLED:
        if (uamul && uamul->otyp == AMULET_OF_STRANGULATION) {
            Your(TR("amulet vanishes!", "护身符消失了！"));
            useup(uamul);
        }
        You(TR("can breathe again.", "又能呼吸了。"));
        Strangled = 0;
        disp.botl = TRUE;
        break;
    case TROUBLE_LAVA:
        /* teleport should always succeed, but if not, just untrap them */
        if (!safe_teleds(TELEDS_NO_FLAGS))
            reset_utrap(TRUE);
        rescued_from_terrain(DISSOLVED); /* DISSOLVED: pending cause of death
                                          * if trouble didn't get cured */
        break;
    case TROUBLE_STARVING:
        /* temporarily lost strength recovery now handled by init_uhunger() */
        FALLTHROUGH;
        /* FALLTHRU*/
    case TROUBLE_HUNGRY:
        Your(TR("%s feels content.", "%s感到很满足。"), body_part(STOMACH));
        init_uhunger();
        disp.botl = TRUE;
        break;
    case TROUBLE_SICK:
        You_feel(TR("better.", "好些了。"));
        make_sick(0L, (char *) 0, FALSE, SICK_ALL);
        break;
    case TROUBLE_REGION:
        /* stinking cloud, with hero vulnerable to HP loss */
        region_safety();
        break;
    case TROUBLE_HIT:
        /* "fix all troubles" will keep trying if hero has
           5 or less hit points, so make sure they're always
           boosted to be more than that */
        You_feel(TR("much better.", "好多了。"));
        if (Upolyd) {
            maxhp = u.mhmax + rnd(5);
            setuhpmax(max(maxhp, 5 + 1), FALSE); /* acts as setmhmax() */
            u.mh = u.mhmax;
        }
        maxhp = u.uhpmax;
        if (maxhp < u.ulevel * 5 + 11)
            maxhp += rnd(5);
        /* True: update u.uhpmax even if currently poly'd */
        setuhpmax(max(maxhp, 5 + 1), TRUE);
        u.uhp = u.uhpmax; /* setuhpmax() will do this when u.uhp is higher
                           * than u.uhpmax; prayer also does this if lower */
        disp.botl = TRUE;
        break;
    case TROUBLE_COLLAPSING:
        /* override Fixed_abil; uncurse that if feasible */
        You_feel(TR("%sstronger.", "%s更强壮了。"),
                 (AMAX(A_STR) - ABASE(A_STR) > 6) ? TR("much ", "许多") : "");
        ABASE(A_STR) = AMAX(A_STR);
        disp.botl = TRUE;
        if (Fixed_abil) {
            if ((otmp = stuck_ring(uleft, RIN_SUSTAIN_ABILITY)) != 0) {
                if (otmp == uleft)
                    what = leftglow;
            } else if ((otmp = stuck_ring(uright, RIN_SUSTAIN_ABILITY))
                       != 0) {
                if (otmp == uright)
                    what = rightglow;
            }
            if (otmp) {
                fix_curse_trouble(otmp, what);
                break;
            }
        }
        break;
    case TROUBLE_STUCK_IN_WALL:
        /* no control, but works on no-teleport levels */
        if (safe_teleds(TELEDS_NO_FLAGS)) {
            Your(TR("surroundings change.", "周围的环境变了。"));
        } else {
            /* safe_teleds() couldn't find a safe place; perhaps the
               level is completely full.  As a last resort, confer
               intrinsic wall/rock-phazing.  Hero might get stuck
               again fairly soon....
               Without something like this, fix_all_troubles can get
               stuck in an infinite loop trying to fix STUCK_IN_WALL
               and repeatedly failing. */
            set_itimeout(&HPasses_walls, (long) (d(4, 4) + 4)); /* 8..20 */
            /* how else could you move between packed rocks or among
               lattice forming "solid" rock? */
            You_feel(TR("much slimmer.", "苗条多了。"));
        }
        break;
    case TROUBLE_CURSED_LEVITATION:
        if (Cursed_obj(uarmf, LEVITATION_BOOTS)) {
            otmp = uarmf;
        } else if ((otmp = stuck_ring(uleft, RIN_LEVITATION)) != 0) {
            if (otmp == uleft)
                what = leftglow;
        } else if ((otmp = stuck_ring(uright, RIN_LEVITATION)) != 0) {
            if (otmp == uright)
                what = rightglow;
        }
        fix_curse_trouble(otmp, what);
        break;
    case TROUBLE_UNUSEABLE_HANDS:
        if (welded(uwep)) {
            otmp = uwep;
            fix_curse_trouble(otmp, what);
            break;
        }
        if (Upolyd && nohands(gy.youmonst.data)) {
            if (!Unchanging) {
                Your(TR("shape becomes uncertain.", "身形变得不稳定。"));
                rehumanize(); /* "You return to {normal} form." */
            } else if ((otmp = unchanger()) != 0 && otmp->cursed) {
                /* otmp is an amulet of unchanging */
                fix_curse_trouble(otmp, what);
                break;
            }
        }
        if (nohands(gy.youmonst.data) || !freehand())
            impossible("fix_worst_trouble: couldn't cure hands.");
        break;
    case TROUBLE_CURSED_BLINDFOLD:
        otmp = ublindf;
        fix_curse_trouble(otmp, what);
        break;
    case TROUBLE_LYCANTHROPE:
        you_unwere(TRUE);
        break;
    /*
     */
    case TROUBLE_PUNISHED:
        Your(TR("chain disappears.", "锁链消失了。"));
        if (u.utrap && u.utraptype == TT_BURIEDBALL)
            buried_ball_to_freedom();
        else
            unpunish();
        break;
    case TROUBLE_FUMBLING:
        if (Cursed_obj(uarmg, GAUNTLETS_OF_FUMBLING))
            otmp = uarmg;
        else if (Cursed_obj(uarmf, FUMBLE_BOOTS))
            otmp = uarmf;
        fix_curse_trouble(otmp, what);
        break;
    case TROUBLE_CURSED_ITEMS:
        otmp = worst_cursed_item();
        if (otmp == uright)
            what = rightglow;
        else if (otmp == uleft)
            what = leftglow;
        fix_curse_trouble(otmp, what);
        break;
    case TROUBLE_POISONED:
        /* override Fixed_abil; ignore items which confer that */
        if (Hallucination)
            pline(TR("There's a tiger in your tank.", "你的油箱里有只老虎。"));
        else
            You_feel(TR("in good health again.", "又恢复健康了。"));
        for (i = 0; i < A_MAX; i++) {
            if (ABASE(i) < AMAX(i)) {
                ABASE(i) = AMAX(i);
                disp.botl = TRUE;
            }
        }
        encumber_msg();
        break;
    case TROUBLE_BLIND: { /* handles deafness as well as blindness */
        char msgbuf[BUFSZ];
        const char *eyes = body_part(EYE);
        boolean cure_deaf = (HDeaf & TIMEOUT) ? TRUE : FALSE;

        msgbuf[0] = '\0';
        if (Blinded) {
            if (eyecount(gy.youmonst.data) != 1)
                eyes = makeplural(eyes);
            Sprintf(msgbuf, TR("Your %s %s better", "你的%s%s好多了"), eyes, vtense(eyes, TR("feel", "摸起来")));
            u.ucreamed = 0;
            make_blinded(0L, FALSE);
        }
        if (cure_deaf) {
            make_deaf(0L, FALSE);
            if (!Deaf)
                Sprintf(eos(msgbuf), TR("%s can hear again", "%s又能听见了"),
                        !*msgbuf ? TR("You", "你") : TR(" and you", "，你"));
        }
        if (*msgbuf)
            pline("%s.", msgbuf);
        break;
    }
    case TROUBLE_WOUNDED_LEGS:
        heal_legs(0);
        break;
    case TROUBLE_STUNNED:
        make_stunned(0L, TRUE);
        break;
    case TROUBLE_CONFUSED:
        make_confused(0L, TRUE);
        break;
    case TROUBLE_HALLUCINATION:
        pline(TR("Looks like you are back in Kansas.", "看来你回到了堪萨斯。"));
        (void) make_hallucinated(0L, FALSE, 0L);
        break;
    case TROUBLE_SADDLE:
        otmp = which_armor(u.usteed, W_SADDLE);
        if (!Blind) {
            pline("%s %s.", Yobjnam2(otmp, TR("softly glow", "柔和地发光")), hcolor(NH_AMBER));
            set_bknown(otmp, 1);
        }
        uncurse(otmp);
        break;
    }
}

/* "I am sometimes shocked by... the nuns who never take a bath without
 * wearing a bathrobe all the time.  When asked why, since no man can see them,
 * they reply 'Oh, but you forget the good God'.  Apparently they conceive of
 * the Deity as a Peeping Tom, whose omnipotence enables Him to see through
 * bathroom walls, but who is foiled by bathrobes." --Bertrand Russell, 1943
 * Divine wrath, dungeon walls, and armor follow the same principle.
 */
staticfn void
god_zaps_you(aligntyp resp_god)
{
    if (u.uswallow) {
        pline(
          TR("Suddenly a bolt of lightning comes down at you from the heavens!", "突然，一道闪电从天而降，劈向你！"));
        pline(TR("It strikes %s!", "它击中了%s！"), mon_nam(u.ustuck));
        if (!resists_elec(u.ustuck)) {
            pline(TR("%s fries to a crisp!", "%s被烤焦了！"), Monnam(u.ustuck));
            /* Yup, you get experience.  It takes guts to successfully
             * pull off this trick on your god, anyway.
             * Other credit/blame applies (luck or alignment adjustments),
             * but not direct kill count (pacifist conduct).
             */
            xkilled(u.ustuck, XKILL_NOMSG | XKILL_NOCONDUCT);
        } else
            pline(TR("%s seems unaffected.", "%s似乎没有受到影响。"), Monnam(u.ustuck));
    } else {
        pline(TR("Suddenly, a bolt of lightning strikes you!", "突然，一道闪电击中了你！"));
        if (Reflecting) {
            shieldeff(u.ux, u.uy);
            if (Blind)
                pline(TR("For some reason you're unaffected.", "不知为何，你没有受到影响。"));
            else
                (void) ureflects("%s reflects from your %s.", "It");
            monstseesu(M_SEEN_REFL);
        } else if (Shock_resistance) {
            shieldeff(u.ux, u.uy);
            pline(TR("It seems not to affect you.", "它似乎对你没有影响。"));
            monstseesu(M_SEEN_ELEC);
            monstunseesu(M_SEEN_REFL);
        } else {
            fry_by_god(resp_god, FALSE);
            monstunseesu(M_SEEN_REFL | M_SEEN_ELEC);
        }
    }

    pline(TR("%s is not deterred...", "%s没有被吓退……"), align_gname(resp_god));
    if (u.uswallow) {
        pline(TR("A wide-angle disintegration beam aimed at you hits %s!", "一道瞄准你的广角解离光束击中了%s！"),
              mon_nam(u.ustuck));
        if (!resists_disint(u.ustuck)) {
            pline(TR("%s disintegrates into a pile of dust!", "%s分解成了一堆尘土！"), Monnam(u.ustuck));
            xkilled(u.ustuck, XKILL_NOMSG | XKILL_NOCORPSE | XKILL_NOCONDUCT);
        } else
            pline(TR("%s seems unaffected.", "%s似乎没有受到影响。"), Monnam(u.ustuck));
    } else {
        pline(TR("A wide-angle disintegration beam hits you!", "一道广角解离光束击中了你！"));

        /* disintegrate shield and body armor before disintegrating
         * the impudent mortal, like black dragon breath -3.
         */
        if (uarms && !(EReflecting & W_ARMS)
            && !(EDisint_resistance & W_ARMS))
            (void) disintegrate_arm(uarms);
        if (uarmc && !(EReflecting & W_ARMC)
            && !(EDisint_resistance & W_ARMC))
            (void) disintegrate_arm(uarmc);
        if (uarm && !(EReflecting & W_ARM) && !(EDisint_resistance & W_ARM)
            && !uarmc)
            (void) disintegrate_arm(uarm);
        if (uarmu && !uarm && !uarmc)
            (void) disintegrate_arm(uarmu);
        if (!Disint_resistance) {
            fry_by_god(resp_god, TRUE);
            monstunseesu(M_SEEN_DISINT);
        } else {
            You(TR("bask in its %s glow for a minute...", "在它的%s光芒中沐浴片刻……"), NH_BLACK);
            godvoice(resp_god, "I believe it not!");
            monstseesu(M_SEEN_DISINT);
        }
        if (Is_astralevel(&u.uz) || Is_sanctum(&u.uz)) {
            /* one more try for high altars */
            SetVoice((struct monst *) 0, 0, 80, voice_deity);
            verbalize(TR("Thou cannot escape my wrath, mortal!", "凡人！你逃不出我的怒火！"));
            summon_minion(resp_god, FALSE);
            summon_minion(resp_god, FALSE);
            summon_minion(resp_god, FALSE);
            SetVoice((struct monst *) 0, 0, 80, voice_deity);
            verbalize(TR("Destroy %s, my servants!", "消灭%s，我的仆从们！"), uhim());
        }
    }
}

staticfn void
fry_by_god(aligntyp resp_god, boolean via_disintegration)
{
    You("%s!", !via_disintegration ? TR("fry to a crisp", "被烤焦")
                                   : TR("disintegrate into a pile of dust", "化为一堆尘土"));
    svk.killer.format = KILLED_BY;
    Sprintf(svk.killer.name, TR("the wrath of %s", "%s的怒火"), align_gname(resp_god));
    done(DIED);
}

staticfn void
angrygods(aligntyp resp_god)
{
    int maxanger, new_ublesscnt;

    if (Inhell)
        resp_god = A_NONE;
    u.ublessed = 0; /* lose divine protection */

    /* changed from tmp = u.ugangr + abs (u.uluck) -- rph */
    /* added test for alignment diff -dlc */
    if (resp_god != u.ualign.type)
        maxanger = u.ualign.record / 2 + (Luck > 0 ? -Luck / 3 : -Luck);
    else
        maxanger = 3 * u.ugangr + ((Luck > 0 || u.ualign.record >= STRIDENT)
                                   ? -Luck / 3
                                   : -Luck);
    if (maxanger < 1)
        maxanger = 1; /* possible if bad align & good luck */
    else if (maxanger > 15)
        maxanger = 15; /* be reasonable */

    switch (rn2(maxanger)) {
    case 0:
    case 1:
#ifdef ZHLANG
        You_feel("你感到%s%s。", align_gname(resp_god),
                 Hallucination ? "不高兴" : "不悦");
#else
        You_feel("that %s is %s.", align_gname(resp_god),
                 Hallucination ? "bummed" : "displeased");
#endif
        break;
    case 2:
    case 3:
        godvoice(resp_god, (char *) 0);
#ifdef ZHLANG
        pline("\"汝%s，%s。\"",
              (ugod_is_angry() && resp_god == u.ualign.type)
                  ? "已偏离正道"
                  : "傲慢无礼",
              gy.youmonst.data->mlet == S_HUMAN ? "凡人" : "生灵");
        SetVoice((struct monst *) 0, 0, 80, voice_deity);
        verbalize("汝必须重新学习教训！");
#else
        pline("\"Thou %s, %s.\"",
              (ugod_is_angry() && resp_god == u.ualign.type)
                  ? "hast strayed from the path"
                  : "art arrogant",
              gy.youmonst.data->mlet == S_HUMAN ? "mortal" : "creature");
        SetVoice((struct monst *) 0, 0, 80, voice_deity);
        verbalize("Thou must relearn thy lessons!");
#endif
        (void) adjattrib(A_WIS, -1, FALSE);
        losexp((char *) 0);
        break;
    case 6:
        if (!Punished) {
            gods_angry(resp_god);
            punish((struct obj *) 0);
            break;
        }
        FALLTHROUGH;
        /* FALLTHRU */
    case 4:
    case 5:
        gods_angry(resp_god);
        if (!Blind && !Antimagic)
            pline(TR("%s glow surrounds you.", "%s光芒笼罩着你。"), An(hcolor(NH_BLACK)));
        if (rn2(2) || !attrcurse())
            rndcurse();
        break;
    case 7:
    case 8:
        godvoice(resp_god, (char *) 0);
        SetVoice((struct monst *) 0, 0, 80, voice_deity);
        verbalize(TR("Thou durst %s me?", "你竟敢%s我？"),
                  (on_altar() && (a_align(u.ux, u.uy) != resp_god))
                      ? TR("scorn", "蔑视")
                      : TR("call upon", "呼唤"));
        /* [why isn't this using verbalize()?] */
        pline(TR("\"Then die, %s!\"", "那就去死吧，%s！"),
              (gy.youmonst.data->mlet == S_HUMAN) ? TR("mortal", "凡人") : TR("creature", "生物"));
        summon_minion(resp_god, FALSE);
        break;

    default:
        gods_angry(resp_god);
        god_zaps_you(resp_god);
        break;
    }
    /* even though this might not be in response to prayer, set pray timer */
    new_ublesscnt = rnz(300);
    if (new_ublesscnt > u.ublesscnt)
        u.ublesscnt = new_ublesscnt;
    return;
}

/* helper to print "str appears at your feet", or appropriate */
staticfn void
at_your_feet(const char *str)
{
    if (Blind)
        str = Something;
    if (u.uswallow) {
        /* barrier between you and the floor */
        pline(TR("%s %s into %s %s.", "%s%s进%s%s中。"), str, vtense(str, TR("drop", "掉落")),
              s_suffix(mon_nam(u.ustuck)), mbodypart(u.ustuck, STOMACH));
    } else {
        pline(TR("%s %s %s your %s!", "%s%s%s你的%s！"), str,
              vtense(str, Blind ? TR("land", "陆地") : TR("appear", "出现")),
              Levitation ? TR("beneath", "下方") : "at",
              makeplural(body_part(FOOT)));
    }
}

staticfn void
gcrownu(void)
{
    struct obj *obj;
    const char *what;
    boolean already_exists, in_hand;
    short class_gift;
#define ok_wep(o) ((o) && ((o)->oclass == WEAPON_CLASS || is_weptool(o)))

    HSee_invisible |= FROMOUTSIDE;
    HFire_resistance |= FROMOUTSIDE;
    HCold_resistance |= FROMOUTSIDE;
    HShock_resistance |= FROMOUTSIDE;
    HSleep_resistance |= FROMOUTSIDE;
    HPoison_resistance |= FROMOUTSIDE;
    godvoice(u.ualign.type, (char *) 0);

    class_gift = STRANGE_OBJECT;
    /* 3.3.[01] had this in the A_NEUTRAL case,
       preventing chaotic wizards from receiving a spellbook */
    if (Role_if(PM_WIZARD)
        && !u_wield_art(ART_VORPAL_BLADE)
        && !u_wield_art(ART_STORMBRINGER)
        && !carrying(SPE_FINGER_OF_DEATH)) {
        class_gift = SPE_FINGER_OF_DEATH;
    } else if (Role_if(PM_MONK) && (!uwep || !uwep->oartifact)
               && !carrying(SPE_RESTORE_ABILITY)) {
        /* monks rarely wield a weapon */
        class_gift = SPE_RESTORE_ABILITY;
    }

    obj = ok_wep(uwep) ? uwep : 0;
    already_exists = in_hand = FALSE; /* lint suppression */
    switch (u.ualign.type) {
    case A_LAWFUL:
        u.uevent.uhand_of_elbereth = 1;
        SetVoice((struct monst *) 0, 0, 80, voice_deity);
        verbalize(TR("I crown thee...  The Hand of Elbereth!", "我加冕你为……埃尔贝瑞斯之手！"));
        livelog_printf(LL_DIVINEGIFT,
                       TR("was crowned \"The Hand of Elbereth\" by %s",
                          "被%s加冕为“埃尔贝瑞斯之手”"),
                       u_gname());
        break;
    case A_NEUTRAL:
        u.uevent.uhand_of_elbereth = 2;
        in_hand = u_wield_art(ART_VORPAL_BLADE);
        already_exists = exist_artifact(LONG_SWORD,
                                        artiname(ART_VORPAL_BLADE));
        SetVoice((struct monst *) 0, 0, 80, voice_deity);
        verbalize(TR("Thou shalt be my Envoy of Balance!", "你将成为我的平衡使者！"));
#ifdef ZHLANG
        livelog_printf(LL_DIVINEGIFT, "成为了%s的平衡使者", u_gname());
#else
        livelog_printf(LL_DIVINEGIFT, "became %s Envoy of Balance",
                       s_suffix(u_gname()));
#endif
        break;
    case A_CHAOTIC:
        u.uevent.uhand_of_elbereth = 3;
        in_hand = u_wield_art(ART_STORMBRINGER);
        already_exists = exist_artifact(RUNESWORD,
                                        artiname(ART_STORMBRINGER));
        what = (((already_exists && !in_hand) || class_gift != STRANGE_OBJECT)
                ? TR("take lives", "夺走生命")
                : TR("steal souls", "窃取灵魂"));
        SetVoice((struct monst *) 0, 0, 80, voice_deity);
        verbalize(TR("Thou art chosen to %s for My Glory!", "你被选中，为我的荣耀而%s！"), what);
        livelog_printf(LL_DIVINEGIFT,
                       TR("was chosen to %s for the Glory of %s",
                          "被选中去%s，为了%s的荣耀"),
                       what, u_gname());
        break;
    }

    if (objects[class_gift].oc_class == SPBOOK_CLASS) {
        char bbuf[BUFSZ];

        obj = mksobj(class_gift, TRUE, FALSE);
        /* get book type before dropping (don't think that could destroy
           the book because we need to be on an altar in order to become
           crowned, but be paranoid about it) */
        Strcpy(bbuf, actualoname(obj)); /* for livelog; TR("spellbook of <foo>", "法术书（某某）")
                                         * even if hero doesn't know book */
        bless(obj);
        obj->bknown = 1; /* ok to skip set_bknown() */
        observe_object(obj);
        at_your_feet(upstart(ansimpleoname(obj)));
        dropy(obj);
        u.ugifts++;
        /* not an artifact, but treat like one for this situation;
           classify as a spoiler in case player hasn't IDed the book yet */
        livelog_printf(LL_DIVINEGIFT | LL_ARTIFACT | LL_SPOILER,
                       TR("was bestowed with %s", "被赐予了%s"), bbuf);

        /* when getting a new book for known spell, enhance
           currently wielded weapon rather than the book */
        if (known_spell(class_gift) != spe_Unknown && ok_wep(uwep))
            obj = uwep; /* to be blessed,&c */
    }

    switch (u.ualign.type) {
    case A_LAWFUL:
        if (class_gift != STRANGE_OBJECT) {
            ; /* already got bonus above */
        } else if (obj && obj->otyp == LONG_SWORD && !obj->oartifact) {
            char lbuf[BUFSZ];

            Strcpy(lbuf, simpleonames(obj)); /* before transformation */
            if (!Blind)
                Your(TR("sword shines brightly for a moment.", "剑闪耀了片刻。"));
            obj = oname(obj, artiname(ART_EXCALIBUR),
                        ONAME_GIFT | ONAME_KNOW_ARTI);
            if (is_art(obj, ART_EXCALIBUR)) {
                u.ugifts++;
                livelog_printf(LL_DIVINEGIFT | LL_ARTIFACT,
                               TR("had %s wielded %s transformed into %s",
                                  "%s所持的%s化为了%s"),
                               uhis(), lbuf, artiname(ART_EXCALIBUR));
            }
        }
        /* acquire Excalibur's skill regardless of weapon or gift */
        unrestrict_weapon_skill(P_LONG_SWORD);
        if (is_art(obj, ART_EXCALIBUR))
            discover_artifact(ART_EXCALIBUR);
        break;
    case A_NEUTRAL:
        if (class_gift != STRANGE_OBJECT) {
            ; /* already got bonus above */
        } else if (obj && in_hand) {
            Your(TR("%s goes snicker-snack!", "%s发出咔嚓咔嚓的响声！"), xname(obj));
            observe_object(obj);
        } else if (!already_exists) {
            obj = mksobj(LONG_SWORD, FALSE, FALSE);
            obj = oname(obj, artiname(ART_VORPAL_BLADE),
                        ONAME_GIFT | ONAME_KNOW_ARTI);
            obj->spe = 1;
            at_your_feet("A sword");
            dropy(obj);
            u.ugifts++;
            livelog_printf(LL_DIVINEGIFT | LL_ARTIFACT,
                           TR("was bestowed with %s", "被赐予了%s"),
                           artiname(ART_VORPAL_BLADE));
        }
        /* acquire Vorpal Blade's skill regardless of weapon or gift */
        unrestrict_weapon_skill(P_LONG_SWORD);
        if (is_art(obj, ART_VORPAL_BLADE))
            discover_artifact(ART_VORPAL_BLADE);
        break;
    case A_CHAOTIC: {
        char swordbuf[BUFSZ];

        Sprintf(swordbuf, TR("%s sword", "%s之剑"), hcolor(NH_BLACK));
        if (class_gift != STRANGE_OBJECT) {
            ; /* already got bonus above */
        } else if (obj && in_hand) {
            Your(TR("%s hums ominously!", "%s不祥地嗡鸣着！"), swordbuf);
            observe_object(obj);
        } else if (!already_exists) {
            obj = mksobj(RUNESWORD, FALSE, FALSE);
            obj = oname(obj, artiname(ART_STORMBRINGER),
                        ONAME_GIFT | ONAME_KNOW_ARTI);
            obj->spe = 1;
            at_your_feet(An(swordbuf));
            dropy(obj);
            u.ugifts++;
            livelog_printf(LL_DIVINEGIFT | LL_ARTIFACT,
                           TR("was bestowed with %s", "被赐予了%s"),
                           artiname(ART_STORMBRINGER));
        }
        /* acquire Stormbringer's skill regardless of weapon or gift */
        unrestrict_weapon_skill(P_BROAD_SWORD);
        if (is_art(obj, ART_STORMBRINGER))
            discover_artifact(ART_STORMBRINGER);
        break;
    }
    default:
        obj = 0; /* lint */
        break;
    }

    /* enhance weapon regardless of alignment or artifact status */
    if (ok_wep(obj)) {
        bless(obj);
        obj->oeroded = obj->oeroded2 = 0;
        obj->oerodeproof = TRUE;
        obj->bknown = obj->rknown = 1; /* ok to skip set_bknown() */
        if (obj->spe < 1)
            obj->spe = 1;
        /* acquire skill in this weapon */
        unrestrict_weapon_skill(weapon_type(obj));
    } else if (class_gift == STRANGE_OBJECT) {
        /* opportunity knocked, but there was nobody home... */
        You_feel(TR("unworthy.", "感到自己不配。"));
    }
    update_inventory();

    /* lastly, confer an extra skill slot/credit beyond the
       up-to-29 you can get from gaining experience levels */
    add_weapon_skill(1);
    return;
}

staticfn void
give_spell(void)
{
    struct obj *otmp;
    char spe_let;
    int spe_knowledge, trycnt = u.ulevel + 1;

    /* not yet known spells and forgotten spells are given preference over
       usable ones; also, try to grant spell that hero could gain skill in
       (even though being restricted doesn't prevent learning and casting) */
    otmp = mkobj(SPBOOK_no_NOVEL, TRUE);
    while (--trycnt > 0) {
        if (otmp->otyp != SPE_BLANK_PAPER) {
            if (known_spell(otmp->otyp) <= spe_Unknown
                && !P_RESTRICTED(spell_skilltype(otmp->otyp)))
                break; /* forgotten or not yet known */
        } else {
            /* blank paper is acceptable if not discovered yet or
               if hero has a magic marker to write something on it
               (doesn't matter if marker is out of charges); it will
               become discovered (below) without needing to be read */
            if (!objects[SPE_BLANK_PAPER].oc_name_known
                || carrying(MAGIC_MARKER))
                break;
        }
        otmp->otyp = rnd_class(svb.bases[SPBOOK_CLASS], SPE_BLANK_PAPER);
    }
    /*
     * 25% chance of learning the spell directly instead of
     * receiving the book for it, unless it's already well known.
     * The chance is not influenced by whether hero is illiterate.
     */
    if (otmp->otyp != SPE_BLANK_PAPER && !rn2(4)
        && (spe_knowledge = known_spell(otmp->otyp)) != spe_Fresh) {
        /* force_learn_spell() should only return '\0' if the book
           is blank paper or the spell is known and has retention
           of spe_Fresh, so no 'else' case is needed here */
        if ((spe_let = force_learn_spell(otmp->otyp)) != '\0') {
            /* for spellbook class, OBJ_NAME() yields the name of
               the spell rather than "spellbook of <spell-name>" */
            const char *spe_name = OBJ_NAME(objects[otmp->otyp]);

            if (spe_knowledge == spe_Unknown) /* prior to learning */
                /* appending "spell 'a'" seems slightly silly but
                   is similar to "added to your repertoire, as 'a'"
                   and without any spellbook on hand a novice player
                   might not recognize that 'spe_name' is a spell */
                pline(TR("Divine knowledge of %s fills your mind!  Spell '%c'.", "关于%s的神圣知识涌入你的脑海！法术「%c」。"),
                      spe_name, spe_let);
            else
                Your(TR("knowledge of spell '%c' - %s is %s.", "关于法术「%c」的知识——%s已%s。"),
                     spe_let, spe_name,
                     (spe_knowledge == spe_Forgotten) ? TR("restored", "恢复")
                                                      : TR("refreshed", "刷新"));
        }
        obfree(otmp, (struct obj *) 0); /* discard the book */
    } else {
        observe_object(otmp);
        /* don't set bknown */
        /* discovering blank paper will make it less likely to
           be given again; small chance to arbitrarily discover
           some other book type without having to read it first */
        if (otmp->otyp == SPE_BLANK_PAPER || !rn2(100))
            makeknown(otmp->otyp);
        bless(otmp);
        at_your_feet(upstart(ansimpleoname(otmp)));
        place_object(otmp, u.ux, u.uy);
        newsym(u.ux, u.uy);
    }
    return;
}

staticfn void
pleased(aligntyp g_align)
{
    /* don't use p_trouble, worst trouble may get fixed while praying */
    int trouble = in_trouble(); /* what's your worst difficulty? */
    int pat_on_head = 0, kick_on_butt;

#ifdef ZHLANG
    You_feel("你感到%s%s。", align_gname(g_align),
             (u.ualign.record >= DEVOUT)
                 ? Hallucination ? "非常满意" : "十分满意"
                 : (u.ualign.record >= STRIDENT)
                       ? Hallucination ? "笑嘻嘻的" : "很满意"
                       : Hallucination ? "吃饱了" : "还算满意");
#else
    You_feel("that %s is %s.", align_gname(g_align),
             (u.ualign.record >= DEVOUT)
                 ? Hallucination ? "pleased as punch" : "well-pleased"
                 : (u.ualign.record >= STRIDENT)
                       ? Hallucination ? "ticklish" : "pleased"
                       : Hallucination ? "full" : "satisfied");
#endif

    /* not your deity */
    if (on_altar() && gp.p_aligntyp != u.ualign.type) {
        adjalign(-1);
        return;
    } else if (u.ualign.record < 2 && trouble <= 0)
        adjalign(1);

    /*
     * Depending on your luck & align level, the god you prayed to will:
     *  - fix your worst problem if it's major;
     *  - fix all your major problems;
     *  - fix your worst problem if it's minor;
     *  - fix all of your problems;
     *  - do you a gratuitous favor.
     *
     * If you make it to the last category, you roll randomly again
     * to see what they do for you.
     *
     * If your luck is at least 0, then you are guaranteed rescued from
     * your worst major problem.
     */
    if (!trouble && u.ualign.record >= DEVOUT) {
        /* if hero was in trouble, but got better, no special favor */
        if (gp.p_trouble == 0)
            pat_on_head = 1;
    } else {
        int action, prayer_luck;
        int tryct = 0;

        /* Negative luck is normally impossible here (can_pray() forces
           prayer failure in that situation), but it's possible for
           Luck to drop during the period of prayer occupation and
           become negative by the time we get here.  [Reported case
           was lawful character whose stinking cloud caused a delayed
           killing of a peaceful human, triggering the "murderer"
           penalty while successful prayer was in progress.  It could
           also happen due to inconvenient timing on Friday 13th, but
           the magnitude there (-1) isn't big enough to cause trouble.]
           We don't bother remembering start-of-prayer luck, just make
           sure it's at least -1 so that Luck+2 is big enough to avoid
           a divide by zero crash when generating a random number.  */
        prayer_luck = max(Luck, -1); /* => (prayer_luck + 2 > 0) */
        action = rn1(prayer_luck + (on_altar() ? 3 + on_shrine() : 2), 1);
        if (!on_altar())
            action = min(action, 3);
        if (u.ualign.record < STRIDENT)
            action = (u.ualign.record > 0 || !rnl(2)) ? 1 : 0;

        switch (min(action, 5)) {
        case 5:
            pat_on_head = 1;
            FALLTHROUGH;
            /*FALLTHRU*/
        case 4:
            do
                fix_worst_trouble(trouble);
            while ((trouble = in_trouble()) != 0);
            break;

        case 3:
            /* up to 10 troubles */
            fix_worst_trouble(trouble);
            FALLTHROUGH;
            /*FALLTHRU*/
        case 2:
            /* up to 9 troubles */
            while ((trouble = in_trouble()) > 0 && (++tryct < 10))
                fix_worst_trouble(trouble);
            break;

        case 1:
            if (trouble > 0)
                fix_worst_trouble(trouble);
            break;
        case 0:
            break; /* your god blows you off, too bad */
        }
    }

    /* note: can't get pat_on_head unless all troubles have just been
       fixed or there were no troubles to begin with; hallucination
       won't be in effect so special handling for it is superfluous */
    if (pat_on_head)
        switch (rn2((Luck + 6) >> 1)) {
        case 0:
            break;
        case 1:
            if (uwep && (welded(uwep) || uwep->oclass == WEAPON_CLASS
                         || is_weptool(uwep))) {
                char repair_buf[BUFSZ];

                *repair_buf = '\0';
                if (uwep->oeroded || uwep->oeroded2)
                    Sprintf(repair_buf, TR(" and %s now as good as new", "，而%s如今完好如新"),
                            otense(uwep, TR("are", "已经")));

                if (uwep->cursed) {
                    if (!Blind) {
                        pline("%s %s%s.", Yobjnam2(uwep, TR("softly glow", "柔和地发光")),
                              hcolor(NH_AMBER), repair_buf);
                        iflags.last_msg = PLNMSG_OBJ_GLOWS;
                    } else
                        You_feel(TR("the power of %s over %s.", "%s的力量笼罩着%s。"), u_gname(),
                                 yname(uwep));
                    uncurse(uwep);
                    uwep->bknown = 1; /* ok to bypass set_bknown() */
                    *repair_buf = '\0';
                } else if (!uwep->blessed) {
                    if (!Blind) {
                        pline(TR("%s with %s aura%s.", "%s，带着%s的光环%s。"),
                              Yobjnam2(uwep, TR("softly glow", "柔和地发光")),
                              an(hcolor(NH_LIGHT_BLUE)), repair_buf);
                        iflags.last_msg = PLNMSG_OBJ_GLOWS;
                    } else
                        You_feel(TR("the blessing of %s over %s.", "%s的祝福笼罩着%s。"), u_gname(),
                                 yname(uwep));
                    bless(uwep);
                    uwep->bknown = 1; /* ok to bypass set_bknown() */
                    *repair_buf = '\0';
                }

                /* fix any rust/burn/rot damage, but don't protect
                   against future damage */
                if (uwep->oeroded || uwep->oeroded2) {
                    uwep->oeroded = uwep->oeroded2 = 0;
                    /* only give this message if we didn't just bless
                       or uncurse (which has already given a message) */
                    if (*repair_buf)
                        pline(TR("%s as good as new!", "%s完好如新！"),
                              Yobjnam2(uwep, Blind ? TR("feel", "摸起来") : TR("look", "看上去")));
                }
                update_inventory();
            }
            break;
        case 3:
            /* takes 2 hints to get the music to enter the stronghold;
               skip if you've solved it via mastermind or destroyed the
               drawbridge (both set uopened_dbridge) or if you've already
               travelled past the Valley of the Dead (gehennom_entered) */
            if (!u.uevent.uopened_dbridge && !u.uevent.gehennom_entered) {
                if (u.uevent.uheard_tune < 1) {
                    godvoice(g_align, (char *) 0);
                    SetVoice((struct monst *) 0, 0, 80, voice_deity);
                    verbalize(TR("Hark, %s!", "听着，%s！"), is_human(gy.youmonst.data)
                                               ? TR("mortal", "凡人")
                                               : TR("creature", "生物"));
                    SetVoice((struct monst *) 0, 0, 80, voice_deity);
                    verbalize(
                       TR("To enter the castle, thou must play the right tune!", "要进入城堡，你必须奏出正确的曲调！"));
                    u.uevent.uheard_tune++;
                    break;
                } else if (u.uevent.uheard_tune < 2) {
                    Soundeffect(se_divine_music, 50);
                    You_hear(TR("a divine music...", "神圣的音乐……"));
                    pline(TR("It sounds like:  \"%s\".", "听起来像是：「%s」。"), svt.tune);
                    u.uevent.uheard_tune++;
                    record_achievement(ACH_TUNE);
                    break;
                }
            }
            FALLTHROUGH;
            /*FALLTHRU*/
        case 2:
            if (!Blind)
                You(TR("are surrounded by %s glow.", "被%s光芒笼罩着。"), an(hcolor(NH_GOLDEN)));
            /* if any levels have been lost (and not yet regained),
               treat this effect like blessed full healing */
            if (u.ulevel < u.ulevelmax) {
                u.ulevelmax -= 1; /* see potion.c */
                pluslvl(FALSE);
            } else {
                u.uhpmax += 5;
                if (u.uhpmax > u.uhppeak)
                    u.uhppeak = u.uhpmax;
                if (Upolyd)
                    u.mhmax += 5;
            }
            u.uhp = u.uhpmax;
            if (Upolyd)
                u.mh = u.mhmax;
            if (ABASE(A_STR) < AMAX(A_STR)) {
                ABASE(A_STR) = AMAX(A_STR);
                disp.botl = TRUE; /* before potential message */
                encumber_msg();
            }
            if (u.uhunger < 900)
                init_uhunger();
            /* luck couldn't have been negative at start of prayer because
               the prayer would have failed, but might have been decremented
               due to a timed event (delayed death of peaceful monster hit
               by hero-created stinking cloud) during the praying interval */
            if (u.uluck < 0)
                u.uluck = 0;
            /* superfluous; if hero was blinded we'd be handling trouble
               rather than issuing a pat-on-head */
            u.ucreamed = 0;
            make_blinded(0L, TRUE);
            disp.botl = TRUE;
            break;
        case 4: {
            struct obj *otmp, *nextobj;
            int any = 0;

            if (Blind)
                You_feel(TR("the power of %s.", "%s的力量。"), u_gname());
            else
                You(TR("are surrounded by %s aura.", "被%s光环笼罩着。"), an(hcolor(NH_LIGHT_BLUE)));
            for (otmp = gi.invent; otmp; otmp = nextobj) {
                nextobj = otmp->nobj;
                if (otmp->cursed
                    && (otmp != uarmh /* [see worst_cursed_item()] */
                        || uarmh->otyp != HELM_OF_OPPOSITE_ALIGNMENT)) {
                    if (!Blind) {
                        pline("%s %s.", Yobjnam2(otmp, TR("softly glow", "柔和地发光")),
                              hcolor(NH_AMBER));
                        iflags.last_msg = PLNMSG_OBJ_GLOWS;
                        otmp->bknown = 1; /* ok to bypass set_bknown() */
                        ++any;
                    }
                    uncurse(otmp);
                }
            }
            if (any)
                update_inventory();
            break;
        }
        case 5: {
            static NEARDATA const char msg[] =
                "\"and thus I grant thee the gift of %s!\"";

            godvoice(u.ualign.type,
                     "Thou hast pleased me with thy progress,");
            if (!(HTelepat & INTRINSIC)) {
                HTelepat |= FROMOUTSIDE;
                pline(msg, TR("Telepathy", "心灵感应"));
                if (Blind)
                    see_monsters();
            } else if (!(HFast & INTRINSIC)) {
                HFast |= FROMOUTSIDE;
                pline(msg, TR("Speed", "速度"));
            } else if (!(HStealth & INTRINSIC)) {
                HStealth |= FROMOUTSIDE;
                pline(msg, TR("Stealth", "潜行"));
            } else {
                if (!(HProtection & INTRINSIC)) {
                    HProtection |= FROMOUTSIDE;
                    if (!u.ublessed)
                        u.ublessed = rn1(3, 2);
                } else
                    u.ublessed++;
                pline(msg, TR("my protection", "我的庇护"));
            }
            SetVoice((struct monst *) 0, 0, 80, voice_deity);
            verbalize(TR("Use it wisely in my name!", "以我的名义，善加使用！"));
            break;
        }
        case 7:
        case 8:
            if (u.ualign.record >= PIOUS && !u.uevent.uhand_of_elbereth) {
                gcrownu();
                break;
            }
            FALLTHROUGH;
            /*FALLTHRU*/
        case 6:
            give_spell();
            break;
        default:
            impossible("Confused deity!");
            break;
        }

    u.ublesscnt = rnz(350);
    kick_on_butt = u.uevent.udemigod ? 1 : 0;
    if (u.uevent.uhand_of_elbereth)
        kick_on_butt++;
    if (kick_on_butt)
        u.ublesscnt += kick_on_butt * rnz(1000);

    /* Avoid games that go into infinite loops of copy-pasted commands
       with no human interaction; this is a DoS vector against the
       computer running NetHack.  Once the turn counter is over 100000,
       every additional 100 turns increases the prayer timeout by 1,
       thus eventually hunger prayers will fail and some other source
       of nutrition will be required.  The increase gets throttled if
       it ever reaches 32K so that configurations using 16-bit ints are
       still viable. */
    if (svm.moves > 100000L) {
        long incr = (svm.moves - 100000L) / 100L,
             largest_ublesscnt_incr = (long) (LARGEST_INT - u.ublesscnt);

        if (incr > largest_ublesscnt_incr)
            incr = largest_ublesscnt_incr;
        u.ublesscnt += (int) incr;
    }

    return;
}

/* either blesses or curses water on the altar,
 * returns true if it found any water here.
 */
staticfn boolean
water_prayer(boolean bless_water)
{
    struct obj *otmp;
    long changed = 0;
    boolean other = FALSE, bc_known = !(Blind || Hallucination);

    for (otmp = svl.level.objects[u.ux][u.uy]; otmp; otmp = otmp->nexthere) {
        /* turn water into (un)holy water */
        if (otmp->otyp == POT_WATER
            && (bless_water ? !otmp->blessed : !otmp->cursed)) {
            otmp->blessed = bless_water;
            otmp->cursed = !bless_water;
            otmp->bknown = bc_known; /* ok to bypass set_bknown() */
            changed += otmp->quan;
        } else if (otmp->oclass == POTION_CLASS)
            other = TRUE;
    }
    if (!Blind && changed) {
        pline(TR("%s potion%s on the altar glow%s %s for a moment.", "%s祭坛上的药剂%s短暂地发出%s%s色的光芒。"),
              ((other && changed > 1L) ? TR("Some of the", "一部分")
                                       : (other ? TR("One of the", "其中一个") : TR("The", "这"))),
              ((other || changed > 1L) ? "s" : ""), (changed > 1L ? "" : "s"),
              (bless_water ? hcolor(NH_LIGHT_BLUE) : hcolor(NH_BLACK)));
    }
    return (boolean) (changed > 0L);
}

staticfn void
godvoice(aligntyp g_align, const char *words)
{
    const char *quot = "";

    if (words)
        quot = "\"";
    else
        words = "";

    pline_The(TR("voice of %s %s: %s%s%s", "%s的声音%s：%s%s%s"), align_gname(g_align),
              ROLL_FROM(godvoices), quot, words, quot);
}

staticfn void
gods_angry(aligntyp g_align)
{
#ifdef ZHLANG
    godvoice(g_align, "你惹怒了我。");
#else
    godvoice(g_align, "Thou hast angered me.");
#endif
}

/* The g_align god is upset with you. */
staticfn void
gods_upset(aligntyp g_align)
{
    if (g_align == u.ualign.type)
        u.ugangr++;
    else if (u.ugangr)
        u.ugangr--;
    angrygods(g_align);
}

staticfn void
consume_offering(struct obj *otmp)
{
    if (Hallucination)
        switch (rn2(3)) {
        case 0:
            Your(TR("sacrifice sprouts wings and a propeller and roars away!", "祭品长出翅膀和螺旋桨，轰隆隆地飞走了！"));
            break;
        case 1:
            Your(TR("sacrifice puffs up, swelling bigger and bigger, and pops!", "祭品膨胀起来，越变越大，然后啪地炸开了！"));
            break;
        case 2:
            Your(
     TR("sacrifice collapses into a cloud of dancing particles and fades away!", "祭品坍缩成一团舞动的微粒，然后消散了！"));
            break;
        }
    else if (Blind && u.ualign.type == A_LAWFUL)
        Your(TR("sacrifice disappears!", "祭品消失了！"));
    else
        Your(TR("sacrifice is consumed in a %s!", "祭品在一%s中消失了！"),
             (u.ualign.type == A_LAWFUL)
                ? TR("flash of light", "道闪光")
                : (u.ualign.type == A_NEUTRAL)
                    ? TR("plume of smoke", "缕烟雾")
                    : TR("burst of flame", "团火焰"));
    if (carried(otmp))
        useup(otmp);
    else
        useupf(otmp, 1L);
    exercise(A_WIS, TRUE);
}

/* feedback when attempting to offer the Amulet on a "low altar" (not one of
   the high altars in the temples on the Astral Plane or Moloch's Sanctum) */
staticfn void
offer_too_soon(aligntyp altaralign)
{
    if (altaralign == A_NONE && Inhell) {
        /* offering on an unaligned altar in Gehennom;
           hero has left Moloch's Sanctum (caller handles that)
           so is in the process of getting away with the Amulet;
           for any unaligned altar outside of Gehennom, give the
           "you feel ashamed" feedback for wrong alignment below */
        gods_upset(A_NONE); /* Moloch becomes angry */
        return;
    }
    You_feel("%s.", Hallucination
                    ? TR("homesick", "想家")
                    /* if on track, give a big hint */
                    : (altaralign == u.ualign.type)
                        ? TR("an urge to return to the surface", "想回到地表的冲动")
                        /* else headed towards celestial disgrace */
                        : TR("ashamed", "羞愧"));
}

void
desecrate_altar(boolean highaltar, aligntyp altaralign)
{
    char gvbuf[BUFSZ];

    /*
     * REAL BAD NEWS!!! High altars cannot be converted.  Even an attempt
     * gets the god who owns it truly pissed off.  The same effect for
     * deliberately destroying a normal altar.
     */
    /* if you did this to your own altar, your god will hold a grudge... */
    if (altaralign == u.ualign.type) {
        adjalign(-20);
        u.ugangr += 5;
    }
    You_feel(TR("the air around you grow charged...", "感到周围的空气变得带电……"));
    pline(TR("Suddenly, you realize that %s has noticed you...", "突然间，你意识到%s注意到了你……"),
          align_gname(altaralign));
    Sprintf(gvbuf, TR("So, mortal!  You dare desecrate my %s!", "凡人！你竟敢亵渎我的%s！"),
            highaltar ? TR("High Temple", "高等神殿") : TR("altar", "祭坛"));
    godvoice(altaralign, gvbuf);
    /* Throw everything we have at the player */
    god_zaps_you(altaralign);
}

/* offering the Amulet on a high altar (checked by caller) ends the game;
   we don't declare this 'NORETURN' because done() can return (if called
   with some reasons other than ASCENDED and ESCAPED) */
staticfn void
offer_real_amulet(struct obj *otmp, aligntyp altaralign)
{
    static NEARDATA const char
        cloud_of_smoke[] = "A cloud of %s smoke surrounds you...";

    /* The final Test.  Did you win? */
    if (uamul == otmp)
        Amulet_off();
    if (carried(otmp))
        useup(otmp); /* well, it's gone now */
    else
        useupf(otmp, 1L);

    You(TR("offer the Amulet of Yendor to %s...", "向%s献上岩德护身符……"), a_gname());

    if (altaralign == A_NONE) {
        /* Moloch's high altar at the bottom of Gehennom. */
        if (u.ualign.record > -99)
            u.ualign.record = -99;
        pline(TR("An invisible choir chants, and you are bathed in darkness...", "一支无形的合唱队吟唱起来，你被黑暗笼罩……"));
        /*[apparently shrug/snarl can be sensed without being seen]*/
        pline(TR("%s shrugs and retains dominion over %s,", "%s耸了耸肩，继续保有对%s的统治权，"), Moloch, u_gname());
        pline(TR("then mercilessly snuffs out your life.", "然后无情地掐灭了你的生命。"));
        Sprintf(svk.killer.name, TR("%s indifference", "%s的冷漠"), s_suffix(Moloch));
        svk.killer.format = KILLED_BY;
        done(DIED);
        /* life-saved (or declined to die in wizard/explore mode) */
        pline(TR("%s snarls and tries again...", "%s咆哮着，再次尝试……"), Moloch);
        fry_by_god(A_NONE, TRUE); /* wrath of Moloch */
        /* declined to die in wizard or explore mode */
        pline(cloud_of_smoke, hcolor(NH_BLACK));
        done(ESCAPED);
        /*NOTREACHED*/
    } else if (u.ualign.type != altaralign) {
        /* And the opposing team picks you up and carries you off
           on their shoulders. */
        adjalign(-99);
        pline(TR("%s accepts your gift, and gains dominion over %s...", "%s接受了你的礼物，并获得了对%s的统治权……"),
              a_gname(), u_gname());
        pline(TR("%s is enraged...", "%s暴怒……"), u_gname());
        pline(TR("Fortunately, %s permits you to live...", "幸运的是，%s允许你活下去……"), a_gname());
        pline(cloud_of_smoke, hcolor(NH_ORANGE));
        done(ESCAPED);
        /*NOTREACHED*/
    } else {
        /* You've won the game!  Feedback-wise, it's a bit of a let down. */
        u.uevent.ascended = 1;
        adjalign(10);
        pline(TR("An invisible choir sings, and you are bathed in radiance...", "一支无形的合唱队唱起歌来，你沐浴在光辉中……"));
        godvoice(altaralign, "Mortal, thou hast done well!");
        display_nhwindow(WIN_MESSAGE, FALSE);
        SetVoice((struct monst *) 0, 0, 80, voice_deity);
        verbalize(
          TR("In return for thy service, I grant thee the gift of Immortality!", "作为你侍奉的回报，我赐予你永生的礼物！"));
        You(TR("ascend to the status of Demigod%s...", "晋升为半神%s……"),
            flags.female ? TR("dess", "（女）") : "");
        done(ASCENDED);
        /*NOTREACHED*/
    }
    /*NOTREACHED*/
}

staticfn void
offer_negative_valued(boolean highaltar, aligntyp altaralign)
{
    if (altaralign != u.ualign.type && highaltar) {
        desecrate_altar(highaltar, altaralign);
    } else {
        gods_upset(altaralign);
    }
}

staticfn void
offer_fake_amulet(
    struct obj *otmp,
    boolean highaltar,
    aligntyp altaralign)
{
    if (!highaltar && !otmp->known) {
        offer_too_soon(altaralign);
        return;
    }
    Soundeffect(se_thunderclap, 100);
    You_hear(TR("a nearby thunderclap.", "附近传来一声霹雳。"));
    if (!otmp->known) {
        You(TR("realize you have made a %s.", "意识到自己犯了个%s。"),
            Hallucination ? TR("boo-boo", "小错") : TR("mistake", "错误"));
        otmp->known = TRUE;
        change_luck(-1);
    } else {
        /* don't you dare try to fool the gods */
        if (Deaf)
            pline("Oh, no."); /* didn't hear thunderclap */
        change_luck(-3);
        adjalign(-1);
        u.ugangr += 3;
        offer_negative_valued(highaltar, altaralign);
    }
}

/* possibly convert an altar's alignment or the hero's alignment */
staticfn void
offer_different_alignment_altar(
    struct obj *otmp,
    aligntyp altaralign)
{
    /* Is this a conversion ? */
    /* An unaligned altar in Gehennom will always elicit rejection. */
    if (ugod_is_angry() || (altaralign == A_NONE && Inhell)) {
        if (u.ualignbase[A_CURRENT] == u.ualignbase[A_ORIGINAL]
            && altaralign != A_NONE) {
            You(TR("have a strong feeling that %s is angry...", "强烈地感到%s发怒了……"), u_gname());
            consume_offering(otmp);
            pline(TR("%s accepts your allegiance.", "%s接受了你的效忠。"), a_gname());

            uchangealign(altaralign, A_CG_CONVERT);
            /* Beware, Conversion is costly */
            change_luck(-3);
            u.ublesscnt += 300;
        } else {
            u.ugangr += 3;
            adjalign(-5);
            pline(TR("%s rejects your sacrifice!", "%s拒绝了你的祭品！"), a_gname());
            godvoice(altaralign, "Suffer, infidel!");
            change_luck(-5);
            (void) adjattrib(A_WIS, -2, TRUE);
            if (!Inhell)
                angrygods(u.ualign.type);
        }
    } else {
        consume_offering(otmp);
        You(TR("sense a conflict between %s and %s.", "感觉到%s与%s之间存在冲突。"), u_gname(), a_gname());
        if (rn2(8 + u.ulevel) > 5) {
            struct monst *pri;
            boolean shrine;

            You_feel(TR("the power of %s increase.", "%s的力量增强了。"), u_gname());
            exercise(A_WIS, TRUE);
            change_luck(1);
            shrine = on_shrine();
            levl[u.ux][u.uy].altarmask = Align2amask(u.ualign.type);
            if (shrine)
                levl[u.ux][u.uy].altarmask |= AM_SHRINE;
            newsym(u.ux, u.uy); /* in case Invisible to self */
            if (!Blind)
                pline_The(TR("altar glows %s.", "祭坛发出%s色的光。"),
                          hcolor((u.ualign.type == A_LAWFUL) ? NH_WHITE
                                 : u.ualign.type ? NH_BLACK
                                   : (const char *) TR("gray", "灰")));

            if (rnl(u.ulevel) > 6 && u.ualign.record > 0
                && rnd(u.ualign.record) > (3 * ALIGNLIM) / 4)
                summon_minion(altaralign, TRUE);
            /* anger priest; test handles bones files */
            if ((pri = findpriest(temple_occupied(u.urooms)))
                && !p_coaligned(pri))
                angry_priest();
        } else {
            pline(TR("Unluckily, you feel the power of %s decrease.", "不幸的是，你感到%s的力量减弱了。"), u_gname());
            change_luck(-1);
            exercise(A_WIS, FALSE);
            if (rnl(u.ulevel) > 6 && u.ualign.record > 0
                && rnd(u.ualign.record) > (7 * ALIGNLIM) / 8)
                summon_minion(altaralign, TRUE);
        }
    }
}

staticfn void
sacrifice_your_race(
    struct obj *otmp,
    boolean highaltar,
    aligntyp altaralign)
{
    int pm;

    if (is_demon(gy.youmonst.data)) {
        You(TR("find the idea very satisfying.", "觉得这个主意非常令人满意。"));
        exercise(A_WIS, TRUE);
    } else if (u.ualign.type != A_CHAOTIC) {
        pline(TR("You'll regret this infamous offense!", "你会为这桩臭名昭著的罪行后悔的！"));
        exercise(A_WIS, FALSE);
    }

    if (highaltar
        && (altaralign != A_CHAOTIC || u.ualign.type != A_CHAOTIC)) {
        desecrate_altar(highaltar, altaralign);
        return;
    } else if (altaralign != A_CHAOTIC && altaralign != A_NONE) {
        /* curse the lawful/neutral altar */
        pline_The(TR("altar is stained with %s blood.", "祭坛被%s的鲜血染红了。"), gu.urace.adj);
        levl[u.ux][u.uy].altarmask = AM_CHAOTIC;
        newsym(u.ux, u.uy); /* in case Invisible to self */
        angry_priest();
    } else {
        struct monst *dmon;
        const char *demonless_msg;

        /* Human sacrifice on a chaotic or unaligned altar */
        /* is equivalent to demon summoning */
        if (altaralign == A_CHAOTIC && u.ualign.type != A_CHAOTIC) {
            pline(
            TR("The blood floods the altar, which vanishes in %s cloud!", "鲜血淹没了祭坛，祭坛在一%s云中消失了！"),
                    an(hcolor(NH_BLACK)));
            levl[u.ux][u.uy].typ = ROOM;
            levl[u.ux][u.uy].altarmask = 0;
            newsym(u.ux, u.uy);
            angry_priest();
            demonless_msg = "cloud dissipates";
        } else {
            /* either you're chaotic or altar is Moloch's or both */
            pline_The(TR("blood covers the altar!", "鲜血覆盖了祭坛！"));
            change_luck(altaralign == A_NONE ? -2 : 2);
            demonless_msg = "blood coagulates";
        }
        if ((pm = dlord(altaralign)) != NON_PM
            && (dmon = makemon(&mons[pm], u.ux, u.uy, MM_NOMSG))
                    != 0) {
            char dbuf[BUFSZ];

            Strcpy(dbuf, a_monnam(dmon));
            if (!strcmpi(dbuf, "it"))
                Strcpy(dbuf, TR("something dreadful", "某种可怕的东西"));
            else
                dmon->mstrategy &= ~STRAT_APPEARMSG;
            You(TR("have summoned %s!", "召唤出了%s！"), dbuf);
            if (sgn(u.ualign.type) == sgn(dmon->data->maligntyp))
                dmon->mpeaceful = TRUE;
            You(TR("are terrified, and unable to move.", "被吓得动弹不得。"));
            nomul(-3);
            gm.multi_reason = TR("being terrified of a demon", "正被恶魔吓坏");
            gn.nomovemsg = 0;
        } else
            pline_The("%s.", demonless_msg);
    }

    if (u.ualign.type != A_CHAOTIC) {
        adjalign(-5);
        u.ugangr += 3;
        (void) adjattrib(A_WIS, -1, TRUE);
        if (!Inhell)
            angrygods(u.ualign.type);
        change_luck(-5);
    } else
        adjalign(5);
    if (carried(otmp))
        useup(otmp);
    else
        useupf(otmp, 1L);
}

staticfn int
bestow_artifact(uchar max_giftvalue)
{
    int nartifacts = nartifact_exist();
    boolean do_bestow = u.ulevel > 2 && u.uluck >= 0;
    if (do_bestow) {
        /* you were already in pretty good standing */
        /* The player can gain an artifact */
        /* The chance goes down as the number of artifacts goes up */
        if (wizard)
            do_bestow = y_n("Gift an artifact?") == 'y';
        else
            do_bestow = !rn2(6 + (2 * u.ugifts * nartifacts));
    }

    if (do_bestow) {
        struct obj *otmp;
        /* mk_artifact() with NULL obj and a_align() arg can return NULL */
        otmp = mk_artifact((struct obj *) 0, a_align(u.ux, u.uy),
                           max_giftvalue, TRUE);
        if (otmp) {
            char buf[BUFSZ];

            artifact_origin(otmp, ONAME_GIFT | ONAME_KNOW_ARTI);
            if (otmp->spe < 0)
                otmp->spe = 0;
            if (otmp->cursed)
                uncurse(otmp);
            otmp->oerodeproof = TRUE;
            Strcpy(buf, (Hallucination ? TR("a doodad", "一个小玩意")
                            : Blind ? TR("an object", "一个物体")
                            : ansimpleoname(otmp)));
            if (!Blind)
                Sprintf(eos(buf), TR(" named %s", "，名为%s"),
                        bare_artifactname(otmp));
            at_your_feet(upstart(buf));
            dropy(otmp);
            godvoice(u.ualign.type, "Use my gift wisely!");
            u.ugifts++;
            u.ublesscnt = rnz(300 + (50 * nartifacts));
            exercise(A_WIS, TRUE);
            livelog_printf (LL_DIVINEGIFT | LL_ARTIFACT,
                            TR("was bestowed with %s by %s", "蒙赐%s，来自%s"),
                            artiname(otmp->oartifact),
                            align_gname(u.ualign.type));
            /* make sure we can use this weapon */
            unrestrict_weapon_skill(weapon_type(otmp));
            if (!Hallucination && !Blind) {
                observe_object(otmp);
                makeknown(otmp->otyp);
                discover_artifact(otmp->oartifact);
            }
            return TRUE;
        }
    }
    return FALSE;
}

staticfn int
sacrifice_value(struct obj *otmp)
{
    int value = 0;

    if (otmp->corpsenm == PM_ACID_BLOB
        || (svm.moves <= peek_at_iced_corpse_age(otmp) + 50)) {
        value = mons[otmp->corpsenm].difficulty + 1;
        if (otmp->oeaten)
            value = eaten_stat(value, otmp);
    }
    return value;
}

/* the #offer command - sacrifice something to the gods */
int
dosacrifice(void)
{
    struct obj *otmp;
    boolean highaltar;
    aligntyp altaralign = a_align(u.ux, u.uy);

    if (!on_altar() || u.uswallow) {
        You(TR("are not %s an altar.", "不在祭坛%s。"),
            (Levitation || Flying) ? TR("over", "上方") : "on");
        return ECMD_OK;
    } else if (Confusion || Stunned) {
        You(TR("are too impaired to perform the rite.", "神志不清，无法举行仪式。"));
        return ECMD_OK;
    }
    highaltar = (levl[u.ux][u.uy].altarmask & AM_SANCTUM);

    otmp = floorfood("sacrifice", 1);
    if (!otmp)
        return ECMD_OK;

    if (otmp->otyp == AMULET_OF_YENDOR) {
        if (!highaltar) {
            offer_too_soon(altaralign);
            return ECMD_TIME;
        } else {
            offer_real_amulet(otmp, altaralign);
            /*NOTREACHED*/
        }
    } /* real Amulet */

    if (otmp->otyp == FAKE_AMULET_OF_YENDOR) {
        offer_fake_amulet(otmp, highaltar, altaralign);
        return ECMD_TIME;
    } /* fake Amulet */

    if (otmp->otyp == CORPSE) {
        offer_corpse(otmp, highaltar, altaralign);
        return ECMD_TIME;
    }

    pline1(nothing_happens);
    return ECMD_TIME;
}

staticfn int
eval_offering(struct obj *otmp, aligntyp altaralign)
{
    struct permonst *ptr;
    int value;

    value = sacrifice_value(otmp);

    if (!value)
        return 0;

    ptr = &mons[otmp->corpsenm];

    if (is_undead(ptr)) { /* Not demons--no demon corpses */
        /* most undead that leave a corpse yield 'human' (or other race)
           corpse so won't get here; the exception is wraith; give the
           bonus for wraith to chaotics too because they are sacrificing
           something valuable (unless hero refuses to eat such things) */
        if (u.ualign.type != A_CHAOTIC
            /* reaching this side of the 'or' means hero is chaotic */
            || (ptr == &mons[PM_WRAITH] && u.uconduct.unvegetarian))
            value += 1;
    } else if (is_unicorn(ptr)) {
        int unicalign = sgn(ptr->maligntyp);

        if (unicalign == altaralign) {
            /* When same as altar, always a very bad action.
             */
            pline(TR("Such an action is an insult to %s!", "这种行为是对%s的侮辱！"),
                  (unicalign == A_CHAOTIC) ? TR("chaos", "混沌")
                     : unicalign ? TR("law", "守序") : TR("balance", "平衡"));
            (void) adjattrib(A_WIS, -1, TRUE);
            return -1;
        } else if (u.ualign.type == altaralign) {
            /* When different from altar, and altar is same as yours,
             * it's a very good action.
             */
            if (u.ualign.record < ALIGNLIM)
                You_feel(TR("appropriately %s.", "感到恰如其分地%s。"), align_str(u.ualign.type));
            else
                You_feel(TR("you are thoroughly on the right path.", "彻底走在正确的道路上。"));
            adjalign(5);
            value += 3;
        } else if (unicalign == u.ualign.type) {
            /* When sacrificing unicorn of your alignment to altar not of
             * your alignment, your god gets angry and it's a conversion.
             */
            u.ualign.record = -1;
            value = 1;
        } else {
            /* Otherwise, unicorn's alignment is different from yours
             * and different from the altar's.  It's an ordinary (well,
             * with a bonus) sacrifice on a cross-aligned altar.
             */
            value += 3;
        }
    }
    return value;
}

staticfn void
offer_corpse(struct obj *otmp, boolean highaltar, aligntyp altaralign)
{
    int value;
    struct permonst *ptr;
    struct monst *mtmp;

    /*
     * Was based on nutritional value and aging behavior (< 50 moves).
     * Sacrificing a food ration got you max luck instantly, making the
     * gods as easy to please as an angry dog!
     *
     * Now only accepts corpses, based on the game's evaluation of their
     * toughness.  Human and pet sacrifice, as well as sacrificing unicorns
     * of your alignment, is strongly discouraged.
     */
#define MAXVALUE 24 /* Highest corpse value (besides Wiz) */

    /* KMH, conduct */
    if (!u.uconduct.gnostic++)
        livelog_printf(LL_CONDUCT,
                       TR("rejected atheism by offering %s on an altar of %s",
                          "拒绝了无神论：献上了%s，供于%s的祭坛上"),
                       corpse_xname(otmp, (const char *) 0, CXN_ARTICLE),
                       a_gname());

    /* you're handling this corpse, even if it was killed upon the altar
     */
    feel_cockatrice(otmp, TRUE);
    if (rider_corpse_revival(otmp, FALSE))
        return;

    ptr = &mons[otmp->corpsenm];

    /* same race or former pet results apply even if the corpse is
       too old (value==0) */
    if (your_race(ptr)) {
        sacrifice_your_race(otmp, highaltar, altaralign);
        return;
    }
    if (has_omonst(otmp)
               && (mtmp = get_mtraits(otmp, FALSE)) != 0
               && mtmp->mtame) {
            /* mtmp is a temporary pointer to a tame monster's attributes,
             * not a real monster */
        pline(TR("So this is how you repay loyalty?", "这就是你回报忠诚的方式？"));
        adjalign(-3);
        HAggravate_monster |= FROMOUTSIDE;
        offer_negative_valued(highaltar, altaralign);
        return;
    }

    value = eval_offering(otmp, altaralign);
    if (value == 0) {
        /* too old; don't give undead or unicorn bonus or penalty */
        pline1(nothing_happens);
        return;
    }
    if (value < 0) {
        offer_negative_valued(highaltar, altaralign);
        return;
    }

    if (altaralign != u.ualign.type && highaltar) {
        desecrate_altar(highaltar, altaralign);
        return;
    }
    if (u.ualign.type != altaralign) {
        /* Sacrificing at an altar of a different alignment */
        offer_different_alignment_altar(otmp, altaralign);
        return;
    }
    consume_offering(otmp);
    /* OK, you get brownie points. */
    if (u.ugangr) {
        int saved_anger = u.ugangr;
        u.ugangr -= ((value * (u.ualign.type == A_CHAOTIC ? 2 : 3))
                     / MAXVALUE);
        if (u.ugangr < 0)
            u.ugangr = 0;
        if (u.ugangr != saved_anger) {
            if (u.ugangr) {
                pline(TR("%s seems %s.", "%s似乎%s。"), u_gname(),
                      Hallucination ? TR("groovy", "心情不错") : TR("slightly mollified", "略微消气了"));

                if ((int) u.uluck < 0)
                    change_luck(1);
            } else {
                pline(TR("%s seems %s.", "%s似乎%s。"), u_gname(),
                      Hallucination ? TR("cosmic (not a new fact)", "超凡脱俗（并非新情况）")
                                    : TR("mollified", "消气了"));

                if ((int) u.uluck < 0)
                    u.uluck = 0;
            }
        } else { /* not satisfied yet */
            if (Hallucination)
                pline_The(TR("gods seem tall.", "神祇显得很高大。"));
            else
#ifdef ZHLANG
                You("感到力不从心。");
#else
                You("have a feeling of inadequacy.");
#endif
        }
    } else if (ugod_is_angry()) {
        if (value > MAXVALUE)
            value = MAXVALUE;
        if (value > -u.ualign.record)
            value = -u.ualign.record;
        adjalign(value);
        You_feel(TR("partially absolved.", "部分罪过得到了赦免。"));
    } else if (u.ublesscnt > 0) {
        int saved_cnt = u.ublesscnt;
        u.ublesscnt -= ((value * (u.ualign.type == A_CHAOTIC ? 500 : 300))
                        / MAXVALUE);
        if (u.ublesscnt < 0)
            u.ublesscnt = 0;
        if (u.ublesscnt != saved_cnt) {
            if (u.ublesscnt) {
                if (Hallucination)
                    You(TR("realize that the gods are not like you and I.", "意识到神祇与你我不同。"));
                else
                    You(TR("have a hopeful feeling.", "感到充满希望。"));
                if ((int) u.uluck < 0)
                    change_luck(1);
            } else {
                if (Hallucination)
                    pline(TR("Overall, there is a smell of fried onions.", "总的来说，空气中有一股炸洋葱味。"));
                else
                    You(TR("have a feeling of reconciliation.", "感到与神祇和解了。"));
                if ((int) u.uluck < 0)
                    u.uluck = 0;
            }
        }
    } else {
        int orig_luck, luck_increase;

        if (bestow_artifact(value))
            return;

        orig_luck = u.uluck;
        luck_increase = (value * LUCKMAX) / (MAXVALUE * 2);

        /* sacrificing can't increase non-bonus Luck to above the value of the
           sacrifice; this prevents players immediately maxing their Luck as
           soon as they find an altar and a few rations via sacrificing lots
           of low-valued corpses, which can unbalance the early game */
        if (orig_luck > value)
            luck_increase = 0;
        else if (orig_luck + luck_increase > value)
            luck_increase = value - orig_luck;

        change_luck(luck_increase);
        if ((int) u.uluck < 0)
            u.uluck = 0;
        if (u.uluck != orig_luck) {
            if (Blind)
                You(TR("think %s brushed your %s.", "觉得%s拂过了你的%s。"), something,
                    body_part(FOOT));
            else
                You(Hallucination
                ? TR("see crabgrass at your %s.  A funny thing in a dungeon.", "看到你的%s上长着马唐草。地牢里可真稀奇。")
                        : TR("glimpse a four-leaf clover at your %s.", "瞥见你的%s上有一株四叶草。"),
                    makeplural(body_part(FOOT)));
        }
    }
}

/* determine prayer results in advance; also used for enlightenment */
boolean
can_pray(boolean praying) /* false means no messages should be given */
{
    int alignment;

    gp.p_aligntyp = on_altar() ? a_align(u.ux, u.uy) : u.ualign.type;
    gp.p_trouble = in_trouble();

    if (is_demon(gy.youmonst.data) /* ok if chaotic or none (Moloch) */
        && (gp.p_aligntyp == A_LAWFUL || gp.p_aligntyp != A_NEUTRAL)) {
        if (praying)
            pline_The(TR("very idea of praying to a %s god is repugnant to you.", "一想到向%s神祈祷就令你厌恶。"),
                      gp.p_aligntyp ? TR("lawful", "守序") : TR("neutral", "中立"));
        return FALSE;
    }

    if (praying)
#ifdef ZHLANG
        You("你开始向%s祈祷。", align_gname(gp.p_aligntyp));
#else
        You("begin praying to %s.", align_gname(gp.p_aligntyp));
#endif

    if (u.ualign.type && u.ualign.type == -gp.p_aligntyp)
        alignment = -u.ualign.record; /* Opposite alignment altar */
    else if (u.ualign.type != gp.p_aligntyp)
        alignment = u.ualign.record / 2; /* Different alignment altar */
    else
        alignment = u.ualign.record;

    if (gp.p_aligntyp == A_NONE) /* praying to Moloch */
        gp.p_type = -2;
    else if ((gp.p_trouble > 0) ? (u.ublesscnt > 200)   /* big trouble */
             : (gp.p_trouble < 0) ? (u.ublesscnt > 100) /* minor difficulty */
               : (u.ublesscnt > 0))                     /* not in trouble */
        gp.p_type = 0;                     /* too soon... */
    else if ((int) Luck < 0 || u.ugangr || alignment < 0)
        gp.p_type = 1; /* too naughty... */
    else /* alignment >= 0 */ {
        if (on_altar() && u.ualign.type != gp.p_aligntyp)
            gp.p_type = 2;
        else
            gp.p_type = 3;
    }

    if (is_undead(gy.youmonst.data) && !Inhell
        && (gp.p_aligntyp == A_LAWFUL
            || (gp.p_aligntyp == A_NEUTRAL && !rn2(10))))
        gp.p_type = -1;
    /* Note:  when !praying, the random factor for neutrals makes the
       return value a non-deterministic approximation for enlightenment.
       This case should be uncommon enough to live with... */

    return !praying ? (boolean) (gp.p_type == 3 && !Inhell) : TRUE;
}

/* return TRUE if praying revived a pet corpse */
staticfn boolean
pray_revive(void)
{
    struct obj *otmp;

    for (otmp = svl.level.objects[u.ux][u.uy]; otmp; otmp = otmp->nexthere)
        if ((otmp->otyp == CORPSE || otmp->otyp == STATUE)
            && has_omonst(otmp)
            && OMONST(otmp)->mtame && !OMONST(otmp)->isminion)
            break;

    if (!otmp)
        return FALSE;

    if (otmp->otyp == CORPSE)
        return (revive(otmp, TRUE) != NULL);
    else {
        return (animate_statue(otmp, u.ux, u.uy, ANIMATE_SPELL, NULL) != NULL);
    }
}

/* #pray command */
int
dopray(void)
{
    boolean ok;

    /*
     * If ParanoidPray is set, confirm prayer to avoid accidental slips
     * of Alt+p.  If ParanoidConfirm is also set, require "yes" rather
     * than just "y" (will also require "no" to decline).
     */
    if (ParanoidPray) {
        ok = paranoid_query(ParanoidConfirm,
                            TR("Are you sure you want to pray?",
                               "你确定要祈祷吗？"));
#if 0
        /* clear command recall buffer; otherwise ^A to repeat p(ray) would
           do so without confirmation (if 'ok') or do nothing (if '!ok') */
        cmdq_clear(CQ_REPEAT);
        cmdq_add_ec(CQ_REPEAT, dopray);
#endif
        if (!ok) /* declined the "are you sure?" confirmation */
            return ECMD_OK;
    }

    if (!u.uconduct.gnostic++)
        /* breaking conduct should probably occur in can_pray() at
         * "You begin praying to %s", as demons who find praying repugnant
         * should not break conduct.  Also we can add more detail to the
         * livelog message as p_aligntyp will be known.
         */
        livelog_printf(LL_CONDUCT,
                       TR("rejected atheism with a prayer",
                          "以祈祷拒绝了无神论"));

    /* set up p_type and p_alignment */
    if (!can_pray(TRUE))
        return ECMD_OK;

    if (wizard && gp.p_type >= 0) {
        static const char forcesuccess[] = "Force the gods to be pleased?";

        /* if we asked "are you sure?" above we suppressed the response
           from the do-again buffer, so need to suppress this response too;
           otherwise subsequent ^A would use this answer for "are you sure?"
           and bypass confirmation */
        if (ParanoidPray) {
            boolean save_doagain = gi.in_doagain;

            gi.in_doagain = FALSE;
            ok = (YN(forcesuccess) == 'y');
            gi.in_doagain = save_doagain;
        } else {
            ok = (y_n(forcesuccess) == 'y');
        }
        if (ok) {
            u.ublesscnt = 0;
            if (u.uluck < 0)
                u.uluck = 0;
            if (u.ualign.record <= 0)
                u.ualign.record = 1;
            u.ugangr = 0;
            if (gp.p_type < 2)
                gp.p_type = 3;
        }
    }
    nomul(-3);
    gm.multi_reason = TR("praying", "祈祷");
    gn.nomovemsg = TR("You finish your prayer.", "你完成了祈祷。");
    ga.afternmv = prayer_done;

    if (gp.p_type == 3 && !Inhell) {
        /* if you've been true to your god you can't die while you pray */
        if (!Blind)
            You(TR("are surrounded by a shimmering light.", "被闪烁的光芒环绕着。"));
        u.uinvulnerable = TRUE;
    }

    return ECMD_TIME;
}

staticfn int
prayer_done(void) /* M. Stephenson (1.0.3b) */
{
    aligntyp alignment = gp.p_aligntyp;

    u.uinvulnerable = FALSE;
    if (gp.p_type == -2) {
        /* praying at an unaligned altar, not necessarily in Gehennom */
        You(TR("%s diabolical laughter all around you...", "%s四周传来恶魔般的笑声……"),
            !Deaf ? TR("hear", "听到") : TR("intuit", "直觉感到"));
        wake_nearby(FALSE);
        adjalign(-2);
        exercise(A_WIS, FALSE);
        if (!Inhell) {
            /* hero's god[dess] seems to be keeping his/her head down */
            pline(TR("Nothing else happens.", "没有别的事情发生。")); /* not actually true... */
            return 1;
        } /* else use regular Inhell result below */
    } else if (gp.p_type == -1) {
        /* praying while poly'd into an undead creature while non-chaotic */
        godvoice(alignment,
                 (alignment == A_LAWFUL)
                    ? "Vile creature, thou durst call upon me?"
                    : "Walk no more, perversion of nature!");
        You_feel(TR("like you are falling apart.", "感到自己正在分崩离析。"));
        /* KMH -- Gods have mastery over unchanging */
        rehumanize();
        /* no Half_physical_damage adjustment here */
        losehp(rnd(20), "residual undead turning effect", KILLED_BY_AN);
        exercise(A_CON, FALSE);
        return 1;
    }
    if (Inhell) {
        pline(TR("Since you are in Gehennom, %s can't help you.", "由于身处葛汉诺姆，%s无法帮助你。"),
              align_gname(alignment));
        /* haltingly aligned is least likely to anger */
        if (u.ualign.record <= 0 || rnl(u.ualign.record))
            angrygods(u.ualign.type);
        return 0;
    }

    if (gp.p_type == 0) {
        if (on_altar() && u.ualign.type != alignment)
            (void) water_prayer(FALSE);
        u.ublesscnt += rnz(250);
        change_luck(-3);
        gods_upset(u.ualign.type);
    } else if (gp.p_type == 1) {
        if (on_altar() && u.ualign.type != alignment)
            (void) water_prayer(FALSE);
        angrygods(u.ualign.type); /* naughty */
    } else if (gp.p_type == 2) {
        if (water_prayer(FALSE)) {
            /* attempted water prayer on a non-coaligned altar */
            u.ublesscnt += rnz(250);
            change_luck(-3);
            gods_upset(u.ualign.type);
        } else
            pleased(alignment);
    } else {
        /* coaligned */
        if (on_altar()) {
            (void) pray_revive();
            (void) water_prayer(TRUE);
        }
        pleased(alignment); /* nice */
    }
    return 1;
}

/* iterable for undead turning by priest/knight */
staticfn void
maybe_turn_mon_iter(struct monst *mtmp)
{
    /* 3.6.3: used to use cansee() here but the purpose is to prevent
       #turn operating through walls, not to require that the hero be
       able to see the target location */
    if (!couldsee(mtmp->mx, mtmp->my)
        || mdistu(mtmp) > turn_undead_range)
        return;

    if (!mtmp->mpeaceful
        && (is_undead(mtmp->data) || is_vampshifter(mtmp)
            || (is_demon(mtmp->data) && (u.ulevel > (MAXULEV / 2))))) {
        mtmp->msleeping = 0;
        if (Confusion) {
            if (!turn_undead_msg_cnt++)
                pline(TR("Unfortunately, your voice falters.", "不幸的是，你的声音颤抖了。"));
            mtmp->mflee = 0;
            mtmp->mfrozen = 0;
            mtmp->mcanmove = 1;
        } else if (!resist(mtmp, '\0', 0, TELL)) {
            int xlev = 6;

            switch (mtmp->data->mlet) {
                /* this is intentional, lichs are tougher
                   than zombies. */
            case S_LICH:
                xlev += 2;
                FALLTHROUGH;
                /*FALLTHRU*/
            case S_GHOST:
                xlev += 2;
                FALLTHROUGH;
                /*FALLTHRU*/
            case S_VAMPIRE:
                xlev += 2;
                FALLTHROUGH;
                /*FALLTHRU*/
            case S_WRAITH:
                xlev += 2;
                FALLTHROUGH;
                /*FALLTHRU*/
            case S_MUMMY:
                xlev += 2;
                FALLTHROUGH;
                /*FALLTHRU*/
            case S_ZOMBIE:
                if (u.ulevel >= xlev && !resist(mtmp, '\0', 0, NOTELL)) {
                    if (u.ualign.type == A_CHAOTIC) {
                        mtmp->mpeaceful = 1;
                        set_malign(mtmp);
                    } else { /* damn them */
                        killed(mtmp);
                    }
                    break;
                } /* else flee */
                FALLTHROUGH;
                /*FALLTHRU*/
            default:
                monflee(mtmp, 0, FALSE, TRUE);
                break;
            }
        }
    }
}

/* #turn command */
int
doturn(void)
{
    /* Knights & Priest(esse)s only please */
    const char *Gname;

    if (!Role_if(PM_CLERIC) && !Role_if(PM_KNIGHT)) {
        /* Try to use the "turn undead" spell. */
        if (known_spell(SPE_TURN_UNDEAD))
            return spelleffects(SPE_TURN_UNDEAD, FALSE, FALSE);
        You(TR("don't know how to turn undead!", "不知道如何驱散亡灵！"));
        return ECMD_OK;
    }
    if (!u.uconduct.gnostic++)
        livelog_printf(LL_CONDUCT,
                       TR("rejected atheism by turning undead",
                          "通过驱散亡灵拒绝了无神论"));

    Gname = halu_gname(u.ualign.type);

    /* [What about needing free hands (does #turn involve any gesturing)?] */
    if (!can_chant(&gy.youmonst)) {
        /* "evilness": "demons and undead" is too verbose and too precise */
        You(TR("are %s upon %s to turn aside evilness.", "%s%s，以驱散邪恶。"),
            Strangled ? TR("not able to call", "无法呼唤") : TR("incapable of calling", "无力呼唤"), Gname);
        /* violates agnosticism due to intent; conduct tracking is not
           supposed to affect play but we make an exception here:  use a
           move if this is the first time agnostic conduct has been broken */
        return (u.uconduct.gnostic == 1) ? ECMD_TIME : ECMD_OK;
    }
    if ((u.ualign.type != A_CHAOTIC
         && (is_demon(gy.youmonst.data)
             || is_undead(gy.youmonst.data) || is_vampshifter(&gy.youmonst)))
        || u.ugangr > 6) { /* "Die, mortal!" */
        pline(TR("For some reason, %s seems to ignore you.", "不知为何，%s似乎无视了你。"), Gname);
        aggravate();
        exercise(A_WIS, FALSE);
        return ECMD_TIME;
    }
    if (Inhell) {
        pline(TR("Since you are in Gehennom, %s %s help you.", "由于身处葛汉诺姆，%s%s帮助你。"),
              /* not actually calling upon Moloch but use alternate
                 phrasing anyway if hallucinatory feedback says it's him */
              Gname, !strcmp(Gname, Moloch) ? TR("won't", "不会") : TR("can't", "不能"));
        aggravate();
        return ECMD_TIME;
    }
    pline(TR("Calling upon %s, you chant an arcane formula.", "呼唤着%s，你念出了一句奥术咒语。"), Gname);
    exercise(A_WIS, TRUE);

    /* note: does not perform unturn_dead() on victims' inventories */
    turn_undead_range = BOLT_LIM + (u.ulevel / 5); /* 8 to 14 */
    turn_undead_range *= turn_undead_range;
    turn_undead_msg_cnt = 0;

    iter_mons(maybe_turn_mon_iter);

    /*
     *  There is no detrimental effect on self for successful #turn
     *  while in demon or undead form.  That can only be done while
     *  chaotic oneself (see "For some reason" above) and chaotic
     *  turning only makes targets peaceful.
     *
     *  Paralysis duration probably ought to be based on the strength
     *  of turned creatures rather than on turner's level.
     *  Why doesn't this honor Free_action?  [Because being able to
     *  repeat #turn every turn would be too powerful.  Maybe instead
     *  of nomul(-N) we should add the equivalent of mon->mspec_used
     *  for the hero and refuse to #turn when it's non-zero?  Or have
     *  both and u.uspec_used only matters when Free_action prevents
     *  the brief paralysis?]
     */
    nomul(-(5 - ((u.ulevel - 1) / 6))); /* -5 .. -1 */
    gm.multi_reason = TR("trying to turn the monsters", "正试图驱退怪物");
    gn.nomovemsg = You_can_move_again;
    return ECMD_TIME;
}

int
altarmask_at(coordxy x, coordxy y)
{
    int res = 0;

    if (isok(x, y)) {
        struct monst *mon = m_at(x, y);

        if (mon && M_AP_TYPE(mon) == M_AP_FURNITURE
            && mon->mappearance == S_altar)
            res = has_mcorpsenm(mon) ? MCORPSENM(mon) : 0;
        else if (IS_ALTAR(levl[x][y].typ))
            res = levl[x][y].altarmask;
    }
    return res;
}

const char *
a_gname(void)
{
    return a_gname_at(u.ux, u.uy);
}

/* returns the name of an altar's deity */
const char *
a_gname_at(coordxy x, coordxy y)
{
    if (!IS_ALTAR(levl[x][y].typ))
        return (char *) 0;

    return align_gname(a_align(x, y));
}

/* returns the name of the hero's deity */
const char *
u_gname(void)
{
    return align_gname(u.ualign.type);
}

const char *
align_gname(aligntyp alignment)
{
    const char *gnam;

    switch (alignment) {
    case A_NONE:
        gnam = Moloch;
        break;
    case A_LAWFUL:
        gnam = gu.urole.lgod;
        break;
    case A_NEUTRAL:
        gnam = gu.urole.ngod;
        break;
    case A_CHAOTIC:
        gnam = gu.urole.cgod;
        break;
    default:
        impossible("unknown alignment.");
        gnam = "someone";
        break;
    }
    if (*gnam == '_')
        ++gnam;
    return gnam;
}

static const char *const hallu_gods[] = {
    "the Flying Spaghetti Monster", /* Church of the FSM */
    "Eris",                         /* Discordianism */
    "the Martians",                 /* every science fiction ever */
    "Xom",                          /* Crawl */
    "AnDoR dRaKoN",                 /* ADOM */
    "the Central Bank of Yendor",   /* economics */
    "Tooth Fairy",                  /* real world(?) */
    "Om",                           /* Discworld */
    "Yawgmoth",                     /* Magic: the Gathering */
    "Morgoth",                      /* LoTR */
    "Cthulhu",                      /* Lovecraft */
    "the Ori",                      /* Stargate */
    "destiny",                      /* why not? */
    "your Friend the Computer",     /* Paranoia */
};

/* hallucination handling for priest/minion names: select a random god
   iff character is hallucinating */
const char *
halu_gname(aligntyp alignment)
{
    const char *gnam = NULL;
    int which;

    if (!Hallucination)
        return align_gname(alignment);

    /* Some roles (Priest) don't have a pantheon unless we're playing as
       that role, so keep trying until we get a role which does have one.
       [If playing a Priest, the current pantheon will be twice as likely
       to get picked as any of the others.  That's not significant enough
       to bother dealing with.] */
    do
        which = randrole(TRUE);
    while (!roles[which].lgod);

    switch (rn2_on_display_rng(9)) {
    case 0:
    case 1:
        gnam = roles[which].lgod;
        break;
    case 2:
    case 3:
        gnam = roles[which].ngod;
        break;
    case 4:
    case 5:
        gnam = roles[which].cgod;
        break;
    case 6:
    case 7:
        gnam = hallu_gods[rn2_on_display_rng(SIZE(hallu_gods))];
        break;
    case 8:
        gnam = Moloch;
        break;
    default:
        impossible("rn2 broken in halu_gname?!?");
    }
    if (!gnam) {
        impossible("No random god name?");
        gnam = "your Friend the Computer"; /* Paranoia */
    }
    if (*gnam == '_')
        ++gnam;
    return gnam;
}

/* deity's title */
const char *
align_gtitle(aligntyp alignment)
{
    const char *gnam, *result = "god";

    switch (alignment) {
    case A_LAWFUL:
        gnam = gu.urole.lgod;
        break;
    case A_NEUTRAL:
        gnam = gu.urole.ngod;
        break;
    case A_CHAOTIC:
        gnam = gu.urole.cgod;
        break;
    default:
        gnam = 0;
        break;
    }
    if (gnam && *gnam == '_')
        result = "goddess";
    return result;
}

void
altar_wrath(coordxy x, coordxy y)
{
    aligntyp altaralign = a_align(x, y);

    if (u.ualign.type == altaralign && u.ualign.record > -rn2(4)) {
        godvoice(altaralign, "How darest thou desecrate my altar!");
        (void) adjattrib(A_WIS, -1, FALSE);
        u.ualign.record--;
    } else {
        pline("%s %s%s:",
              !Deaf ? TR("A voice (could it be", "一个声音（难道是")
                    : TR("Despite your deafness, you seem to hear", "尽管你耳聋，你似乎听到"),
              align_gname(altaralign),
              !Deaf ? TR("?) whispers", "？）低语道") : TR(" say", "说道"));
        SetVoice((struct monst *) 0, 0, 80, voice_deity);
        verbalize(TR("Thou shalt pay, infidel!", "你会付出代价的，异教徒！"));
        /* higher luck is more likely to be reduced; as it approaches -5
           the chance to lose another point drops down, eventually to 0 */
        if (Luck > -5 && rn2(Luck + 6))
            change_luck(rn2(20) ? -1 : -2);
    }
}

/* assumes isok() at one space away, but not necessarily at two */
staticfn boolean
blocked_boulder(int dx, int dy)
{
    struct obj *otmp;
    int nx, ny;
    long count = 0L;

    for (otmp = svl.level.objects[u.ux + dx][u.uy + dy]; otmp;
         otmp = otmp->nexthere) {
        if (otmp->otyp == BOULDER)
            count += otmp->quan;
    }

    nx = u.ux + 2 * dx, ny = u.uy + 2 * dy; /* next spot beyond boulder(s) */
    switch (count) {
    case 0:
        /* no boulders--not blocked */
        return FALSE;
    case 1:
        /* possibly blocked depending on if it's pushable */
        break;
    case 2:
        /* this is only approximate since multiple boulders might sink */
        if (is_pool_or_lava(nx, ny)) /* does its own isok() check */
            break; /* still need Sokoban check below */
        FALLTHROUGH;
        /*FALLTHRU*/
    default:
        /* more than one boulder--blocked after they push the top one;
           don't force them to push it first to find out */
        return TRUE;
    }

    if (dx && dy && Sokoban) /* can't push boulder diagonally in Sokoban */
        return TRUE;
    if (!isok(nx, ny))
        return TRUE;
    if (IS_OBSTRUCTED(levl[nx][ny].typ))
        return TRUE;
    if (sobj_at(BOULDER, nx, ny))
        return TRUE;

    return FALSE;
}

/*pray.c*/
