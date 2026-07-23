/* NetHack 5.0	sit.c	$NHDT-Date: 1781973067 2026/06/20 16:31:07 $  $NHDT-Branch: NetHack-5.0 $:$NHDT-Revision: 1.112 $ */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/*-Copyright (c) Robert Patrick Rankin, 2012. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "artifact.h"

staticfn void throne_sit_effect(void);
staticfn int lay_an_egg(void);

/* take away the hero's money */
void
take_gold(void)
{
    struct obj *otmp, *nobj;
    int lost_money = 0;

    for (otmp = gi.invent; otmp; otmp = nobj) {
        nobj = otmp->nobj;
        if (otmp->oclass == COIN_CLASS) {
            lost_money = 1;
            remove_worn_item(otmp, FALSE);
            delobj(otmp);
        }
    }
    if (!lost_money) {
        #ifdef ZHLANG
        You_feel("一种奇怪的感觉。");
        #else
        You_feel("a strange sensation.");
        #endif
    } else {
        #ifdef ZHLANG
        You("注意到你没有金币了！");
        #else
        You("notice you have no gold!");
        #endif
        disp.botl = TRUE;
    }
}

staticfn void special_throne_effect(int effect);

/* maybe do something when hero sits on a throne */
staticfn void
throne_sit_effect(void)
{
    coordxy tx = u.ux, ty = u.uy;

    boolean special_throne = !!In_V_tower(&u.uz);

    if (rnd(6) > 4) { /* [why so convoluted? it's the same as '!rn2(3)'] */
        int effect = rnd(13);

        if (wizard && !iflags.debug_fuzzer) {
            char buf[BUFSZ];
            int which;

            buf[0] = '\0';
            getlin("Throne sit effect (1..13) [0=random]", buf);
            if (buf[0] == '\033') {
                pline("%s", Never_mind);
                return; /* caller will still cause a move to elapse */
            }
            which = atoi(buf);
            if (which >= 1 && which <= 13)
                effect = which;
        }

        if (special_throne) {
            special_throne_effect(effect);
            return;
        }

        switch (effect) {
        case 1:
            (void) adjattrib(rn2(A_MAX), -rn1(4, 3), FALSE);
            losehp(rnd(10), "cursed throne", KILLED_BY_AN);
            break;
        case 2:
            (void) adjattrib(rn2(A_MAX), 1, FALSE);
            break;
        case 3:
            #ifdef ZHLANG
            pline("一股%s电流穿过你的身体！",
                  (Shock_resistance) ? "一" : "巨大的");
            #else
            pline("A%s electric shock shoots through your body!",
                  (Shock_resistance) ? "n" : " massive");
            #endif
            losehp(Shock_resistance ? rnd(6) : rnd(30), "electric chair",
                   KILLED_BY_AN);
            exercise(A_CON, FALSE);
            break;
        case 4:
            #ifdef ZHLANG
            You_feel("好多了，好多了！");
            #else
            You_feel("much, much better!");
            #endif
            if (Upolyd) {
                if (u.mh >= (u.mhmax - 5))
                    u.mhmax += 4;
                u.mh = u.mhmax;
            }
            if (u.uhp >= (u.uhpmax - 5)) {
                u.uhpmax += 4;
                if (u.uhpmax > u.uhppeak)
                    u.uhppeak = u.uhpmax;
            }
            u.uhp = u.uhpmax;
            u.ucreamed = 0;
            make_blinded(0L, TRUE);
            make_sick(0L, (char *) 0, FALSE, SICK_ALL);
            heal_legs(0);
            disp.botl = TRUE;
            break;
        case 5:
            take_gold();
            break;
        case 6:
            if (u.uluck + rn2(5) < 0) {
                #ifdef ZHLANG
                You_feel("你的运气正在改变。");
                #else
                You_feel("your luck is changing.");
                #endif
                change_luck(1);
            } else
                makewish();
            break;
        case 7:
            {
                int cnt = rnd(10);

                /* Magical voice not affected by deafness */
                #ifdef ZHLANG
                pline("一个声音回荡着：");
                #else
                pline("A voice echoes:");
                #endif
                SetVoice((struct monst *) 0, 0, 80, voice_throne);
                #ifdef ZHLANG
                verbalize("你的听众已被召来，%s！",
                          flags.female ? "Dame" : "Sire");
                #else
                verbalize("Thine audience hath been summoned, %s!",
                          flags.female ? "Dame" : "Sire");
                #endif
                while (cnt--)
                    (void) makemon(courtmon(), tx, ty, NO_MM_FLAGS);
                break;
            }
        case 8:
            /* Magical voice not affected by deafness */
            #ifdef ZHLANG
            pline("一个声音回荡着：");
            #else
            pline("A voice echoes:");
            #endif
            SetVoice((struct monst *) 0, 0, 80, voice_throne);
            #ifdef ZHLANG
            verbalize("遵照你的命令，%s...",
                      flags.female ? "Dame" : "Sire");
            #else
            verbalize("By thine Imperious order, %s...",
                      flags.female ? "Dame" : "Sire");
            #endif
            do_genocide(5); /* REALLY|ONTHRONE, see do_genocide() */
            break;
        case 9:
            /* Magical voice not affected by deafness */
            #ifdef ZHLANG
            pline("一个声音回荡着：");
            #else
            pline("A voice echoes:");
            #endif
            SetVoice((struct monst *) 0, 0, 80, voice_throne);
            verbalize(
                 "A curse upon thee for sitting upon this most holy throne!");
            if (Luck > 0) {
                make_blinded(BlindedTimeout + rn1(100, 250), TRUE);
                change_luck((Luck > 1) ? -rnd(2) : -1);
            } else
                rndcurse();
            break;
        case 10:
            if (Luck < 0 || (HSee_invisible & INTRINSIC)) {
                if (svl.level.flags.nommap) {
                    #ifdef ZHLANG
                    pline("一阵可怕的嗡鸣充满你的头部！");
                    #else
                    pline("A terrible drone fills your head!");
                    #endif
                    make_confused((HConfusion & TIMEOUT) + (long) rnd(30),
                                  FALSE);
                } else {
                    #ifdef ZHLANG
                    pline("一个画面在你脑海中形成。");
                    #else
                    pline("An image forms in your mind.");
                    #endif
                    do_mapping();
                }
            } else {
                /* avoid "vision clears" if hero can't see */
                if (!Blind) {
                    #ifdef ZHLANG
                    Your("视野变得清晰。");
                    #else
                    Your("vision becomes clear.");
                    #endif
                } else {
                    int num_of_eyes = eyecount(gy.youmonst.data);
                    const char *eye = body_part(EYE);

                    /* note: 1 eye case won't actually happen--can't
                       sit on throne when poly'd into always-levitating
                       floating eye and can't polymorph into Cyclops */
                    switch (num_of_eyes) { /* 2, 1, or 0 */
                    default:
                    case 2: /* more than 1 eye */
                        eye = makeplural(eye);
                        FALLTHROUGH;
                        /*FALLTHRU*/
                    case 1: /* one eye (Cyclops, floating eye) */
                        #ifdef ZHLANG
                        Your("%s%s...", eye, vtense(eye, "刺痛"));
                        #else
                        Your("%s %s...", eye, vtense(eye, "tingle"));
                        #endif
                        break;
                    case 0: /* no eyes */
                        #ifdef ZHLANG
                        You("你的%s有一种非常奇怪的感觉。",
                            body_part(HEAD));
                        #else
                        You("have a very strange feeling in your %s.",
                            body_part(HEAD));
                        #endif
                        break;
                    }
                }
                HSee_invisible |= FROMOUTSIDE;
                newsym(u.ux, u.uy);
            }
            break;
        case 11:
            if (Luck < 0) {
                #ifdef ZHLANG
                You_feel("受到威胁。");
                #else
                You_feel("threatened.");
                #endif
                aggravate();
            } else {
                #ifdef ZHLANG
                You_feel("一种扭曲的感觉。");
                #else
                You_feel("a wrenching sensation.");
                #endif
                tele(); /* teleport him */
            }
            break;
        case 12:
            #ifdef ZHLANG
            You("获得了洞察！");
            #else
            You("are granted an insight!");
            #endif
            if (gi.invent) {
                /* rn2(5) agrees w/seffects() */
                identify_pack(rn2(5), FALSE);
            }
            break;
        case 13:
            #ifdef ZHLANG
            Your("大脑扭成了麻花！");
            #else
            Your("mind turns into a pretzel!");
            #endif
            make_confused((HConfusion & TIMEOUT) + (long) rn1(7, 16),
                          FALSE);
            break;
        default:
            impossible("throne effect");
            break;
        }
    } else {
        if (is_prince(gy.youmonst.data) || u.uevent.uhand_of_elbereth)
            #ifdef ZHLANG
            You_feel("在这里非常舒适。");
            #else
            You_feel("very comfortable here.");
            #endif
        else
            #ifdef ZHLANG
            You_feel("有些格格不入...");
            #else
            You_feel("somehow out of place...");
            #endif
    }

    /* 5.0: when the random chance for removal is hit, ask for confirmation
       if in wizard mode, and remove the throne even if hero was teleported
       away from it.  [This used to remove a throne at hero's current
       location if there happened to be one, so for the teleport case that
       only happened when teleporting back to the same point where hero
       started from.]  "Analyzing a throne" doesn't really make any sense
       but if the answer is yes than it will vanish in a puff of logic. */
    if (!special_throne &&
        !rn2(3) && (!wizard || y_n("Analyze throne?") == 'y')) {
        levl[tx][ty].typ = ROOM, levl[tx][ty].flags = 0;
        map_background(tx, ty, FALSE);
        newsym_force(tx, ty);
        /* "[God] promptly vanishes in a puff of logic" is from
           Douglas Adams' _The_Hitchhiker's_Guide_to_the_Galaxy_. */
        #ifdef ZHLANG
        pline_The("王座%s在逻辑烟雾中。",
                  cansee(tx, ty) ? "消失了" : "已消失");
        #else
        pline_The("throne %s in a puff of logic.",
                  cansee(tx, ty) ? "vanishes" : "has vanished");
        #endif
    }
}

