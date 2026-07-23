/* NetHack 5.0	polyself.c	$NHDT-Date: 1781973061 2026/06/20 16:31:01 $  $NHDT-Branch: NetHack-5.0 $:$NHDT-Revision: 1.233 $ */
/*      Copyright (C) 1987, 1988, 1989 by Ken Arromdee */
/* NetHack may be freely redistributed.  See license for details. */

/*
 * Polymorph self routine.
 *
 * Note:  the light source handling code assumes that gy.youmonst.m_id
 * always remains 1 and gy.youmonst.mx will always remain 0 when it handles
 * the case of the player polymorphed into a light-emitting monster.
 *
 * Transformation sequences:
 *              /-> polymon                 poly into monster form
 *    polyself =
 *              \-> newman -> polyman       fail to poly, get human form
 *
 *    rehumanize -> polyman                 return to original form
 *
 *    polymon (called directly)             usually golem petrification
 */

#include "hack.h"

staticfn void check_strangling(boolean);
staticfn void polyman(const char *, const char *);
staticfn void dropp(struct obj *);
staticfn void break_armor(void);
staticfn void drop_weapon(int);
staticfn int armor_to_dragon(int);
staticfn void newman(void);
staticfn void polysense(void);

static const char no_longer_petrify_resistant[] =
    "No longer petrify-resistant, you";

/* update the gy.youmonst.data structure pointer and intrinsics */
void
set_uasmon(void)
{
    struct permonst *mdat = &mons[u.umonnum];
    boolean was_vampshifter = valid_vampshiftform(gy.youmonst.cham, u.umonnum);

    set_mon_data(&gy.youmonst, mdat);
    gy.youmonst.m_id = 1;

    if (Protection_from_shape_changers)
        gy.youmonst.cham = NON_PM;
    else if (is_vampire(gy.youmonst.data))
        gy.youmonst.cham = gy.youmonst.mnum;
    /* assume hero-as-chameleon/doppelganger/sandestin doesn't change shape */
    else if (!was_vampshifter)
        gy.youmonst.cham = NON_PM;
    u.mcham = gy.youmonst.cham; /* for save/restore since youmonst isn't */

#define PROPSET(PropIndx, ON)                          \
    do {                                               \
        if (ON)                                        \
            u.uprops[PropIndx].intrinsic |= FROMFORM;  \
        else                                           \
            u.uprops[PropIndx].intrinsic &= ~FROMFORM; \
    } while (0)
#define resist_from_form(MRtyp) ((gy.youmonst.data->mresists & (MRtyp)) != 0)

    PROPSET(FIRE_RES, resist_from_form(MR_FIRE));
    PROPSET(COLD_RES, resist_from_form( MR_COLD));
    PROPSET(SLEEP_RES, resist_from_form(MR_SLEEP));
    PROPSET(DISINT_RES, resist_from_form(MR_DISINT));
    PROPSET(SHOCK_RES, resist_from_form(MR_ELEC));
    PROPSET(POISON_RES, resist_from_form(MR_POISON));
    PROPSET(ACID_RES, resist_from_form(MR_ACID));
    PROPSET(STONE_RES, resist_from_form(MR_STONE));
    {
        /* resists_drli() takes wielded weapon into account; suppress it */
        struct obj *save_uwep = uwep;

        uwep = 0;
        PROPSET(DRAIN_RES, resists_drli(&gy.youmonst));
        uwep = save_uwep;
    }
    /* resists_magm() takes wielded, worn, and carried equipment into
       into account; cheat and duplicate its monster-specific part */
    PROPSET(ANTIMAGIC, (dmgtype(mdat, AD_MAGM)
                        || mdat == &mons[PM_BABY_GRAY_DRAGON]
                        || dmgtype(mdat, AD_RBRE)));
    PROPSET(SICK_RES, (mdat->mlet == S_FUNGUS || mdat == &mons[PM_GHOUL]));

    PROPSET(STUNNED, (mdat == &mons[PM_STALKER] || is_bat(mdat)));
    PROPSET(HALLUC_RES, dmgtype(mdat, AD_HALU));
    PROPSET(SEE_INVIS, perceives(mdat));
    PROPSET(TELEPAT, telepathic(mdat));
    /* note that Infravision uses mons[race] rather than usual mons[role] */
    PROPSET(INFRAVISION, infravision(Upolyd ? mdat : &mons[gu.urace.mnum]));
    PROPSET(INVIS, pm_invisible(mdat));
    PROPSET(TELEPORT, can_teleport(mdat));
    PROPSET(TELEPORT_CONTROL, control_teleport(mdat));
    PROPSET(LEVITATION, is_floater(mdat));
    /* floating eye is the only 'floater'; it is also flagged as a 'flyer';
       suppress flying for it so that enlightenment doesn't confusingly
       show latent flight capability always blocked by levitation */
    PROPSET(FLYING, (is_flyer(mdat) && !is_floater(mdat)));
    PROPSET(SWIMMING, is_swimmer(mdat));
    /* [don't touch MAGICAL_BREATHING here; both Amphibious and Breathless
       key off of it but include different monster forms...] */
    PROPSET(PASSES_WALLS, passes_walls(mdat));
    PROPSET(REGENERATION, regenerates(mdat));
    PROPSET(REFLECTING, (mdat == &mons[PM_SILVER_DRAGON]));
    PROPSET(BLINDED, !haseyes(mdat));
    PROPSET(BLND_RES, (dmgtype_fromattack(mdat, AD_BLND, AT_EXPL)
                       || dmgtype_fromattack(mdat, AD_BLND, AT_GAZE)));
#undef PROPSET
#undef resist_from_form

    /* whether the player is flying/floating depends on their steed,
       which won't be known during the restore process: but BFlying
       and BStealth should be set correctly already in that case, so
       there's nothing to do */
    if (!program_state.restoring)
        float_vs_flight(); /* maybe toggle (BFlying & I_SPECIAL) */
    polysense();

#ifdef STATUS_HILITES
    if (VIA_WINDOWPORT())
        status_initialize(REASSESS_ONLY);
#endif
    /* we can reset this now, having just done what it is meant to trigger */
    gw.were_changes = 0L;
}

/* Levitation overrides Flying; set or clear BFlying|I_SPECIAL */
void
float_vs_flight(void)
{
    boolean stuck_in_floor = (u.utrap && u.utraptype != TT_PIT);

    /* floating overrides flight; so does being trapped in the floor */
    if ((HLevitation || ELevitation)
        || ((HFlying || EFlying) && stuck_in_floor))
        BFlying |= I_SPECIAL;
    else
        BFlying &= ~I_SPECIAL;
    /* being trapped on the ground (bear trap, web, molten lava survived
       with fire resistance, former lava solidified via cold, tethered
       to a buried iron ball) overrides floating--the floor is reachable */
    if ((HLevitation || ELevitation) && stuck_in_floor)
        BLevitation |= I_SPECIAL;
    else
        BLevitation &= ~I_SPECIAL;

    /* riding blocks stealth unless hero+steed fly, so a change in flying
       might cause a change in stealth */
    steed_vs_stealth();

    disp.botl = TRUE;
}

/* riding blocks stealth unless hero+steed fly */
void
steed_vs_stealth(void)
{
    if (u.usteed && !Flying && !Levitation)
        BStealth |= FROMOUTSIDE;
    else
        BStealth &= ~FROMOUTSIDE;
}

/* for changing into form that's immune to strangulation */
staticfn void
check_strangling(boolean on)
{
    /* on -- maybe resume strangling */
    if (on) {
        boolean was_strangled = (Strangled != 0L);

        /* when Strangled is already set, polymorphing from one
           vulnerable form into another causes the counter to be reset */
        if (uamul && uamul->otyp == AMULET_OF_STRANGULATION
            && can_be_strangled(&gy.youmonst)) {
            Strangled = 6L;
            disp.botl = TRUE;
#ifdef ZHLANG
            Your("%s%s了你的%s！", simpleonames(uamul),
                 was_strangled ? "仍在勒紧" : "开始勒紧",
                 body_part(NECK));
#else
            Your("%s %s your %s!", simpleonames(uamul),
                 was_strangled ? "still constricts" : "begins constricting",
                 body_part(NECK));
#endif
            makeknown(AMULET_OF_STRANGULATION);
        }

    /* off -- maybe block strangling */
    } else {
        if (Strangled && !can_be_strangled(&gy.youmonst)) {
            Strangled = 0L;
            disp.botl = TRUE;
#ifdef ZHLANG
            You("不再被勒颈了。");
#else
            You("are no longer being strangled.");
#endif
        }
    }
}

DISABLE_WARNING_FORMAT_NONLITERAL

/* make a (new) human out of the player */
staticfn void
polyman(const char *fmt, const char *arg)
{
    boolean sticking = (sticks(gy.youmonst.data) && u.ustuck && !u.uswallow),
            was_mimicking = (U_AP_TYPE != M_AP_NOTHING);
    boolean was_blind = !!Blind,
            had_see_invis = !!See_invisible;

    if (Upolyd) {
        u.acurr = u.macurr; /* restore old attribs */
        u.amax = u.mamax;
        u.umonnum = u.umonster;
        flags.female = u.mfemale;
    }
    set_uasmon();

    u.mh = u.mhmax = 0;
    u.mtimedone = 0;
    skinback(FALSE);
    u.uundetected = 0;

    if (sticking)
        uunstick();
    find_ac();
    if (was_mimicking) {
        if (gm.multi < 0)
            unmul("");
        gy.youmonst.m_ap_type = M_AP_NOTHING;
        gy.youmonst.mappearance = 0;
    }

    newsym(u.ux, u.uy);

    urgent_pline(fmt, arg);
    /* check whether player foolishly genocided self while poly'd */
    if (ugenocided()) {
        /* intervening activity might have clobbered genocide info */
        struct kinfo *kptr = find_delayed_killer(POLYMORPH);

        if (kptr != (struct kinfo *) 0 && kptr->name[0]) {
            svk.killer.format = kptr->format;
            Strcpy(svk.killer.name, kptr->name);
        } else {
            svk.killer.format = KILLED_BY;
            Strcpy(svk.killer.name, "self-genocide");
        }
        dealloc_killer(kptr);
        done(GENOCIDED);
    }

    if (!!See_invisible ^ had_see_invis)
        set_mimic_blocking(); /* See_invisible just toggled */

    if (u.twoweap && !could_twoweap(gy.youmonst.data))
        untwoweapon();

    if (u.utrap && u.utraptype == TT_PIT) {
        set_utrap(rn1(6, 2), TT_PIT); /* time to escape resets */
    }
    if (was_blind && !Blind) { /* reverting from eyeless */
        set_itimeout(&HBlinded, 1L);
        make_blinded(0L, TRUE); /* remove blindness */
    }
    check_strangling(TRUE);

    if (!Levitation && !u.ustuck && is_pool_or_lava(u.ux, u.uy))
        spoteffects(TRUE);

    see_monsters();
}

RESTORE_WARNING_FORMAT_NONLITERAL

void
change_sex(void)
{
    /* Some monsters are always of one sex and their sex can't be changed;
     * Succubi/incubi can change, but are handled below.
     *
     * !Upolyd check necessary because is_male() and is_female()
     * may be true for certain roles
     */
    if (!Upolyd
        || (!is_male(gy.youmonst.data) && !is_female(gy.youmonst.data)
            && !is_neuter(gy.youmonst.data)))
        flags.female = !flags.female;
    if (Upolyd) /* poly'd: also change saved sex */
        u.mfemale = !u.mfemale;
    max_rank_sz(); /* [this appears to be superfluous] */
    if ((Upolyd ? u.mfemale : flags.female) && gu.urole.name.f)
        Strcpy(svp.pl_character, gu.urole.name.f);
    else
        Strcpy(svp.pl_character, gu.urole.name.m);
    if (!Upolyd) {
        u.umonnum = u.umonster;
    } else if (u.umonnum == PM_AMOROUS_DEMON) {
        flags.female = !flags.female;
#if 0
        /* change monster type to match new sex; disabled with
           PM_AMOROUS_DEMON */
        u.umonnum = (u.umonnum == PM_SUCCUBUS) ? PM_INCUBUS : PM_SUCCUBUS;
#endif
        set_uasmon();
    }
}

