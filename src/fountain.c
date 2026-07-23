/* NetHack 5.0	fountain.c	$NHDT-Date: 1781973050 2026/06/20 16:30:50 $  $NHDT-Branch: NetHack-5.0 $:$NHDT-Revision: 1.121 $ */
/*      Copyright Scott R. Turner, srt@ucla, 10/27/86 */
/* NetHack may be freely redistributed.  See license for details. */

/* Code for drinking from fountains. */

#include "hack.h"

staticfn void dowatersnakes(void);
staticfn void dowaterdemon(void);
staticfn void dowaternymph(void);
staticfn void gush(coordxy, coordxy, genericptr_t) NONNULLARG3;
staticfn void dofindgem(void);
staticfn boolean watchman_warn_fountain(struct monst *) NONNULLARG1;

DISABLE_WARNING_FORMAT_NONLITERAL

/* used when trying to dip in or drink from fountain or sink or pool while
   levitating above it, or when trying to move downwards in that state */
void
floating_above(const char *what)
{
    const char *umsg;
#ifdef ZHLANG
    umsg = "你正高高漂浮在%s上方。";
#else
    umsg = "are floating high above the %s.";
#endif

    if (u.utrap && (u.utraptype == TT_INFLOOR || u.utraptype == TT_LAVA)) {
        /* when stuck in floor (not possible at fountain or sink location,
           so must be attempting to move down), override the usual message */
#ifdef ZHLANG
        umsg = "你被困在了%s中。";
#else
        umsg = "are trapped in the %s.";
#endif
        what = surface(u.ux, u.uy); /* probably redundant */
    }
    You(umsg, what);
}

RESTORE_WARNING_FORMAT_NONLITERAL

/* Fountain of snakes! */
staticfn void
dowatersnakes(void)
{
    int num = rn1(5, 2);
    struct monst *mtmp;

    if (!(svm.mvitals[PM_WATER_MOCCASIN].mvflags & G_GONE)) {
        if (!Blind) {
#ifdef ZHLANG
            pline("无尽的%s喷涌而出！",
                  Hallucination ? makeplural(rndmonnam(NULL)) : "蛇");
#else
            pline("An endless stream of %s pours forth!",
                  Hallucination ? makeplural(rndmonnam(NULL)) : "snakes");
#endif
        } else {
            Soundeffect(se_snakes_hissing, 75);
#ifdef ZHLANG
            You_hear("%s的嘶嘶声！", something);
#else
            You_hear("%s hissing!", something);
#endif
        }
        while (num-- > 0)
            if ((mtmp = makemon(&mons[PM_WATER_MOCCASIN], u.ux, u.uy,
                                MM_NOMSG)) != 0
                && t_at(mtmp->mx, mtmp->my))
                (void) mintrap(mtmp, NO_TRAP_FLAGS);
    } else {
        Soundeffect(se_furious_bubbling, 20);
#ifdef ZHLANG
        pline_The("喷泉猛烈地冒泡了一会儿，然后平静下来。");
#else
        pline_The("fountain bubbles furiously for a moment, then calms.");
#endif
    }
}

/* Water demon */
staticfn void
dowaterdemon(void)
{
    struct monst *mtmp;

    if (!(svm.mvitals[PM_WATER_DEMON].mvflags & G_GONE)) {
        if ((mtmp = makemon(&mons[PM_WATER_DEMON], u.ux, u.uy,
                            MM_NOMSG)) != 0) {
            if (!Blind)
#ifdef ZHLANG
                You("释放了%s！", a_monnam(mtmp));
#else
                You("unleash %s!", a_monnam(mtmp));
#endif
            else
#ifdef ZHLANG
                You_feel("邪恶的存在。");
#else
                You_feel("the presence of evil.");
#endif

            /* Give those on low levels a (slightly) better chance of survival
             */
            if (rnd(100) > (80 + level_difficulty())) {
#ifdef ZHLANG
                pline("感激于%s释放，%s赐予你一个愿望！",
                      mhis(mtmp), mhe(mtmp));
#else
                pline("Grateful for %s release, %s grants you a wish!",
                      mhis(mtmp), mhe(mtmp));
#endif
                /* give a wish and discard the monster (mtmp set to null) */
                mongrantswish(&mtmp);
            } else if (t_at(mtmp->mx, mtmp->my))
                (void) mintrap(mtmp, NO_TRAP_FLAGS);
        }
    } else {
        Soundeffect(se_furious_bubbling, 20);
#ifdef ZHLANG
        pline_The("喷泉猛烈地冒泡了一会儿，然后平静下来。");
#else
        pline_The("fountain bubbles furiously for a moment, then calms.");
#endif
    }
}

