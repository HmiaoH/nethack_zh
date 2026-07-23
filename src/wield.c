/* NetHack 5.0	wield.c	$NHDT-Date: 1781973073 2026/06/20 16:31:13 $  $NHDT-Branch: NetHack-5.0 $:$NHDT-Revision: 1.124 $ */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/*-Copyright (c) Robert Patrick Rankin, 2009. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

/* KMH -- Differences between the three weapon slots.
 *
 * The main weapon (uwep):
 * 1.  Is filled by the (w)ield command.
 * 2.  Can be filled with any type of item.
 * 3.  May be carried in one or both hands.
 * 4.  Is used as the melee weapon and as the launcher for
 *     ammunition.
 * 5.  Only conveys intrinsics when it is a weapon, weapon-tool,
 *     or artifact.
 * 6.  Certain cursed items will weld to the hand and cannot be
 *     unwielded or dropped.  See erodeable_wep() and will_weld()
 *     below for the list of which items apply.
 *
 * The secondary weapon (uswapwep):
 * 1.  Is filled by the e(x)change command, which swaps this slot
 *     with the main weapon.  If the "pushweapon" option is set,
 *     the (w)ield command will also store the old weapon in the
 *     secondary slot.
 * 2.  Can be filled with anything that will fit in the main weapon
 *     slot; that is, any type of item.
 * 3.  Is usually NOT considered to be carried in the hands.
 *     That would force too many checks among the main weapon,
 *     second weapon, shield, gloves, and rings; and it would
 *     further be complicated by bimanual weapons.  A special
 *     exception is made for two-weapon combat.
 * 4.  Is used as the second weapon for two-weapon combat, and as
 *     a convenience to swap with the main weapon.
 * 5.  Never conveys intrinsics.
 * 6.  Cursed items never weld (see #3 for reasons), but they also
 *     prevent two-weapon combat.
 *
 * The quiver (uquiver):
 * 1.  Is filled by the (Q)uiver command.
 * 2.  Can be filled with any type of item.
 * 3.  Is considered to be carried in a special part of the pack.
 * 4.  Is used as the item to throw with the (f)ire command.
 *     This is a convenience over the normal (t)hrow command.
 * 5.  Never conveys intrinsics.
 * 6.  Cursed items never weld; their effect is handled by the normal
 *     throwing code.
 * 7.  The autoquiver option will fill it with something deemed
 *     suitable if (f)ire is used when it's empty.
 *
 * No item may be in more than one of these slots.
 */

staticfn boolean cant_wield_corpse(struct obj *) NONNULLARG1;
staticfn int ready_weapon(struct obj *) NO_NNARGS;
staticfn int ready_ok(struct obj *) NO_NNARGS;
staticfn int wield_ok(struct obj *) NO_NNARGS;
staticfn void finish_splitting(struct obj *);

/* used by will_weld() */
/* probably should be renamed */
#define erodeable_wep(optr)                             \
    ((optr)->oclass == WEAPON_CLASS || is_weptool(optr) \
     || (optr)->otyp == HEAVY_IRON_BALL || (optr)->otyp == IRON_CHAIN)

/* used by welded(), and also while wielding */
#define will_weld(optr) \
    ((optr)->cursed && (erodeable_wep(optr) || (optr)->otyp == TIN_OPENER))

/* to dual-wield, 'obj' must be a weapon or a weapon-tool, and not a bow
   or arrow or missile (dart, shuriken, boomerang), so not matching the
   one-handed weapons which yield "you begin bashing" if used for melee;
   empty hands and two-handed weapons have to be handled separately */
#define TWOWEAPOK(obj) \
    (((obj)->oclass == WEAPON_CLASS)                            \
     ? !(is_launcher(obj) || is_ammo(obj) || is_missile(obj))   \
     : is_weptool(obj))

static const char
    are_no_longer_twoweap[] = "are no longer using two weapons at once",
    can_no_longer_twoweap[] = "can no longer wield two weapons at once";

/*** Functions that place a given item in a slot ***/
/* Proper usage includes:
 * 1.  Initializing the slot during character generation or a
 *     restore.
 * 2.  Setting the slot due to a player's actions.
 * 3.  If one of the objects in the slot is split off, these
 *     functions can be used to put the remainder back in the slot.
 * 4.  Putting an item that was thrown and returned back into the slot.
 * 5.  Emptying the slot, by passing a null object.  NEVER pass
 *     cg.zeroobj!
 *
 * If the item is being moved from another slot, it is the caller's
 * responsibility to handle that.  It's also the caller's responsibility
 * to print the appropriate messages.
 */
void
setuwep(struct obj *obj)
{
    struct obj *olduwep = uwep;

    if (obj == uwep)
        return; /* necessary to not set gu.unweapon */
    setworn(obj, W_WEP);
    /* handle Ogresmasher before Sunsword; even though they can't be happening
       at the same time, botl flag update should come before pline message */
    if (uwep == obj
        && ((uwep && uwep->oartifact == ART_OGRESMASHER)
            || (olduwep && olduwep->oartifact == ART_OGRESMASHER)))
        disp.botl = TRUE; /* gaining or losing Con bonus */
    /* This message isn't printed in the caller because it happens
     * *whenever* Sunsword is unwielded, from whatever cause. */
    if (uwep == obj && artifact_light(olduwep) && olduwep->lamplit) {
        end_burn(olduwep, FALSE);
        if (!Blind)
#ifdef ZHLANG
            pline("%s不再发光了。", Tobjnam(olduwep, "停止"));
#else
            pline("%s shining.", Tobjnam(olduwep, "stop"));
#endif
    }
    if (uwep == obj
        && (u_wield_art(ART_OGRESMASHER)
            || is_art(olduwep, ART_OGRESMASHER)))
        disp.botl = TRUE;
    /* Note: Explicitly wielding a pick-axe will not give a "bashing"
     * message.  Wielding one via 'a'pplying it will.
     * 3.2.2:  Wielding arbitrary objects will give bashing message too.
     */
    if (obj) {
        gu.unweapon = (obj->oclass == WEAPON_CLASS)
                       ? is_launcher(obj) || is_ammo(obj) || is_missile(obj)
            || (is_pole(obj) && !u.usteed && !is_art(obj, ART_SNICKERSNEE))
                       : !is_weptool(obj) && !is_wet_towel(obj);
    } else
        gu.unweapon = TRUE; /* for "bare hands" message */
}