/* log a message if non-poly'd hero's gender has changed */
void
livelog_newform(boolean viapoly, int oldgend, int newgend)
{
    char buf[BUFSZ];
    const char *oldrole, *oldrank, *newrole, *newrank;

    /*
     * TODO?
     *  Give other logging feedback here instead of in newman().
     */

    if (!Upolyd) {
        if (newgend != oldgend) {
            oldrole = (oldgend && gu.urole.name.f) ? gu.urole.name.f
                                                  : gu.urole.name.m;
            newrole = (newgend && gu.urole.name.f) ? gu.urole.name.f
                                                  : gu.urole.name.m;
            oldrank = rank_of(u.ulevel, Role_switch, oldgend);
            newrank = rank_of(u.ulevel, Role_switch, newgend);
            Sprintf(buf, "%.10s %.30s", genders[flags.female].adj, newrank);
            livelog_printf(LL_MINORAC, "%s into %s",
                           viapoly ? "polymorphed" : "transformed",
                           an(strcmp(newrole, oldrole) ? newrole
                              : strcmp(newrank, oldrank) ? newrank
                                : buf));
        }
    }
}

staticfn void
newman(void)
{
    const char *newform;
    int i, oldlvl, newlvl, oldgend, newgend, hpmax, enmax;

    oldlvl = u.ulevel;
    newlvl = oldlvl + rn1(5, -2);     /* new = old + {-2,-1,0,+1,+2} */
    if (newlvl > 127 || newlvl < 1) { /* level went below 0? */
        goto dead; /* old level is still intact (in case of lifesaving) */
    }
    if (newlvl > MAXULEV)
        newlvl = MAXULEV;
    /* If your level goes down, your peak level goes down by
       the same amount so that you can't simply use blessed
       full healing to undo the decrease.  But if your level
       goes up, your peak level does *not* undergo the same
       adjustment; you might end up losing out on the chance
       to regain some levels previously lost to other causes. */
    if (newlvl < oldlvl)
        u.ulevelmax -= (oldlvl - newlvl);
    if (u.ulevelmax < newlvl)
        u.ulevelmax = newlvl;
    u.ulevel = newlvl;

    oldgend = poly_gender();
    if (gs.sex_change_ok && !rn2(10))
        change_sex();

    adjabil(oldlvl, (int) u.ulevel);

    /* random experience points for the new experience level */
    u.uexp = rndexp(FALSE);

    /* set up new attribute points (particularly Con) */
    redist_attr();

    /*
     * New hit points:
     *  remove "level gain"-based HP from any extra HP accumulated
     *  (the "extra" might actually be negative);
     *  modify the extra, retaining {80%, 90%, 100%, or 110%};
     *  add in newly generated set of level-gain HP.
     *
     * (This used to calculate new HP in direct proportion to old HP,
     * but that was subject to abuse:  accumulate a large amount of
     * extra HP, drain level down to 1, then polyself to level 2 or 3
     * [lifesaving capability needed to handle level 0 and -1 cases]
     * and the extra got multiplied by 2 or 3.  Repeat the level
     * drain and polyself steps until out of lifesaving capability.)
     */
    hpmax = u.uhpmax;
    for (i = 0; i < oldlvl; i++)
        hpmax -= (int) u.uhpinc[i];
    /* hpmax * rn1(4,8) / 10; 0.95*hpmax on average */
    hpmax = rounddiv((long) hpmax * (long) rn1(4, 8), 10);
    for (i = 0; (u.ulevel = i) < newlvl; i++)
        hpmax += newhp();
    if (hpmax < u.ulevel)
        hpmax = u.ulevel; /* min of 1 HP per level */
    /* retain same proportion for current HP; u.uhp * hpmax / u.uhpmax */
    u.uhp = rounddiv((long) u.uhp * (long) hpmax, u.uhpmax);
    setuhpmax(hpmax, TRUE); /* might reduce u.uhp */
    /*
     * Do the same for spell power.
     */
    enmax = u.uenmax;
    for (i = 0; i < oldlvl; i++)
        enmax -= (int) u.ueninc[i];
    enmax = rounddiv((long) enmax * (long) rn1(4, 8), 10);
    for (i = 0; (u.ulevel = i) < newlvl; i++)
        enmax += newpw();
    if (enmax < u.ulevel)
        enmax = u.ulevel;
    u.uen = rounddiv((long) u.uen * (long) enmax,
                     ((u.uenmax < 1) ? 1 : u.uenmax));
    u.uenmax = enmax;
    /* [should alignment record be tweaked too?] */

    u.uhunger = rn1(500, 500);
    if (Sick)
        make_sick(0L, (char *) 0, FALSE, SICK_ALL);
    if (Stoned)
        make_stoned(0L, (char *) 0, 0, (char *) 0);
    if (u.uhp <= 0) {
        if (Polymorph_control) { /* even when Stunned || Unaware */
            if (u.uhp <= 0)
                u.uhp = 1;
        } else {
 dead:      /* we come directly here if experience level went to 0 or less */
            urgent_pline(
                     "Your new form doesn't seem healthy enough to survive.");
            svk.killer.format = KILLED_BY_AN;
            Strcpy(svk.killer.name, "unsuccessful polymorph");
            done(DIED);
            /* must have been life-saved to get here */
            newuhs(FALSE);
            encumber_msg(); /* used to be done by redist_attr() */
            return; /* lifesaved */
        }
    }
    newuhs(FALSE);
    /* use saved gender we're about to revert to, not current */
    newform = ((Upolyd ? u.mfemale : flags.female) && gu.urace.individual.f)
                ? gu.urace.individual.f
                : (gu.urace.individual.m)
                   ? gu.urace.individual.m
                   : gu.urace.noun;
    polyman("You feel like a new %s!", newform);

    newgend = poly_gender();
    /* note: newman() bypasses achievements for new ranks attained and
       doesn't log "new <form>" when that isn't accompanied by level change */
    if (newlvl != oldlvl)
        livelog_printf(LL_MINORAC, "became experience level %d as a new %s",
                       newlvl, newform);
    else
        livelog_newform(TRUE, oldgend, newgend);

    if (Slimed) {
#ifdef ZHLANG
        Your("身体变形了，但你身上还有黏液。");
#else
        Your("body transforms, but there is still slime on you.");
#endif
        make_slimed(10L, (const char *) 0);
    }

    disp.botl = TRUE;
    see_monsters();
    encumber_msg();

    retouch_equipment(2);
    if (!uarmg)
        selftouch(no_longer_petrify_resistant);
}

void
polyself(int psflags)
{
    char buf[BUFSZ];
    int old_light, new_light, mntmp, class, tryct, gvariant = NEUTRAL;
    boolean forcecontrol = ((psflags & POLY_CONTROLLED) != 0),
            low_control = ((psflags & POLY_LOW_CTRL) != 0),
            monsterpoly = ((psflags & POLY_MONSTER) != 0),
            formrevert = ((psflags & POLY_REVERT) != 0),
            draconian = (uarm && Is_dragon_armor(uarm)),
            iswere = (ismnum(u.ulycn)),
            isvamp = (is_vampire(gy.youmonst.data)
                      || is_vampshifter(&gy.youmonst)),
            controllable_poly = Polymorph_control && !(Stunned || Unaware);

    if (Unchanging) {
#ifdef ZHLANG
        You("变形失败！");
#else
        You("fail to transform!");
#endif
        return;
    }
    /* being Stunned|Unaware doesn't negate this aspect of Poly_control */
    if (!Polymorph_control && !forcecontrol && !draconian && !iswere
        && !isvamp) {
        if (rn2(20) > ACURR(A_CON)) {
            You1(shudder_for_moment);
            losehp(rnd(30), "system shock", KILLED_BY_AN);
            exercise(A_CON, FALSE);
            return;
        }
    }
    old_light = emits_light(gy.youmonst.data);
    mntmp = NON_PM;

    if (formrevert) {
        mntmp = gy.youmonst.cham;
        monsterpoly = TRUE;
        controllable_poly = FALSE;
    }

    if (forcecontrol && low_control
        && (draconian || monsterpoly || isvamp || iswere))
        forcecontrol = FALSE;

    if (monsterpoly && isvamp)
        goto do_vampyr;

    if (controllable_poly || forcecontrol) {
        buf[0] = '\0';
        tryct = 5;

        do {
            mntmp = NON_PM;
            getlin("Become what kind of monster? [type the name]", buf);
            (void) mungspaces(buf);
            if (*buf == '\033') {
                /* user is cancelling controlled poly */
                if (forcecontrol) { /* wizard mode #polyself */
                    pline1(Never_mind);
                    return;
                }
                Strcpy(buf, "*"); /* resort to random */
            }
            if (!strcmp(buf, "*") || !strcmp(buf, "random")) {
                /* explicitly requesting random result */
                tryct = 0; /* will skip thats_enough_tries */
                continue;  /* end do-while(--tryct > 0) loop */
            }
            class = 0;
            mntmp = name_to_mon(buf, &gvariant);
            if (mntmp < LOW_PM) {
 by_class:
                class = name_to_monclass(buf, &mntmp);
                if (class && mntmp == NON_PM)
                    mntmp = (draconian && class == S_DRAGON)
                            ? armor_to_dragon(uarm->otyp)
                            : mkclass_poly(class);

            /* placeholder monsters are for corpses and all flagged
               M2_NOPOLY but they are reasonable polymorph targets;
               pick a suitable substitute (which might be geno'd) */
            } else if (is_placeholder(&mons[mntmp])
                       /* when your own race, fall to !polyok() case */
                       && !your_race(&mons[mntmp])
                       /* same for generic human, even if hero isn't human */
                       && mntmp != PM_HUMAN) {
                /* far less general than mkclass() */
                if (mntmp == PM_ORC)
                    mntmp = rn2(3) ? PM_HILL_ORC : PM_MORDOR_ORC;
                else if (mntmp == PM_ELF)
                    mntmp = rn2(3) ? PM_GREEN_ELF : PM_GREY_ELF;
                else if (mntmp == PM_GIANT)
                    mntmp = rn2(3) ? PM_STONE_GIANT : PM_HILL_GIANT;
                /* note: PM_DWARF and PM_GNOME are ordinary monsters and
                   no longer flagged no-poly so have no need for placeholder
                   handling; PM_HUMAN is a placeholder without a suitable
                   substitute so gets handled differently below */
            }

            if (mntmp < LOW_PM) {
                if (!class)
#ifdef ZHLANG
                    pline("我从未听说过这种怪物。");
#else
                    pline("I've never heard of such monsters.");
#endif
                else
#ifdef ZHLANG
                    You_cant("无法变形为任何那些生物。");
#else
                    You_cant("polymorph into any of those.");
#endif
            } else if (wizard && Upolyd
                       && (mntmp == u.umonster
                           /* "priest" and "priestess" match the monster
                              rather than the role; override that unless
                              the text explicitly contains "aligned" */
                           || (u.umonster == PM_CLERIC
                               && mntmp == PM_ALIGNED_CLERIC
                               && !strstri(buf, "aligned")))) {
                /* in wizard mode, picking own role while poly'd reverts to
                   normal without newman()'s chance of level or sex change */
                rehumanize();
                old_light = 0; /* rehumanize() extinguishes u-as-mon light */
                goto made_change;
            } else if (iswere && (were_beastie(mntmp) == u.ulycn
                                  || mntmp == counter_were(u.ulycn)
                                  || (Upolyd && mntmp == PM_HUMAN))) {
                goto do_shift;
            } else if (!polyok(&mons[mntmp])
                       /* Note:  humans are illegal as monsters, but an
                          illegal monster forces newman(), which is what
                          we want if they specified a human.... (unless
                          they specified a unique monster) */
                       && !(mntmp == PM_HUMAN
                            || (your_race(&mons[mntmp])
                                && (mons[mntmp].geno & G_UNIQ) == 0)
                            || mntmp == gu.urole.mnum)) {
                const char *pm_name;

                /* mkclass_poly() can pick a !polyok()
                   candidate; if so, usually try again */
                if (class) {
                    if (rn2(3) || --tryct > 0)
                        goto by_class;
                    /* no retries left; put one back on counter
                       so that end of loop decrement will yield
                       0 and trigger thats_enough_tries message */
                    ++tryct;
                }
                pm_name = pmname(&mons[mntmp], flags.female ? FEMALE : MALE);
                if (the_unique_pm(&mons[mntmp]))
                    pm_name = the(pm_name);
                else if (!type_is_pname(&mons[mntmp]))
                    pm_name = an(pm_name);
#ifdef ZHLANG
                You_cant("无法变形为%s。", pm_name);
#else
                You_cant("polymorph into %s.", pm_name);
#endif
            } else
                break;
        } while (--tryct > 0);

        if (!tryct)
            pline1(thats_enough_tries);
        /* allow skin merging, even when polymorph is controlled */
        if (draconian && (tryct <= 0 || mntmp == armor_to_dragon(uarm->otyp)))
            goto do_merge;
        if (isvamp && (tryct <= 0 || mntmp == PM_WOLF || mntmp == PM_FOG_CLOUD
                       || is_bat(&mons[mntmp])))
            goto do_vampyr;
    } else if (draconian || iswere || isvamp) {
        /* special changes that don't require polyok() */
        if (draconian) {
 do_merge:
            mntmp = armor_to_dragon(uarm->otyp);
            if (!(svm.mvitals[mntmp].mvflags & G_GENOD)) {
                unsigned was_lit = uarm->lamplit;
                int arm_light = artifact_light(uarm) ? arti_light_radius(uarm)
                                                     : 0;

                /* allow G_EXTINCT */
                if (Is_dragon_scales(uarm)) {
                    /* dragon scales remain intact as uskin */
#ifdef ZHLANG
                    You("与你的鳞甲融合了。");
#else
                    You("merge with your scaly armor.");
#endif
                } else { /* dragon scale mail reverts to scales */
                    /* similar to noarmor(invent.c),
                       shorten to "<color> scale mail" */
                    Strcpy(buf, simpleonames(uarm));
                    strsubst(buf, " dragon ", " ");
                    /* tricky phrasing; dragon scale mail is singular, dragon
                       scales are plural (note: we don't use "set of scales",
                       which usually overrides the distinction, here) */
#ifdef ZHLANG
                    Your("%s在你融合时变回了鳞片。", buf);
#else
                    Your("%s reverts to scales as you merge with them.", buf);
#endif
                    /* uarm->spe enchantment remains unchanged;
                       re-converting scales to mail poses risk
                       of evaporation due to over enchanting */
                    uarm->otyp += GRAY_DRAGON_SCALES - GRAY_DRAGON_SCALE_MAIL;
                    observe_object(uarm);
                    disp.botl = TRUE; /* AC is changing */
                }
                uskin = uarm;
                uarm = (struct obj *) 0;
                /* save/restore hack */
                uskin->owornmask |= I_SPECIAL;
                if (was_lit)
                    maybe_adjust_light(uskin, arm_light);
                update_inventory();
            }
        } else if (iswere) {
 do_shift:
            if (Upolyd && were_beastie(mntmp) != u.ulycn)
                mntmp = PM_HUMAN; /* Illegal; force newman() */
            else
                mntmp = u.ulycn;
        } else if (isvamp) {
 do_vampyr:
            if (mntmp < LOW_PM || (mons[mntmp].geno & G_UNIQ)) {
                mntmp = (gy.youmonst.data == &mons[PM_VAMPIRE_LEADER]
                         && !rn2(10)) ? PM_WOLF
                                      : !rn2(4) ? PM_FOG_CLOUD
                                                : PM_VAMPIRE_BAT;
                if (ismnum(gy.youmonst.cham)
                    && !is_vampire(gy.youmonst.data) && !rn2(2))
                    mntmp = gy.youmonst.cham;
            }
            if (controllable_poly) {
                Sprintf(buf, "Become %s?",
                        an(pmname(&mons[mntmp], gvariant)));
                if (y_n(buf) != 'y')
                    return;
            }
        }
        /* if polymon fails, "you feel" message has been given
           so don't follow up with another polymon or newman;
           sex_change_ok left disabled here */
        if (mntmp == PM_HUMAN)
            newman(); /* werecritter */
        else
            (void) polymon(mntmp);
        goto made_change; /* maybe not, but this is right anyway */
    }

    if (mntmp < LOW_PM) {
        tryct = 200;
        do {
            /* randomly pick an "ordinary" monster */
            mntmp = rn1(SPECIAL_PM - LOW_PM, LOW_PM);
            if (polyok(&mons[mntmp]) && !is_placeholder(&mons[mntmp]))
                break;
        } while (--tryct > 0);
    }

    /* The below polyok() fails either if everything is genocided, or if
     * we deliberately chose something illegal to force newman().
     */
    gs.sex_change_ok++;
    if (!polyok(&mons[mntmp]) || (!forcecontrol && !rn2(5))
        || your_race(&mons[mntmp])) {
        newman();
    } else {
        (void) polymon(mntmp);
    }
    gs.sex_change_ok--; /* reset */

 made_change:
    new_light = emits_light(gy.youmonst.data);
    if (old_light != new_light) {
        if (old_light)
            del_light_source(LS_MONSTER, monst_to_any(&gy.youmonst));
        if (new_light == 1)
            ++new_light; /* otherwise it's undetectable */
        if (new_light)
            new_light_source(u.ux, u.uy, new_light, LS_MONSTER,
                             monst_to_any(&gy.youmonst));
    }
}