/* special throne in Vlad's tower: effect is 1 to 13 inclusive */
staticfn void
special_throne_effect(int effect) {
    coordxy tx = u.ux, ty = u.uy;

    switch (effect) {
    case 1:
    case 2:
    case 3:
    case 4:
        /* 4 chances of a wish, but then the throne disappears.

           This is the only way the throne can disappear from sitting
           on it, so if you sit on it enough (enduring the negative
           effects) you are guaranteed an eventual wish. */
        makewish();
        levl[tx][ty].typ = ROOM, levl[tx][ty].flags = 0;
        map_background(tx, ty, FALSE);
        newsym_force(tx, ty);
#ifdef ZHLANG
        pline_The("王座耗尽了力量，化为碎片。");
#else
        pline_The("throne disintegrates, having spent its power.");
#endif
        break;
    case 5:
        /* permanent level drain */
        #ifdef ZHLANG
        pline("坐在王座上是一次可怕的经历。");
        #else
        pline("Sitting on the throne was a terrible experience.");
        #endif
        if (!Drain_resistance) {
            losexp("a bad experience sitting on a throne");
            if (u.ulevelmax > u.ulevel)
                u.ulevelmax -= 1;
        }
        break;
    case 6:
    {
        /* grease hands and inventory

           Same rules for which items can be affected as grease_ok in apply.c */
        struct obj *otmp;

        #ifdef ZHLANG
        pline("油腊的液体喷了你一身！");
        #else
        pline("A greasy liquid sprays all over you!");
        #endif
        for (otmp = gi.invent; otmp; otmp = otmp->nobj)
            if (otmp->oclass != COIN_CLASS)
                otmp->greased = 1;
        make_glib(rn1(101, 100));
        update_inventory();
        break;
    }
    case 7:
        /* lose an intrinsic */
        attrcurse();
#ifdef ZHLANG
        pline_The("王座似乎在嘲笑。");
#else
        pline_The("throne somehow seems to be amused.");
#endif
        break;
    case 8:
    {
        /* level teleport to Vibrating Square level */
        d_level vs_level;
        find_hell(&vs_level);
        vs_level.dlevel = svd.dungeons[vs_level.dnum].num_dunlevs - 1;
        if (u.uhave.amulet)
            #ifdef ZHLANG
            You_feel("瞬间极度迷失方向。");
            #else
            You_feel("extremely disoriented for a moment.");
            #endif
        else
            schedule_goto(
                &vs_level, UTOTYPE_NONE, (char *) 0,
                "You feel extremely out of place.");
        break;
    }
    case 9:
    {
        /* summon demons; a NULL argument to msummon summons demons as
           though they were summoned by the Wizard of Yendor */
#ifdef ZHLANG
        pline_The("王座似乎在呼救！");
#else
        pline_The("throne seeems to be calling for help!");
#endif
        msummon(NULL);
        msummon(NULL);
        msummon(NULL);
        break;
    }
    case 10:
    {
        /* confused blessed remove curse effect */
        struct obj fake_spellbook;
        long save_confusion = HConfusion;

        fake_spellbook = cg.zeroobj;
        fake_spellbook.otyp = SPE_REMOVE_CURSE;
        fake_spellbook.oclass = SPBOOK_CLASS;
        fake_spellbook.blessed = 1;
        HConfusion = 1L;
        (void) seffects(&fake_spellbook);
        HConfusion = save_confusion;
        break;
    }
    case 11:
        /* polymorph effect (not blocked by magic resistance, but other things
           that protect from polymorphs work) */
        if (is_vampire(gy.youmonst.data)) {
            #ifdef ZHLANG
            You_feel("不配。");
            #else
            You_feel("unworthy.");
            #endif
        } else {
            #ifdef ZHLANG
            pline("这王座不是为你这样的人准备的！");
            #else
            pline("This throne was not meant for those such as you!");
            #endif
            #ifdef ZHLANG
            You_feel("你身上发生了变化。");
            #else
            You_feel("a change coming over you.");
            #endif
            polyself(POLY_NOFLAGS);
        }
        break;
    case 12:
        /* acid damage */
        #ifdef ZHLANG
        pline("王座上覆盖着酸液！");
        #else
        pline("The throne is covered in acid!");
        #endif
        losehp(Acid_resistance ? rnd(16) : rnd(80), "acidic chair",
               KILLED_BY_AN);
        exercise(A_CON, FALSE);
        break;
    case 13:
    {
        /* ability shuffle */
        int ability;
        #ifdef ZHLANG
        pline("当你坐上王座，你的身体和思维开始扭曲。");
        #else
        pline("As you sit on the throne, your body and mind start to warp.");
        #endif
        for (ability = 0; ability < A_MAX; ++ability) {
            adjattrib(ability, rn2(5) - 2, -1);
        }
        break;
    }
    }
}