staticfn boolean
cant_wield_corpse(struct obj *obj)
{
    char kbuf[BUFSZ];

    if (uarmg || obj->otyp != CORPSE || !touch_petrifies(&mons[obj->corpsenm])
        || Stone_resistance)
        return FALSE;

    /* Prevent wielding cockatrice when not wearing gloves --KAA */
#ifdef ZHLANG
    You("赤手%s挥舞着%s。",
        makeplural(body_part(HAND)),
        corpse_xname(obj, (const char *) 0, CXN_PFX_THE));
#else
    You("wield %s in your bare %s.",
        corpse_xname(obj, (const char *) 0, CXN_PFX_THE),
        makeplural(body_part(HAND)));
#endif
    Sprintf(kbuf, "wielding %s bare-handed", killer_xname(obj));
    instapetrify(kbuf);
    return TRUE;
}

/* description of hands when not wielding anything; also used
   by #seeweapon (')'), #attributes (^X), and #takeoffall ('A') */
const char *
empty_handed(void)
{
    return uarmg ? "empty handed" /* gloves imply hands */
           : humanoid(gy.youmonst.data)
             /* hands but no weapon and no gloves */
             ? "bare handed"
               /* alternate phrasing for paws or lack of hands */
               : "not wielding anything";
}

staticfn int
ready_weapon(struct obj *wep)
{
    /* Separated function so swapping works easily */
    int res = ECMD_OK;
    boolean was_twoweap = u.twoweap, had_wep = (uwep != 0);

    if (!wep) {
        /* No weapon */
        if (uwep) {
#ifdef ZHLANG
            You("现在%s了。", empty_handed());
#else
            You("are %s.", empty_handed());
#endif
            setuwep((struct obj *) 0);
            res = ECMD_TIME;
        } else
#ifdef ZHLANG
            You("已经处于%s状态。", empty_handed());
#else
            You("are already %s.", empty_handed());
#endif
    } else if (wep->otyp == CORPSE && cant_wield_corpse(wep)) {
        /* hero must have been life-saved to get here; use a turn */
        res = ECMD_TIME; /* corpse won't be wielded */
    } else if (uarms && bimanual(wep)) {
#ifdef ZHLANG
        You("在装备盾牌时无法挥舞双手%s。",
            is_sword(wep) ? "剑" : wep->otyp == BATTLE_AXE ? "斧"
                                                               : "武器");
#else
        You("cannot wield a two-handed %s while wearing a shield.",
            is_sword(wep) ? "sword" : wep->otyp == BATTLE_AXE ? "axe"
                                                               : "weapon");
#endif
        res = ECMD_FAIL;
    } else if (!retouch_object(&wep, FALSE)) {
        res = ECMD_TIME; /* takes a turn even though it doesn't get wielded */
    } else {
        /* Weapon WILL be wielded after this point */
        res = ECMD_TIME;
        if (will_weld(wep)) {
            const char *tmp = xname(wep), *thestr = "The ";

            if (strncmp(tmp, thestr, 4) && !strncmp(The(tmp), thestr, 4))
                tmp = thestr;
            else
                tmp = "";
#ifdef ZHLANG
            pline("%s%s到你的%s%s！", tmp, aobjnam(wep, "粘在"),
                  wep->quan == 1L ? "自身" : "它们",
                  bimanual(wep) ? (const char *) makeplural(body_part(HAND))
                                : body_part(HAND));
#else
            pline("%s%s %s to your %s%s!", tmp, aobjnam(wep, "weld"),
                  (wep->quan == 1L) ? "itself" : "themselves", /* a3 */
                  bimanual(wep) ? "" :
                      (URIGHTY ? "dominant right " : "dominant left "),
                  bimanual(wep) ? (const char *) makeplural(body_part(HAND))
                                : body_part(HAND));
#endif
            set_bknown(wep, 1);
        } else {
            /* The message must be printed before setuwep (since
             * you might die and be revived from changing weapons),
             * and the message must be before the death message and
             * Lifesaved rewielding.  Yet we want the message to
             * say "weapon in hand", thus this kludge.
             * [That comment is obsolete.  It dates from the days (3.0)
             * when unwielding Firebrand could cause hero to be burned
             * to death in Hell due to loss of fire resistance.
             * "Lifesaved re-wielding or re-wearing" is ancient history.]
             */
            long dummy = wep->owornmask;

            wep->owornmask |= W_WEP;
            if (wep->otyp == AKLYS && (wep->owornmask & W_WEP) != 0)
#ifdef ZHLANG
                You("系紧了系绳。");
#else
                You("secure the tether.");
#endif
            prinv((char *) 0, wep, 0L);
            wep->owornmask = dummy;
        }

        setuwep(wep);
        if (was_twoweap && !u.twoweap && flags.verbose) {
            /* skip this message if we already got "empty handed" one above;
               also, Null is not safe for neither TWOWEAPOK() or bimanual() */
            if (uwep)
#ifdef ZHLANG
                You("%s。", ((TWOWEAPOK(uwep) && !bimanual(uwep))
                            ? "不再同时使用两把武器"
                            : "无法再同时挥舞两把武器"));
#else
                You("%s.", ((TWOWEAPOK(uwep) && !bimanual(uwep))
                            ? are_no_longer_twoweap
                            : can_no_longer_twoweap));
#endif
        }

        /* KMH -- Talking artifacts are finally implemented */
        if (wep->oartifact) {
            res |= arti_speak(wep); /* sets ECMD_TIME bit if artifact speaks */
        }

        if (artifact_light(wep) && !wep->lamplit) {
            begin_burn(wep, FALSE);
            if (!Blind)
#ifdef ZHLANG
                pline("%s散发出%s的光芒！", Tobjnam(wep, "开始"),
                      arti_light_description(wep));
#else
                pline("%s to shine %s!", Tobjnam(wep, "begin"),
                      arti_light_description(wep));
#endif
        }
#if 0
        /* we'll get back to this someday, but it's not balanced yet */
        if (Race_if(PM_ELF) && !wep->oartifact
            && objects[wep->otyp].oc_material == IRON) {
            /* Elves are averse to wielding cold iron */
            You("have an uneasy feeling about wielding cold iron.");
            change_luck(-1);
        }
#endif
        if (wep->unpaid) {
            struct monst *this_shkp;

            if ((this_shkp = shop_keeper(inside_shop(u.ux, u.uy)))
                != (struct monst *) 0) {
                /* check msound because we don't have access to muteshk() */
                if (!Deaf && this_shkp->data->msound > MS_ANIMAL)
#ifdef ZHLANG
                    pline("%s%s“小心点用我的%s！”",
                          shkname(this_shkp), says(), xname(wep));
#else
                    pline("%s %s \"You be careful with my %s!\"",
                          shkname(this_shkp), says(), xname(wep));
#endif
                else
#ifdef ZHLANG
                    pline("%s对你挥舞%s%s感到担忧。",
                          shkname(this_shkp), mhis(this_shkp), xname(wep));
#else
                    pline("%s looks apprehensive about your wielding %s %s.",
                          shkname(this_shkp), mhis(this_shkp), xname(wep));
#endif
            }
        }
    }
    if ((had_wep != (uwep != 0)) && condtests[bl_bareh].enabled)
        disp.botl = TRUE;
    return res;
}