/* (try to) make a mntmp monster out of the player; return 1 if successful */
int
polymon(int mntmp)
{
    char buf[BUFSZ], ustuckNam[BUFSZ];
    boolean sticking = sticks(gy.youmonst.data) && u.ustuck && !u.uswallow,
            was_blind = !!Blind, dochange = FALSE, was_expelled = FALSE,
            was_hiding_under = u.uundetected && hides_under(gy.youmonst.data);
    int mlvl, newMaxStr;

    if (svm.mvitals[mntmp].mvflags & G_GENOD) { /* allow G_EXTINCT */
#ifdef ZHLANG
        You_feel("有点%s的样子。",
                 pmname(&mons[mntmp], flags.female ? FEMALE : MALE));
#else
        You_feel("rather %s-ish.",
                 pmname(&mons[mntmp], flags.female ? FEMALE : MALE));
#endif
        exercise(A_WIS, TRUE);
        return 0;
    }

    /* KMH, conduct */
    if (!u.uconduct.polyselfs++)
        livelog_printf(LL_CONDUCT,
                       "changed form for the first time, becoming %s",
                       an(pmname(&mons[mntmp], flags.female ? FEMALE : MALE)));

    /* exercise used to be at the very end but only Wis was affected
       there since the polymorph was always in effect by then */
    exercise(A_CON, FALSE);
    exercise(A_WIS, TRUE);

    if (!Upolyd) {
        /* Human to monster; save human stats */
        u.macurr = u.acurr;
        u.mamax = u.amax;
        u.mfemale = flags.female;
    } else {
        /* Monster to monster; restore human stats, to be
         * immediately changed to provide stats for the new monster
         */
        u.acurr = u.macurr;
        u.amax = u.mamax;
        flags.female = u.mfemale;
    }

    /* if stuck mimicking gold, stop immediately */
    if (gm.multi < 0 && U_AP_TYPE == M_AP_OBJECT
        && gy.youmonst.data->mlet != S_MIMIC)
        unmul("");
    /* if becoming a non-mimic, stop mimicking anything */
    if (mons[mntmp].mlet != S_MIMIC) {
        /* as in polyman() */
        gy.youmonst.m_ap_type = M_AP_NOTHING;
        gy.youmonst.mappearance = 0;
    }
    if (is_male(&mons[mntmp])) {
        if (flags.female)
            dochange = TRUE;
    } else if (is_female(&mons[mntmp])) {
        if (!flags.female)
            dochange = TRUE;
    } else if (!is_neuter(&mons[mntmp]) && mntmp != u.ulycn) {
        if (gs.sex_change_ok && !rn2(10))
            dochange = TRUE;
    }

    Strcpy(ustuckNam, u.ustuck ? Some_Monnam(u.ustuck) : "");

    Strcpy(buf, (u.umonnum != mntmp) ? "" : "new ");
    if (dochange) {
        flags.female = !flags.female;
        Strcat(buf, (is_male(&mons[mntmp]) || is_female(&mons[mntmp]))
                       ? "" : flags.female ? "female " : "male ");
    }
    Strcat(buf, pmname(&mons[mntmp], flags.female ? FEMALE : MALE));
#ifdef ZHLANG
    You("%s%s！", (u.umonnum != mntmp) ? "变成了" : "感觉自己像", an(buf));
#else
    You("%s %s!", (u.umonnum != mntmp) ? "turn into" : "feel like", an(buf));
#endif

    if (Stoned && poly_when_stoned(&mons[mntmp])) {
        /* poly_when_stoned already checked stone golem genocide */
        mntmp = PM_STONE_GOLEM;
        make_stoned(0L, "You turn to stone!", 0, (char *) 0);
    }

    u.mtimedone = rn1(500, 500);
    u.umonnum = mntmp;
    set_uasmon();

    /* New stats for monster, to last only as long as polymorphed.
     * Currently only strength gets changed.
     */
    newMaxStr = uasmon_maxStr();
    if (strongmonst(&mons[mntmp])) {
        ABASE(A_STR) = AMAX(A_STR) = (schar) newMaxStr;
    } else {
        /* not a strongmonst(); if hero has exceptional strength, remove it
           (note: removal is temporary until returning to original form);
           we don't attempt to enforce lower maximum for wimpy forms;
           unlike for strongmonst, current strength does not get set to max */
        AMAX(A_STR) = (schar) newMaxStr;
        /* make sure current is not higher than max (strip exceptional Str) */
        if (ABASE(A_STR) > AMAX(A_STR))
            ABASE(A_STR) = AMAX(A_STR);
    }

    if (Stone_resistance && Stoned) { /* parnes@eniac.seas.upenn.edu */
        make_stoned(0L, "You no longer seem to be petrifying.", 0,
                    (char *) 0);
    }
    if (Sick_resistance && Sick) {
        make_sick(0L, (char *) 0, FALSE, SICK_ALL);
#ifdef ZHLANG
        You("不再感到恶心。");
#else
        You("no longer feel sick.");
#endif
    }
    if (Slimed) {
        if (flaming(gy.youmonst.data)) {
            make_slimed(0L, "The slime burns away!");
        } else if (mntmp == PM_GREEN_SLIME) {
            /* do it silently */
            make_slimed(0L, (char *) 0);
        }
    }
    check_strangling(FALSE); /* maybe stop strangling */
    if (nohands(gy.youmonst.data))
        make_glib(0);

    /*
    mlvl = adj_lev(&mons[mntmp]);
     * We can't do the above, since there's no such thing as an
     * "experience level of you as a monster" for a polymorphed character.
     */
    mlvl = (int) mons[mntmp].mlevel;
    if (gy.youmonst.data->mlet == S_DRAGON && mntmp >= PM_GRAY_DRAGON) {
        u.mhmax = In_endgame(&u.uz) ? (8 * mlvl) : (4 * mlvl + d(mlvl, 4));
    } else if (is_golem(gy.youmonst.data)) {
        u.mhmax = golemhp(mntmp);
    } else {
        if (!mlvl)
            u.mhmax = rnd(4);
        else
            u.mhmax = d(mlvl, 8);
        if (is_home_elemental(&mons[mntmp]))
            u.mhmax *= 3;
    }
    u.mh = u.mhmax;

    if (u.ulevel < mlvl) {
        /* Low level characters can't become high level monsters for long */
#ifdef DUMB
        /* DRS/NS 2.2.6 messes up -- Peter Kendell */
        int mtd = u.mtimedone, ulv = u.ulevel;

        u.mtimedone = mtd * ulv / mlvl;
#else
        u.mtimedone = u.mtimedone * u.ulevel / mlvl;
#endif
    }

    if (uskin && mntmp != armor_to_dragon(uskin->otyp))
        skinback(FALSE);
    break_armor();
    drop_weapon(1);
    find_ac(); /* (repeated below) */
    /* if hiding under something and can't hide anymore, unhide now;
       but don't auto-hide when not already hiding-under */
    if (was_hiding_under)
        (void) hideunder(&gy.youmonst);

    if (u.utrap && u.utraptype == TT_PIT) {
        set_utrap(rn1(6, 2), TT_PIT); /* time to escape resets */
    }
    if (was_blind && !Blind) { /* previous form was eyeless */
        set_itimeout(&HBlinded, 1L);
        make_blinded(0L, TRUE); /* remove blindness */
    }
    newsym(u.ux, u.uy); /* Change symbol */

    /* you now know what an egg of your type looks like; [moved from
       below in case expels() -> spoteffects() drops hero onto any eggs] */
    if (lays_eggs(gy.youmonst.data)) {
        learn_egg_type(u.umonnum);
        /* make queen bees recognize killer bee eggs */
        learn_egg_type(egg_type_from_parent(u.umonnum, TRUE));
    }

    if (u.uswallow) {
        uchar usiz;

        /* if new form can't be swallowed, make engulfer expel hero */
        if (unsolid(gy.youmonst.data)
            /* subset of engulf_target() */
            || (usiz = gy.youmonst.data->msize) >= MZ_HUGE
            || (u.ustuck->data->msize < usiz && !is_whirly(u.ustuck->data))) {
            boolean expels_mesg = TRUE;

            if (unsolid(gy.youmonst.data)) {
                if (canspotmon(u.ustuck)) /* [see below for explanation] */
                    Strcpy(ustuckNam, Monnam(u.ustuck));
#ifdef ZHLANG
                pline("%s无法再容纳你了。", ustuckNam);
#else
                pline("%s can no longer contain you.", ustuckNam);
#endif
                expels_mesg = FALSE;
            }
            expels(u.ustuck, u.ustuck->data, expels_mesg);
            was_expelled = TRUE;
            /* FIXME? if expels() triggered rehumanize then we should
               return early */
        }

    /* [note:  this 'sticking' handling is only sufficient for changing from
       grabber to engulfer or vice versa because engulfing by poly'd hero
       always ends immediately so won't be in effect during a polymorph] */
    } else if (u.ustuck && !sticking /* && !u.uswallow */
               /* being held; if now capable of holding, make holder
                  release so that hero doesn't automagically start holding
                  it; or, release if no longer capable of being held */
               && (sticks(gy.youmonst.data) || unsolid(gy.youmonst.data))) {
        /* u.ustuck name was saved above in case we're changing from can-see
           to can't-see; but might have changed from can't-see to can-see so
           override here if hero knows who u.ustuck is */
        if (canspotmon(u.ustuck))
            Strcpy(ustuckNam, Monnam(u.ustuck));
        set_ustuck((struct monst *) 0);
#ifdef ZHLANG
        pline("%s松开了对你的抓握。", ustuckNam);
#else
        pline("%s loses its grip on you.", ustuckNam);
#endif
    } else if (sticking && !sticks(gy.youmonst.data)) {
        /* was holding onto u.ustuck but no longer capable of that */
        uunstick();
    }

    if (u.usteed) {
        if (touch_petrifies(u.usteed->data) && !Stone_resistance && rnl(3)) {
#ifdef ZHLANG
            pline("%s触碰了%s。", no_longer_petrify_resistant,
                  mon_nam(u.usteed));
#else
            pline("%s touch %s.", no_longer_petrify_resistant,
                  mon_nam(u.usteed));
#endif
            Sprintf(buf, "riding %s",
                    an(pmname(u.usteed->data, Mgender(u.usteed))));
            instapetrify(buf);
        }
        if (!can_ride(u.usteed))
            dismount_steed(DISMOUNT_POLY);
    }

    find_ac();
    if (((!Levitation && !u.ustuck && !Flying && is_pool_or_lava(u.ux, u.uy))
         || (Underwater && !Swimming))
        /* if expelled above, expels() already called spoteffects() */
        && !was_expelled) {
        spoteffects(TRUE);
        /* FIXME? if spoteffects() triggered rehumanize then we should
           return early */
    }
    if (Passes_walls && u.utrap
        && (u.utraptype == TT_INFLOOR || u.utraptype == TT_BURIEDBALL)) {
        if (u.utraptype == TT_INFLOOR) {
#ifdef ZHLANG
            pline_The("岩石似乎不再困住你了。");
#else
            pline_The("rock seems to no longer trap you.");
#endif
        } else {
#ifdef ZHLANG
            pline_The("埋藏的铁球不再束缚你了。");
#else
            pline_The("buried ball is no longer bound to you.");
#endif
            buried_ball_to_freedom();
        }
        reset_utrap(TRUE);
    } else if (likes_lava(gy.youmonst.data) && u.utrap
               && u.utraptype == TT_LAVA) {
#ifdef ZHLANG
        pline_The("%s现在感觉舒适。", hliquid("lava"));
#else
        pline_The("%s now feels soothing.", hliquid("lava"));
#endif
        reset_utrap(TRUE);
    }
    if (amorphous(gy.youmonst.data) || is_whirly(gy.youmonst.data)
        || unsolid(gy.youmonst.data)) {
        if (Punished) {
#ifdef ZHLANG
            You("从铁链中滑出。");
#else
            You("slip out of the iron chain.");
#endif
            unpunish();
        } else if (u.utrap && u.utraptype == TT_BURIEDBALL) {
#ifdef ZHLANG
            You("从埋藏的铁球和锁链中挣脱。");
#else
            You("slip free of the buried ball and chain.");
#endif
            buried_ball_to_freedom();
        }
    }
    if (u.utrap && (u.utraptype == TT_WEB || u.utraptype == TT_BEARTRAP)
        && (amorphous(gy.youmonst.data) || is_whirly(gy.youmonst.data)
            || unsolid(gy.youmonst.data)
            || (gy.youmonst.data->msize <= MZ_SMALL
                && u.utraptype == TT_BEARTRAP))) {
#ifdef ZHLANG
        You("不再被困在%s中了。",
            u.utraptype == TT_WEB ? "蛛网" : "捕熊夹");
#else
        You("are no longer stuck in the %s.",
            u.utraptype == TT_WEB ? "web" : "bear trap");
#endif
        /* probably should burn webs too if PM_FIRE_ELEMENTAL */
        reset_utrap(TRUE);
    }
    if (webmaker(gy.youmonst.data) && u.utrap && u.utraptype == TT_WEB) {
#ifdef ZHLANG
        You("在蛛网上调整了方向。");
#else
        You("orient yourself on the web.");
#endif
        reset_utrap(TRUE);
    }
    check_strangling(TRUE); /* maybe start strangling */

    disp.botl = TRUE;
    gv.vision_full_recalc = 1;
    see_monsters();
    encumber_msg();

    retouch_equipment(2);
    /* this might trigger a recursive call to polymon() [stone golem
       wielding cockatrice corpse and hit by stone-to-flesh, becomes
       flesh golem above, now gets transformed back into stone golem;
       fortunately neither form uses #monster] */
    if (!uarmg)
        selftouch(no_longer_petrify_resistant);

    /* the explanation of '#monster' used to be shown sooner, but there are
       possible fatalities above and it isn't useful unless hero survives */
    if (flags.verbose) {
        static const char use_thec[] = "Use the command #%s to %s.";
        static const char monsterc[] = "monster";
        struct permonst *uptr = gy.youmonst.data;
        boolean might_hide = (is_hider(uptr) || hides_under(uptr));

        if (can_breathe(uptr))
#ifdef ZHLANG
            pline(use_thec, monsterc, "使用你的喷吐武器");
#else
            pline(use_thec, monsterc, "use your breath weapon");
#endif
        if (attacktype(uptr, AT_SPIT))
#ifdef ZHLANG
            pline(use_thec, monsterc, "吐毒液");
#else
            pline(use_thec, monsterc, "spit venom");
#endif
        if (uptr->mlet == S_NYMPH)
#ifdef ZHLANG
            pline(use_thec, monsterc, "移除铁球");
#else
            pline(use_thec, monsterc, "remove an iron ball");
#endif
        if (attacktype(uptr, AT_GAZE))
#ifdef ZHLANG
            pline(use_thec, monsterc, "凝视怪物");
#else
            pline(use_thec, monsterc, "gaze at monsters");
#endif
        if (might_hide && webmaker(uptr))
#ifdef ZHLANG
            pline(use_thec, monsterc, "躲藏或织网");
#else
            pline(use_thec, monsterc, "hide or to spin a web");
#endif
        else if (might_hide)
#ifdef ZHLANG
            pline(use_thec, monsterc, "躲藏");
#else
            pline(use_thec, monsterc, "hide");
#endif
        else if (webmaker(uptr))
#ifdef ZHLANG
            pline(use_thec, monsterc, "织网");
#else
            pline(use_thec, monsterc, "spin a web");
#endif
        if (is_were(uptr))
#ifdef ZHLANG
            pline(use_thec, monsterc, "召唤帮助");
#else
            pline(use_thec, monsterc, "summon help");
#endif
        if (u.umonnum == PM_GREMLIN)
#ifdef ZHLANG
            pline(use_thec, monsterc, "在喷泉中繁殖");
#else
            pline(use_thec, monsterc, "multiply in a fountain");
#endif
        if (is_unicorn(uptr))
#ifdef ZHLANG
            pline(use_thec, monsterc, "使用你的角");
#else
            pline(use_thec, monsterc, "use your horn");
#endif
        if (is_mind_flayer(uptr))
#ifdef ZHLANG
            pline(use_thec, monsterc, "发出精神冲击");
#else
            pline(use_thec, monsterc, "emit a mental blast");
#endif
        if (uptr->msound == MS_SHRIEK) /* worthless, actually */
#ifdef ZHLANG
            pline(use_thec, monsterc, "尖叫");
#else
            pline(use_thec, monsterc, "shriek");
#endif
        if (is_vampire(uptr) || is_vampshifter(&gy.youmonst))
#ifdef ZHLANG
            pline(use_thec, monsterc, "变形");
#else
            pline(use_thec, monsterc, "change shape");
#endif

        if (lays_eggs(uptr) && flags.female
            && !(uptr == &mons[PM_GIANT_EEL]
                 || uptr == &mons[PM_ELECTRIC_EEL]))
#ifdef ZHLANG
            pline(use_thec, "sit",
                  eggs_in_water(uptr) ? "在水中产卵" : "产蛋");
#else
            pline(use_thec, "sit",
                  eggs_in_water(uptr) ? "spawn in the water" : "lay an egg");
#endif
    }
    return 1;
}