/* hero lays an egg */
staticfn int
lay_an_egg(void)
{
    struct obj *uegg;

    if (!flags.female) {
        #ifdef ZHLANG
        pline("%s无法产蛋！",
              Hallucination
              ? "你可能觉得自己是鸭嘴兽，但雄性仍然"
              : "雄性");
        #else
        pline("%s can't lay eggs!",
              Hallucination
              ? "You may think you are a platypus, but a male still"
              : "Males");
        #endif
        return ECMD_OK;
    } else if (u.uhunger < (int) objects[EGG].oc_nutrition) {
        #ifdef ZHLANG
        You("没有足够的能量产卵。");
        #else
        You("don't have enough energy to lay an egg.");
        #endif
        return ECMD_OK;
    } else if (eggs_in_water(gy.youmonst.data)) {
        if (!(Underwater || Is_waterlevel(&u.uz))) {
            #ifdef ZHLANG
            pline("你不是溅水鱼。");
            #else
            pline("A splash tetra you are not.");
            #endif
            return ECMD_OK;
        }
        if (Upolyd
            && (gy.youmonst.data == &mons[PM_GIANT_EEL]
                || gy.youmonst.data == &mons[PM_ELECTRIC_EEL])) {
            #ifdef ZHLANG
            You("渴望马尾藻海。");
            #else
            You("yearn for the Sargasso Sea.");
            #endif
            return ECMD_OK;
        }
    }
    uegg = mksobj(EGG, FALSE, FALSE);
    uegg->spe = 1;
    uegg->quan = 1L;
    uegg->owt = weight(uegg);
    /* this sets hatch timers if appropriate */
    set_corpsenm(uegg, egg_type_from_parent(u.umonnum, FALSE));
    uegg->known = 1;
    observe_object(uegg);
    #ifdef ZHLANG
    You("%s an egg.", eggs_in_water(gy.youmonst.data) ? "产卵" : "产下");
    #else
    You("%s an egg.", eggs_in_water(gy.youmonst.data) ? "spawn" : "lay");
    #endif
    dropy(uegg);
    stackobj(uegg);
    morehungry((int) objects[EGG].oc_nutrition);
    return ECMD_TIME;
}