/* Water Nymph */
staticfn void
dowaternymph(void)
{
    struct monst *mtmp;

    if (!(svm.mvitals[PM_WATER_NYMPH].mvflags & G_GONE)
        && (mtmp = makemon(&mons[PM_WATER_NYMPH], u.ux, u.uy,
                           MM_NOMSG)) != 0) {
        if (!Blind)
#ifdef ZHLANG
            You("吸引了%s！", a_monnam(mtmp));
#else
            You("attract %s!", a_monnam(mtmp));
#endif
        else
#ifdef ZHLANG
            You_hear("一个诱人的声音。");
#else
            You_hear("a seductive voice.");
#endif
        mtmp->msleeping = 0;
        if (t_at(mtmp->mx, mtmp->my))
            (void) mintrap(mtmp, NO_TRAP_FLAGS);
    } else if (!Blind) {
        Soundeffect(se_bubble_rising, 50);
        Soundeffect(se_loud_pop, 50);
#ifdef ZHLANG
        pline("一个大泡泡升到水面并破裂了。");
#else
        pline("A large bubble rises to the surface and pops.");
#endif
    } else {
        Soundeffect(se_loud_pop, 50);
#ifdef ZHLANG
        You_hear("一声响亮的爆裂声。");
#else
        You_hear("a loud pop.");
#endif
    }
}

/* Gushing forth along LOS from (u.ux, u.uy) */
void
dogushforth(int drinking)
{
    int madepool = 0;

    do_clear_area(u.ux, u.uy, 7, gush, (genericptr_t) &madepool);
    if (!madepool) {
        if (drinking)
#ifdef ZHLANG
            Your("口渴得到了缓解。");
#else
            Your("thirst is quenched.");
#endif
        else
#ifdef ZHLANG
            pline("水喷了你一身。");
#else
            pline("Water sprays all over you.");
#endif
    }
}

staticfn void
gush(coordxy x, coordxy y, genericptr_t poolcnt)
{
    struct monst *mtmp;
    struct trap *ttmp;

    if (((x + y) % 2) || u_at(x, y)
        || (rn2(1 + distmin(u.ux, u.uy, x, y))) || (levl[x][y].typ != ROOM)
        || (sobj_at(BOULDER, x, y)) || nexttodoor(x, y))
        return;

    if ((ttmp = t_at(x, y)) != 0 && !delfloortrap(ttmp))
        return;

    if (!((*(int *) poolcnt)++))
#ifdef ZHLANG
        pline("水从溢出的喷泉中喷涌而出！");
#else
        pline("Water gushes forth from the overflowing fountain!");
#endif

    /* Put a pool at x, y */
    set_levltyp(x, y, POOL);
    levl[x][y].flags = 0;
    /* No kelp! */
    del_engr_at(x, y);
    water_damage_chain(svl.level.objects[x][y], TRUE);

    if ((mtmp = m_at(x, y)) != 0)
        (void) minliquid(mtmp);
    else
        newsym(x, y);
}

/* Find a gem in the sparkling waters. */
staticfn void
dofindgem(void)
{
    if (!Blind)
#ifdef ZHLANG
        You("在闪亮的水中看到了一颗宝石！");
#else
        You("spot a gem in the sparkling waters!");
#endif
    else
#ifdef ZHLANG
        You_feel("这里有颗宝石！");
#else
        You_feel("a gem here!");
#endif
    (void) mksobj_at(rnd_class(DILITHIUM_CRYSTAL, LUCKSTONE - 1), u.ux, u.uy,
                     FALSE, FALSE);
    SET_FOUNTAIN_LOOTED(u.ux, u.uy);
    newsym(u.ux, u.uy);
    exercise(A_WIS, TRUE); /* a discovery! */
}

staticfn boolean
watchman_warn_fountain(struct monst *mtmp)
{
    if (is_watch(mtmp->data) && couldsee(mtmp->mx, mtmp->my)
        && mtmp->mpeaceful) {
        if (!Deaf) {
#ifdef ZHLANG
            pline("%s喊道：", Amonnam(mtmp));
            verbalize("嘿，别再用那个喷泉了！");
#else
            pline("%s yells:", Amonnam(mtmp));
            verbalize("Hey, stop using that fountain!");
#endif
        } else {
#ifdef ZHLANG
            pline("%s认真地%s%s%s！",
                  Amonnam(mtmp),
                  nolimbs(mtmp->data) ? "摇晃着" : "挥舞着",
                  mhis(mtmp),
                  nolimbs(mtmp->data)
                  ? mbodypart(mtmp, HEAD)
                  : makeplural(mbodypart(mtmp, ARM)));
#else
            pline("%s earnestly %s %s %s!",
                  Amonnam(mtmp),
                  nolimbs(mtmp->data) ? "shakes" : "waves",
                  mhis(mtmp),
                  nolimbs(mtmp->data)
                  ? mbodypart(mtmp, HEAD)
                  : makeplural(mbodypart(mtmp, ARM)));
#endif
        }
        return TRUE;
    }
    return FALSE;
}