void
setuqwep(struct obj *obj)
{
    setworn(obj, W_QUIVER);
    /* no extra handling needed; this used to include a call to
       update_inventory() but that's already performed by setworn() */
    return;
}

void
setuswapwep(struct obj *obj)
{
    setworn(obj, W_SWAPWEP);
    return;
}

/* getobj callback for object to ready for throwing/shooting;
   this filter lets worn items through so that caller can reject them */
staticfn int
ready_ok(struct obj *obj)
{
    if (!obj) /* '-', will empty quiver slot if chosen */
        return uquiver ? GETOBJ_SUGGEST : GETOBJ_DOWNPLAY;

    /* downplay when wielded, unless more than one */
    if (obj == uwep || (obj == uswapwep && u.twoweap))
        return (obj->quan == 1) ? GETOBJ_DOWNPLAY : GETOBJ_SUGGEST;

    if (is_ammo(obj)) {
        return ((uwep && ammo_and_launcher(obj, uwep))
                || (uswapwep && ammo_and_launcher(obj, uswapwep)))
                ? GETOBJ_SUGGEST
                : GETOBJ_DOWNPLAY;
    } else if (is_launcher(obj)) { /* part of 'possible extension' below */
        return GETOBJ_DOWNPLAY;
    } else {
        if (obj->oclass == WEAPON_CLASS || obj->oclass == COIN_CLASS)
            return GETOBJ_SUGGEST;
        /* Possible extension: exclude weapons that make no sense to throw,
           such as whips, bows, slings, rubber hoses. */
    }

#if 0   /* superseded by ammo_and_launcher handling above */
    /* Include gems/stones as likely candidates if either primary
       or secondary weapon is a sling. */
    if (obj->oclass == GEM_CLASS
        && (uslinging()
            || (uswapwep && objects[uswapwep->otyp].oc_skill == P_SLING)))
        return GETOBJ_SUGGEST;
#endif

    return GETOBJ_DOWNPLAY;
}

/* getobj callback for object to wield */
staticfn int
wield_ok(struct obj *obj)
{
    if (!obj)
        return GETOBJ_SUGGEST;

    if (obj->oclass == COIN_CLASS)
        return GETOBJ_EXCLUDE;

    if (obj->oclass == WEAPON_CLASS || is_weptool(obj))
        return GETOBJ_SUGGEST;

    return GETOBJ_DOWNPLAY;
}

staticfn void
finish_splitting(struct obj *obj)
{
    /* obj was split off from something; give it its own invlet */
    freeinv(obj);
    addinv_nomerge(obj);
}

/* the #wield command - wield a weapon */
int
dowield(void)
{
    char qbuf[QBUFSZ];
    struct obj *wep, *oldwep;
    int result;

    /* May we attempt this? */
    gm.multi = 0;
    if (cantwield(gy.youmonst.data)) {
#ifdef ZHLANG
        pline("别开玩笑了！");
#else
        pline("Don't be ridiculous!");
#endif
        return ECMD_FAIL;
    }
    /* Keep going even if inventory is completely empty, since wielding '-'
       to wield nothing can be construed as a positive act even when done
       so redundantly. */

    /* Prompt for a new weapon */
    clear_splitobjs();
    if (!(wep = getobj("wield", wield_ok, GETOBJ_PROMPT | GETOBJ_ALLOWCNT))) {
        /* Cancelled */
        return ECMD_CANCEL;
    } else if (wep == uwep) {
 already_wielded:
#ifdef ZHLANG
        You("已经在挥舞那个了！");
#else
        You("are already wielding that!");
#endif
        if (is_weptool(wep) || is_wet_towel(wep))
            gu.unweapon = FALSE; /* [see setuwep()] */
        return ECMD_FAIL;
    } else if (welded(uwep)) {
        weldmsg(uwep);
        /* previously interrupted armor removal mustn't be resumed */
        reset_remarm();
        /* if player chose a partial stack but can't wield it, undo split */
        if (wep->o_id && wep->o_id == svc.context.objsplit.child_oid)
            unsplitobj(wep);
        return ECMD_FAIL;
    } else if (wep->o_id && wep->o_id == svc.context.objsplit.child_oid) {
        /* if wep is the result of supplying a count to getobj()
           we don't want to split something already wielded; for
           any other item, we need to give it its own inventory slot */
        if (uwep && uwep->o_id == svc.context.objsplit.parent_oid) {
            unsplitobj(wep);
            /* wep was merged back to uwep, already_wielded uses wep */
            wep = uwep;
            goto already_wielded;
        }
        finish_splitting(wep);
        goto wielding;
    }

    /* Handle no object, or object in other slot */
    if (wep == &hands_obj) {
        wep = (struct obj *) 0;
    } else if (wep == uswapwep) {
        return doswapweapon();
    } else if (wep == uquiver) {
        /* offer to split stack if multiple are quivered */
        if (uquiver->quan > 1L && inv_cnt(FALSE) < invlet_basic
                                    && splittable(uquiver)) {
#ifdef ZHLANG
            Sprintf(qbuf, "你准备了%ld%s。要挥舞其中一个吗？",
                    uquiver->quan, simpleonames(uquiver));
#else
            Sprintf(qbuf, "You have %ld %s readied.  Wield one?",
                    uquiver->quan, simpleonames(uquiver));
#endif
            switch (ynq(qbuf)) {
            case 'q':
                return ECMD_OK;
            case 'y':
                /* leave N-1 quivered, split off 1 to wield */
                wep = splitobj(uquiver, 1L);
                finish_splitting(wep);
                goto wielding;
            default:
                break;
            }
#ifdef ZHLANG
            Strcpy(qbuf, "改为挥舞所有物品？");
#else
            Strcpy(qbuf, "Wield all of them instead?");
#endif
        } else {
            boolean use_plural = (is_plural(uquiver) || pair_of(uquiver));

#ifdef ZHLANG
            Sprintf(qbuf, "你准备好了%s。改为挥舞%s？",
                    !use_plural ? "那个物品" : "那些物品",
                    !use_plural ? "它" : "它们");
#else
            Sprintf(qbuf, "You have %s readied.  Wield %s instead?",
                    !use_plural ? "that" : "those",
                    !use_plural ? "it" : "them");
#endif
        }
        /* require confirmation to wield the quivered weapon */
        if (ynq(qbuf) != 'y') {
            (void) Shk_Your(qbuf, uquiver); /* replace qbuf[] contents */
#ifdef ZHLANG
            pline("%s%s%s保持就绪状态。", qbuf,
                  simpleonames(uquiver), otense(uquiver, ""));
#else
            pline("%s%s %s readied.", qbuf,
                  simpleonames(uquiver), otense(uquiver, "remain"));
#endif
            return ECMD_OK;
        }
        /* wielding whole readied stack, so no longer quivered */
        setuqwep((struct obj *) 0);
    } else if (wep->owornmask & (W_ARMOR | W_ACCESSORY | W_SADDLE)) {
#ifdef ZHLANG
        You("无法挥舞那个！");
#else
        You("cannot wield that!");
#endif
        return ECMD_FAIL;
    }

 wielding:
    /* Set your new primary weapon */
    oldwep = uwep;
    result = ready_weapon(wep);
    if (flags.pushweapon && oldwep && uwep != oldwep)
        setuswapwep(oldwep);
    untwoweapon();

    return result;
}