/* determine hero's temporary strength value used while polymorphed;
   hero poly'd into M2_STRONG monster usually gets 18/100 strength but
   there are exceptions; non-M2_STRONG get maximum strength set to 18 */
schar
uasmon_maxStr(void)
{
    const struct Race *R;
    int newMaxStr;
    int mndx = u.umonnum;
    struct permonst *ptr = &mons[mndx];

    if (is_orc(ptr)) {
        if (mndx != PM_URUK_HAI && mndx != PM_ORC_CAPTAIN)
            mndx = PM_ORC;
    } else if (is_elf(ptr)) {
        mndx = PM_ELF;
    } else if (is_dwarf(ptr)) {
        mndx = PM_DWARF;
    } else if (is_gnome(ptr)) {
        mndx = PM_GNOME;
#if 0   /* use the mons[] value for humans */
    } else if (is_human(ptr)) {
        mndx = PM_HUMAN;
#endif
    }
    R = character_race(mndx);

    if (strongmonst(ptr)) {
        /* ettins, titans and minotaurs don't pass the is_giant() test;
           giant mummies and giant zombies do but we throttle those */
        boolean live_H = is_giant(ptr) && !is_undead(ptr);

        /* hero orcs are limited to 18/50 for maximum strength, so treat
           hero poly'd into an orc the same; goblins, orc shamans, and orc
           zombies don't have strongmonst() attribute so won't get here;
           hobgoblins and orc mummies do get here and are limited to 18/50
           like normal orcs; however, orc captains and Uruk-hai retain 18/100
           strength; hero gnomes are also limited to 18/50; hero elves are
           limited to 18/00 regardless of whether they're strongmonst, but
           the two strongmonst types (monarchs and nobles) have current
           strength set to 18 [by polymon()], the others don't */
        newMaxStr = R ? R->attrmax[A_STR] : live_H ? STR19(19) : STR18(100);
    } else {
        newMaxStr = R ? R->attrmax[A_STR] : 18; /* 18 is same as STR18(0) */
    }
    return (schar) newMaxStr;
}

/* dropx() jacket for break_armor() */
staticfn void
dropp(struct obj *obj)
{
    struct obj *otmp;

    /*
     * Dropping worn armor while polymorphing might put hero into water
     * (loss of levitation boots or water walking boots that the new
     * form can't wear), where emergency_disrobe() could remove it from
     * inventory.  Without this, dropx() could trigger an 'object lost'
     * panic.  Right now, boots are the only armor which might encounter
     * this situation, but handle it for all armor.
     *
     * Hypothetically, 'obj' could have merged with something (not
     * applicable for armor) and no longer be a valid pointer, so scan
     * inventory for it instead of trusting obj->where.
     */
    for (otmp = gi.invent; otmp; otmp = otmp->nobj) {
        if (otmp == obj) {
            dropx(obj);
            /* Note that otmp->nobj is pointing at fobj now,
             * as a result of:
             * dropx() -> dropy() -> dropz() -> place_object(),
             * and no longer pointing at the next obj in inventory.
             * That would be an issue if this loop were allowed
             * to continue, but the break statement that
             * follows prevents the loop from continuing on with
             * objects on the floor.
             */
            break;
        }
    }
}