/* #sit command */
int
dosit(void)
{
#ifdef ZHLANG
    static const char sit_message[] = "坐在%s上。";
#else
    static const char sit_message[] = "sit on the %s.";
#endif
    struct trap *trap = t_at(u.ux, u.uy);
    int typ = levl[u.ux][u.uy].typ;

    if (u.usteed) {
        #ifdef ZHLANG
        You("已经坐在%s上了。", mon_nam(u.usteed));
        #else
        You("are already sitting on %s.", mon_nam(u.usteed));
        #endif
        return ECMD_OK;
    }
    if (u.uundetected && is_hider(gy.youmonst.data)
        && u.umonnum != PM_TRAPPER) /* trapper can stay hidden on floor */
        u.uundetected = 0; /* no longer on the ceiling */

    if (!can_reach_floor(FALSE)) {
        if (u.uswallow)
#ifdef ZHLANG
            There("这里没有座位！");
#else
            There("are no seats in here!");
#endif
        else if (Levitation)
            #ifdef ZHLANG
            You("原地翻滚。");
            #else
            You("tumble in place.");
            #endif
        else
            #ifdef ZHLANG
            You("坐在空中。");
            #else
            You("are sitting on air.");
            #endif
        return ECMD_OK;
    } else if (u.ustuck && !sticks(gy.youmonst.data)) {
        /* holding monster is next to hero rather than beneath, but
           hero is in no condition to actually sit at has/her own spot */
        if (humanoid(u.ustuck->data))
            #ifdef ZHLANG
            pline("%s不会提供%s的大腿。", Monnam(u.ustuck), mhis(u.ustuck));
            #else
            pline("%s won't offer %s lap.", Monnam(u.ustuck), mhis(u.ustuck));
            #endif
        else
            #ifdef ZHLANG
            pline("%s没有大腿。", Monnam(u.ustuck));
            #else
            pline("%s has no lap.", Monnam(u.ustuck));
            #endif
        return ECMD_OK;
    } else if (is_pool(u.ux, u.uy) && !Underwater) { /* water walking */
        goto in_water;
    } else if (Upolyd && u.umonnum == PM_GREMLIN
               && (levl[u.ux][u.uy].typ == FOUNTAIN || is_pool(u.ux, u.uy))) {
        goto in_water;
    }

    if (OBJ_AT(u.ux, u.uy)
        /* ensure we're not standing on the precipice */
        && !(uteetering_at_seen_pit(trap) || uescaped_shaft(trap))) {
        struct obj *obj;

        obj = svl.level.objects[u.ux][u.uy];
        if (gy.youmonst.data->mlet == S_DRAGON && obj->oclass == COIN_CLASS) {
            #ifdef ZHLANG
            You("盘绕在你的%s宝库上。",
                (obj->quan + money_cnt(gi.invent) < u.ulevel * 1000)
                ? "微薄的" : "");
            #else
            You("coil up around your %shoard.",
                (obj->quan + money_cnt(gi.invent) < u.ulevel * 1000)
                ? "meager " : "");
            #endif
        } else if (obj->otyp == TOWEL) {
            #ifdef ZHLANG
            pline("现在可能不是野餐的好时候...");
            #else
            pline("It's probably not a good time for a picnic...");
            #endif
        } else {
            if (slithy(gy.youmonst.data))
                #ifdef ZHLANG
                You("盘绕在%s上。", the(xname(obj)));
                #else
                You("coil up around %s.", the(xname(obj)));
                #endif
            else
                #ifdef ZHLANG
                You("坐在%s上。", the(xname(obj)));
                #else
                You("sit on %s.", the(xname(obj)));
                #endif
            if (obj->otyp == CORPSE && amorphous(&mons[obj->corpsenm]))
                #ifdef ZHLANG
                pline("它软绵绵的...");
                #else
                pline("It's squishy...");
                #endif
            else if (obj->otyp == CREAM_PIE) {
                 if (!Deaf) {
                   Soundeffect(se_squelch, 30);
                   #ifdef ZHLANG
                   pline("噗噔！");
                   #else
                   pline("Squelch!");
                   #endif
                }
                useupf(obj, obj->quan);
            } else if (!(Is_box(obj)
                         || objects[obj->otyp].oc_material == CLOTH))
                #ifdef ZHLANG
                pline("它不太舒服...");
                #else
                pline("It's not very comfortable...");
                #endif
        }
    } else if (trap != 0 || (u.utrap && (u.utraptype >= TT_LAVA))) {
        if (u.utrap) {
            exercise(A_WIS, FALSE); /* you're getting stuck longer */
            if (u.utraptype == TT_BEARTRAP) {
                #ifdef ZHLANG
                You_cant("脚%s被捕熊夹夹着，无法坐下。",
                         body_part(FOOT));
                #else
                You_cant("sit down with your %s in the bear trap.",
                         body_part(FOOT));
                #endif
                u.utrap++;
            } else if (u.utraptype == TT_PIT) {
                if (trap && trap->ttyp == SPIKED_PIT) {
                    #ifdef ZHLANG
                    You("坐在了尖刺上。好痛！");
                    #else
                    You("sit down on a spike.  Ouch!");
                    #endif
                    losehp(Half_physical_damage ? rn2(2) : 1,
                           "sitting on an iron spike", KILLED_BY);
                    exercise(A_STR, FALSE);
                } else
                    #ifdef ZHLANG
                    You("在陷阱里坐下了。");
                    #else
                    You("sit down in the pit.");
                    #endif
                u.utrap += rn2(5);
            } else if (u.utraptype == TT_WEB) {
                #ifdef ZHLANG
                You("坐在蜘蛛网上，被缠得更紧了！");
                #else
                You("sit in the spider web and get entangled further!");
                #endif
                u.utrap += rn1(10, 5);
            } else if (u.utraptype == TT_LAVA) {
                /* Must have fire resistance or they'd be dead already */
                #ifdef ZHLANG
                You("坐在%s中！", hliquid("lava"));
                #else
                You("sit in the %s!", hliquid("lava"));
                #endif
                if (Slimed)
                    burn_away_slime();
                u.utrap += rnd(4);
                losehp(d(2, 10), "sitting in lava",
                       KILLED_BY); /* lava damage */
            } else if (u.utraptype == TT_INFLOOR
                       || u.utraptype == TT_BURIEDBALL) {
                #ifdef ZHLANG
                You_cant("无法调整姿势坐下！");
                #else
                You_cant("maneuver to sit!");
                #endif
                u.utrap++;
            }
        } else {
            /* when flying, "you land" might need some refinement; it sounds
               as if you're staying on the ground but you will immediately
               take off again unless you become stuck in a holding trap */
            #ifdef ZHLANG
            You("%s.", Flying ? "降落" : "坐下");
            #else
            You("%s.", Flying ? "land" : "sit down");
            #endif
            dotrap(trap, VIASITTING);
        }
    } else if ((Underwater || Is_waterlevel(&u.uz))
                && !eggs_in_water(gy.youmonst.data)) {
        if (Is_waterlevel(&u.uz))
#ifdef ZHLANG
            There("附近没有漂浮的坐垫。");
#else
            There("are no cushions floating nearby.");
#endif
        else
            #ifdef ZHLANG
            You("坐在泥浦的底部。");
            #else
            You("sit down on the muddy bottom.");
            #endif
    } else if (is_pool(u.ux, u.uy) && !eggs_in_water(gy.youmonst.data)) {
 in_water:
        #ifdef ZHLANG
        You("坐在%s中。", hliquid("water"));
        #else
        You("sit in the %s.", hliquid("water"));
        #endif
        if (Upolyd && u.umonnum == PM_GREMLIN) {
            if (split_mon(&gy.youmonst, (struct monst *) 0)) {
                if (levl[u.ux][u.uy].typ == FOUNTAIN)
                    dryup(u.ux, u.uy, TRUE);
            }
            /* splitting--or failing to do so--protects gear from the water */
        } else {
            if (!rn2(10) && uarm)
                (void) water_damage(uarm, "armor", TRUE);
            if (!rn2(10) && uarmf && uarmf->otyp != WATER_WALKING_BOOTS)
                (void) water_damage(uarm, "armor", TRUE);
        }
    } else if (IS_SINK(typ)) {
#ifdef ZHLANG
        You(sit_message, "水槽");
#else
        You(sit_message, defsyms[S_sink].explanation);
#endif
        #ifdef ZHLANG
        Your("%s弄湿了。",
             humanoid(gy.youmonst.data) ? "rump" : "underside");
        #else
        Your("%s gets wet.",
             humanoid(gy.youmonst.data) ? "rump" : "underside");
        #endif
    } else if (IS_ALTAR(typ)) {
        You(sit_message, defsyms[S_altar].explanation);
        altar_wrath(u.ux, u.uy);
    } else if (IS_GRAVE(typ)) {
        You(sit_message, defsyms[S_grave].explanation);
    } else if (typ == STAIRS) {
#ifdef ZHLANG
        You(sit_message, "楼梯");
#else
        You(sit_message, "stairs");
#endif
    } else if (typ == LADDER) {
#ifdef ZHLANG
        You(sit_message, "梯子");
#else
        You(sit_message, "ladder");
#endif
    } else if (is_lava(u.ux, u.uy)) {
        /* must be WWalking */
        You(sit_message, hliquid("lava"));
        burn_away_slime();
        if (likes_lava(gy.youmonst.data)) {
    #ifdef ZHLANG
        pline_The("%s感觉温暖。", hliquid("lava"));
#else
        pline_The("%s feels warm.", hliquid("lava"));
#endif
            return ECMD_TIME;
        }
#ifdef ZHLANG
        pline_The("%s烧伤了你！", hliquid("lava"));
#else
        pline_The("%s burns you!", hliquid("lava"));
#endif
        losehp(d((Fire_resistance ? 2 : 10), 10), /* lava damage */
               "sitting on lava", KILLED_BY);
    } else if (is_ice(u.ux, u.uy)) {
        You(sit_message, defsyms[S_ice].explanation);
        if (!Cold_resistance)
#ifdef ZHLANG
            pline_The("冰感觉冰冷。");
#else
            pline_The("ice feels cold.");
#endif
    } else if (typ == DRAWBRIDGE_DOWN) {
#ifdef ZHLANG
        You(sit_message, "吊桥");
#else
        You(sit_message, "drawbridge");
#endif
    } else if (IS_THRONE(typ)) {
        You(sit_message, defsyms[S_throne].explanation);
        throne_sit_effect();
    } else if (lays_eggs(gy.youmonst.data)) {
        return lay_an_egg();
    } else {
        #ifdef ZHLANG
        pline("坐在%s上有趣吗？", surface(u.ux, u.uy));
        #else
        pline("Having fun sitting on the %s?", surface(u.ux, u.uy));
        #endif
    }
    return ECMD_TIME;
}