/* the #swap command - swap wielded and secondary weapons */
int
doswapweapon(void)
{
    struct obj *oldwep, *oldswap;
    int result = 0;

    /* May we attempt this? */
    gm.multi = 0;
    if (cantwield(gy.youmonst.data)) {
#ifdef ZHLANG
        pline("别开玩笑了！");
#else
        pline("Don't be ridiculous!");
#endif
        return ECMD_FAIL;
    }
    if (welded(uwep)) {
        weldmsg(uwep);
        return ECMD_FAIL;
    }

    /* Unwield your current secondary weapon */
    oldwep = uwep;
    oldswap = uswapwep;
    setuswapwep((struct obj *) 0);

    /* Set your new primary weapon */
    result = ready_weapon(oldswap);

    /* Set your new secondary weapon */
    if (uwep == oldwep) {
        /* Wield failed for some reason */
        setuswapwep(oldswap);
    } else {
        setuswapwep(oldwep);
        if (uswapwep)
            prinv((char *) 0, uswapwep, 0L);
        else
#ifdef ZHLANG
            You("没有准备副武器。");
#else
            You("have no secondary weapon readied.");
#endif
    }

    if (u.twoweap && !can_twoweapon())
        untwoweapon();

    return result;
}

/* the #quiver command */
int
dowieldquiver(void)
{
    return doquiver_core("ready");
}