void
dryup(coordxy x, coordxy y, boolean isyou)
{
    if (IS_FOUNTAIN(levl[x][y].typ)
        && (!rn2(3) || FOUNTAIN_IS_WARNED(x, y))) {
        if (isyou && in_town(x, y) && !FOUNTAIN_IS_WARNED(x, y)) {
            struct monst *mtmp;

            SET_FOUNTAIN_WARNED(x, y);
            /* Warn about future fountain use. */
            mtmp = get_iter_mons(watchman_warn_fountain);
            /* You can see or hear this effect */
            if (!mtmp)
#ifdef ZHLANG
                pline_The("水流减少到只有一滴一滴的了。");
#else
                pline_The("flow reduces to a trickle.");
#endif
            return;
        }
        if (isyou && wizard) {
            if (y_n("Dry up fountain?") == 'n')
                return;
        }
        /* FIXME: sight-blocking clouds should use block_point() when
           being created and unblock_point() when going away, then this
           glyph hackery wouldn't be necessary */
        if (cansee(x, y)) {
            int glyph = glyph_at(x, y);

            if (!glyph_is_cmap(glyph) || glyph_to_cmap(glyph) != S_cloud)
#ifdef ZHLANG
                pline_The("喷泉干涸了！");
#else
                pline_The("fountain dries up!");
#endif
        }
        /* replace the fountain with ordinary floor */
        set_levltyp(x, y, ROOM); /* updates level.flags.nfountains */
        levl[x][y].flags = 0;
        levl[x][y].blessedftn = 0;
        /* The location is seen if the hero/monster is invisible
           or felt if the hero is blind. */
        newsym(x, y);
        if (isyou && in_town(x, y))
            (void) angry_guards(FALSE);
    }
}