staticfn void
break_armor(void)
{
    struct obj *otmp;
    struct permonst *uptr = gy.youmonst.data;

    if (breakarm(uptr)) {
        if ((otmp = uarm) != 0) {
            if (donning(otmp))
                cancel_don();
            /* for gold DSM, we don't want Armor_gone() to report that it
               stops shining _after_ we've been told that it is destroyed */
            if (otmp->lamplit)
                end_burn(otmp, FALSE);

#ifdef ZHLANG
            You("挣脱了你的盔甲！");
#else
            You("break out of your armor!");
#endif
            exercise(A_STR, FALSE);
            (void) Armor_gone();
            useup(otmp);
        }
        if ((otmp = uarmc) != 0
            /* mummy wrapping adapts to small and very big sizes */
            && (otmp->otyp != MUMMY_WRAPPING || !WrappingAllowed(uptr))) {
            if (otmp->otyp == MUMMY_WRAPPING) {
                /* doesn't have a clasp to break open */
#ifdef ZHLANG
                Your("%s被撕碎了！", cloak_simple_name(otmp));
#else
                Your("%s tears apart!", cloak_simple_name(otmp));
#endif
                (void) Cloak_off();
                useup(otmp);
            } else if (otmp->otyp == ALCHEMY_SMOCK) {
#ifdef ZHLANG
                pline_The("你的%s上的结被拉开了！", cloak_simple_name(otmp));
#else
                pline_The("knot on your %s is pulled apart!", cloak_simple_name(otmp));
#endif
                (void) Cloak_off();
                dropp(otmp);
            } else {
#ifdef ZHLANG
                pline_The("你的%s上的扣子崩开了！", cloak_simple_name(otmp));
#else
                pline_The("clasp on your %s breaks open!", cloak_simple_name(otmp));
#endif
                (void) Cloak_off();
                dropp(otmp);
            }
        }
        if (uarmu) {
#ifdef ZHLANG
            Your("衬衫撕成了碎片！");
#else
            Your("shirt rips to shreds!");
#endif
            useup(uarmu);
        }
    } else if (sliparm(uptr)) {
        if ((otmp = uarm) != 0 && racial_exception(&gy.youmonst, otmp) < 1) {
            if (donning(otmp))
                cancel_don();
#ifdef ZHLANG
            Your("盔甲从你身边脱落！");
#else
            Your("armor falls around you!");
#endif
            /* [note: _gone() instead of _off() dates to when life-saving
               could force fire resisting armor back on if hero burned in
               hell (3.0, predating Gehennom); the armor isn't actually
               gone here but also isn't available to be put back on] */
            (void) Armor_gone();
            dropp(otmp);
        }
        if ((otmp = uarmc) != 0
            /* mummy wrapping adapts to small and very big sizes */
            && (otmp->otyp != MUMMY_WRAPPING || !WrappingAllowed(uptr))) {
            if (is_whirly(uptr))
#ifdef ZHLANG
                Your("%s因无支撑而掉落！", cloak_simple_name(otmp));
#else
                Your("%s falls, unsupported!", cloak_simple_name(otmp));
#endif
            else
#ifdef ZHLANG
                You("缩小并从%s中滑出！", cloak_simple_name(otmp));
#else
                You("shrink out of your %s!", cloak_simple_name(otmp));
#endif
            (void) Cloak_off();
            dropp(otmp);
        }
        if ((otmp = uarmu) != 0) {
            if (is_whirly(uptr))
#ifdef ZHLANG
                You("直接渗过了衬衫！");
#else
                You("seep right through your shirt!");
#endif
            else
#ifdef ZHLANG
                You("变得太小，衬衫穿不住了！");
#else
                You("become much too small for your shirt!");
#endif
            setworn((struct obj *) 0, otmp->owornmask & W_ARMU);
            dropp(otmp);
        }
    }
    if (has_horns(uptr)) {
        if ((otmp = uarmh) != 0) {
            if (is_flimsy(otmp) && !donning(otmp)) {
                char hornbuf[BUFSZ];

                /* Future possibilities: This could damage/destroy helmet */
                Sprintf(hornbuf, "horn%s", plur(num_horns(uptr)));
#ifdef ZHLANG
                Your("%s%s%s。", hornbuf, "刺穿了",
                     yname(otmp));
#else
                Your("%s %s through %s.", hornbuf, vtense(hornbuf, "pierce"),
                     yname(otmp));
#endif
            } else {
                if (donning(otmp))
                    cancel_don();
#ifdef ZHLANG
                Your("%s掉落在%s上！", helm_simple_name(otmp),
                     surface(u.ux, u.uy));
#else
                Your("%s falls to the %s!", helm_simple_name(otmp),
                     surface(u.ux, u.uy));
#endif
                (void) Helmet_off();
                dropp(otmp);
            }
        }
    }
    if (nohands(uptr) || verysmall(uptr)) {
        if ((otmp = uarmg) != 0) {
            if (donning(otmp))
                cancel_don();
            /* Drop weapon along with gloves */
#ifdef ZHLANG
            You("扔下了手套%s！", uwep ? "和武器" : "");
#else
            You("drop your gloves%s!", uwep ? " and weapon" : "");
#endif
            drop_weapon(0);
            (void) Gloves_off();
            /* Glib manipulation (ends immediately) handled by Gloves_off */
            dropp(otmp);
        }
        if ((otmp = uarms) != 0) {
#ifdef ZHLANG
            You("无法再握住盾牌！");
#else
            You("can no longer hold your shield!");
#endif
            (void) Shield_off();
            dropp(otmp);
        }
        if ((otmp = uarmh) != 0) {
            if (donning(otmp))
                cancel_don();
#ifdef ZHLANG
            Your("%s掉落在%s上！", helm_simple_name(otmp),
                 surface(u.ux, u.uy));
#else
            Your("%s falls to the %s!", helm_simple_name(otmp),
                 surface(u.ux, u.uy));
#endif
            (void) Helmet_off();
            dropp(otmp);
        }
    }
    if (nohands(uptr) || verysmall(uptr)
        || slithy(uptr) || uptr->mlet == S_CENTAUR) {
        if ((otmp = uarmf) != 0) {
            if (donning(otmp))
                cancel_don();
            if (is_whirly(uptr))
#ifdef ZHLANG
                Your("靴子脱落了！");
#else
                Your("boots fall away!");
#endif
            else
#ifdef ZHLANG
                Your("靴子%s从你的脚上掉了下来！",
                     verysmall(uptr) ? "滑落" : "被挤落");
#else
                Your("boots %s off your feet!",
                     verysmall(uptr) ? "slide" : "are pushed");
#endif
            (void) Boots_off();
            dropp(otmp);
        }
    }
    /* not armor, but eyewear shouldn't stay worn without a head to wear
       it/them on (should also come off if head is too tiny or too huge,
       but putting accessories on doesn't reject those cases [yet?]);
       amulet stays worn */
    if ((otmp = ublindf) != 0 && !has_head(uptr)) {
        int l;
        const char *eyewear = simpleonames(otmp); /* blindfold|towel|lenses */

        if (!strncmp(eyewear, "pair of ", l = 8)) /* lenses */
            eyewear += l;
#ifdef ZHLANG
        Your("%s掉落了！", eyewear);
#else
        Your("%s %s off!", eyewear, vtense(eyewear, "fall"));
#endif
        (void) Blindf_off((struct obj *) 0); /* Null: skip usual off mesg */
        dropp(otmp);
    }
    /* rings stay worn even when no hands */
}

staticfn void
drop_weapon(int alone)
{
    struct obj *otmp;
    const char *what, *which, *whichtoo;
    boolean candropwep, candropswapwep, updateinv = TRUE;

    if (uwep) {
        /* !alone check below is currently superfluous but in the
         * future it might not be so if there are monsters which cannot
         * wear gloves but can wield weapons
         */
        if (!alone || cantwield(gy.youmonst.data)) {
            candropwep = canletgo(uwep, "");
            candropswapwep = !u.twoweap || canletgo(uswapwep, "");
            if (alone) {
                what = (candropwep && candropswapwep) ? "drop" : "release";
                which = is_sword(uwep) ? "sword" : weapon_descr(uwep);
                if (u.twoweap) {
                    whichtoo =
                        is_sword(uswapwep) ? "sword" : weapon_descr(uswapwep);
                    if (strcmp(which, whichtoo))
                        which = "weapon";
                }
                if (uwep->quan != 1L || u.twoweap)
                    which = makeplural(which);

#ifdef ZHLANG
                You("发现你必须%s%s%s！", what,
                    the_your[!!strncmp(which, "corpse", 6)], which);
#else
                You("find you must %s %s %s!", what,
                    the_your[!!strncmp(which, "corpse", 6)], which);
#endif
            }
            /* if either uwep or wielded uswapwep is flagged as 'in_use'
               then don't drop it or explicitly update inventory; leave
               those actions to caller (or caller's caller, &c) */
            if (u.twoweap) {
                otmp = uswapwep;
                uswapwepgone();
                if (otmp->in_use)
                    updateinv = FALSE;
                else if (candropswapwep)
                    dropx(otmp);
            }
            otmp = uwep;
            uwepgone();
            if (otmp->in_use)
                updateinv = FALSE;
            else if (candropwep)
                dropx(otmp);
            /* [note: dropp vs dropx -- if heart of ahriman is wielded, we
               might be losing levitation by dropping it; but that won't
               happen until the drop, unlike Boots_off() dumping hero into
               water and triggering emergency_disrobe() before dropx()] */

            if (updateinv)
                update_inventory();
        } else if (!could_twoweap(gy.youmonst.data)) {
            untwoweapon();
        }
    }
}

/* return to original form, usually either due to polymorph timing out
   or dying from loss of hit points while being polymorphed */
void
rehumanize(void)
{
    boolean was_flying = (Flying != 0);

    /* You can't revert back while unchanging */
    if (Unchanging) {
        if (u.mh < 1) {
            svk.killer.format = NO_KILLER_PREFIX;
            Strcpy(svk.killer.name, "killed while stuck in creature form");
            done(DIED);
            /* can get to here if declining to die in explore or wizard
               mode; since we're wearing an amulet of unchanging we can't
               be wearing an amulet of life-saving */
            return; /* don't rehumanize after all */
        } else if (uamul && uamul->otyp == AMULET_OF_UNCHANGING) {
#ifdef ZHLANG
            Your("%s失效了！", simpleonames(uamul));
#else
            Your("%s %s!", simpleonames(uamul), otense(uamul, "fail"));
#endif
            observe_object(uamul);
            makeknown(AMULET_OF_UNCHANGING);
        }
    }

    /*
     * Right now, dying while being a shifted vampire (bat, cloud, wolf)
     * reverts to human rather than to vampire.
     */

    if (emits_light(gy.youmonst.data))
        del_light_source(LS_MONSTER, monst_to_any(&gy.youmonst));
    polyman("You return to %s form!", gu.urace.adj);

    if (u.uhp < 1) {
        /* can only happen if some bit of code reduces u.uhp
           instead of u.mh while poly'd */
    #ifdef ZHLANG
    Your("旧形态不够健康，无法存活。");
#else
    Your("old form was not healthy enough to survive.");
#endif
        Sprintf(svk.killer.name, "reverting to unhealthy %s form",
                gu.urace.adj);
        svk.killer.format = KILLED_BY;
        done(DIED);
    }
    nomul(0);

    disp.botl = TRUE;
    gv.vision_full_recalc = 1;
    encumber_msg();
    update_inventory();
    if (was_flying && !Flying && u.usteed)
#ifdef ZHLANG
        You("和%s轻轻落回%s。",
            mon_nam(u.usteed), surface(u.ux, u.uy));
#else
        You("and %s return gently to the %s.",
            mon_nam(u.usteed), surface(u.ux, u.uy));
#endif
    retouch_equipment(2);
    if (!uarmg)
        selftouch(no_longer_petrify_resistant);
}