/* guts of #quiver command; also used by #fire when refilling empty quiver */
int
doquiver_core(const char *verb) /* "ready" or "fire" */
{
    char qbuf[QBUFSZ];
    struct obj *newquiver;
    int res;
    boolean was_uwep = FALSE, was_twoweap = u.twoweap;

    /* Since the quiver isn't in your hands, don't check cantwield(),
       will_weld(), touch_petrifies(), etc. */
    gm.multi = 0;
    if (!gi.invent) {
        /* could accept '-' to empty quiver, but there's no point since
           inventory is empty so uquiver is already Null */
#ifdef ZHLANG
        You("没有准备任何用于射击的物品。");
#else
        You("have nothing to ready for firing.");
#endif
        return ECMD_OK;
    }

    /* forget last splitobj() before calling getobj() with GETOBJ_ALLOWCNT */
    clear_splitobjs();
    /* Prompt for a new quiver: "What do you want to {ready|fire}?" */
    newquiver = getobj(verb, ready_ok, GETOBJ_PROMPT | GETOBJ_ALLOWCNT);

    if (!newquiver) {
        /* Cancelled */
        return ECMD_CANCEL;
    } else if (newquiver == &hands_obj) { /* no object */
        /* Explicitly nothing */
        if (uquiver) {
#ifdef ZHLANG
            You("现在没有准备任何弹药。");
#else
            You("now have no ammunition readied.");
#endif
            /* skip 'quivering: prinv()' */
            setuqwep((struct obj *) 0);
        } else {
#ifdef ZHLANG
            You("已经没有准备弹药了！");
#else
            You("already have no ammunition readied!");
#endif
        }
        return ECMD_OK;
    } else if (newquiver->o_id == svc.context.objsplit.child_oid) {
        /* if newquiver is the result of supplying a count to getobj()
           we don't want to split something already in the quiver;
           for any other item, we need to give it its own inventory slot */
        if (uquiver && uquiver->o_id == svc.context.objsplit.parent_oid) {
            unsplitobj(newquiver);
            goto already_quivered;
        } else if (newquiver->oclass == COIN_CLASS) {
            /* don't allow splitting a stack of coins into quiver */
#ifdef ZHLANG
            You("不能只准备一部分金币。");
#else
            You("can't ready only part of your gold.");
#endif
            unsplitobj(newquiver);
            return ECMD_OK;
        }
        finish_splitting(newquiver);
    } else if (newquiver == uquiver) {
 already_quivered:
#ifdef ZHLANG
        pline("那弹药已经准备好了！");
#else
        pline("That ammunition is already readied!");
#endif
        return ECMD_OK;
    } else if (newquiver->owornmask & (W_ARMOR | W_ACCESSORY | W_SADDLE)) {
#ifdef ZHLANG
        You("无法%s那个！", verb);
#else
        You("cannot %s that!", verb);
#endif
        return ECMD_OK;
    } else if (newquiver == uwep) {
        int weld_res = !uwep->bknown;

        if (welded(uwep)) {
            weldmsg(uwep);
            reset_remarm(); /* same as dowield() */
            return weld_res ? ECMD_TIME : ECMD_OK;
        }
        /* offer to split stack if wielding more than 1 */
        if (uwep->quan > 1L && inv_cnt(FALSE) < invlet_basic
                                    && splittable(uwep)) {
#ifdef ZHLANG
            Sprintf(qbuf, "你正在挥舞%ld%s。要准备其中的%ld个吗？",
                    uwep->quan, simpleonames(uwep), uwep->quan - 1L);
#else
            Sprintf(qbuf, "You are wielding %ld %s.  Ready %ld of them?",
                    uwep->quan, simpleonames(uwep), uwep->quan - 1L);
#endif
            switch (ynq(qbuf)) {
            case 'q':
                return ECMD_OK;
            case 'y':
                /* leave 1 wielded, split rest off and put into quiver */
                newquiver = splitobj(uwep, uwep->quan - 1L);
                finish_splitting(newquiver);
                goto quivering;
            default:
                break;
            }
#ifdef ZHLANG
            Strcpy(qbuf, "改为全部准备？");
#else
            Strcpy(qbuf, "Ready all of them instead?");
#endif
        } else {
            boolean use_plural = (is_plural(uwep) || pair_of(uwep));

#ifdef ZHLANG
            Sprintf(qbuf, "你正在挥舞%s。改为准备%s？",
                    !use_plural ? "那个物品" : "那些物品",
                    !use_plural ? "它" : "它们");
#else
            Sprintf(qbuf, "You are wielding %s.  Ready %s instead?",
                    !use_plural ? "that" : "those",
                    !use_plural ? "it" : "them");
#endif
        }
        /* require confirmation to ready the main weapon */
        if (ynq(qbuf) != 'y') {
            (void) Shk_Your(qbuf, uwep); /* replace qbuf[] contents */
#ifdef ZHLANG
            pline("%s%s%s被挥舞着。", qbuf,
                  simpleonames(uwep), otense(uwep, ""));
#else
            pline("%s%s %s wielded.", qbuf,
                  simpleonames(uwep), otense(uwep, "remain"));
#endif
            return ECMD_OK;
        }
        /* quivering main weapon, so no longer wielding it */
        setuwep((struct obj *) 0);
        untwoweapon();
        was_uwep = TRUE;
    } else if (newquiver == uswapwep) {
        if (uswapwep->quan > 1L && inv_cnt(FALSE) < invlet_basic
            && splittable(uswapwep)) {
#ifdef ZHLANG
            Sprintf(qbuf, "%s%ld%s。要准备其中的%ld个吗？",
                    u.twoweap ? "你正在双持" : "你的备用武器是",
                    uswapwep->quan, simpleonames(uswapwep),
                    uswapwep->quan - 1L);
#else
            Sprintf(qbuf, "%s %ld %s.  Ready %ld of them?",
                    u.twoweap ? "You are dual wielding"
                              : "Your alternate weapon is",
                    uswapwep->quan, simpleonames(uswapwep),
                    uswapwep->quan - 1L);
#endif
            switch (ynq(qbuf)) {
            case 'q':
                return ECMD_OK;
            case 'y':
                /* leave 1 alt-wielded, split rest off and put into quiver */
                newquiver = splitobj(uswapwep, uswapwep->quan - 1L);
                finish_splitting(newquiver);
                goto quivering;
            default:
                break;
            }
#ifdef ZHLANG
            Strcpy(qbuf, "改为全部准备？");
#else
            Strcpy(qbuf, "Ready all of them instead?");
#endif
        } else {
            boolean use_plural = (is_plural(uswapwep) || pair_of(uswapwep));

#ifdef ZHLANG
            Sprintf(qbuf, "%s你的%s武器。改为准备%s？",
                    !use_plural ? "那是" : "那些是",
                    u.twoweap ? "第二" : "备用",
                    !use_plural ? "它" : "它们");
#else
            Sprintf(qbuf, "%s your %s weapon.  Ready %s instead?",
                    !use_plural ? "That is" : "Those are",
                    u.twoweap ? "second" : "alternate",
                    !use_plural ? "it" : "them");
#endif
        }
        /* require confirmation to ready the alternate weapon */
        if (ynq(qbuf) != 'y') {
            (void) Shk_Your(qbuf, uswapwep); /* replace qbuf[] contents */
#ifdef ZHLANG
            pline("%s%s%s%s。", qbuf,
                  simpleonames(uswapwep), otense(uswapwep, "被"),
                  u.twoweap ? "挥舞着" : "作为副武器装备");
#else
            pline("%s%s %s %s.", qbuf,
                  simpleonames(uswapwep), otense(uswapwep, "remain"),
                  u.twoweap ? "wielded" : "as secondary weapon");
#endif
            return ECMD_OK;
        }
        /* quivering alternate weapon, so no more uswapwep */
        setuswapwep((struct obj *) 0);
        untwoweapon();
    }

 quivering:
    if (!strcmp(verb, "ready")) {
        /* place item in quiver before printing so that inventory feedback
           includes "(at the ready)" */
        setuqwep(newquiver);
        prinv((char *) 0, newquiver, 0L);
    } else { /* verb=="fire", manually refilling quiver during 'f'ire */
        /* prefix item with description of action, so don't want that to
           include "(at the ready)" */
#ifdef ZHLANG
        prinv("你准备好了：", newquiver, 0L);
#else
        prinv("You ready:", newquiver, 0L);
#endif
        setuqwep(newquiver);
    }

    /* quiver is a convenience slot and manipulating it ordinarily
       consumes no time, but unwielding primary or secondary weapon
       should take time (perhaps we're adjacent to a rust monster
       or disenchanter and want to hit it immediately, but not with
       something we're wielding that's vulnerable to its damage) */
    res = 0;
    if (was_uwep) {
#ifdef ZHLANG
        You("现在%s了。", empty_handed());
#else
        You("are now %s.", empty_handed());
#endif
        res = 1;
    } else if (was_twoweap && !u.twoweap) {
#ifdef ZHLANG
        You("不再同时使用两把武器了。");
#else
        You("%s.", are_no_longer_twoweap);
#endif
        res = 1;
    }
    return res ? ECMD_TIME : ECMD_OK;
}