/* quaff from a fountain when standing on its location */
void
drinkfountain(void)
{
    /* What happens when you drink from a fountain? */
    boolean mgkftn = (levl[u.ux][u.uy].blessedftn == 1);
    int fate = rnd(30);

    if (Levitation) {
        floating_above("fountain");
        return;
    }

    if (mgkftn && u.uluck >= 0 && fate >= 10) {
        int i, ii, littleluck = (u.uluck < 4);

#ifdef ZHLANG
        pline("哇！这让你感觉棒极了！");
#else
        pline("Wow!  This makes you feel great!");
#endif
        /* blessed restore ability */
        for (ii = 0; ii < A_MAX; ii++)
            if (ABASE(ii) < AMAX(ii)) {
                ABASE(ii) = AMAX(ii);
                disp.botl = TRUE;
            }
        /* gain ability, blessed if "natural" luck is high */
        i = rn2(A_MAX); /* start at a random attribute */
        for (ii = 0; ii < A_MAX; ii++) {
            if (adjattrib(i, 1, littleluck ? -1 : 0) && littleluck)
                break;
            if (++i >= A_MAX)
                i = 0;
        }
        display_nhwindow(WIN_MESSAGE, FALSE);
#ifdef ZHLANG
        pline("一缕蒸汽从喷泉中飘出……");
#else
        pline("A wisp of vapor escapes the fountain...");
#endif
        exercise(A_WIS, TRUE);
        levl[u.ux][u.uy].blessedftn = 0;
        return;
    }

    if (fate < 10) {
#ifdef ZHLANG
        pline_The("清凉的饮料让你精神焕发。");
#else
        pline_The("cool draught refreshes you.");
#endif
        u.uhunger += rnd(10); /* don't choke on water */
        newuhs(FALSE);
        if (mgkftn)
            return;
    } else {
        switch (fate) {
        case 19: /* Self-knowledge */
#ifdef ZHLANG
            You_feel("自我认知增强了……");
#else
            You_feel("self-knowledgeable...");
#endif
            display_nhwindow(WIN_MESSAGE, FALSE);
            enlightenment(MAGICENLIGHTENMENT, ENL_GAMEINPROGRESS);
            exercise(A_WIS, TRUE);
#ifdef ZHLANG
            pline_The("感觉消退了。");
#else
            pline_The("feeling subsides.");
#endif
            break;
        case 20: /* Foul water */
#ifdef ZHLANG
            pline_The("水是污浊的！你恶心呕吐。");
#else
            pline_The("water is foul!  You gag and vomit.");
#endif
            morehungry(rn1(20, 11));
            vomit();
            break;
        case 21: /* Poisonous */
#ifdef ZHLANG
            pline_The("水被污染了！");
#else
            pline_The("water is contaminated!");
#endif
            if (Poison_resistance) {
#ifdef ZHLANG
                pline("也许是附近%s农场的径流。",
                      fruitname(FALSE));
#else
                pline("Perhaps it is runoff from the nearby %s farm.",
                      fruitname(FALSE));
#endif
                losehp(rnd(4), "unrefrigerated sip of juice", KILLED_BY_AN);
                break;
            }
            poison_strdmg(rn1(4, 3), rnd(10), "contaminated water",
                          KILLED_BY);
            exercise(A_CON, FALSE);
            break;
        case 22: /* Fountain of snakes! */
            dowatersnakes();
            break;
        case 23: /* Water demon */
            dowaterdemon();
            break;
        case 24: { /* Maybe curse some items */
            struct obj *obj, *nextobj;
            int buc_changed = 0;

#ifdef ZHLANG
            pline("这水不对劲！");
#else
            pline("This water's no good!");
#endif
            morehungry(rn1(20, 11));
            exercise(A_CON, FALSE);
            /* this is more severe than rndcurse() */
            for (obj = gi.invent; obj; obj = nextobj) {
                nextobj = obj->nobj;
                if (obj->oclass != COIN_CLASS && !obj->cursed && !rn2(5)) {
                    curse(obj);
                    ++buc_changed;
                }
            }
            if (buc_changed)
                update_inventory();
            break;
        }
        case 25: /* See invisible */
            if (Blind) {
                if (Invisible) {
#ifdef ZHLANG
                    You("感觉自己变得透明了。");
#else
                    You("feel transparent.");
#endif
                } else {
#ifdef ZHLANG
                    You("感到非常难为情。");
                    pline("然后这种感觉过去了。");
#else
                    You("feel very self-conscious.");
                    pline("Then it passes.");
#endif
                }
            } else {
#ifdef ZHLANG
                You_see("一个跟踪你的身影。");
                pline("但它消失了。");
#else
                You_see("an image of someone stalking you.");
                pline("But it disappears.");
#endif
            }
            HSee_invisible |= FROMOUTSIDE;
            newsym(u.ux, u.uy);
            exercise(A_WIS, TRUE);
            break;
        case 26: /* See Monsters */
            if (monster_detect((struct obj *) 0, 0))
#ifdef ZHLANG
                pline_The("%s尝起来什么味道都没有。", hliquid("water"));
#else
                pline_The("%s tastes like nothing.", hliquid("water"));
#endif
            exercise(A_WIS, TRUE);
            break;
        case 27: /* Find a gem in the sparkling waters. */
            if (!FOUNTAIN_IS_LOOTED(u.ux, u.uy)) {
                dofindgem();
                break;
            }
            FALLTHROUGH;
            /*FALLTHRU*/
        case 28: /* Water Nymph */
            dowaternymph();
            break;
        case 29: /* Scare */
        {
            struct monst *mtmp;

#ifdef ZHLANG
            pline("这%s让你口臭！",
                  hliquid("water"));
#else
            pline("This %s gives you bad breath!",
                  hliquid("water"));
#endif
            for (mtmp = fmon; mtmp; mtmp = mtmp->nmon) {
                if (DEADMONSTER(mtmp))
                    continue;
                monflee(mtmp, 0, FALSE, FALSE);
            }
            break;
        }
        case 30: /* Gushing forth in this room */
            dogushforth(TRUE);
            break;
        default:
#ifdef ZHLANG
            pline("这温热的%s淡而无味。",
                  hliquid("water"));
#else
            pline("This tepid %s is tasteless.",
                  hliquid("water"));
#endif
            break;
        }
    }
    dryup(u.ux, u.uy, TRUE);
}