/* curse a few inventory items at random! */
void
rndcurse(void)
{
    int nobj = 0;
    int cnt, onum;
    struct obj *otmp;
#ifdef ZHLANG
    static const char mal_aura[] = "感到一股恶意灵气包围了%s。";
#else
    static const char mal_aura[] = "feel a malignant aura surround %s.";
#endif

    if (u_wield_art(ART_MAGICBANE) && rn2(20)) {
#ifdef ZHLANG
        You(mal_aura, "吸魔剑刃");
#else
        You(mal_aura, "the magic-absorbing blade");
#endif
        return;
    }

    if (Antimagic) {
        shieldeff(u.ux, u.uy);
    }

#ifdef ZHLANG
    You(mal_aura, "你");
#else
    You(mal_aura, "you");
#endif

    for (otmp = gi.invent; otmp; otmp = otmp->nobj) {
        /* gold isn't subject to being cursed or blessed */
        if (otmp->oclass == COIN_CLASS)
            continue;
        nobj++;
    }
    cnt = rnd(6 / ((!!Antimagic) + (!!Half_spell_damage) + 1));
    if (nobj) {
        for (; cnt > 0; cnt--) {
            onum = rnd(nobj);
            for (otmp = gi.invent; otmp; otmp = otmp->nobj) {
                /* as above */
                if (otmp->oclass == COIN_CLASS)
                    continue;
                if (--onum == 0)
                    break; /* found the target */
            }
            /* the !otmp case should never happen; picking an already
               cursed item happens--avoid "resists" message in that case */
            if (!otmp || otmp->cursed)
                continue; /* next target */

            if (otmp->oartifact && spec_ability(otmp, SPFX_INTEL)
                && rn2(10) < 8) {
                #ifdef ZHLANG
                pline("%s!", Tobjnam(otmp, "抵抗"));
                #else
                pline("%s!", Tobjnam(otmp, "resist"));
                #endif
                continue;
            }

            if (otmp->blessed)
                unbless(otmp);
            else
                curse(otmp);
        }
        update_inventory();
    }

    /* treat steed's saddle as extended part of hero's inventory */
    if (u.usteed && !rn2(4) && (otmp = which_armor(u.usteed, W_SADDLE)) != 0
        && !otmp->cursed) { /* skip if already cursed */
        if (otmp->blessed)
            unbless(otmp);
        else
            curse(otmp);
        if (!Blind) {
            #ifdef ZHLANG
            pline("%s%s。", Yobjnam2(otmp, "发光"),
                  hcolor(otmp->cursed ? NH_BLACK : (const char *) "棕色"));
            #else
            pline("%s %s.", Yobjnam2(otmp, "glow"),
                  hcolor(otmp->cursed ? NH_BLACK : (const char *) "brown"));
            #endif
            otmp->bknown = Hallucination ? 0 : 1; /* bypass set_bknown() */
        } else {
            otmp->bknown = 0; /* bypass set_bknown() */
        }
    }
}