/* used for #rub and for applying pick-axe, whip, grappling hook or polearm */
boolean
wield_tool(struct obj *obj,
           const char *verb) /* "rub",&c */
{
    const char *what;
    boolean more_than_1;

    if (uwep && obj == uwep)
        return TRUE; /* nothing to do if already wielding it */

    if (!verb)
        verb = "wield";
    what = xname(obj);
    more_than_1 = (obj->quan > 1L || strstri(what, "pair of ") != 0
                   || strstri(what, "s of ") != 0);

    if (obj->owornmask & (W_ARMOR | W_ACCESSORY)) {
#ifdef ZHLANG
        You_cant("在穿戴%s时无法%s%s。", more_than_1 ? "它们" : "它",
                 verb, yname(obj));
#else
        You_cant("%s %s while wearing %s.", verb, yname(obj),
                 more_than_1 ? "them" : "it");
#endif
        return FALSE;
    }
    if (uwep && welded(uwep)) {
        if (flags.verbose) {
            const char *hand = body_part(HAND);

            if (bimanual(uwep))
                hand = makeplural(hand);
            if (strstri(what, "pair of ") != 0)
                more_than_1 = FALSE;
#ifdef ZHLANG
            pline("由于武器粘在你的%s上，你无法%s%s%s。",
                  hand, verb, more_than_1 ? "那些" : "那个", xname(obj));
#else
            pline(
               "Since your weapon is welded to your %s, you cannot %s %s %s.",
                  hand, verb, more_than_1 ? "those" : "that", xname(obj));
#endif
        } else {
#ifdef ZHLANG
            You_cant("这样做。");
#else
            You_cant("do that.");
#endif
        }
        return FALSE;
    }
    if (cantwield(gy.youmonst.data)) {
#ifdef ZHLANG
        You_cant("无法牢牢握住%s。", more_than_1 ? "它们" : "它");
#else
        You_cant("hold %s strongly enough.", more_than_1 ? "them" : "it");
#endif
        return FALSE;
    }
    /* check shield */
    if (uarms && bimanual(obj)) {
#ifdef ZHLANG
        You("在装备盾牌时无法%s双手%s。", verb,
            (obj->oclass == WEAPON_CLASS) ? "武器" : "工具");
#else
        You("cannot %s a two-handed %s while wearing a shield.", verb,
            (obj->oclass == WEAPON_CLASS) ? "weapon" : "tool");
#endif
        return FALSE;
    }

    if (uquiver == obj)
        setuqwep((struct obj *) 0);
    if (uswapwep == obj) {
        (void) doswapweapon();
        /* doswapweapon might fail */
        if (uswapwep == obj)
            return FALSE;
    } else {
        struct obj *oldwep = uwep;

        if (will_weld(obj)) {
            /* hope none of ready_weapon()'s early returns apply here... */
            (void) ready_weapon(obj);
        } else {
#ifdef ZHLANG
            You("现在挥舞着%s。", doname(obj));
#else
            You("now wield %s.", doname(obj));
#endif
            setuwep(obj);
        }
        if (flags.pushweapon && oldwep && uwep != oldwep)
            setuswapwep(oldwep);
    }
    if (uwep && uwep != obj)
        return FALSE; /* rewielded old object after dying */
    /* applying weapon or tool that gets wielded ends two-weapon combat */
    if (u.twoweap)
        untwoweapon();
    if (obj->oclass != WEAPON_CLASS)
        gu.unweapon = TRUE;
    return TRUE;
}

int
can_twoweapon(void)
{
    struct obj *otmp;

    if (!could_twoweap(gy.youmonst.data)) {
        if (Upolyd)
#ifdef ZHLANG
            You_cant("以你当前形态无法使用两把武器。");
#else
            You_cant("use two weapons in your current form.");
#endif
        else
#ifdef ZHLANG
            pline("%s无法同时使用两把武器。",
                  makeplural((flags.female && gu.urole.name.f)
                             ? gu.urole.name.f : gu.urole.name.m));
#else
            pline("%s aren't able to use two weapons at once.",
                  makeplural((flags.female && gu.urole.name.f)
                             ? gu.urole.name.f : gu.urole.name.m));
#endif
    } else if (!uwep || !uswapwep) {
        const char *hand_s = body_part(HAND);

        if (!uwep && !uswapwep)
            hand_s = makeplural(hand_s);
        /* "your hands are empty" or "your {left|right} hand is empty" */
#ifdef ZHLANG
        Your("%s%s是空的。", uwep ? "左" : uswapwep ? "右" : "",
             hand_s);
#else
        Your("%s%s %s empty.", uwep ? "left " : uswapwep ? "right " : "",
             hand_s, vtense(hand_s, "are"));
#endif
    } else if (!TWOWEAPOK(uwep) || !TWOWEAPOK(uswapwep)) {
        otmp = !TWOWEAPOK(uwep) ? uwep : uswapwep;
#ifdef ZHLANG
        pline("%s%s适合的%s武器%s。", Yname2(otmp),
              is_plural(otmp) ? "不是" : "不是一个",
              (otmp == uwep) ? "主" : "副",
              plur(otmp->quan));
#else
        pline("%s %s suitable %s weapon%s.", Yname2(otmp),
              is_plural(otmp) ? "aren't" : "isn't a",
              (otmp == uwep) ? "primary" : "secondary",
              plur(otmp->quan));
#endif
    } else if (bimanual(uwep) || bimanual(uswapwep)) {
        otmp = bimanual(uwep) ? uwep : uswapwep;
#ifdef ZHLANG
        pline("%s不是单手武器。", Yname2(otmp));
#else
        pline("%s isn't one-handed.", Yname2(otmp));
#endif
    } else if (uarms) {
#ifdef ZHLANG
        You_cant("在装备盾牌时无法使用两把武器。");
#else
        You_cant("use two weapons while wearing a shield.");
#endif
    } else if (uswapwep->oartifact) {
#ifdef ZHLANG
        pline("%s抗拒被作为第二把武器使用！",
              Yobjnam2(uswapwep, ""));
#else
        pline("%s being held second to another weapon!",
              Yobjnam2(uswapwep, "resist"));
#endif
    } else if (uswapwep->otyp == CORPSE && cant_wield_corpse(uswapwep)) {
        /* [Note: !TWOWEAPOK() check prevents ever getting here...] */
        ; /* must be life-saved to reach here; return FALSE */
    } else if (Glib || uswapwep->cursed) {
        if (!Glib)
            set_bknown(uswapwep, 1);
        drop_uswapwep();
    } else
        return TRUE;
    return FALSE;
}