/* dip an object into a fountain when standing on its location */
void
dipfountain(struct obj *obj)
{
    int er = ER_NOTHING;
    boolean is_hands = (obj == &hands_obj);

    if (Levitation) {
        floating_above("fountain");
        return;
    }

    if (obj->otyp == LONG_SWORD && u.ulevel >= 5
        && !rn2(Role_if(PM_KNIGHT) ? 6 : 30)
        /* once upon a time it was possible to poly N daggers into N swords */
        && obj->quan == 1L && !obj->oartifact
        && !exist_artifact(LONG_SWORD, artiname(ART_EXCALIBUR))) {
        static const char lady[] = "Lady of the Lake";

        if (u.ualign.type != A_LAWFUL) {
            /* Ha!  Trying to cheat her. */
#ifdef ZHLANG
            pline("一股冰冷的雾气从%s中升起，包裹住了剑。",
                  hliquid("water"));
#else
            pline("A freezing mist rises from the %s"
                  " and envelopes the sword.",
                  hliquid("water"));
#endif
#ifdef ZHLANG
            pline_The("喷泉消失了！");
#else
            pline_The("fountain disappears!");
#endif
            curse(obj);
            if (obj->spe > -6 && !rn2(3))
                obj->spe--;
            obj->oerodeproof = FALSE;
            exercise(A_WIS, FALSE);
            livelog_printf(LL_ARTIFACT,
                           "was denied %s!  The %s has deemed %s unworthy",
                           artiname(ART_EXCALIBUR), lady, uhim());
        } else {
            /* The lady of the lake acts! - Eric Backus */
            /* Be *REAL* nice */
#ifdef ZHLANG
            pline("从幽暗的深处，一只手伸上来祝福了这把剑。");
            pline("手缩回时，喷泉消失了！");
#else
            pline(
              "From the murky depths, a hand reaches up to bless the sword.");
            pline("As the hand retreats, the fountain disappears!");
#endif
            obj = oname(obj, artiname(ART_EXCALIBUR),
                        ONAME_VIA_DIP | ONAME_KNOW_ARTI);
            discover_artifact(ART_EXCALIBUR);
            bless(obj);
            obj->oeroded = obj->oeroded2 = 0;
            obj->oerodeproof = TRUE;
            exercise(A_WIS, TRUE);
            livelog_printf(LL_ARTIFACT, "was given %s by the %s",
                           artiname(ART_EXCALIBUR), lady);
        }
        update_inventory();
        set_levltyp(u.ux, u.uy, ROOM); /* updates level.flags.nfountains */
        levl[u.ux][u.uy].flags = 0;
        newsym(u.ux, u.uy);
        if (in_town(u.ux, u.uy))
            (void) angry_guards(FALSE);
        return;
    } else if (is_hands || obj == uarmg) {
        er = wash_hands();
    } else {
        er = water_damage(obj, NULL, TRUE);
    }

    if (er == ER_DESTROYED || (er != ER_NOTHING && !rn2(2))) {
        return; /* no further effect */
    }

    switch (rnd(30)) {
    case 16: /* Curse the item */
        if (!is_hands && obj->oclass != COIN_CLASS && !obj->cursed) {
            curse(obj);
        }
        break;
    case 17:
    case 18:
    case 19:
    case 20: /* Uncurse the item */
        if (!is_hands && obj->cursed) {
            if (!Blind)
#ifdef ZHLANG
                pline_The("%s发光了一会儿。", hliquid("water"));
#else
                pline_The("%s glows for a moment.", hliquid("water"));
#endif
            uncurse(obj);
        } else {
#ifdef ZHLANG
            pline("一种失落感涌上心头。");
#else
            pline("A feeling of loss comes over you.");
#endif
        }
        break;
    case 21: /* Water Demon */
        dowaterdemon();
        break;
    case 22: /* Water Nymph */
        dowaternymph();
        break;
    case 23: /* an Endless Stream of Snakes */
        dowatersnakes();
        break;
    case 24: /* Find a gem */
        if (!FOUNTAIN_IS_LOOTED(u.ux, u.uy)) {
            dofindgem();
            break;
        }
        FALLTHROUGH;
        /*FALLTHRU*/
    case 25: /* Water gushes forth */
        dogushforth(FALSE);
        break;
    case 26: /* Strange feeling */
#ifdef ZHLANG
        pline("一阵奇怪的刺痛感沿着你的%s蔓延。", body_part(ARM));
#else
        pline("A strange tingling runs up your %s.", body_part(ARM));
#endif
        break;
    case 27: /* Strange feeling */
#ifdef ZHLANG
        You_feel("突然一阵寒意。");
#else
        You_feel("a sudden chill.");
#endif
        break;
    case 28: /* Strange feeling */
#ifdef ZHLANG
        pline("一股想洗澡的冲动淹没了你。");
#else
        pline("An urge to take a bath overwhelms you.");
#endif
        {
            long money = money_cnt(gi.invent);
            struct obj *otmp, *nextobj;

            if (money > 10) {
                /* Amount to lose.  Might get rounded up as fountains don't
                 * pay change... */
                money = somegold(money) / 10;
                for (otmp = gi.invent; otmp && money > 0; otmp = nextobj) {
                    nextobj = otmp->nobj;
                    if (otmp->oclass == COIN_CLASS) {
                        int denomination = objects[otmp->otyp].oc_cost;
                        long coin_loss =
                            (money + denomination - 1) / denomination;
                        coin_loss = min(coin_loss, otmp->quan);
                        otmp->quan -= coin_loss;
                        money -= coin_loss * denomination;
                        if (!otmp->quan)
                            delobj(otmp);
                    }
                }
#ifdef ZHLANG
                You("在喷泉里弄丢了一些金币！");
#else
                You("lost some of your gold in the fountain!");
#endif
                CLEAR_FOUNTAIN_LOOTED(u.ux, u.uy);
                exercise(A_WIS, FALSE);
            }
        }
        break;
    case 29: /* You see coins */
        /* We make fountains have more coins the closer you are to the
         * surface.  After all, there will have been more people going
         * by.  Just like a shopping mall!  Chris Woodbury  */

        if (FOUNTAIN_IS_LOOTED(u.ux, u.uy))
            break;
        SET_FOUNTAIN_LOOTED(u.ux, u.uy);
        (void) mkgold((long) (rnd((dunlevs_in_dungeon(&u.uz) - dunlev(&u.uz)
                                   + 1) * 2) + 5),
                      u.ux, u.uy);
        if (!Blind)
#ifdef ZHLANG
            pline("在你下方远处，你看到%s中闪闪发光的硬币。",
                  hliquid("water"));
#else
            pline("Far below you, you see coins glistening in the %s.",
                  hliquid("water"));
#endif
        exercise(A_WIS, TRUE);
        newsym(u.ux, u.uy);
        break;
    default:
        if (er == ER_NOTHING)
            pline1(nothing_seems_to_happen);
        break;
    }
    update_inventory();
    dryup(u.ux, u.uy, TRUE);
}