/* remove a random INTRINSIC ability from hero.
   returns the intrinsic property which was removed,
   or 0 if nothing was removed. */
int
attrcurse(void)
{
    int ret = 0;

    switch (rnd(11)) {
    case 1:
        if (HFire_resistance & INTRINSIC) {
            HFire_resistance &= ~INTRINSIC;
            #ifdef ZHLANG
            You_feel("更暖了。");
            #else
            You_feel("warmer.");
            #endif
            ret = FIRE_RES;
            break;
        }
        FALLTHROUGH;
        /*FALLTHRU*/
    case 2:
        if (HTeleportation & INTRINSIC) {
            HTeleportation &= ~INTRINSIC;
            #ifdef ZHLANG
            You_feel("不那么跳脱了。");
            #else
            You_feel("less jumpy.");
            #endif
            ret = TELEPORT;
            break;
        }
        FALLTHROUGH;
        /*FALLTHRU*/
    case 3:
        if (HPoison_resistance & INTRINSIC) {
            HPoison_resistance &= ~INTRINSIC;
            #ifdef ZHLANG
            You_feel("有点恶心！");
            #else
            You_feel("a little sick!");
            #endif
            ret = POISON_RES;
            break;
        }
        FALLTHROUGH;
        /*FALLTHRU*/
    case 4:
        if (HTelepat & INTRINSIC) {
            HTelepat &= ~INTRINSIC;
            if (Blind && !Blind_telepat)
                see_monsters(); /* Can't sense mons anymore! */
            #ifdef ZHLANG
            Your("感官失效了！");
            #else
            Your("senses fail!");
            #endif
            ret = TELEPAT;
            break;
        }
        FALLTHROUGH;
        /*FALLTHRU*/
    case 5:
        if (HCold_resistance & INTRINSIC) {
            HCold_resistance &= ~INTRINSIC;
            #ifdef ZHLANG
            You_feel("更凉了。");
            #else
            You_feel("cooler.");
            #endif
            ret = COLD_RES;
            break;
        }
        FALLTHROUGH;
        /*FALLTHRU*/
    case 6:
        if (HInvis & INTRINSIC) {
            HInvis &= ~INTRINSIC;
            #ifdef ZHLANG
            You_feel("多疑。");
            #else
            You_feel("paranoid.");
            #endif
            ret = INVIS;
            break;
        }
        FALLTHROUGH;
        /*FALLTHRU*/
    case 7:
        if (HSee_invisible & INTRINSIC) {
            HSee_invisible &= ~INTRINSIC;
            if (!See_invisible) {
                set_mimic_blocking();
                see_monsters();
                /* might not be able to see self anymore */
                newsym(u.ux, u.uy);
            }
            #ifdef ZHLANG
            You("%s!", Hallucination ? "你以为你看到了一个猫猫"
                                     : "以为你看到了什么");
            #else
            You("%s!", Hallucination ? "tawt you taw a puttie tat"
                                     : "thought you saw something");
            #endif
            ret = SEE_INVIS;
            break;
        }
        FALLTHROUGH;
        /*FALLTHRU*/
    case 8:
        if (HFast & INTRINSIC) {
            HFast &= ~INTRINSIC;
            #ifdef ZHLANG
            You_feel("更慢了。");
            #else
            You_feel("slower.");
            #endif
            ret = FAST;
            break;
        }
        FALLTHROUGH;
        /*FALLTHRU*/
    case 9:
        if (HStealth & INTRINSIC) {
            HStealth &= ~INTRINSIC;
            #ifdef ZHLANG
            You_feel("笨拙。");
            #else
            You_feel("clumsy.");
            #endif
            ret = STEALTH;
            break;
        }
        FALLTHROUGH;
        /*FALLTHRU*/
    case 10:
        /* intrinsic protection is just disabled, not set back to 0 */
        if (HProtection & INTRINSIC) {
            HProtection &= ~INTRINSIC;
            #ifdef ZHLANG
            You_feel("脆弱。");
            #else
            You_feel("vulnerable.");
            #endif
            ret = PROTECTION;
            break;
        }
        FALLTHROUGH;
        /*FALLTHRU*/
    case 11:
        if (HAggravate_monster & INTRINSIC) {
            HAggravate_monster &= ~INTRINSIC;
            #ifdef ZHLANG
            You_feel("不那么吸引人了。");
            #else
            You_feel("less attractive.");
            #endif
            ret = AGGRAVATE_MONSTER;
            break;
        }
        FALLTHROUGH;
        /*FALLTHRU*/
    default:
        break;
    }
    return ret;
}

/*sit.c*/