/* uswapwep has become cursed while in two-weapon combat mode or hero is
   attempting to dual-wield when it is already cursed or hands are slippery */
void
drop_uswapwep(void)
{
    char left_hand[QBUFSZ];
    struct obj *obj = uswapwep;

    /* this used to use makeplural(body_part(HAND)) but in order to be
       dual-wielded, or to get this far attempting to achieve that,
       uswapwep must be one-handed; since it's secondary, the hand must
       be the left one */
    Sprintf(left_hand, "left %s", body_part(HAND));
    if (!obj->cursed)
        /* attempting to two-weapon while Glib */
#ifdef ZHLANG
        pline("%s从你的%s滑落！", Yobjnam2(obj, ""), left_hand);
#else
        pline("%s from your %s!", Yobjnam2(obj, "slip"), left_hand);
#endif
    else if (!u.twoweap)
        /* attempting to two-weapon when uswapwep is cursed */
#ifdef ZHLANG
        pline("%s挣脱了你的抓握，从你的%s掉落！",
              Yobjnam2(obj, ""), left_hand);
#else
        pline("%s your grasp and %s from your %s!",
              Yobjnam2(obj, "evade"), otense(obj, "drop"), left_hand);
#endif
    else
        /* already two-weaponing but can't anymore because uswapwep has
           become cursed */
#ifdef ZHLANG
        Your("%s抽搐了一下，丢掉了%s！", left_hand, yobjnam(obj, (char *) 0));
#else
        Your("%s spasms and drops %s!", left_hand, yobjnam(obj, (char *) 0));
#endif
    dropx(obj);
}

void
set_twoweap(boolean on_off)
{
    if (on_off != u.twoweap) {
        u.twoweap = on_off;
        if (flags.weaponstatus)
            disp.botl = TRUE;
    }
}

/* the #twoweapon command */
int
dotwoweapon(void)
{
    /* You can always toggle it off */
    if (u.twoweap) {
#ifdef ZHLANG
        You("切换到你的主武器。");
#else
        You("switch to your primary weapon.");
#endif
        set_twoweap(FALSE); /* u.twoweap = FALSE */
        update_inventory();
        return ECMD_OK;
    }

    /* May we use two weapons? */
    if (can_twoweapon()) {
        /* Success! */
#ifdef ZHLANG
        You("开始双持武器战斗。");
#else
        You("begin two-weapon combat.");
#endif
        set_twoweap(TRUE); /* u.twoweap = TRUE */
        update_inventory();
        return (rnd(20) > ACURR(A_DEX)) ? ECMD_TIME : ECMD_OK;
    }
    return ECMD_OK;
}

/*** Functions to empty a given slot ***/
/* These should be used only when the item can't be put back in
 * the slot by life saving.  Proper usage includes:
 * 1.  The item has been eaten, stolen, burned away, or rotted away.
 * 2.  Making an item disappear for a bones pile.
 */
void
uwepgone(void)
{
    if (uwep) {
        if (artifact_light(uwep) && uwep->lamplit) {
            end_burn(uwep, FALSE);
            if (!Blind)
#ifdef ZHLANG
                pline("%s不再发光了。", Tobjnam(uwep, "停止"));
#else
                pline("%s shining.", Tobjnam(uwep, "stop"));
#endif
        }
        setworn((struct obj *) 0, W_WEP);
        gu.unweapon = TRUE;
        update_inventory();
    }
}

void
uswapwepgone(void)
{
    if (uswapwep) {
        setworn((struct obj *) 0, W_SWAPWEP);
        update_inventory();
    }
}

void
uqwepgone(void)
{
    if (uquiver) {
        setworn((struct obj *) 0, W_QUIVER);
        update_inventory();
    }
}

void
untwoweapon(void)
{
    if (u.twoweap) {
#ifdef ZHLANG
        You("无法再同时挥舞两把武器了。");
#else
        You("%s.", can_no_longer_twoweap);
#endif
        set_twoweap(FALSE); /* u.twoweap = FALSE */
        update_inventory();
    }
    return;
}