int
dobreathe(void)
{
    struct attack *mattk;

    if (Strangled) {
#ifdef ZHLANG
        You_cant("呼吸。抱歉。");
#else
        You_cant("breathe.  Sorry.");
#endif
        return ECMD_OK;
    }
    if (u.uen < 15) {
#ifdef ZHLANG
        You("没有足够的能量来呼吸！");
#else
        You("don't have enough energy to breathe!");
#endif
        return ECMD_OK;
    }
    u.uen -= 15;
    disp.botl = TRUE;

    if (!getdir((char *) 0))
        return ECMD_CANCEL;

    mattk = attacktype_fordmg(gy.youmonst.data, AT_BREA, AD_ANY);
    if (!mattk)
        impossible("bad breath attack?"); /* mouthwash needed... */
    else if (!u.dx && !u.dy && !u.dz)
        ubreatheu(mattk);
    else
        ubuzz(BZ_U_BREATH(BZ_OFS_AD(mattk->adtyp)), (int) mattk->damn);
    return ECMD_TIME;
}

int
dospit(void)
{
    struct obj *otmp;
    struct attack *mattk;

    if (!getdir((char *) 0))
        return ECMD_CANCEL;
    mattk = attacktype_fordmg(gy.youmonst.data, AT_SPIT, AD_ANY);
    if (!mattk) {
        impossible("bad spit attack?");
    } else {
        switch (mattk->adtyp) {
        case AD_BLND:
        case AD_DRST:
            otmp = mksobj(BLINDING_VENOM, TRUE, FALSE);
            break;
        default:
            impossible("bad attack type in dospit");
            FALLTHROUGH;
            /*FALLTHRU*/
        case AD_ACID:
            otmp = mksobj(ACID_VENOM, TRUE, FALSE);
            break;
        }
        otmp->spe = 1; /* to indicate it's yours */
        throwit(otmp, 0L, FALSE, (struct obj *) 0);
    }
    return ECMD_TIME;
}

int
doremove(void)
{
    if (!Punished) {
        if (u.utrap && u.utraptype == TT_BURIEDBALL) {
#ifdef ZHLANG
            pline_The("铁球和锁链牢牢埋在%s中。",
                      surface(u.ux, u.uy));
#else
            pline_The("ball and chain are buried firmly in the %s.",
                      surface(u.ux, u.uy));
#endif
            return ECMD_OK;
        }
#ifdef ZHLANG
        You("没有锁链在身！");
#else
        You("are not chained to anything!");
#endif
        return ECMD_OK;
    }
    unpunish();
    return ECMD_TIME;
}

int
dospinweb(void)
{
    coordxy x = u.ux, y = u.uy;
    struct trap *ttmp = t_at(x, y);
    /* disallow webs on water, lava, air & cloud */
    boolean reject_terrain = is_pool_or_lava(x, y) || IS_AIR(levl[x][y].typ);

    /* [at the time this was written, it was not possible to be both a
       webmaker and a flyer, but with the advent of amulet of flying that
       became a possibility; at present hero can spin a web while flying] */
    if (Levitation || reject_terrain) {
#ifdef ZHLANG
        You("必须在%s地面上才能织网。",
            reject_terrain ? "坚实" : "平坦");
#else
        You("must be on %s ground to spin a web.",
            reject_terrain ? "solid" : "the");
#endif
        return ECMD_OK;
    }
    if (u.uswallow) {
#ifdef ZHLANG
        You("在%s体内释放蛛网液体。", mon_nam(u.ustuck));
#else
        You("release web fluid inside %s.", mon_nam(u.ustuck));
#endif
        if (is_animal(u.ustuck->data)) {
            expels(u.ustuck, u.ustuck->data, TRUE);
            return ECMD_OK;
        }
        if (is_whirly(u.ustuck->data)) {
            int i;

            for (i = 0; i < NATTK; i++)
                if (u.ustuck->data->mattk[i].aatyp == AT_ENGL)
                    break;
            if (i == NATTK)
                impossible("Swallower has no engulfing attack?");
            else {
                char sweep[30];

                sweep[0] = '\0';
                switch (u.ustuck->data->mattk[i].adtyp) {
                case AD_FIRE:
                    Strcpy(sweep, "ignites and ");
                    break;
                case AD_ELEC:
                    Strcpy(sweep, "fries and ");
                    break;
                case AD_COLD:
                    Strcpy(sweep, "freezes, shatters and ");
                    break;
                }
#ifdef ZHLANG
                pline_The("蛛网%s被冲走了！", sweep);
#else
                pline_The("web %sis swept away!", sweep);
#endif
            }
            return ECMD_OK;
        } /* default: a nasty jelly-like creature */
#ifdef ZHLANG
        pline_The("蛛网溶解入%s。", mon_nam(u.ustuck));
#else
        pline_The("web dissolves into %s.", mon_nam(u.ustuck));
#endif
        return ECMD_OK;
    }
    if (u.utrap) {
#ifdef ZHLANG
        You("被困陷阱时无法织网。");
#else
        You("cannot spin webs while stuck in a trap.");
#endif
        return ECMD_OK;
    }
    exercise(A_DEX, TRUE);
    if (ttmp) {
        switch (ttmp->ttyp) {
        case PIT:
        case SPIKED_PIT:
#ifdef ZHLANG
            You("织了一张网，盖住了陷阱。");
#else
            You("spin a web, covering up the pit.");
#endif
            deltrap(ttmp);
            bury_objs(x, y);
            newsym(x, y);
            return ECMD_TIME;
        case SQKY_BOARD:
#ifdef ZHLANG
            pline_The("吱吱作响的地板被闷住了。");
#else
            pline_The("squeaky board is muffled.");
#endif
            deltrap(ttmp);
            newsym(x, y);
            return ECMD_TIME;
        case TELEP_TRAP:
        case LEVEL_TELEP:
        case MAGIC_PORTAL:
        case VIBRATING_SQUARE:
#ifdef ZHLANG
            Your("蛛网消失了！");
#else
            Your("webbing vanishes!");
#endif
            return ECMD_OK;
        case WEB:
#ifdef ZHLANG
            You("把蛛网加厚了。");
#else
            You("make the web thicker.");
#endif
            return ECMD_TIME;
        case HOLE:
        case TRAPDOOR:
#ifdef ZHLANG
            You("在%s上织网遮住。",
                (ttmp->ttyp == TRAPDOOR) ? "活板门" : "洞");
#else
            You("web over the %s.",
                (ttmp->ttyp == TRAPDOOR) ? "trap door" : "hole");
#endif
            deltrap(ttmp);
            newsym(x, y);
            return ECMD_TIME;
        case ROLLING_BOULDER_TRAP:
#ifdef ZHLANG
            You("织网卡住了触发器。");
#else
            You("spin a web, jamming the trigger.");
#endif
            deltrap(ttmp);
            newsym(x, y);
            return ECMD_TIME;
        case ARROW_TRAP:
        case DART_TRAP:
        case BEAR_TRAP:
        case ROCKTRAP:
        case FIRE_TRAP:
        case LANDMINE:
        case SLP_GAS_TRAP:
        case RUST_TRAP:
        case MAGIC_TRAP:
        case ANTI_MAGIC:
        case POLY_TRAP:
#ifdef ZHLANG
            You("触发了陷阱！");
#else
            You("have triggered a trap!");
#endif
            dotrap(ttmp, NO_TRAP_FLAGS);
            return ECMD_TIME;
        default:
            impossible("Webbing over trap type %d?", ttmp->ttyp);
            return ECMD_OK;
        }
    } else if (On_stairs(x, y)) {
        /* cop out: don't let them hide the stairs */
#ifdef ZHLANG
        Your("蛛网未能阻碍通往%s的道路。",
             (levl[x][y].typ == STAIRS) ? "楼梯" : "梯子");
#else
        Your("web fails to impede access to the %s.",
             (levl[x][y].typ == STAIRS) ? "stairs" : "ladder");
#endif
        return ECMD_TIME;
    }
    ttmp = maketrap(x, y, WEB);
    if (ttmp) {
#ifdef ZHLANG
        You("织了一张网。");
#else
        You("spin a web.");
#endif
        ttmp->madeby_u = 1;
        feeltrap(ttmp);
        if (*in_rooms(x, y, SHOPBASE))
            add_damage(x, y, SHOP_WEB_COST);
    }
    return ECMD_TIME;
}

int
dosummon(void)
{
    int placeholder;
    if (u.uen < 10) {
#ifdef ZHLANG
        You("缺乏发出求救信号的能量！");
#else
        You("lack the energy to send forth a call for help!");
#endif
        return ECMD_OK;
    }
    u.uen -= 10;
    disp.botl = TRUE;

#ifdef ZHLANG
    You("召唤你的同胞寻求帮助！");
#else
    You("call upon your brethren for help!");
#endif
    exercise(A_WIS, TRUE);
    if (!were_summon(gy.youmonst.data, TRUE, &placeholder, (char *) 0))
#ifdef ZHLANG
        pline("但没有人来。");
#else
        pline("But none arrive.");
#endif
    return ECMD_TIME;
}

int
dogaze(void)
{
    struct monst *mtmp;
    int looked = 0;
    char qbuf[QBUFSZ];
    int i;
    uchar adtyp = 0;

    for (i = 0; i < NATTK; i++) {
        if (gy.youmonst.data->mattk[i].aatyp == AT_GAZE) {
            adtyp = gy.youmonst.data->mattk[i].adtyp;
            break;
        }
    }
    if (adtyp != AD_CONF && adtyp != AD_FIRE) {
        impossible("gaze attack %d?", adtyp);
        return ECMD_OK;
    }

    if (Blind) {
#ifdef ZHLANG
                You_cant("看不到任何可以凝视的东西。");
#else
                You_cant("see anything to gaze at.");
#endif
        return ECMD_OK;
    } else if (Hallucination) {
#ifdef ZHLANG
        You_cant("无法凝视任何你能看到的东西。");
#else
        You_cant("gaze at anything you can see.");
#endif
        return ECMD_OK;
    }
    if (u.uen < 15) {
#ifdef ZHLANG
        You("缺乏使用特殊凝视的能量！");
#else
        You("lack the energy to use your special gaze!");
#endif
        return ECMD_OK;
    }
    u.uen -= 15;
    disp.botl = TRUE;

    for (mtmp = fmon; mtmp; mtmp = mtmp->nmon) {
        if (DEADMONSTER(mtmp))
            continue;
        if (canseemon(mtmp) && couldsee(mtmp->mx, mtmp->my)) {
            looked++;
            if (Invis && !perceives(mtmp->data)) {
#ifdef ZHLANG
                pline("%s似乎没有注意到你的凝视。", Monnam(mtmp));
#else
                pline("%s seems not to notice your gaze.", Monnam(mtmp));
#endif
            } else if (mtmp->minvis && !See_invisible) {
#ifdef ZHLANG
                You_cant("看不到该注视%s哪里。", Monnam(mtmp));
#else
                You_cant("see where to gaze at %s.", Monnam(mtmp));
#endif
            } else if (M_AP_TYPE(mtmp) == M_AP_FURNITURE
                       || M_AP_TYPE(mtmp) == M_AP_OBJECT) {
                looked--;
                continue;
            } else if (flags.safe_dog && mtmp->mtame && !Confusion) {
        #ifdef ZHLANG
        You("避免注视%s。", y_monnam(mtmp));
#else
        You("avoid gazing at %s.", y_monnam(mtmp));
#endif
            } else {
                if (flags.confirm && mtmp->mpeaceful && !Confusion) {
                    Sprintf(qbuf, "Really %s %s?",
                            (adtyp == AD_CONF) ? "confuse" : "attack",
                            mon_nam(mtmp));
                    if (y_n(qbuf) != 'y')
                        continue;
                }
                setmangry(mtmp, TRUE);
                if (helpless(mtmp) || mtmp->mstun
                    || !mtmp->mcansee || !haseyes(mtmp->data)) {
                    looked--;
                    continue;
                }
                /* No reflection check for consistency with when a monster
                 * gazes at *you*--only medusa gaze gets reflected then.
                 */
                if (adtyp == AD_CONF) {
                    if (!mtmp->mconf)
#ifdef ZHLANG
                        Your("凝视迷惑了%s！", mon_nam(mtmp));
#else
                        Your("gaze confuses %s!", mon_nam(mtmp));
#endif
                    else
        #ifdef ZHLANG
                pline("%s越来越困惑了。",
                              Monnam(mtmp));
#else
                pline("%s is getting more and more confused.",
                              Monnam(mtmp));
#endif
                    mtmp->mconf = 1;
                } else if (adtyp == AD_FIRE) {
                    int dmg = d(2, 6), orig_dmg = dmg, lev = (int) u.ulevel;

#ifdef ZHLANG
                    You("用灼热凝视攻击%s！", mon_nam(mtmp));
#else
                    You("attack %s with a fiery gaze!", mon_nam(mtmp));
#endif
                    if (resists_fire(mtmp)) {
#ifdef ZHLANG
                        pline_The("火焰没有烧伤%s！", mon_nam(mtmp));
#else
                        pline_The("fire doesn't burn %s!", mon_nam(mtmp));
#endif
                        dmg = 0;
                    }
                    if (lev > rn2(20)) {
                        dmg += destroy_items(mtmp, AD_FIRE, orig_dmg);
                        ignite_items(mtmp->minvent);
                    }
                    if (dmg)
                        mtmp->mhp -= dmg;
                    if (DEADMONSTER(mtmp))
                        killed(mtmp);
                }
                /* For consistency with passive() in uhitm.c, this only
                 * affects you if the monster is still alive.
                 */
                if (DEADMONSTER(mtmp))
                    continue;

                if (mtmp->data == &mons[PM_FLOATING_EYE] && !mtmp->mcan) {
                    if (!Free_action) {
                #ifdef ZHLANG
        You("被%s的目光冻结了！",
                            s_suffix(mon_nam(mtmp)));
#else
        You("are frozen by %s gaze!",
                            s_suffix(mon_nam(mtmp)));
#endif
                        nomul((u.ulevel > 6 || rn2(4))
                                  ? -d((int) mtmp->m_lev + 1,
                                       (int) mtmp->data->mattk[0].damd)
                                  : -200);
                        gm.multi_reason = "frozen by a monster's gaze";
                        gn.nomovemsg = 0;
                        return ECMD_TIME;
                    } else
#ifdef ZHLANG
                        You("在%s的注视下瞬间僵硬了。",
                            s_suffix(mon_nam(mtmp)));
#else
                        You("stiffen momentarily under %s gaze.",
                            s_suffix(mon_nam(mtmp)));
#endif
                }
                /* Technically this one shouldn't affect you at all because
                 * the Medusa gaze is an active monster attack that only
                 * works on the monster's turn, but for it to *not* have an
                 * effect would be too weird.
                 */
                if (mtmp->data == &mons[PM_MEDUSA] && !mtmp->mcan) {
#ifdef ZHLANG
                    pline("凝视着清醒的%s不是个好主意。",
                          l_monnam(mtmp));
#else
                    pline("Gazing at the awake %s is not a very good idea.",
                          l_monnam(mtmp));
#endif
                    /* as if gazing at a sleeping anything is fruitful... */
#ifdef ZHLANG
                    urgent_pline("你变成了石头...");
#else
                    urgent_pline("You turn to stone...");
#endif
                    svk.killer.format = KILLED_BY;
                    Strcpy(svk.killer.name,
                           "deliberately meeting Medusa's gaze");
                    done(STONING);
                }
            }
        }
    }
    if (!looked)
#ifdef ZHLANG
        You("没有注视任何特定目标。");
#else
        You("gaze at no place in particular.");
#endif
    return ECMD_TIME;
}