/* dipping '-' in fountain, pool, or sink */
int
wash_hands(void)
{
    const char *hands = makeplural(body_part(HAND));
    int res = ER_NOTHING;
    boolean was_glib = !!Glib;

#ifdef ZHLANG
    You("在%s中洗了洗你的%s。", hliquid("water"),
        uarmg ? "戴手套的手" : hands);
#else
    You("wash your %s%s in the %s.", uarmg ? "gloved " : "", hands,
        hliquid("water"));
#endif
    if (Glib) {
        make_glib(0);
#ifdef ZHLANG
        Your("%s不再滑腻了。", fingers_or_gloves(TRUE));
#else
        Your("%s are no longer slippery.", fingers_or_gloves(TRUE));
#endif
    }
    if (uarmg)
        res = water_damage(uarmg, (const char *) 0, TRUE);
    /* not what ER_GREASED is for, but the checks in dipfountain just
       compare the result to ER_DESTROYED and ER_NOTHING, so it works */
    if (was_glib && res == ER_NOTHING)
        res = ER_GREASED;
    return res;
}

/* convert a sink into a fountain */
void
breaksink(coordxy x, coordxy y)
{
    if (cansee(x, y) || u_at(x, y))
#ifdef ZHLANG
        pline_The("管道破裂了！水喷涌而出！");
#else
        pline_The("pipes break!  Water spurts out!");
#endif
    /* updates level.flags.nsinks and level.flags.nfountains */
    set_levltyp(x, y, FOUNTAIN);
    levl[x][y].looted = 0;
    levl[x][y].blessedftn = 0;
    SET_FOUNTAIN_LOOTED(x, y);
    newsym(x, y);
}