/* enchant wielded weapon */
int
chwepon(struct obj *otmp, int amount)
{
    const char *color = hcolor((amount < 0) ? NH_BLACK : NH_BLUE);
    const char *xtime, *wepname = "";
    boolean multiple;
    int otyp = STRANGE_OBJECT;

    if (!uwep || (uwep->oclass != WEAPON_CLASS && !is_weptool(uwep))) {
        char buf[BUFSZ];

        if (amount >= 0 && uwep && will_weld(uwep)) { /* cursed tin opener */
            if (!Blind) {
#ifdef ZHLANG
                Sprintf(buf, "%s%s的光晕。",
                        Yobjnam2(uwep, "闪耀着"), an(hcolor(NH_AMBER)));
#else
                Sprintf(buf, "%s with %s aura.",
                        Yobjnam2(uwep, "glow"), an(hcolor(NH_AMBER)));
#endif
                uwep->bknown = !Hallucination; /* ok to bypass set_bknown() */
            } else {
                /* cursed tin opener is wielded in right hand */
#ifdef ZHLANG
                Sprintf(buf, "你的右%s有刺痛感。", body_part(HAND));
#else
                Sprintf(buf, "Your right %s tingles.", body_part(HAND));
#endif
            }
            uncurse(uwep);
            update_inventory();
        } else {
#ifdef ZHLANG
            Sprintf(buf, "你的%s%s。", makeplural(body_part(HAND)),
                    (amount >= 0) ? "抽搐了一下" : "发痒");
#else
            Sprintf(buf, "Your %s %s.", makeplural(body_part(HAND)),
                    (amount >= 0) ? "twitch" : "itch");
#endif
        }
        strange_feeling(otmp, buf); /* pline()+docall()+useup() */
        exercise(A_DEX, (boolean) (amount >= 0));
        return 0;
    }

    if (otmp && otmp->oclass == SCROLL_CLASS)
        otyp = otmp->otyp;

    if (uwep->otyp == WORM_TOOTH && amount >= 0) {
        multiple = (uwep->quan > 1L);
        /* order: message, transformation, shop handling */
#ifdef ZHLANG
        Your("%s%s现在锋利多了。", simpleonames(uwep),
             multiple ? "融合在一起，" : "");
#else
        Your("%s %s much sharper now.", simpleonames(uwep),
             multiple ? "fuse, and become" : "is");
#endif
        uwep->otyp = CRYSKNIFE;
        uwep->oerodeproof = 0;
        if (multiple) {
            uwep->quan = 1L;
            uwep->owt = weight(uwep);
        }
        if (uwep->cursed)
            uncurse(uwep);
        /* update shop bill to reflect new higher value */
        if (uwep->unpaid)
            alter_cost(uwep, 0L);
        if (otyp != STRANGE_OBJECT)
            makeknown(otyp);
        if (multiple)
            encumber_msg();
        return 1;
    } else if (uwep->otyp == CRYSKNIFE && amount < 0) {
        multiple = (uwep->quan > 1L);
        /* order matters: message, shop handling, transformation */
#ifdef ZHLANG
        Your("%s%s现在钝多了。", simpleonames(uwep),
             multiple ? "融合在一起，" : "");
#else
        Your("%s %s much duller now.", simpleonames(uwep),
             multiple ? "fuse, and become" : "is");
#endif
        costly_alteration(uwep, COST_DEGRD); /* DECHNT? other? */
        uwep->otyp = WORM_TOOTH;
        uwep->oerodeproof = 0;
        if (multiple) {
            uwep->quan = 1L;
            uwep->owt = weight(uwep);
        }
        if (otyp != STRANGE_OBJECT && otmp->bknown)
            makeknown(otyp);
        if (multiple)
            encumber_msg();
        return 1;
    }

    if (has_oname(uwep))
        wepname = ONAME(uwep);
    if (amount < 0 && uwep->oartifact && restrict_name(uwep, wepname)) {
        if (!Blind)
#ifdef ZHLANG
            pline("%s%s。", Yobjnam2(uwep, "发出微弱的"), color);
#else
            pline("%s %s.", Yobjnam2(uwep, "faintly glow"), color);
#endif
        return 1;
    }
    /* there is a (soft) upper and lower limit to uwep->spe */
    if (((uwep->spe > 5 && amount >= 0) || (uwep->spe < -5 && amount < 0))
        && rn2(3)) {
        if (!Blind)
#ifdef ZHLANG
            pline("%s剧烈地发光了一会儿然后%s了。",
                  Yobjnam2(uwep, ""), color,
                  otense(uwep, "蒸发"));
#else
            pline("%s %s for a while and then %s.",
                  Yobjnam2(uwep, "violently glow"), color,
                  otense(uwep, "evaporate"));
#endif
        else
#ifdef ZHLANG
            pline("%s消失了。", Yobjnam2(uwep, ""));
#else
            pline("%s.", Yobjnam2(uwep, "evaporate"));
#endif

        useupall(uwep); /* let all of them disappear */
        return 1;
    }
    if (!Blind) {
        xtime = (amount * amount == 1) ? "moment" : "while";
#ifdef ZHLANG
        pline("%s持续了%s的%s色光芒。",
              Yobjnam2(uwep, amount == 0 ? "剧烈地闪耀" : "闪耀"), xtime,
              color);
#else
        pline("%s %s for a %s.",
              Yobjnam2(uwep, amount == 0 ? "violently glow" : "glow"), color,
              xtime);
#endif
        if (otyp != STRANGE_OBJECT && uwep->known
            && (amount > 0 || (amount < 0 && otmp->bknown)))
            makeknown(otyp);
    }
    if (amount < 0)
        costly_alteration(uwep, COST_DECHNT);
    uwep->spe += amount;
    if (amount > 0) {
        if (uwep->cursed)
            uncurse(uwep);
        /* update shop bill to reflect new higher price */
        if (uwep->unpaid)
            alter_cost(uwep, 0L);
    }

    /*
     * Enchantment, which normally improves a weapon, has an
     * additional adverse reaction on Magicbane whose effects are
     * spe dependent.  Give an obscure clue here.
     */
    if (u_wield_art(ART_MAGICBANE) && uwep->spe >= 0) {
#ifdef ZHLANG
        Your("右%s%s了一下！", body_part(HAND),
             (((amount > 1) && (uwep->spe > 1)) ? "抽" : "抖"));
#else
        Your("right %s %sches!", body_part(HAND),
             (((amount > 1) && (uwep->spe > 1)) ? "flin" : "it"));
#endif
    }

    /* an elven magic clue, cookie@keebler */
    /* elven weapons vibrate warningly when enchanted beyond a limit */
    if ((uwep->spe > 5)
        && (is_elven_weapon(uwep) || uwep->oartifact || !rn2(7)))
#ifdef ZHLANG
        pline("%s突然振动了一下。", Yobjnam2(uwep, ""));
#else
        pline("%s unexpectedly.", Yobjnam2(uwep, "suddenly vibrate"));
#endif

    return 1;
}

int
welded(struct obj *obj)
{
    if (obj && obj == uwep && will_weld(obj)) {
        set_bknown(obj, 1);
        return 1;
    }
    return 0;
}

void
weldmsg(struct obj *obj)
{
    long savewornmask;
    const char *hand = body_part(HAND);

    if (bimanual(obj))
        hand = makeplural(hand);
    savewornmask = obj->owornmask;
    obj->owornmask = 0L; /* suppress doname()'s "(weapon in hand)";
                          * Yobjnam2() doesn't actually need this because
                          * it is based on xname() rather than doname() */
#ifdef ZHLANG
    pline("%s粘在你的%s上了！", Yobjnam2(obj, ""), hand);
#else
    pline("%s welded to your %s!", Yobjnam2(obj, "are"), hand);
#endif
    obj->owornmask = savewornmask;
}

/* test whether monster's wielded weapon is stuck to hand/paw/whatever */
boolean
mwelded(struct obj *obj)
{
    /* caller is responsible for making sure this is a monster's item */
    if (obj && (obj->owornmask & W_WEP) && will_weld(obj))
        return TRUE;
    return FALSE;
}

/*wield.c*/