/* called by domonability() for #monster */
int
dohide(void)
{
    boolean ismimic = gy.youmonst.data->mlet == S_MIMIC,
            on_ceiling = is_clinger(gy.youmonst.data) || Flying;

    /* can't hide while being held (or holding) or while trapped
       (except for floor hiders [trapper or mimic] in pits) */
    if (u.ustuck || (u.utrap && (u.utraptype != TT_PIT || on_ceiling))) {
#ifdef ZHLANG
        You_cant("在%s时无法躲藏。",
                 !u.ustuck ? "被困"
                   : u.uswallow ? (digests(u.ustuck->data) ? "被消化"
                                                            : "被吞噬")
                     : !sticks(gy.youmonst.data) ? "被抓住"
                       : (humanoid(u.ustuck->data) ? "抱着某人"
                                                    : "抱着那个生物"));
#else
        You_cant("hide while you're %s.",
                 !u.ustuck ? "trapped"
                   : u.uswallow ? (digests(u.ustuck->data) ? "swallowed"
                                                            : "engulfed")
                     : !sticks(gy.youmonst.data) ? "being held"
                       : (humanoid(u.ustuck->data) ? "holding someone"
                                                    : "holding that creature"));
#endif
        if (u.uundetected || (ismimic && U_AP_TYPE != M_AP_NOTHING)) {
            u.uundetected = 0;
            gy.youmonst.m_ap_type = M_AP_NOTHING;
            newsym(u.ux, u.uy);
        }
        return ECMD_OK;
    }
    /* note: hero-as-eel handling is incomplete but unnecessary;
       such critters aren't offered the option of hiding via #monster */
    if (gy.youmonst.data->mlet == S_EEL && !is_pool(u.ux, u.uy)) {
        if (IS_FOUNTAIN(levl[u.ux][u.uy].typ))
    #ifdef ZHLANG
        pline_The("喷泉不够深，无法躲藏。");
#else
        pline_The("fountain is not deep enough to hide in.");
#endif
        else
#ifdef ZHLANG
            There("这里没有可以藏身的%s。", hliquid("water"));
#else
            There("is no %s to hide in here.", hliquid("water"));
#endif
        u.uundetected = 0;
        return ECMD_OK;
    }
    if (hides_under(gy.youmonst.data)) {
        long ct = 0L;
        struct obj *otmp, *otop = svl.level.objects[u.ux][u.uy];

        if (!otop) {
#ifdef ZHLANG
            There("这里没有可以藏在下面的东西。");
#else
            There("is nothing to hide under here.");
#endif
            u.uundetected = 0;
            return ECMD_OK;
        }
        for (otmp = otop;
             otmp && otmp->otyp == CORPSE
                  && touch_petrifies(&mons[otmp->corpsenm]);
             otmp = otmp->nexthere)
            ct += otmp->quan;
        /* otmp will be Null iff the entire pile consists of 'trice corpses */
        if (!otmp && !Stone_resistance) {
            char kbuf[BUFSZ];
            const char *corpse_name = cxname(otop);

            /* for the plural case, we'll say "cockatrice corpses" or
               "chickatrice corpses" depending on the top of the pile
               even if both types are present */
            if (ct == 1)
                corpse_name = an(corpse_name);
            /* no need to check poly_when_stoned(); no hide-underers can
               turn into stone golems instead of becoming petrified */
#ifdef ZHLANG
            pline("躲在%s% s下是致命的错误...",
                  corpse_name, plur(ct));
#else
            pline("Hiding under %s%s is a fatal mistake...",
                  corpse_name, plur(ct));
#endif
            Sprintf(kbuf, "hiding under %s%s", corpse_name, plur(ct));
            instapetrify(kbuf);
            /* only reach here if life-saved */
            u.uundetected = 0;
            return ECMD_TIME;
        }
    }
    /* Planes of Air and Water */
    if (on_ceiling && !has_ceiling(&u.uz)) {
#ifdef ZHLANG
        There("你上方无处可藏。");
#else
        There("is nowhere to hide above you.");
#endif
        u.uundetected = 0;
        return ECMD_OK;
    }
    if ((is_hider(gy.youmonst.data) && !Flying) /* floor hider */
        && (Is_airlevel(&u.uz) || Is_waterlevel(&u.uz))) {
#ifdef ZHLANG
        There("你下方无处可藏。");
#else
        There("is nowhere to hide beneath you.");
#endif
        u.uundetected = 0;
        return ECMD_OK;
    }
    /* TODO? inhibit floor hiding at furniture locations, or
     * else make youhiding() give smarter messages at such spots.
     */

    if (u.uundetected || (ismimic && U_AP_TYPE != M_AP_NOTHING)) {
        youhiding(FALSE, 1); /* "you are already hiding" */
        return ECMD_OK;
    }

    if (ismimic) {
        /* should bring up a dialog "what would you like to imitate?" */
        gy.youmonst.m_ap_type = M_AP_OBJECT;
        gy.youmonst.mappearance = STRANGE_OBJECT;
    } else
        u.uundetected = 1;
    newsym(u.ux, u.uy);
    youhiding(FALSE, 0); /* "you are now hiding" */
    return ECMD_TIME;
}

int
dopoly(void)
{
    struct permonst *savedat = gy.youmonst.data;

    if (is_vampire(gy.youmonst.data) || is_vampshifter(&gy.youmonst)) {
        polyself(POLY_MONSTER);
        if (savedat != gy.youmonst.data) {
    #ifdef ZHLANG
        You("变身为%s。",
                an(pmname(gy.youmonst.data, Ugender)));
#else
        You("transform into %s.",
                an(pmname(gy.youmonst.data, Ugender)));
#endif
            newsym(u.ux, u.uy);
        }
    }
    return ECMD_TIME;
}

/* #monster for hero-as-mind_flayer giving psychic blast */
int
domindblast(void)
{
    struct monst *mtmp, *nmon;
    int dmg;

    if (u.uen < 10) {
#ifdef ZHLANG
        You("集中精神，但缺乏维持的能量。");
#else
        You("concentrate but lack the energy to maintain doing so.");
#endif
        return ECMD_OK;
    }
    u.uen -= 10;
    disp.botl = TRUE;

#ifdef ZHLANG
    You("集中精神。");
#else
    You("concentrate.");
#endif
#ifdef ZHLANG
    pline("一股精神能量波涌出。");
#else
    pline("A wave of psychic energy pours out.");
#endif
    for (mtmp = fmon; mtmp; mtmp = nmon) {
        int u_sen;

        nmon = mtmp->nmon;
        if (DEADMONSTER(mtmp))
            continue;
        if (mdistu(mtmp) > BOLT_LIM * BOLT_LIM)
            continue;
        if (mtmp->mpeaceful)
            continue;
        if (mindless(mtmp->data))
            continue;
        u_sen = telepathic(mtmp->data) && !mtmp->mcansee;
        if (u_sen || (telepathic(mtmp->data) && rn2(2)) || !rn2(10)) {
            dmg = rnd(15);
            /* wake it up first, to bring hidden monster out of hiding;
               but in case it is currently peaceful, don't make it hostile
               unless it will survive the psychic blast, otherwise hero
               would avoid the penalty for killing it while peaceful */
            wakeup(mtmp, (dmg > mtmp->mhp) ? TRUE : FALSE);
#ifdef ZHLANG
            You("锁定%s的%s。", s_suffix(mon_nam(mtmp)),
                u_sen ? "心灵感应"
                : telepathic(mtmp->data) ? "潜在心灵感应"
                  : "思维");
#else
            You("lock in on %s %s.", s_suffix(mon_nam(mtmp)),
                u_sen ? "telepathy"
                : telepathic(mtmp->data) ? "latent telepathy"
                  : "mind");
#endif
            mtmp->mhp -= dmg;
            if (DEADMONSTER(mtmp))
                killed(mtmp);
        }
    }
    return ECMD_TIME;
}

void
uunstick(void)
{
    struct monst *mtmp = u.ustuck;

    if (!mtmp) {
        impossible("uunstick: no ustuck?");
        return;
    }
    set_ustuck((struct monst *) 0); /* before pline() */
#ifdef ZHLANG
    pline("%s 不再在你的掌控之中。", Monnam(mtmp));
#else
    pline("%s is no longer in your clutches.", Monnam(mtmp));
#endif
}

void
skinback(boolean silently)
{
    if (uskin) {
        int old_light = arti_light_radius(uskin);

        if (!silently)
#ifdef ZHLANG
            Your("皮肤恢复原状。");
#else
            Your("skin returns to its original form.");
#endif
        uarm = uskin;
        uskin = (struct obj *) 0;
        /* undo save/restore hack */
        uarm->owornmask &= ~I_SPECIAL;

        if (artifact_light(uarm))
            maybe_adjust_light(uarm, old_light);
    }
}