/* quaff from a sink while standing on its location */
void
drinksink(void)
{
    struct obj *otmp;
    struct monst *mtmp;

    if (Levitation) {
        floating_above("sink");
        return;
    }
    switch (rn2(20)) {
    case 0:
#ifdef ZHLANG
        You("喝了一小口冰凉的%s。", hliquid("water"));
#else
        You("take a sip of very cold %s.", hliquid("water"));
#endif
        break;
    case 1:
#ifdef ZHLANG
        You("喝了一小口温热的%s。", hliquid("water"));
#else
        You("take a sip of very warm %s.", hliquid("water"));
#endif
        break;
    case 2:
#ifdef ZHLANG
        You("喝了一小口滚烫的%s。", hliquid("water"));
#else
        You("take a sip of scalding hot %s.", hliquid("water"));
#endif
        if (Fire_resistance) {
#ifdef ZHLANG
            pline("喝起来还挺美味的。");
#else
            pline("It seems quite tasty.");
#endif
            monstseesu(M_SEEN_FIRE);
        } else {
            losehp(rnd(6), "sipping boiling water", KILLED_BY);
            monstunseesu(M_SEEN_FIRE);
        }
        /* boiling water burns considered fire damage */
        break;
    case 3:
        if (svm.mvitals[PM_SEWER_RAT].mvflags & G_GONE)
#ifdef ZHLANG
            pline_The("水槽看起来很脏。");
#else
            pline_The("sink seems quite dirty.");
#endif
        else {
            mtmp = makemon(&mons[PM_SEWER_RAT], u.ux, u.uy, MM_NOMSG);
            if (mtmp)
#ifdef ZHLANG
                pline("呀！水槽里有%s！",
                      (Blind || !canspotmon(mtmp)) ? "某种扭动的东西"
                                                   : a_monnam(mtmp));
#else
                pline("Eek!  There's %s in the sink!",
                      (Blind || !canspotmon(mtmp)) ? "something squirmy"
                                                   : a_monnam(mtmp));
#endif
        }
        break;
    case 4:
        for (;;) {
            otmp = mkobj(POTION_CLASS, FALSE);
            if (otmp->otyp != POT_WATER)
                break;
            /* reject water and try again */
            obfree(otmp, (struct obj *) 0);
        }
        otmp->cursed = otmp->blessed = 0;
#ifdef ZHLANG
        pline("从水龙头里流出了一些%s液体。",
              Blind ? "奇怪的" : hcolor(OBJ_DESCR(objects[otmp->otyp])));
#else
        pline("Some %s liquid flows from the faucet.",
              Blind ? "odd" : hcolor(OBJ_DESCR(objects[otmp->otyp])));
#endif
        if(!(Blind || Hallucination))
            observe_object(otmp);
        otmp->quan++;       /* Avoid panic upon useup() */
        otmp->fromsink = 1; /* kludge for docall() */
        (void) dopotion(otmp);
        obfree(otmp, (struct obj *) 0);
        break;
    case 5:
        if (!(levl[u.ux][u.uy].looted & S_LRING)) {
#ifdef ZHLANG
            You("在水槽里发现了一枚戒指！");
#else
            You("find a ring in the sink!");
#endif
            (void) mkobj_at(RING_CLASS, u.ux, u.uy, TRUE);
            levl[u.ux][u.uy].looted |= S_LRING;
            exercise(A_WIS, TRUE);
            newsym(u.ux, u.uy);
        } else
#ifdef ZHLANG
            pline("一些脏兮兮的%s从排水管里涌了上来。", hliquid("water"));
#else
            pline("Some dirty %s backs up in the drain.", hliquid("water"));
#endif
        break;
    case 6:
        breaksink(u.ux, u.uy);
        break;
    case 7:
#ifdef ZHLANG
        pline_The("%s仿佛有自己的意志般移动着！", hliquid("water"));
#else
        pline_The("%s moves as though of its own will!", hliquid("water"));
#endif
        if ((svm.mvitals[PM_WATER_ELEMENTAL].mvflags & G_GONE)
            || !makemon(&mons[PM_WATER_ELEMENTAL], u.ux, u.uy, MM_NOMSG))
#ifdef ZHLANG
            pline("但它安静了下来。");
#else
            pline("But it quiets down.");
#endif
        break;
    case 8:
#ifdef ZHLANG
        pline("呀，这%s尝起来太难喝了。", hliquid("water"));
#else
        pline("Yuk, this %s tastes awful.", hliquid("water"));
#endif
        more_experienced(1, 0);
        newexplevel();
        break;
    case 9:
#ifdef ZHLANG
        pline("呕……这尝起来像污水！你吐了。");
#else
        pline("Gaggg... this tastes like sewage!  You vomit.");
#endif
        morehungry(rn1(30 - ACURR(A_CON), 11));
        vomit();
        break;
    case 10:
#ifdef ZHLANG
        pline("这%s含有有毒废物！", hliquid("water"));
#else
        pline("This %s contains toxic wastes!", hliquid("water"));
#endif
        if (!Unchanging) {
#ifdef ZHLANG
            You("经历了一次怪异的变形！");
#else
            You("undergo a freakish metamorphosis!");
#endif
            polyself(POLY_NOFLAGS);
        }
        break;
    /* more odd messages --JJB */
    case 11:
        Soundeffect(se_clanking_pipe, 50);
#ifdef ZHLANG
        You_hear("管道里传来叮当声……");
#else
        You_hear("clanking from the pipes...");
#endif
        break;
    case 12:
        Soundeffect(se_sewer_song, 100);
#ifdef ZHLANG
        You_hear("下水道里传来断断续续的歌声……");
#else
        You_hear("snatches of song from among the sewers...");
#endif
        break;
    case 13:
#ifdef ZHLANG
        pline("呃，好臭啊！");
#else
        pline("Ew, what a stench!");
#endif
        create_gas_cloud(u.ux, u.uy, 1, 4);
        break;
    case 19:
        if (Hallucination) {
#ifdef ZHLANG
            pline("从昏暗的排水管里，一只手伸了上来……——哎呀——");
#else
            pline("From the murky drain, a hand reaches up... --oops--");
#endif
            break;
        }
        FALLTHROUGH;
        /*FALLTHRU*/
    default:
#ifdef ZHLANG
        You("喝了一小口%s的%s。",
            rn2(3) ? (rn2(2) ? "冰凉" : "温热") : "滚烫",
            hliquid("water"));
#else
        You("take a sip of %s %s.",
            rn2(3) ? (rn2(2) ? "cold" : "warm") : "hot",
            hliquid("water"));
#endif
    }
}