const char *
mbodypart(struct monst *mon, int part)
{
    static NEARDATA const char
        *humanoid_parts[] = { "arm",       "eye",  "face",         "finger",
                              "fingertip", "foot", "hand",         "handed",
                              "head",      "leg",  "light headed", "neck",
                              "spine",     "toe",  "hair",         "blood",
                              "lung",      "nose", "stomach" },
        *jelly_parts[] = { "pseudopod", "dark spot", "front",
                           "pseudopod extension", "pseudopod extremity",
                           "pseudopod root", "grasp", "grasped",
                           "cerebral area", "lower pseudopod", "viscous",
                           "middle", "surface", "pseudopod extremity",
                           "ripples", "juices", "surface", "sensor",
                           "stomach" },
        *animal_parts[] = { "forelimb",  "eye",           "face",
                            "foreclaw",  "claw tip",      "rear claw",
                            "foreclaw",  "clawed",        "head",
                            "rear limb", "light headed",  "neck",
                            "spine",     "rear claw tip", "fur",
                            "blood",     "lung",          "nose",
                            "stomach" },
        *bird_parts[] = { "wing",     "eye",  "face",         "wing",
                          "wing tip", "foot", "wing",         "winged",
                          "head",     "leg",  "light headed", "neck",
                          "spine",    "toe",  "feathers",     "blood",
                          "lung",     "bill", "stomach" },
        *horse_parts[] = { "foreleg",  "eye",           "face",
                           "forehoof", "hoof tip",      "rear hoof",
                           "forehoof", "hooved",        "head",
                           "rear leg", "light headed",  "neck",
                           "backbone", "rear hoof tip", "mane",
                           "blood",    "lung",          "nose",
                           "stomach" },
        *sphere_parts[] = { "appendage", "optic nerve", "body", "tentacle",
                            "tentacle tip", "lower appendage", "tentacle",
                            "tentacled", "body", "lower tentacle",
                            "rotational", "equator", "body",
                            "lower tentacle tip", "cilia", "life force",
                            "retina", "olfactory nerve", "interior" },
        *fungus_parts[] = { "mycelium", "visual area", "front",
                            "hypha",    "hypha",       "root",
                            "strand",   "stranded",    "cap area",
                            "rhizome",  "sporulated",  "stalk",
                            "root",     "rhizome tip", "spores",
                            "juices",   "gill",        "gill",
                            "interior" },
        *vortex_parts[] = { "region",        "eye",           "front",
                            "minor current", "minor current", "lower current",
                            "swirl",         "swirled",       "central core",
                            "lower current", "addled",        "center",
                            "currents",      "edge",          "currents",
                            "life force",    "center",        "leading edge",
                            "interior" },
        *snake_parts[] = { "vestigial limb", "eye", "face", "large scale",
                           "large scale tip", "rear region", "scale gap",
                           "scale gapped", "head", "rear region",
                           "light headed", "neck", "length", "rear scale",
                           "scales", "blood", "lung", "forked tongue",
                           "stomach" },
        *worm_parts[] = { "anterior segment", "light sensitive cell",
                          "clitellum", "setae", "setae", "posterior segment",
                          "segment", "segmented", "anterior segment",
                          "posterior", "over stretched", "clitellum",
                          "length", "posterior setae", "setae", "blood",
                          "skin", "prostomium", "stomach" },
        *spider_parts[] = { "pedipalp", "eye", "face", "pedipalp", "tarsus",
                            "claw", "pedipalp", "palped", "cephalothorax",
                            "leg", "spun out", "cephalothorax", "abdomen",
                            "claw", "hair", "hemolymph", "book lung",
                            "labrum", "digestive tract" },
        *fish_parts[] = { "fin", "eye", "premaxillary", "pelvic axillary",
                          "pelvic fin", "anal fin", "pectoral fin", "finned",
                          "head", "peduncle", "played out", "gills",
                          "dorsal fin", "caudal fin", "scales", "blood",
                          "gill", "nostril", "stomach" };
    /* claw attacks are overloaded in mons[]; most humanoids with
       such attacks should still reference hands rather than claws */
    static const char not_claws[] = {
        S_HUMAN,     S_MUMMY,   S_ZOMBIE, S_ANGEL, S_NYMPH, S_LEPRECHAUN,
        S_QUANTMECH, S_VAMPIRE, S_ORC,    S_GIANT, /* quest nemeses */
        '\0' /* string terminator; assert( S_xxx != 0 ); */
    };
    struct permonst *mptr = mon->data;

    if (part <= NO_PART) {
        impossible("mbodypart: bad part %d", part);
        return "mystery part";
    }

    /* some special cases */
    if (mptr->mlet == S_DOG || mptr->mlet == S_FELINE
        || mptr->mlet == S_RODENT || mptr == &mons[PM_OWLBEAR]) {
        switch (part) {
        case HAND:
            return "paw";
        case HANDED:
            return "pawed";
        case FOOT:
            return "rear paw";
        case ARM:
        case LEG:
            return horse_parts[part]; /* "foreleg", "rear leg" */
        default:
            break; /* for other parts, use animal_parts[] below */
        }
    } else if (mptr->mlet == S_YETI) { /* excl. owlbear due to 'if' above */
        /* opposable thumbs, hence "hands", "arms", "legs", &c */
        return humanoid_parts[part]; /* yeti/sasquatch, monkey/ape */
    }
    if ((part == HAND || part == HANDED)
        && (humanoid(mptr) && attacktype(mptr, AT_CLAW)
            && !strchr(not_claws, mptr->mlet) && mptr != &mons[PM_STONE_GOLEM]
            && mptr != &mons[PM_AMOROUS_DEMON]))
        return (part == HAND) ? "claw" : "clawed";
    if ((mptr == &mons[PM_MUMAK] || mptr == &mons[PM_MASTODON])
        && part == NOSE)
        return "trunk";
    if (mptr == &mons[PM_SHARK] && part == HAIR)
        return "skin"; /* sharks don't have scales */
    if ((mptr == &mons[PM_JELLYFISH] || mptr == &mons[PM_KRAKEN])
        && (part == ARM || part == FINGER || part == HAND || part == FOOT
            || part == TOE))
        return "tentacle";
    if (mptr == &mons[PM_FLOATING_EYE] && part == EYE)
        return "cornea";
    if (humanoid(mptr) && (part == ARM || part == FINGER || part == FINGERTIP
                           || part == HAND || part == HANDED))
        return humanoid_parts[part];
    if (mptr->mlet == S_COCKATRICE)
        return (part == HAIR) ? snake_parts[part] : bird_parts[part];
    if (mptr == &mons[PM_RAVEN])
        return bird_parts[part];
    if (mptr->mlet == S_CENTAUR || mptr->mlet == S_UNICORN
        || mptr == &mons[PM_KI_RIN]
        || (mptr == &mons[PM_ROTHE] && part != HAIR))
        return horse_parts[part];
    if (mptr->mlet == S_LIGHT) {
        if (part == HANDED)
            return "rayed";
        else if (part == ARM || part == FINGER || part == FINGERTIP
                 || part == HAND)
            return "ray";
        else
            return "beam";
    }
    if (mptr == &mons[PM_STALKER] && part == HEAD)
        return "head";
    if (mptr->mlet == S_EEL && mptr != &mons[PM_JELLYFISH])
        return fish_parts[part];
    if (mptr->mlet == S_WORM)
        return worm_parts[part];
    if (mptr->mlet == S_SPIDER)
        return spider_parts[part];
    if (slithy(mptr) || (mptr->mlet == S_DRAGON && part == HAIR))
        return snake_parts[part];
    if (mptr->mlet == S_EYE)
        return sphere_parts[part];
    if (mptr->mlet == S_JELLY || mptr->mlet == S_PUDDING
        || mptr->mlet == S_BLOB || mptr == &mons[PM_JELLYFISH])
        return jelly_parts[part];
    if (mptr->mlet == S_VORTEX || mptr->mlet == S_ELEMENTAL)
        return vortex_parts[part];
    if (mptr->mlet == S_FUNGUS)
        return fungus_parts[part];
    if (humanoid(mptr))
        return humanoid_parts[part];
    return animal_parts[part];
}

const char *
body_part(int part)
{
    return mbodypart(&gy.youmonst, part);
}

int
poly_gender(void)
{
    /* Returns gender of polymorphed player;
     * 0/1=same meaning as flags.female, 2=none.
     */
    if (is_neuter(gy.youmonst.data) || !humanoid(gy.youmonst.data))
        return 2;
    return flags.female;
}

void
ugolemeffects(int damtype, int dam)
{
    int heal = 0;

    /* We won't bother with "slow"/"haste" since players do not
     * have a monster-specific slow/haste so there is no way to
     * restore the old velocity once they are back to human.
     */
    if (u.umonnum != PM_FLESH_GOLEM && u.umonnum != PM_IRON_GOLEM)
        return;
    switch (damtype) {
    case AD_ELEC:
        if (u.umonnum == PM_FLESH_GOLEM)
            heal = (dam + 5) / 6; /* Approx 1 per die */
        break;
    case AD_FIRE:
        if (u.umonnum == PM_IRON_GOLEM)
            heal = dam;
        break;
    }
    if (heal && (u.mh < u.mhmax)) {
        u.mh += heal;
        if (u.mh > u.mhmax)
            u.mh = u.mhmax;
        disp.botl = TRUE;
#ifdef ZHLANG
        pline("奇怪的是，你感觉比以前更好了。");
#else
        pline("Strangely, you feel better than before.");
#endif
        exercise(A_STR, TRUE);
    }
}

staticfn int
armor_to_dragon(int atyp)
{
    switch (atyp) {
    case GRAY_DRAGON_SCALE_MAIL:
    case GRAY_DRAGON_SCALES:
        return PM_GRAY_DRAGON;
    case SILVER_DRAGON_SCALE_MAIL:
    case SILVER_DRAGON_SCALES:
        return PM_SILVER_DRAGON;
    case GOLD_DRAGON_SCALE_MAIL:
    case GOLD_DRAGON_SCALES:
        return PM_GOLD_DRAGON;
#if 0 /* DEFERRED */
    case SHIMMERING_DRAGON_SCALE_MAIL:
    case SHIMMERING_DRAGON_SCALES:
        return PM_SHIMMERING_DRAGON;
#endif
    case RED_DRAGON_SCALE_MAIL:
    case RED_DRAGON_SCALES:
        return PM_RED_DRAGON;
    case ORANGE_DRAGON_SCALE_MAIL:
    case ORANGE_DRAGON_SCALES:
        return PM_ORANGE_DRAGON;
    case WHITE_DRAGON_SCALE_MAIL:
    case WHITE_DRAGON_SCALES:
        return PM_WHITE_DRAGON;
    case BLACK_DRAGON_SCALE_MAIL:
    case BLACK_DRAGON_SCALES:
        return PM_BLACK_DRAGON;
    case BLUE_DRAGON_SCALE_MAIL:
    case BLUE_DRAGON_SCALES:
        return PM_BLUE_DRAGON;
    case GREEN_DRAGON_SCALE_MAIL:
    case GREEN_DRAGON_SCALES:
        return PM_GREEN_DRAGON;
    case YELLOW_DRAGON_SCALE_MAIL:
    case YELLOW_DRAGON_SCALES:
        return PM_YELLOW_DRAGON;
    default:
        return NON_PM;
    }
}

/* some species have awareness of other species */
staticfn void
polysense(void)
{
    short warnidx = NON_PM;

    svc.context.warntype.speciesidx = NON_PM;
    svc.context.warntype.species = 0;
    svc.context.warntype.polyd = 0;
    HWarn_of_mon &= ~FROMRACE;

    switch (u.umonnum) {
    case PM_PURPLE_WORM:
    case PM_BABY_PURPLE_WORM:
        warnidx = PM_SHRIEKER;
        break;
    case PM_VAMPIRE:
    case PM_VAMPIRE_LEADER:
        svc.context.warntype.polyd = M2_HUMAN | M2_ELF;
        HWarn_of_mon |= FROMRACE;
        return;
    }
    if (ismnum(warnidx)) {
        svc.context.warntype.speciesidx = warnidx;
        svc.context.warntype.species = &mons[warnidx];
        HWarn_of_mon |= FROMRACE;
    }
}

/* True iff hero's role or race has been genocided */
boolean
ugenocided(void)
{
    return ((svm.mvitals[gu.urole.mnum].mvflags & G_GENOD)
            || (svm.mvitals[gu.urace.mnum].mvflags & G_GENOD));
}

/* how hero feels "inside" after self-genocide of role or race */
const char *
udeadinside(void)
{
    /* self-genocide used to always say "you feel dead inside" but that
       seems silly when you're polymorphed into something undead;
       monkilled() distinguishes between living (killed) and non (destroyed)
       for monster death message; we refine the nonliving aspect a bit */
    return !nonliving(gy.youmonst.data)
             ? "dead"          /* living, including demons */
             : !weirdnonliving(gy.youmonst.data)
                 ? "condemned" /* undead plus manes */
                 : "empty";    /* golems plus vortices */
}

/*polyself.c*/