/* for #dip(potion.c) when standing on a sink */
void
dipsink(struct obj *obj)
{
    boolean try_call = FALSE,
            not_looted_yet = (levl[u.ux][u.uy].looted & S_LRING) == 0,
            is_hands = (obj == &hands_obj || (uarmg && obj == uarmg));

    if (!rn2(not_looted_yet ? 25 : 15)) {
        /* can't rely on using sink for unlimited scroll blanking; however,
           since sink will be converted into a fountain, hero can dip again */
        breaksink(u.ux, u.uy); /* "The pipes break!  Water spurts out!" */
        if (Glib && is_hands)
    #ifdef ZHLANG
        Your("%s仍然很滑。", fingers_or_gloves(TRUE));
#else
        Your("%s are still slippery.", fingers_or_gloves(TRUE));
#endif
        return;
    } else if (is_hands) {
        (void) wash_hands();
        return;
    } else if (obj->oclass != POTION_CLASS) {
#ifdef ZHLANG
        You("把%s放在水龙头下面。", the(xname(obj)));
#else
        You("hold %s under the tap.", the(xname(obj)));
#endif
        if (water_damage(obj, (const char *) 0, TRUE) == ER_NOTHING)
            pline1(nothing_seems_to_happen);
        return;
    }

    /* at this point the object must be a potion */
#ifdef ZHLANG
    You("把%s%s倒进了排水管。", (obj->quan > 1L ? "其中一个" : ""),
        the(xname(obj)));
#else
    You("pour %s%s down the drain.", (obj->quan > 1L ? "one of " : ""),
        the(xname(obj)));
#endif
    switch (obj->otyp) {
    case POT_POLYMORPH:
        polymorph_sink();
        try_call = TRUE;
        break;
    case POT_OIL:
        if (!Blind) {
#ifdef ZHLANG
            pline("它在水槽上留下了一层油膜。");
#else
            pline("It leaves an oily film on the basin.");
#endif
            try_call = TRUE;
        } else {
            pline1(nothing_seems_to_happen);
        }
        break;
    case POT_ACID:
        /* acts like a drain cleaner product */
        try_call = TRUE;
        if (!Blind) {
#ifdef ZHLANG
            pline_The("排水管好像不那么堵了。");
#else
            pline_The("drain seems less clogged.");
#endif
        } else if (!Deaf) {
#ifdef ZHLANG
            You_hear("一阵吸入声。");
#else
            You_hear("a sucking sound.");
#endif
        } else {
            pline1(nothing_seems_to_happen);
            try_call = FALSE;
        }
        break;
    case POT_LEVITATION:
        sink_backs_up(u.ux, u.uy);
        try_call = TRUE;
        break;
    case POT_OBJECT_DETECTION:
        if (!(levl[u.ux][u.uy].looted & S_LRING)) {
#ifdef ZHLANG
            You("感知到一枚掉进排水管的戒指。");
#else
            You("sense a ring lost down the drain.");
#endif
            try_call = TRUE;
            break;
        }
        FALLTHROUGH;
        /* FALLTHRU */
    case POT_GAIN_LEVEL:
    case POT_GAIN_ENERGY:
    case POT_MONSTER_DETECTION:
    case POT_FRUIT_JUICE:
    case POT_WATER:
        /* potions with no potionbreathe() effects, plus water.  if effects
           are added to potionbreathe these should go to that instead (except
           for water). */
        pline1(nothing_seems_to_happen);
        break;
    default:
        /* hero can feel the vapor on her skin, so no need to check Blind or
           breathless for this message */
#ifdef ZHLANG
        pline("一缕蒸汽升腾而起……");
#else
        pline("A wisp of vapor rises up...");
#endif
        /* NB: potionbreathe calls trycall or makeknown as appropriate */
        if (!breathless(gy.youmonst.data) || haseyes(gy.youmonst.data))
            potionbreathe(obj);
        break;
    }
    if (try_call && obj->dknown)
        trycall(obj);
    useup(obj);
}

/* find a ring in a sink */
void
sink_backs_up(coordxy x, coordxy y)
{
    char buf[BUFSZ];

    if (!Blind)
#ifdef ZHLANG
        Strcpy(buf, "泥泞的废物从排水管里喷了出来");
#else
        Strcpy(buf, "Muddy waste pops up from the drain"); /* Deaf-aware */
#endif
    else if (!Deaf)
#ifdef ZHLANG
        Strcpy(buf, "你听到泼溅声");
#else
        Strcpy(buf, "You hear a sloshing sound"); /* Deaf-aware */
#endif
    else
#ifdef ZHLANG
        Sprintf(buf, "有什么东西溅到了你的%s上", body_part(FACE));
#else
        Sprintf(buf, "Something splashes you in the %s", body_part(FACE));
#endif
#ifdef ZHLANG
    pline("%s%s。", !Deaf ? "咕噜！  " : "", buf);
#else
    pline("%s%s.", !Deaf ? "Flupp!  " : "", buf);
#endif

    if (!(levl[x][y].looted & S_LRING)) { /* once per sink */
        if (!Blind)
#ifdef ZHLANG
            You_see("一枚戒指在其中闪闪发光。");
#else
            You_see("a ring shining in its midst.");
#endif
        (void) mkobj_at(RING_CLASS, x, y, TRUE);
        newsym(x, y);
        exercise(A_DEX, TRUE);
        exercise(A_WIS, TRUE); /* a discovery! */
        levl[x][y].looted |= S_LRING;
    }
}

/*fountain.c*/
