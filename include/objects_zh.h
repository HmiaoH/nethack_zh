/* NetHack 5.0	objects.h	$NHDT-Date: 1781973085 2026/06/20 16:31:25 $  $NHDT-Branch: NetHack-5.0 $:$NHDT-Revision: 1.39 $ */
/* Copyright (c) Mike Threepoint, 1989.                           */
/* NetHack may be freely redistributed.  See license for details. */

/*
   The data in this file is processed multiple times by its inclusion
   in several places in the code. The results depend on the definition
   of the following:
     OBJECTS_ENUM        to construct object onames enum entries (decl.h).
     OBJECTS_DESCR_INIT to construct obj_descr[] array entries (objects.c).
     OBJECTS_INIT       to construct objects[] array entries (objects.c).
*/

#ifndef NoDes
#define NoDes (char *) 0 /* less visual distraction for 'no description' */
#endif

#ifndef lint
#define HARDGEM(n) (n >= 8)
#else
#define HARDGEM(n) (0)
#endif

/*
 * Note...
 *  OBJECTS() currently has 15 parameters; it more become needed, some
 *  will need to be combined the way BITS() is used, because compilers
 *  are allowed to impose a limit of 15.
 */

#if defined(OBJECTS_DESCR_INIT)
#define OBJ(name,desc)  name, desc
#define OBJECT(obj,bits,prp,sym,prob,dly,wt, \
               cost,sdam,ldam,oc1,oc2,nut,color,sn)  { obj }
#define MARKER(tag,sn) /*empty*/

#elif defined(OBJECTS_INIT)
/* notes: 'sub' was once a bitfield but got changed to separate schar when
   it was overloaded to hold negative weapon skill indices; the first zero
   is padding for oc_prediscovered which has variable init at run-time;
   the second zero is oc_spare1 for padding between oc_tough and oc_dir */
#define BITS(nmkn,mrg,uskn,ctnr,mgc,chrg,uniq,nwsh,big,tuf,dir,sub,mtrl) \
    nmkn,mrg,uskn,0,mgc,chrg,uniq,nwsh,big,tuf,0,dir,mtrl,sub /*cpp fodder*/
/* note: 0UL-1UL is a method of expressing the largest possible
   unsigned long value whilst working around a false-positive warning
   in Microsoft Visual C (which assumes that a negative number was
   intended despite the explicit U suffix) */
#define OBJECT(obj,bits,prp,sym,prob,dly,wt,        \
               cost,sdam,ldam,oc1,oc2,nut,color,sn) \
  { 0, 0, (char *) 0, bits, prp, sym, dly, color, prob, wt, \
    cost, sdam, ldam, oc1, oc2, nut, (0UL-1UL), 0, (0UL-1UL), 0 }
#define MARKER(tag,sn) /*empty*/

#elif defined(OBJECTS_ENUM)
#define OBJ(name,desc)
#define OBJECT(obj,bits,prp,sym,prob,dly,wt,        \
               cost,sdam,ldam,oc1,oc2,nut,color,sn) \
    sn
#define MARKER(tag,sn) tag = sn,

#elif defined(DUMP_ENUMS)
#define OBJ(name,desc)
#define OBJECT(obj,bits,prp,sym,prob,dly,wt,        \
               cost,sdam,ldam,oc1,oc2,nut,color,sn) \
  { sn, #sn }
#define MARKER(tag,sn) /*empty*/

#else
#error Unproductive inclusion of objects.h
#endif  /* OBJECTS_DESCR_INIT || OBJECTS_INIT || OBJECTS_ENUM */

#define GENERIC(desc, class, gen_enum) \
    OBJECT(OBJ("未知" desc, desc),                                  \
           BITS(0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, P_NONE, 0),            \
           0, class, 0, 0, 0, 0, 0, 0, 0, 0, 0, CLR_GRAY, gen_enum)

/* dummy object[0] -- description [2nd arg] *must* be NULL */
OBJECT(OBJ("奇怪的物体",NoDes),
       BITS(1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, P_NONE, 0),
       0, ILLOBJ_CLASS, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, STRANGE_OBJECT),
/* slots [1] through [MAXOCLASSES-1] are indexed by class; some are
   used for display purposes, most aren't used; none are actual objects;
   note that 'real' strange object is in slot [0] but ILLOBJ_CLASS is 1
   so we add a dummy for it in slot [1] to simplify accessing the rest;
   there isn't any entry for RANDOM_CLASS (0) */
GENERIC("奇怪的物体",    ILLOBJ_CLASS,  GENERIC_ILLOBJ),  /* [1] */
GENERIC("武器",     WEAPON_CLASS,  GENERIC_WEAPON),  /* [2] */
GENERIC("防具",      ARMOR_CLASS,   GENERIC_ARMOR),   /* [3] */
GENERIC("戒指",       RING_CLASS,    GENERIC_RING),    /* [4] */
GENERIC("护身符",     AMULET_CLASS,  GENERIC_AMULET),  /* [5] */
GENERIC("工具",       TOOL_CLASS,    GENERIC_TOOL),    /* [6] */
GENERIC("食物",       FOOD_CLASS,    GENERIC_FOOD),    /* [7] */
GENERIC("药水",     POTION_CLASS,  GENERIC_POTION),  /* [8] */
GENERIC("卷轴",     SCROLL_CLASS,  GENERIC_SCROLL),  /* [9] */
GENERIC("魔法书",  SPBOOK_CLASS,  GENERIC_SPBOOK),  /* [10] */
GENERIC("魔杖",       WAND_CLASS,    GENERIC_WAND),    /* [11] */
GENERIC("金币",       COIN_CLASS,    GENERIC_COIN),    /* [12] */
GENERIC("宝石",        GEM_CLASS,     GENERIC_GEM),     /* [13] */
GENERIC("巨石", ROCK_CLASS,    GENERIC_ROCK),    /* [14] bldr+statue */
GENERIC("铁球",  BALL_CLASS,    GENERIC_BALL),    /* [15] */
GENERIC("铁链", CHAIN_CLASS,   GENERIC_CHAIN),   /* [16] */
GENERIC("毒液",      VENOM_CLASS,   GENERIC_VENOM),   /* [17] */
#undef GENERIC
/* FIRST_OBJECT: it would be simpler just to use MARKER(FIRST_OBJECT,ARROW)
   below but that is vulnerable to neglecting to update the marker enum
   after inserting something in front of arrow */
MARKER(LAST_GENERIC, GENERIC_VENOM)
MARKER(FIRST_OBJECT, LAST_GENERIC + 1)
/* this definition of FIRST_OBJECT advances the default value for next enum;
   backtrack to fix that, otherwise ARROW and the rest would be off by 1 */
MARKER(OBJCLASS_HACK, FIRST_OBJECT - 1)

/* weapons ... */
#define WEAPON(name,desc,kn,mg,bi,prob,wt,                          \
               cost,sdam,ldam,hitbon,typ,sub,metal,color,sn)        \
    OBJECT(OBJ(name,desc),                                          \
           BITS(kn, mg, 1, 0, 0, 1, 0, 0, bi, 0, typ, sub, metal),  \
           0, WEAPON_CLASS, prob, 0, wt,                            \
           cost, sdam, ldam, hitbon, 0, wt, color,sn)
#define PROJECTILE(name,desc,kn,prob,wt,                            \
                   cost,sdam,ldam,hitbon,metal,sub,color,sn)        \
    OBJECT(OBJ(name,desc),                                          \
           BITS(kn, 1, 1, 0, 0, 1, 0, 0, 0, 0, PIERCE, sub, metal), \
           0, WEAPON_CLASS, prob, 0, wt,                            \
           cost, sdam, ldam, hitbon, 0, wt, color, sn)
#define BOW(name,desc,kn,prob,wt,cost,hitbon,metal,sub,color,sn)    \
    OBJECT(OBJ(name,desc),                                          \
           BITS(kn, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, sub, metal),      \
           0, WEAPON_CLASS, prob, 0, wt,                            \
           cost, 2, 2, hitbon, 0, wt, color, sn)

/* Note: for weapons that don't do an even die of damage (ex. 2-7 or 3-18)
   the extra damage is added on in weapon.c, not here! */

/* weapon strike mode overloads the oc_dir field */
#define P PIERCE
#define S SLASH
#define B WHACK

/* missiles; materiel reflects the arrowhead, not the shaft */
PROJECTILE("箭",NoDes,
           1, 55, 1, 2, 6, 6, 0,        IRON, -P_BOW, HI_METAL,
                                                        ARROW),
PROJECTILE("精灵箭","符文箭",
           0, 20, 1, 2, 7, 6, 0,        WOOD, -P_BOW, HI_WOOD,
                                                        ELVEN_ARROW),
PROJECTILE("兽人箭","粗糙的箭",
           0, 20, 1, 2, 5, 6, 0,        IRON, -P_BOW, CLR_BLACK,
                                                        ORCISH_ARROW),
PROJECTILE("银箭",NoDes,
           1, 12, 1, 5, 6, 6, 0,        SILVER, -P_BOW, HI_SILVER,
                                                        SILVER_ARROW),
PROJECTILE("矢","竹箭",
           0, 15, 1, 4, 7, 7, 1,        METAL, -P_BOW, HI_METAL, YA),
PROJECTILE("弩箭",NoDes,
           1, 55, 1, 2, 4, 6, 0,        IRON, -P_CROSSBOW, HI_METAL,
                                                        CROSSBOW_BOLT),

/* missiles that don't use a launcher */
WEAPON("飞镖",NoDes,
       1, 1, 0, 60,   1,   2,  3,  2, 0, P,   -P_DART, IRON, HI_METAL,
                                                        DART),
WEAPON("手里剑","投掷星镖",
       0, 1, 0, 35,   1,   5,  8,  6, 2, P,   -P_SHURIKEN, IRON, HI_METAL,
                                                        SHURIKEN),
WEAPON("回旋镖",NoDes,
       1, 1, 0, 15,   5,  20,  9,  9, 0, 0,   -P_BOOMERANG, WOOD, HI_WOOD,
                                                        BOOMERANG),

/* spears [note: javelin used to have a separate skill from spears,
   because the latter are primarily stabbing weapons rather than
   throwing ones; but for playability, they've been merged together
   under spear skill and spears can now be thrown like javelins] */
WEAPON("矛",NoDes,
       1, 1, 0, 50,  30,   3,  6,  8, 0, P,   P_SPEAR, IRON, HI_METAL,
                                                        SPEAR),
WEAPON("精灵矛","符文矛",
       0, 1, 0, 10,  30,   3,  7,  8, 0, P,   P_SPEAR, WOOD, HI_WOOD,
                                                        ELVEN_SPEAR),
WEAPON("兽人矛","粗糙的矛",
       0, 1, 0, 13,  30,   3,  5,  8, 0, P,   P_SPEAR, IRON, CLR_BLACK,
                                                        ORCISH_SPEAR),
WEAPON("矮人矛","结实的矛",
       0, 1, 0, 12,  35,   3,  8,  8, 0, P,   P_SPEAR, IRON, HI_METAL,
                                                        DWARVISH_SPEAR),
WEAPON("银矛",NoDes,
       1, 1, 0,  2,  36,  40,  6,  8, 0, P,   P_SPEAR, SILVER, HI_SILVER,
                                                        SILVER_SPEAR),
WEAPON("标枪","投掷矛",
       0, 1, 0, 10,  20,   3,  6,  6, 0, P,   P_SPEAR, IRON, HI_METAL,
                                                        JAVELIN),

/* spearish; doesn't stack, not intended to be thrown */
WEAPON("三叉戟",NoDes,
       1, 0, 0,  8,  25,   5,  6,  4, 0, P,   P_TRIDENT, IRON, HI_METAL,
                                                        TRIDENT),
        /* +1 small, +2d4 large */

/* blades; all stack */
WEAPON("匕首",NoDes,
       1, 1, 0, 30,  10,   4,  4,  3, 2, P,   P_DAGGER, IRON, HI_METAL,
                                                        DAGGER),
WEAPON("精灵匕首","符文匕首",
       0, 1, 0, 10,  10,   4,  5,  3, 2, P,   P_DAGGER, WOOD, HI_WOOD,
                                                        ELVEN_DAGGER),
WEAPON("兽人匕首","粗糙的匕首",
       0, 1, 0, 12,  10,   4,  3,  3, 2, P,   P_DAGGER, IRON, CLR_BLACK,
                                                        ORCISH_DAGGER),
WEAPON("银匕首",NoDes,
       1, 1, 0,  3,  12,  40,  4,  3, 2, P,   P_DAGGER, SILVER, HI_SILVER,
                                                        SILVER_DAGGER),
WEAPON("仪式刀",NoDes,
       1, 1, 0,  0,  10,   4,  4,  3, 2, S,   P_DAGGER, IRON, HI_METAL,
                                                        ATHAME),
WEAPON("手术刀",NoDes,
       1, 1, 0,  0,   5,   6,  3,  3, 2, S,   P_KNIFE, METAL, HI_METAL,
                                                        SCALPEL),
WEAPON("刀",NoDes,
       1, 1, 0, 20,   5,   4,  3,  2, 0, P|S, P_KNIFE, IRON, HI_METAL,
                                                        KNIFE),
WEAPON("细刺剑",NoDes,
       1, 1, 0,  5,   5,   4,  3,  2, 0, P|S, P_KNIFE, IRON, HI_METAL,
                                                        STILETTO),
/* 3.6: worm teeth and crysknives now stack;
   when a stack of teeth is enchanted at once, they fuse into one crysknife;
   when a stack of crysknives drops, the whole stack reverts to teeth */
/* 5.0: change crysknife from MINERAL to BONE and worm tooth from 0 to BONE */
WEAPON("蠕虫牙",NoDes,
       1, 1, 0,  0,  20,   2,  2,  2, 0, 0,   P_KNIFE, BONE, CLR_WHITE,
                                                        WORM_TOOTH),
WEAPON("水晶刀",NoDes,
       1, 1, 0,  0,  20, 100, 10, 10, 3, P,   P_KNIFE, BONE, CLR_WHITE,
                                                        CRYSKNIFE),

/* axes */
WEAPON("斧",NoDes,
       1, 0, 0, 40,  60,   8,  6,  4, 0, S,   P_AXE, IRON, HI_METAL,
                                                        AXE),
WEAPON("战斧","双刃斧",       /* "double-bitted"? */
       0, 0, 1, 10, 120,  40,  8,  6, 0, S,   P_AXE, IRON, HI_METAL,
                                                        BATTLE_AXE),

/* swords */
WEAPON("短剑",NoDes,
       1, 0, 0,  8,  30,  10,  6,  8, 0, P,   P_SHORT_SWORD, IRON, HI_METAL,
                                                        SHORT_SWORD),
WEAPON("精灵短剑","符文短剑",
       0, 0, 0,  2,  30,  10,  8,  8, 0, P,   P_SHORT_SWORD, WOOD, HI_WOOD,
                                                        ELVEN_SHORT_SWORD),
WEAPON("兽人短剑","粗糙的短剑",
       0, 0, 0,  3,  30,  10,  5,  8, 0, P,   P_SHORT_SWORD, IRON, CLR_BLACK,
                                                        ORCISH_SHORT_SWORD),
WEAPON("矮人短剑","宽刃短剑",
       0, 0, 0,  2,  30,  10,  7,  8, 0, P,   P_SHORT_SWORD, IRON, HI_METAL,
                                                        DWARVISH_SHORT_SWORD),
WEAPON("弯刀","弯曲的剑",
       0, 0, 0, 15,  40,  15,  8,  8, 0, S,   P_SABER, IRON, HI_METAL,
                                                        SCIMITAR),
WEAPON("银军刀",NoDes,
       1, 0, 0,  6,  40,  75,  8,  8, 0, S,   P_SABER, SILVER, HI_SILVER,
                                                        SILVER_SABER),
WEAPON("阔剑",NoDes,
       1, 0, 0,  8,  70,  10,  4,  6, 0, S,   P_BROAD_SWORD, IRON, HI_METAL,
                                                        BROADSWORD),
        /* +d4 small, +1 large */
WEAPON("精灵阔剑","符文阔剑",
       0, 0, 0,  4,  70,  10,  6,  6, 0, S,   P_BROAD_SWORD, WOOD, HI_WOOD,
                                                        ELVEN_BROADSWORD),
        /* +d4 small, +1 large */
WEAPON("长剑",NoDes,
       1, 0, 0, 50,  40,  15,  8, 12, 0, S,   P_LONG_SWORD, IRON, HI_METAL,
                                                        LONG_SWORD),
WEAPON("双手剑",NoDes,
       1, 0, 1, 22, 150,  50, 12,  6, 0, S,   P_TWO_HANDED_SWORD,
                                                            IRON, HI_METAL,
                                                        TWO_HANDED_SWORD),
        /* +2d6 large */
WEAPON("武士刀","武士剑",
       0, 0, 0,  4,  40,  80, 10, 12, 1, S,   P_LONG_SWORD, IRON, HI_METAL,
                                                        KATANA),
/* special swords set up for artifacts */
WEAPON("剑","长武士剑",
       0, 0, 1,  0,  60, 500, 16,  8, 2, S,   P_TWO_HANDED_SWORD,
                                                            METAL, HI_METAL,
                                                        TSURUGI),
        /* +2d6 large */
WEAPON("符文剑","符文阔剑",
       0, 0, 0,  0,  40, 300,  4,  6, 0, S,   P_BROAD_SWORD, IRON, CLR_BLACK,
                                                        RUNESWORD),
        /* +d4 small, +1 large; Stormbringer: +5d2 +d8 from level drain */

/* polearms */
/* spear-type */
WEAPON("阔刃矛","普通长柄武器",
       0, 0, 1,  5,  80,  10,  6,  6, 0, P,   P_POLEARMS, IRON, HI_METAL,
                                                        PARTISAN),
        /* +1 large */
WEAPON("三叉矛","带护手的长柄武器",
       0, 0, 1,  5,  50,   6,  4,  4, 0, P,   P_POLEARMS, IRON, HI_METAL,
                                                        RANSEUR),
        /* +d4 both */
WEAPON("蛇形矛","叉形长柄武器",
       0, 0, 1,  5,  50,   5,  6,  6, 0, P,   P_POLEARMS, IRON, HI_METAL,
                                                        SPETUM),
        /* +1 small, +d6 large */
WEAPON("长柄刀","单刃长柄武器",
       0, 0, 1,  8,  75,   6,  6, 10, 0, S,   P_POLEARMS, IRON, HI_METAL,
                                                        GLAIVE),
/* axe-type */
WEAPON("戟","斜刃长柄斧",
       0, 0, 1,  8, 150,  10, 10,  6, 0, P|S, P_POLEARMS, IRON, HI_METAL,
                                                        HALBERD),
        /* +1d6 large */
WEAPON("长柄斧","长刃长柄斧",
       0, 0, 1,  4, 120,   7,  4,  4, 0, S,   P_POLEARMS, IRON, HI_METAL,
                                                        BARDICHE),
        /* +1d4 small, +2d4 large */
WEAPON("钩镰","劈刀长柄武器",
       0, 0, 1,  4, 125,   5,  4,  4, 0, S,   P_POLEARMS, IRON, HI_METAL,
                                                        VOULGE),
        /* +d4 both */
/* curved/hooked */
WEAPON("镰刀矛","长柄镰刀",
       0, 0, 1,  6,  60,   5,  6,  8, 0, P|S, P_POLEARMS, IRON, HI_METAL,
                                                        FAUCHARD),
WEAPON("钩镰刀","修枝钩",
       0, 0, 1,  6,  80,   5,  4,  8, 0, S,   P_POLEARMS, IRON, HI_METAL,
                                                        GUISARME),
        /* +1d4 small */
WEAPON("钩镰戟","钩形长柄武器",
       0, 0, 1,  4, 120,   7,  4, 10, 0, P|S, P_POLEARMS, IRON, HI_METAL,
                                                        BILL_GUISARME),
        /* +1d4 small */
/* other */
WEAPON("长柄锤","叉头长柄武器",
       0, 0, 1,  5, 150,   7,  4,  6, 0, B|P, P_POLEARMS, IRON, HI_METAL,
                                                        LUCERN_HAMMER),
        /* +1d4 small */
WEAPON("鸦喙锤","喙形长柄武器",
       0, 0, 1,  4, 100,   8,  8,  6, 0, B|P, P_POLEARMS, IRON, HI_METAL,
                                                        BEC_DE_CORBIN),

/* formerly grouped with the polearms but don't use polearms skill;
   lance isn't even two-handed */
WEAPON("矮人鹤嘴锄","宽镐",
       0, 0, 1, 13, 120,  50, 12,  8, -1, B,  P_PICK_AXE, IRON, HI_METAL,
                                                        DWARVISH_MATTOCK),
WEAPON("骑枪",NoDes,
       1, 0, 0,  4, 180,  10,  6,  8, 0, P,   P_LANCE, IRON, HI_METAL,
                                                        LANCE),
        /* +2d10 when jousting with lance as primary weapon,
           +2d2 when jousting with it as secondary when dual wielding */

/* bludgeons */
WEAPON("钉锤",NoDes,
       1, 0, 0, 40,  30,   5,  6,  6, 0, B,   P_MACE, IRON, HI_METAL,
                                                        MACE),
        /* +1 small */
WEAPON("银钉锤",NoDes,
       1, 0, 0,  2,  36,  60,  6,  6, 0, B,   P_MACE, SILVER, HI_SILVER,
                                                        SILVER_MACE),
        /* +1 small */
WEAPON("流星锤",NoDes,
       1, 0, 0, 12, 120,  10,  4,  6, 0, B,   P_MORNING_STAR, IRON, HI_METAL,
                                                        MORNING_STAR),
        /* +d4 small, +1 large */
WEAPON("战锤",NoDes,
       1, 0, 0, 15,  50,   5,  4,  4, 0, B,   P_HAMMER, IRON, HI_METAL,
                                                        WAR_HAMMER),
        /* +1 small */
WEAPON("棍棒",NoDes,
       1, 0, 0, 12,  30,   3,  6,  3, 0, B,   P_CLUB, WOOD, HI_WOOD,
                                                        CLUB),
WEAPON("橡皮管",NoDes,
       1, 0, 0,  0,  20,   3,  4,  3, 0, B,   P_WHIP, PLASTIC, CLR_BROWN,
                                                        RUBBER_HOSE),
WEAPON("长棍","棍杖",
       0, 0, 1, 11,  40,   5,  6,  6, 0, B,   P_QUARTERSTAFF, WOOD, HI_WOOD,
                                                        QUARTERSTAFF),
/* two-piece */
WEAPON("投掷棒","带绳棍棒",
       0, 0, 0,  8,  15,   4,  6,  3, 0, B,   P_CLUB, IRON, HI_METAL,
                                                        AKLYS),
WEAPON("连枷",NoDes,
       1, 0, 0, 40,  15,   4,  6,  4, 0, B,   P_FLAIL, IRON, HI_METAL,
                                                        FLAIL),
        /* +1 small, +1d4 large */

/* misc */
WEAPON("牛鞭",NoDes,
       1, 0, 0,  2,  20,   4,  2,  1, 0, 0,   P_WHIP, LEATHER, CLR_BROWN,
                                                        BULLWHIP),

/* bows */
BOW("弓",NoDes,               1, 24, 30, 60, 0, WOOD, P_BOW, HI_WOOD,
                                                        BOW),
BOW("精灵弓","符文弓",  0, 12, 30, 60, 0, WOOD, P_BOW, HI_WOOD,
                                                        ELVEN_BOW),
BOW("兽人弓","粗糙的弓", 0, 12, 30, 60, 0, WOOD, P_BOW, CLR_BLACK,
                                                        ORCISH_BOW),
BOW("弓","长弓",        0,  0, 30, 60, 0, WOOD, P_BOW, HI_WOOD,
                                                        YUMI),
BOW("投石索",NoDes,             1, 40,  3, 20, 0, LEATHER, P_SLING, HI_LEATHER,
                                                        SLING),
BOW("弩",NoDes,          1, 45, 50, 40, 0, WOOD, P_CROSSBOW, HI_WOOD,
                                                        CROSSBOW),

#undef P
#undef S
#undef B

#undef WEAPON
#undef PROJECTILE
#undef BOW

/* armor ... */
        /* IRON denotes ferrous metals, including steel.
         * Only IRON weapons and armor can rust.
         * Only COPPER (including brass) corrodes.
         * Some creatures are vulnerable to SILVER.
         */
#define ARMOR(name,desc,kn,mgc,blk,power,prob,delay,wt,  \
              cost,ac,can,sub,metal,c,sn)                   \
    OBJECT(OBJ(name, desc),                                         \
           BITS(kn, 0, 1, 0, mgc, 1, 0, 0, blk, 0, 0, sub, metal),  \
           power, ARMOR_CLASS, prob, delay, wt,                     \
           cost, 0, 0, 10 - ac, can, wt, c, sn)
#define HELM(name,desc,kn,mgc,power,prob,delay,wt,cost,ac,can,metal,c,sn)  \
    ARMOR(name, desc, kn, mgc, 0, power, prob, delay, wt,  \
          cost, ac, can, ARM_HELM, metal, c, sn)
#define CLOAK(name,desc,kn,mgc,power,prob,delay,wt,cost,ac,can,metal,c,sn)  \
    ARMOR(name, desc, kn, mgc, 0, power, prob, delay, wt,  \
          cost, ac, can, ARM_CLOAK, metal, c,sn)
#define SHIELD(name,desc,kn,mgc,blk,pow,prob,delay,wt,cost,ac,can,metal,c,sn) \
    ARMOR(name, desc, kn, mgc, blk, pow, prob, delay, wt, \
          cost, ac, can, ARM_SHIELD, metal, c,sn)
#define GLOVES(name,desc,kn,mgc,power,prob,delay,wt,cost,ac,can,metal,c,sn)  \
    ARMOR(name, desc, kn, mgc, 0, power, prob, delay, wt,  \
          cost, ac, can, ARM_GLOVES, metal, c,sn)
#define BOOTS(name,desc,kn,mgc,power,prob,delay,wt,cost,ac,can,metal,c,sn)  \
    ARMOR(name, desc, kn, mgc, 0, power, prob, delay, wt,  \
          cost, ac, can, ARM_BOOTS, metal, c,sn)

/* helmets */
HELM("精灵皮盔","皮帽",
     0, 0,           0,  6, 1,  3,  8,  9, 0, LEATHER, HI_LEATHER,
                                                        ELVEN_LEATHER_HELM),
HELM("兽人头盔","铁颅骨帽",
     0, 0,           0,  6, 1, 30, 10,  9, 0, IRON, CLR_BLACK,
                                                        ORCISH_HELM),
HELM("矮人铁盔","硬帽",
     0, 0,           0,  6, 1, 40, 20,  8, 0, IRON, HI_METAL,
                                                        DWARVISH_IRON_HELM),
HELM("软呢帽",NoDes,
     1, 0,           0,  0, 0,  3,  1, 10, 0, CLOTH, CLR_BROWN,
                                                        FEDORA),
HELM("巫师帽","圆锥帽",
     0, 1, CLAIRVOYANT,  5, 1,  4, 80, 10, 1, CLOTH, CLR_BLUE,
        /* name coined by devteam; confers clairvoyance for wizards,
           blocks clairvoyance if worn by role other than wizard */
                                                        CORNUTHAUM),
HELM("笨蛋帽","圆锥帽",
     0, 1,           0,  5, 1,  4,  1, 10, 0, CLOTH, CLR_BLUE,
        /* sets Int and Wis to fixed value of 6, so actually provides
           protection against death caused by Int being drained below 3 */
                                                        DUNCE_CAP),
HELM("凹痕锅",NoDes,
     1, 0,           0,  2, 0, 10,  8,  9, 0, IRON, CLR_BLACK,
                                                        DENTED_POT),
HELM("智慧头盔","水晶头盔",
     0, 1,           0,  6, 1, 40, 50,  9, 0, GLASS, CLR_WHITE,
        /* used to be iron and shuffled as "etched helmet" but required
           special case for the effect of iron armor on spell casting */
                                                        HELM_OF_BRILLIANCE),
/* with shuffled appearances... */
HELM("头盔","羽饰头盔",
     0, 0,           0, 10, 1, 30, 10,  9, 0, IRON, HI_METAL,
                                                        HELMET),
HELM("警觉头盔","蚀刻头盔",
     0, 1,     WARNING,  6, 1, 50, 50,  9, 0, IRON, CLR_GREEN,
                                                        HELM_OF_CAUTION),
HELM("阵营反转头盔","冠饰头盔",
     0, 1,           0, 10, 1, 50, 50,  9, 0, IRON, HI_METAL,
                                                 HELM_OF_OPPOSITE_ALIGNMENT),
HELM("心灵感应头盔","面甲头盔",
     0, 1,     TELEPAT,  4, 1, 50, 50,  9, 0, IRON, HI_METAL,
                                                 HELM_OF_TELEPATHY),

/* suits of armor */
/*
 * There is code in polyself.c that assumes (1) and (2).
 * There is code in obj.h, objnam.c, mon.c, read.c that assumes (2).
 *      (1) The dragon scale mails and the dragon scales are together.
 *      (2) That the order of the dragon scale mail and dragon scales
 *          is the same as order of dragons defined in monst.c.
 */
#define DRGN_ARMR(name,mgc,power,cost,ac,color,snam)  \
    ARMOR(name, NoDes, 1, mgc, 1, power, 0, 5, 40,  \
          cost, ac, 0, ARM_SUIT, DRAGON_HIDE, color,snam)
/* 3.4.1: dragon scale mail reclassified as "magic" since magic is
   needed to create them */
DRGN_ARMR("灰龙鳞甲",    1, ANTIMAGIC,  1200, 1, CLR_GRAY,
                                                    GRAY_DRAGON_SCALE_MAIL),
    /* gold DSM is a light source; there's no property for that */
DRGN_ARMR("金龙鳞甲",    1, 0,           900, 1, HI_GOLD,
                                                    GOLD_DRAGON_SCALE_MAIL),
DRGN_ARMR("银龙鳞甲",  1, REFLECTING, 1200, 1, DRAGON_SILVER,
                                                    SILVER_DRAGON_SCALE_MAIL),
#if 0 /* DEFERRED */
DRGN_ARMR("微光龙鳞甲", 1, DISPLACED, 1200, 1, CLR_CYAN,
                                                SHIMMERING_DRAGON_SCALE_MAIL),
#endif
DRGN_ARMR("红龙鳞甲",     1, FIRE_RES,    900, 1, CLR_RED,
                                                    RED_DRAGON_SCALE_MAIL),
DRGN_ARMR("白龙鳞甲",   1, COLD_RES,    900, 1, CLR_WHITE,
                                                    WHITE_DRAGON_SCALE_MAIL),
DRGN_ARMR("橙龙鳞甲",  1, SLEEP_RES,   900, 1, CLR_ORANGE,
                                                    ORANGE_DRAGON_SCALE_MAIL),
DRGN_ARMR("黑龙鳞甲",   1, DISINT_RES, 1200, 1, CLR_BLACK,
                                                    BLACK_DRAGON_SCALE_MAIL),
DRGN_ARMR("蓝龙鳞甲",    1, SHOCK_RES,   900, 1, CLR_BLUE,
                                                    BLUE_DRAGON_SCALE_MAIL),
DRGN_ARMR("绿龙鳞甲",   1, POISON_RES,  900, 1, CLR_GREEN,
                                                    GREEN_DRAGON_SCALE_MAIL),
DRGN_ARMR("黄龙鳞甲",  1, ACID_RES,    900, 1, CLR_YELLOW,
                                                    YELLOW_DRAGON_SCALE_MAIL),
/* For now, only dragons leave these. */
/* 3.4.1: dragon scales left classified as "non-magic"; they confer magical
   properties but are produced "naturally"; affects use as polypile fodder */
DRGN_ARMR("灰龙鳞",        0, ANTIMAGIC,   700, 7, CLR_GRAY,
                                                        GRAY_DRAGON_SCALES),
DRGN_ARMR("金龙鳞",        0, 0,           500, 7, HI_GOLD,
                                                        GOLD_DRAGON_SCALES),
DRGN_ARMR("银龙鳞",      0, REFLECTING,  700, 7, DRAGON_SILVER,
                                                        SILVER_DRAGON_SCALES),
#if 0 /* DEFERRED */
DRGN_ARMR("微光龙鳞",  0, DISPLACED,   700, 7, CLR_CYAN,
                                                    SHIMMERING_DRAGON_SCALES),
#endif
DRGN_ARMR("红龙鳞",         0, FIRE_RES,    500, 7, CLR_RED,
                                                        RED_DRAGON_SCALES),
DRGN_ARMR("白龙鳞",       0, COLD_RES,    500, 7, CLR_WHITE,
                                                        WHITE_DRAGON_SCALES),
DRGN_ARMR("橙龙鳞",      0, SLEEP_RES,   500, 7, CLR_ORANGE,
                                                        ORANGE_DRAGON_SCALES),
DRGN_ARMR("黑龙鳞",       0, DISINT_RES,  700, 7, CLR_BLACK,
                                                        BLACK_DRAGON_SCALES),
DRGN_ARMR("蓝龙鳞",        0, SHOCK_RES,   500, 7, CLR_BLUE,
                                                        BLUE_DRAGON_SCALES),
DRGN_ARMR("绿龙鳞",       0, POISON_RES,  500, 7, CLR_GREEN,
                                                        GREEN_DRAGON_SCALES),
DRGN_ARMR("黄龙鳞",      0, ACID_RES,    500, 7, CLR_YELLOW,
                                                        YELLOW_DRAGON_SCALES),
#undef DRGN_ARMR
/* other suits */
ARMOR("板甲",NoDes,
      1, 0, 1,  0, 40, 5, 450, 600,  3, 2,  ARM_SUIT, IRON, HI_METAL,
                                                        PLATE_MAIL),
ARMOR("水晶板甲",NoDes,
      1, 0, 1,  0, 10, 5, 415, 820,  3, 2,  ARM_SUIT, GLASS, CLR_WHITE,
                                                        CRYSTAL_PLATE_MAIL),
ARMOR("青铜板甲",NoDes,
      1, 0, 1,  0, 23, 5, 450, 400,  4, 1,  ARM_SUIT, COPPER, HI_COPPER,
                                                        BRONZE_PLATE_MAIL),
ARMOR("鳞甲",NoDes,
      1, 0, 1,  0, 57, 5, 400,  80,  4, 1,  ARM_SUIT, IRON, HI_METAL,
                                                        SPLINT_MAIL),
ARMOR("条板甲",NoDes,
      1, 0, 1,  0, 66, 5, 350,  90,  4, 1,  ARM_SUIT, IRON, HI_METAL,
                                                        BANDED_MAIL),
ARMOR("矮人秘银外套",NoDes,
      1, 0, 0,  0, 10, 1, 150, 240,  4, 2,  ARM_SUIT, MITHRIL, HI_SILVER,
                                                        DWARVISH_MITHRIL_COAT),
ARMOR("精灵秘银外套",NoDes,
      1, 0, 0,  0, 15, 1, 150, 240,  5, 2,  ARM_SUIT, MITHRIL, HI_SILVER,
                                                        ELVEN_MITHRIL_COAT),
ARMOR("链甲",NoDes,
      1, 0, 0,  0, 66, 5, 300,  75,  5, 1,  ARM_SUIT, IRON, HI_METAL,
                                                        CHAIN_MAIL),
ARMOR("兽人链甲","粗糙的链甲",
      0, 0, 0,  0, 19, 5, 300,  75,  6, 1,  ARM_SUIT, IRON, CLR_BLACK,
                                                        ORCISH_CHAIN_MAIL),
ARMOR("鳞片甲",NoDes,
      1, 0, 0,  0, 66, 5, 250,  45,  6, 1,  ARM_SUIT, IRON, HI_METAL,
                                                        SCALE_MAIL),
ARMOR("镶钉皮甲",NoDes,
      1, 0, 0,  0, 66, 3, 200,  15,  7, 1,  ARM_SUIT, LEATHER, HI_LEATHER,
                                                        STUDDED_LEATHER_ARMOR),
ARMOR("环甲",NoDes,
      1, 0, 0,  0, 66, 5, 250, 100,  7, 1,  ARM_SUIT, IRON, HI_METAL,
                                                        RING_MAIL),
ARMOR("兽人环甲","粗糙的环甲",
      0, 0, 0,  0, 19, 5, 250,  80,  8, 1,  ARM_SUIT, IRON, CLR_BLACK,
                                                        ORCISH_RING_MAIL),
ARMOR("皮甲",NoDes,
      1, 0, 0,  0, 75, 3, 150,   5,  8, 1,  ARM_SUIT, LEATHER, HI_LEATHER,
                                                        LEATHER_ARMOR),
ARMOR("皮夹克",NoDes,
      1, 0, 0,  0, 11, 0,  30,  10,  9, 0,  ARM_SUIT, LEATHER, CLR_BLACK,
                                                        LEATHER_JACKET),

/* shirts */
ARMOR("夏威夷衬衫",NoDes,
      1, 0, 0,  0,  8, 0,   5,   3, 10, 0,  ARM_SHIRT, CLOTH, CLR_MAGENTA,
                                                        HAWAIIAN_SHIRT),
ARMOR("T恤",NoDes,
      1, 0, 0,  0,  2, 0,   5,   2, 10, 0,  ARM_SHIRT, CLOTH, CLR_WHITE,
                                                        T_SHIRT),

/* cloaks */
CLOAK("木乃伊裹布",NoDes,
      1, 0,          0,  0, 0,  3,  2, 10, 1,  CLOTH, CLR_GRAY,
                                                        MUMMY_WRAPPING),
        /* worn mummy wrapping blocks invisibility */
CLOAK("精灵披风","褪色的罩衣",
      0, 1,    STEALTH,  8, 0, 10, 60,  9, 1,  CLOTH, CLR_BLACK, ELVEN_CLOAK),
CLOAK("兽人披风","粗糙的斗篷",
      0, 0,          0,  8, 0, 10, 40, 10, 1,  CLOTH, CLR_BLACK,
                                                        ORCISH_CLOAK),
CLOAK("矮人披风","连帽披风",
      0, 0,          0,  8, 0, 10, 50, 10, 1,  CLOTH, HI_CLOTH,
                                                        DWARVISH_CLOAK),
CLOAK("油布披风","光滑的披风",
      0, 0,          0,  8, 0, 10, 50,  9, 2,  CLOTH, HI_CLOTH,
                                                        OILSKIN_CLOAK),
CLOAK("长袍",NoDes,
      1, 1,          0,  6, 0, 15, 50,  8, 2,  CLOTH, CLR_RED, ROBE),
        /* robe was adopted from slash'em, where it's worn as a suit
           rather than as a cloak and there are several variations */
CLOAK("炼金罩衫","围裙",
      0, 1, POISON_RES, 11, 0, 10, 50,  9, 1,  CLOTH, CLR_WHITE,
                                                        ALCHEMY_SMOCK),
CLOAK("皮披风",NoDes,
      1, 0,          0,  8, 0, 15, 40,  9, 1,  LEATHER, CLR_BROWN,
                                                        LEATHER_CLOAK),
/* with shuffled appearances... */
CLOAK("防护披风","破烂的披肩",
      0, 1, PROTECTION, 11, 0, 10, 50,  7, 3,  CLOTH, HI_CLOTH,
                                                        CLOAK_OF_PROTECTION),
        /* cloak of protection is now the only item conferring MC 3 */
CLOAK("隐身披风","歌剧披风",
      0, 1,      INVIS, 12, 0, 10, 60,  9, 1,  CLOTH, CLR_BRIGHT_MAGENTA,
                                                        CLOAK_OF_INVISIBILITY),
CLOAK("抗魔披风","装饰性长袍",
      0, 1,  ANTIMAGIC,  6, 0, 10, 60,  9, 1,  CLOTH, CLR_WHITE,
                                                   CLOAK_OF_MAGIC_RESISTANCE),
        /*  'cope' is not a spelling mistake... leave it be */
CLOAK("位移披风","一块布",
      0, 1,  DISPLACED, 12, 0, 10, 50,  9, 1,  CLOTH, HI_CLOTH,
                                                        CLOAK_OF_DISPLACEMENT),

/* shields */
SHIELD("小盾","木盾",
       0, 0, 0,          0,  6, 0,  30,  3, 9, 0,  WOOD, HI_WOOD,
                                                        SMALL_SHIELD),
SHIELD("防吸取盾","木盾",
       0, 1, 0,  DRAIN_RES, 12, 0,  30, 50, 9, 0,  WOOD, HI_WOOD,
                                                  SHIELD_OF_DRAIN_RESISTANCE),
SHIELD("防电击盾","木盾",
       0, 1, 0,  SHOCK_RES, 12, 0,  30, 50, 9, 0,  WOOD, HI_WOOD,
                                                  SHIELD_OF_SHOCK_RESISTANCE),
SHIELD("精灵盾","蓝绿盾",
       0, 0, 0,          0,  2, 0,  40,  7, 8, 0,  WOOD, CLR_GREEN,
                                                        ELVEN_SHIELD),
SHIELD("乌鲁克族盾","白手盾",
       0, 0, 0,          0,  2, 0,  50,  7, 9, 0,  IRON, HI_METAL,
                                                        URUK_HAI_SHIELD),
SHIELD("兽人盾","红眼盾",
       0, 0, 0,          0,  2, 0,  50,  7, 9, 0,  IRON, CLR_RED,
                                                        ORCISH_SHIELD),
SHIELD("大盾",NoDes,
       1, 0, 1,          0,  4, 0, 100, 10, 8, 0,  IRON, HI_METAL,
                                                        LARGE_SHIELD),
SHIELD("矮人圆盾","大圆盾",
       0, 0, 0,          0,  3, 0, 100, 10, 8, 0,  IRON, HI_METAL,
                                                        DWARVISH_ROUNDSHIELD),
SHIELD("反射盾","抛光银盾",
       0, 1, 0, REFLECTING,  7, 0,  50, 50, 8, 0,  SILVER, HI_SILVER,
                                                        SHIELD_OF_REFLECTION),

/* gloves */
/* These have their color but not material shuffled, so the IRON must
 * stay CLR_BROWN (== HI_LEATHER) even though it's normally either
 * HI_METAL or CLR_BLACK.  All have shuffled descriptions.
 */
GLOVES("皮手套","旧手套",
       0, 0,        0, 15, 1, 10,  8, 9, 0,  LEATHER, HI_LEATHER,
                                                        LEATHER_GLOVES),
GLOVES("笨拙手套","棉手套",
       0, 1, FUMBLING,  8, 1, 10, 50, 9, 0,  LEATHER, HI_LEATHER,
                                                    GAUNTLETS_OF_FUMBLING),
GLOVES("力量手套","骑乘手套",
       0, 1,        0,  8, 1, 30, 50, 9, 0,  IRON, CLR_BROWN,
                                                    GAUNTLETS_OF_POWER),
GLOVES("灵巧手套","击剑手套",
       0, 1,        0,  8, 1, 10, 50, 9, 0,  LEATHER, HI_LEATHER,
                                                    GAUNTLETS_OF_DEXTERITY),

/* boots */
BOOTS("短靴","步行鞋",
      0, 0,          0, 23, 2, 10,  8, 9, 0, LEATHER, HI_LEATHER, LOW_BOOTS),
BOOTS("铁鞋","硬鞋",
      0, 0,          0,  7, 2, 50, 16, 8, 0, IRON, HI_METAL, IRON_SHOES),
BOOTS("长靴","长筒靴",
      0, 0,          0, 14, 2, 20, 12, 8, 0, LEATHER, HI_LEATHER, HIGH_BOOTS),
/* with shuffled appearances... */
BOOTS("速度靴","战斗靴",
      0, 1,       FAST, 12, 2, 20, 50, 9, 0, LEATHER, HI_LEATHER, SPEED_BOOTS),
BOOTS("水上行走靴","丛林靴",
      0, 1,   WWALKING, 12, 2, 15, 50, 9, 0, LEATHER, HI_LEATHER,
                                                        WATER_WALKING_BOOTS),
BOOTS("跳跃靴","徒步靴",
      0, 1,    JUMPING, 12, 2, 20, 50, 9, 0, LEATHER, HI_LEATHER,
                                                        JUMPING_BOOTS),
BOOTS("精灵靴","泥靴",
      0, 1,    STEALTH, 12, 2, 15,  8, 9, 0, LEATHER, HI_LEATHER,
                                                        ELVEN_BOOTS),
BOOTS("踢击靴","带扣靴",
      0, 1,          0, 12, 2, 50,  8, 9, 0, IRON, CLR_BROWN,
                                                        KICKING_BOOTS),
        /* CLR_BROWN for same reason as gauntlets of power */
BOOTS("笨拙靴","骑乘靴",
      0, 1,   FUMBLING, 12, 2, 20, 30, 9, 0, LEATHER, HI_LEATHER,
                                                        FUMBLE_BOOTS),
BOOTS("浮空靴","雪靴",
      0, 1, LEVITATION, 12, 2, 15, 30, 9, 0, LEATHER, HI_LEATHER,
                                                        LEVITATION_BOOTS),
#undef HELM
#undef CLOAK
#undef SHIELD
#undef GLOVES
#undef BOOTS
#undef ARMOR

/* rings ... */
#define RING(name,stone,power,cost,mgc,spec,mohs,metal,color,sn) \
    OBJECT(OBJ(name, stone),                                          \
           BITS(0, 0, spec, 0, mgc, spec, 0, 0, 0,                    \
                HARDGEM(mohs), 0, P_NONE, metal),                     \
           power, RING_CLASS, 1, 0, 3, cost, 0, 0, 0, 0, 15, color,sn)
RING("装饰","木制",
     ADORNED,                  100, 1, 1, 2, WOOD, HI_WOOD, RIN_ADORNMENT),
RING("力量","花岗岩",
     0,                        150, 1, 1, 7, MINERAL, HI_MINERAL,
                                                            RIN_GAIN_STRENGTH),
RING("体质","蛋白石",
     0,                        150, 1, 1, 7, MINERAL, HI_MINERAL,
                                                        RIN_GAIN_CONSTITUTION),
RING("精准","黏土",
     0,                        150, 1, 1, 4, MINERAL, CLR_RED,
                                                        RIN_INCREASE_ACCURACY),
RING("伤害","珊瑚",
     0,                        150, 1, 1, 4, MINERAL, CLR_ORANGE,
                                                        RIN_INCREASE_DAMAGE),
RING("防护","黑玛瑙",
     PROTECTION,               100, 1, 1, 7, MINERAL, CLR_BLACK,
                                                        RIN_PROTECTION),
        /* 'PROTECTION' intrinsic enhances MC from worn armor by +1,
           regardless of ring's enchantment; wearing a second ring of
           protection (or even one ring of protection combined with
           cloak of protection) doesn't give a second MC boost */
RING("再生","月长石",
     REGENERATION,             200, 1, 0,  6, MINERAL, HI_MINERAL,
                                                        RIN_REGENERATION),
RING("搜索","虎眼石",
     SEARCHING,                200, 1, 0,  6, GEMSTONE, CLR_BROWN,
                                                        RIN_SEARCHING  ),
RING("潜行","翡翠",
     STEALTH,                  100, 1, 0,  6, GEMSTONE, CLR_GREEN,
                                                        RIN_STEALTH),
RING("维持能力","青铜",
     FIXED_ABIL,               100, 1, 0,  4, COPPER, HI_COPPER,
                                                        RIN_SUSTAIN_ABILITY),
RING("浮空","玛瑙",
     LEVITATION,               200, 1, 0,  7, GEMSTONE, CLR_RED,
                                                        RIN_LEVITATION),
RING("饥饿","黄玉",
     HUNGER,                   100, 1, 0,  8, GEMSTONE, CLR_CYAN,
                                                        RIN_HUNGER),
RING("激怒怪物","蓝宝石",
     AGGRAVATE_MONSTER,        150, 1, 0,  9, GEMSTONE, CLR_BLUE,
                                                        RIN_AGGRAVATE_MONSTER),
RING("冲突","红宝石",
     CONFLICT,                 300, 1, 0,  9, GEMSTONE, CLR_RED,
                                                        RIN_CONFLICT),
RING("警告","钻石",
     WARNING,                  100, 1, 0, 10, GEMSTONE, CLR_WHITE,
                                                        RIN_WARNING),
RING("防毒","珍珠",
     POISON_RES,               150, 1, 0,  4, BONE, CLR_WHITE,
                                                        RIN_POISON_RESISTANCE),
RING("防火","铁",
     FIRE_RES,                 200, 1, 0,  5, IRON, HI_METAL,
                                                        RIN_FIRE_RESISTANCE),
RING("防寒","黄铜",
     COLD_RES,                 150, 1, 0,  4, COPPER, HI_COPPER,
                                                        RIN_COLD_RESISTANCE),
RING("防电","铜",
     SHOCK_RES,                150, 1, 0,  3, COPPER, HI_COPPER,
                                                        RIN_SHOCK_RESISTANCE),
RING("自由行动","扭曲",
     FREE_ACTION,              200, 1, 0,  6, IRON, HI_METAL,
                                                        RIN_FREE_ACTION),
RING("慢消化","钢",
     SLOW_DIGESTION,           200, 1, 0,  8, IRON, HI_METAL,
                                                        RIN_SLOW_DIGESTION),
RING("传送","银",
     TELEPORT,                 200, 1, 0,  3, SILVER, HI_SILVER,
                                                        RIN_TELEPORTATION),
RING("传送控制","金",
     TELEPORT_CONTROL,         300, 1, 0,  3, GOLD, HI_GOLD,
                                                        RIN_TELEPORT_CONTROL),
RING("变形","象牙",
     POLYMORPH,                300, 1, 0,  4, BONE, CLR_WHITE,
                                                        RIN_POLYMORPH),
RING("变形控制","绿宝石",
     POLYMORPH_CONTROL,        300, 1, 0,  8, GEMSTONE, CLR_BRIGHT_GREEN,
                                                        RIN_POLYMORPH_CONTROL),
RING("隐身","铁丝",
     INVIS,                    150, 1, 0,  5, IRON, HI_METAL,
                                                        RIN_INVISIBILITY),
RING("看见隐形","订婚",
     SEE_INVIS,                150, 1, 0,  5, IRON, HI_METAL,
                                                        RIN_SEE_INVISIBLE),
RING("变形者防护","闪亮",
     PROT_FROM_SHAPE_CHANGERS, 100, 1, 0,  5, IRON, CLR_BRIGHT_CYAN,
                                               RIN_PROTECTION_FROM_SHAPE_CHAN),
#undef RING

/* amulets ... - THE Amulet comes last because it is special */
#define AMULET(name,desc,power,prob,sn) \
    OBJECT(OBJ(name, desc),                                            \
           BITS(0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, P_NONE, IRON),        \
           power, AMULET_CLASS, prob, 0, 20, 150, 0, 0, 0, 0, 20, HI_METAL, sn)
AMULET("感应护身符","圆形", TELEPAT, 120,
                                                        AMULET_OF_ESP),
MARKER(FIRST_AMULET, AMULET_OF_ESP)
AMULET("生命拯救护身符","球形", LIFESAVED, 75,
                                                        AMULET_OF_LIFE_SAVING),
AMULET("勒杀护身符","椭圆形", STRANGLED, 115,
                                                      AMULET_OF_STRANGULATION),
AMULET("沉睡护身符","三角形", SLEEPY, 115,
                                                      AMULET_OF_RESTFUL_SLEEP),
AMULET("防毒护身符","金字塔形", POISON_RES, 115,
                                                        AMULET_VERSUS_POISON),
AMULET("变化护身符","方形", 0, 115,
                                                        AMULET_OF_CHANGE),
AMULET("不变护身符","凹面", UNCHANGING, 60,
                                                        AMULET_OF_UNCHANGING),
AMULET("反射护身符","六角形", REFLECTING, 75,
                                                        AMULET_OF_REFLECTION),
AMULET("魔法呼吸护身符","八角形", MAGICAL_BREATHING, 75,
                                                  AMULET_OF_MAGICAL_BREATHING),
        /* +2 AC and +2 MC; +2 takes naked hero past 'warded' to 'guarded' */
AMULET("防护护身符","穿孔", PROTECTION, 75,
                                                        AMULET_OF_GUARDING),
        /* cubical: some descriptions are already three dimensional and
           parallelogrammatical (real word!) would be way over the top */
AMULET("飞行护身符","立方体", FLYING, 60,
                                                        AMULET_OF_FLYING),
/* fixed descriptions; description duplication is deliberate;
 * fake one must come before real one because selection for
 * description shuffling stops when a non-magic amulet is encountered
 */
OBJECT(OBJ("廉价塑料仿制岩德护身符",
           "岩德护身符"),
       BITS(0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, PLASTIC),
       0, AMULET_CLASS, 0, 0, 20, 0, 0, 0, 0, 0, 1, HI_METAL,
                                                FAKE_AMULET_OF_YENDOR),
OBJECT(OBJ("岩德护身符", /* note: description == name */
           "岩德护身符"),
       BITS(0, 0, 1, 0, 1, 0, 1, 1, 0, 0, 0, 0, MITHRIL),
       0, AMULET_CLASS, 0, 0, 20, 30000, 0, 0, 0, 0, 20, HI_METAL,
                                                AMULET_OF_YENDOR),
MARKER(LAST_AMULET, AMULET_OF_YENDOR)
#undef AMULET

/* tools ... */
/* tools with weapon characteristics come last */
#define TOOL(name,desc,kn,mrg,mgc,chg,prob,wt,cost,mat,color,sn) \
    OBJECT(OBJ(name, desc),                                             \
           BITS(kn, mrg, chg, 0, mgc, chg, 0, 0, 0, 0, 0, P_NONE, mat), \
           0, TOOL_CLASS, prob, 0, wt, cost, 0, 0, 0, 0, wt, color, sn)
#define CONTAINER(name,desc,kn,mgc,chg,prob,wt,cost,mat,color,sn) \
    OBJECT(OBJ(name, desc),                                             \
           BITS(kn, 0, chg, 1, mgc, chg, 0, 0, 0, 0, 0, P_NONE, mat),   \
           0, TOOL_CLASS, prob, 0, wt, cost, 0, 0, 0, 0, wt, color, sn)
#define EYEWEAR(name,desc,kn,prop,prob,wt,cost,mat,color,sn) \
    OBJECT(OBJ(name, desc),                                             \
           BITS(kn, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, P_NONE, mat),         \
           prop, TOOL_CLASS, prob, 0, wt, cost, 0, 0, 0, 0, wt, color, sn)
#define WEPTOOL(name,desc,kn,mgc,bi,prob,wt,cost,sdam,ldam,hitbon,sub, \
                mat,clr,sn)                                             \
    OBJECT(OBJ(name, desc),                                             \
           BITS(kn, 0, 1, 0, mgc, 1, 0, 0, bi, 0, hitbon, sub, mat),    \
           0, TOOL_CLASS, prob, 0, wt, cost, sdam, ldam, hitbon, 0, wt, \
           clr, sn)
/* containers */
CONTAINER("大箱子",NoDes, 1, 0, 0, 40, 350,   8, WOOD, HI_WOOD,
                                                                LARGE_BOX),
CONTAINER("箱子",NoDes, 1, 0, 0, 35, 600,  16, WOOD, HI_WOOD,
                                                                CHEST),
CONTAINER("冰柜",NoDes, 1, 0, 0,  5, 900,  42, PLASTIC, CLR_WHITE,
                                                                ICE_BOX),
CONTAINER("袋子","袋子", 0, 0, 0, 35,  15,   2, CLOTH, HI_CLOTH,
                                                                SACK),
CONTAINER("油布袋","袋子", 0, 0, 0,  5,  15, 100, CLOTH, HI_CLOTH,
                                                                OILSKIN_SACK),
CONTAINER("容纳袋","袋子", 0, 1, 0, 20,  15, 100, CLOTH, HI_CLOTH,
                                                               BAG_OF_HOLDING),
CONTAINER("戏法袋","袋子", 0, 1, 1, 20,  15, 100, CLOTH, HI_CLOTH,
                                                                BAG_OF_TRICKS),
#undef CONTAINER

/* lock opening tools */
TOOL("万能钥匙","钥匙", 0, 0, 0, 0, 80,  3, 10, IRON, HI_METAL,
                                                                SKELETON_KEY),
TOOL("开锁器",NoDes, 1, 0, 0, 0, 60,  4, 20, IRON, HI_METAL,
                                                                LOCK_PICK),
TOOL("信用卡",NoDes, 1, 0, 0, 0, 15,  1, 10, PLASTIC, CLR_WHITE,
                                                                CREDIT_CARD),
/* light sources */
TOOL("牛脂蜡烛","蜡烛", 0, 1, 0, 0, 20,  2, 10, WAX, CLR_WHITE,
                                                                TALLOW_CANDLE),
TOOL("蜂蜡蜡烛","蜡烛", 0, 1, 0, 0,  5,  2, 20, WAX, CLR_WHITE,
                                                                WAX_CANDLE),
TOOL("黄铜提灯",NoDes, 1, 0, 0, 0, 30, 30, 12, COPPER, CLR_YELLOW,
                                                                BRASS_LANTERN),
TOOL("油灯","灯", 0, 0, 0, 0, 45, 20, 10, COPPER, CLR_YELLOW,
                                                                OIL_LAMP),
TOOL("魔法灯","灯", 0, 0, 1, 0, 15, 20, 50, COPPER, CLR_YELLOW,
                                                                MAGIC_LAMP),
/* other tools */
TOOL("昂贵相机",NoDes, 1, 0, 0, 1, 15, 12,200, PLASTIC, CLR_BLACK,
                                                            EXPENSIVE_CAMERA),
TOOL("镜子","镜子", 0, 0, 0, 0, 45, 13, 10, GLASS, HI_SILVER,
                                                                MIRROR),
TOOL("水晶球","玻璃球", 0, 0, 1, 1, 15,150, 60, GLASS, HI_GLASS,
                                                                CRYSTAL_BALL),
/* eyewear - tools which can be worn on the face; (!mrg, !chg, !mgc)
   worn lenses don't confer the Blinded property, blindfolds and towels do;
   wet towel can be used as a weapon but is not a weptool and uses obj->spe
   differently from weapons and weptools */
EYEWEAR("透镜",NoDes, 1,       0,  5,  3, 80, GLASS, HI_GLASS,
                                                                LENSES),
EYEWEAR("眼罩",NoDes, 1, BLINDED, 50,  2, 20, CLOTH, CLR_BLACK,
                                                                BLINDFOLD),
EYEWEAR("毛巾",NoDes, 1, BLINDED, 50,  5, 50, CLOTH, CLR_MAGENTA,
                                                                TOWEL),
#undef EYEWEAR

/* still other tools */
TOOL("马鞍",NoDes, 1, 0, 0, 0,  5,200,150, LEATHER, HI_LEATHER,
                                                                SADDLE),
TOOL("皮带",NoDes, 1, 0, 0, 0, 65, 12, 20, LEATHER, HI_LEATHER,
                                                                LEASH),
TOOL("听诊器",NoDes, 1, 0, 0, 0, 25,  4, 75, IRON, HI_METAL,
                                                                STETHOSCOPE),
TOOL("罐头工具",NoDes, 1, 0, 0, 1, 15,100, 30, IRON, HI_METAL,
                                                                TINNING_KIT),
TOOL("开罐器",NoDes, 1, 0, 0, 0, 35,  4, 30, IRON, HI_METAL,
                                                                TIN_OPENER),
TOOL("润滑油罐",NoDes, 1, 0, 0, 1, 15, 15, 20, IRON, HI_METAL,
                                                                CAN_OF_GREASE),
TOOL("小雕像",NoDes, 1, 0, 1, 0, 25, 50, 80, MINERAL, HI_MINERAL,
                                                                FIGURINE),
        /* monster type specified by obj->corpsenm */
TOOL("魔法笔",NoDes, 1, 0, 1, 1, 15,  2, 50, PLASTIC, CLR_RED,
                                                                MAGIC_MARKER),
/* traps */
TOOL("地雷",NoDes, 1, 0, 0, 0, 0, 200,180, IRON, CLR_RED,
                                                                LAND_MINE),
TOOL("捕兽夹",NoDes, 1, 0, 0, 0, 0, 200, 60, IRON, HI_METAL,
                                                                BEARTRAP),
/* instruments;
   "If tin whistles are made out of tin, what do they make foghorns out of?" */
TOOL("锡哨","哨子", 0, 0, 0, 0,100, 3, 10, METAL, HI_METAL,
                                                                TIN_WHISTLE),
TOOL("魔法哨","哨子", 0, 0, 1, 0, 30, 3, 10, METAL, HI_METAL,
                                                                MAGIC_WHISTLE),
TOOL("木笛","笛子", 0, 0, 0, 0,  4, 5, 12, WOOD, HI_WOOD,
                                                                WOODEN_FLUTE),
TOOL("魔法笛","笛子", 0, 0, 1, 1,  2, 5, 36, WOOD, HI_WOOD,
                                                                MAGIC_FLUTE),
TOOL("加工号角","号角", 0, 0, 0, 0,  5, 18, 15, BONE, CLR_WHITE,
                                                                TOOLED_HORN),
TOOL("寒冰号角","号角", 0, 0, 1, 1,  2, 18, 50, BONE, CLR_WHITE,
                                                                FROST_HORN),
TOOL("烈焰号角","号角", 0, 0, 1, 1,  2, 18, 50, BONE, CLR_WHITE,
                                                                FIRE_HORN),
TOOL("丰饶号角","号角", 0, 0, 1, 1,  2, 18, 50, BONE, CLR_WHITE,
                                                            HORN_OF_PLENTY),
        /* horn, but not an instrument */
TOOL("木竖琴","竖琴", 0, 0, 0, 0,  4, 30, 50, WOOD, HI_WOOD,
                                                                WOODEN_HARP),
TOOL("魔法竖琴","竖琴", 0, 0, 1, 1,  2, 30, 50, WOOD, HI_WOOD,
                                                                MAGIC_HARP),
TOOL("铃铛",NoDes, 1, 0, 0, 0,  2, 30, 50, COPPER, HI_COPPER,
                                                                BELL),
TOOL("号角",NoDes, 1, 0, 0, 0,  4, 10, 15, COPPER, HI_COPPER,
                                                                BUGLE),
TOOL("皮鼓","鼓", 0, 0, 0, 0,  4, 25, 25, LEATHER, HI_LEATHER,
                                                                LEATHER_DRUM),
TOOL("地震鼓","鼓", 0, 0, 1, 1,  2, 25, 25, LEATHER, HI_LEATHER,
                                                          DRUM_OF_EARTHQUAKE),
/* tools useful as weapons */
WEPTOOL("镐",NoDes,
        1, 0, 0, 20, 100,  50,  6,  3, WHACK,  P_PICK_AXE, IRON, HI_METAL,
                                                                PICK_AXE),
WEPTOOL("抓钩",NoDes,
        1, 0, 0,  5,  30,  50,  2,  6, WHACK,  P_FLAIL,    IRON, HI_METAL,
                                                             GRAPPLING_HOOK),
WEPTOOL("独角兽角",NoDes,
        1, 1, 1,  0,  20, 100, 12, 12, PIERCE, P_UNICORN_HORN,
                                                           BONE, CLR_WHITE,
                                                                UNICORN_HORN),
        /* 3.4.1: unicorn horn left classified as "magic" */
/* two unique tools;
 * not artifacts, despite the comment which used to be here
 */
OBJECT(OBJ("祈唤之烛台","烛台"),
       BITS(0, 0, 1, 0, 1, 0, 1, 1, 0, 0, 0, P_NONE, GOLD),
       0, TOOL_CLASS, 0, 0, 10, 5000, 0, 0, 0, 0, 200, HI_GOLD,
                                                   CANDELABRUM_OF_INVOCATION),
OBJECT(OBJ("开启之铃","银铃"),
       BITS(0, 0, 1, 0, 1, 1, 1, 1, 0, 0, 0, P_NONE, SILVER),
       0, TOOL_CLASS, 0, 0, 10, 5000, 0, 0, 0, 0, 50, HI_SILVER,
                                                   BELL_OF_OPENING),
#undef TOOL
#undef WEPTOOL

/* Comestibles ... */
#define FOOD(name, prob, delay, wt, unk, tin, nutrition, color, sn) \
    OBJECT(OBJ(name, NoDes),                                            \
           BITS(1, 1, unk, 0, 0, 0, 0, 0, 0, 0, 0, P_NONE, tin), 0,     \
           FOOD_CLASS, prob, delay, wt, nutrition / 20 + 5, 0, 0, 0, 0, \
           nutrition, color, sn)
/* All types of food (except tins & corpses) must have a delay of at least 1.
 * Delay on corpses is computed and is weight dependent.
 * Domestic pets prefer tripe rations above all others.
 * Fortune cookies can be read, using them up without ingesting them.
 * Carrots improve your vision.
 * +0 tins contain monster meat.
 * +1 tins (of spinach) make you stronger (like Popeye).
 * Meatballs/sticks/rings are only created from objects via stone to flesh.
 */
/* meat */
FOOD("内脏口粮",        140,  2, 10, 0, FLESH, 200, CLR_BROWN,
                                                        TRIPE_RATION),
FOOD("尸体",                0,  1,  0, 0, FLESH,   0, CLR_BROWN,
                                                        CORPSE),
FOOD("蛋",                  85,  1,  1, 1, FLESH,  80, CLR_WHITE,
                                                        EGG),
FOOD("肉丸",              0,  1,  1, 0, FLESH,   5, CLR_BROWN,
                                                        MEATBALL),
FOOD("肉条",            0,  1,  1, 0, FLESH,   5, CLR_BROWN,
                                                        MEAT_STICK),
/* formerly "huge chunk of meat" */
FOOD("巨大肉丸",     0, 20,400, 0, FLESH,2000, CLR_BROWN,
                                                        ENORMOUS_MEATBALL),
/* special case because it's not mergeable */
OBJECT(OBJ("肉环",NoDes),
       BITS(1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, FLESH),
       0, FOOD_CLASS, 0, 1, 5, 1, 0, 0, 0, 0, 5, CLR_BROWN, MEAT_RING),
/* pudding 'corpses' will turn into these and combine;
   must be in same order as the pudding monsters */
FOOD("灰色软泥团",     0,  2, 20, 0, FLESH,  20, CLR_GRAY,
                                                       GLOB_OF_GRAY_OOZE),
FOOD("棕色布丁团", 0,  2, 20, 0, FLESH,  20, CLR_BROWN,
                                                       GLOB_OF_BROWN_PUDDING),
FOOD("绿色史莱姆团",   0,  2, 20, 0, FLESH,  20, CLR_GREEN,
                                                       GLOB_OF_GREEN_SLIME),
FOOD("黑色布丁团", 0,  2, 20, 0, FLESH,  20, CLR_BLACK,
                                                       GLOB_OF_BLACK_PUDDING),

/* fruits & veggies */
FOOD("海藻叶",            0,  1,  1, 0, VEGGY,  30, CLR_GREEN, KELP_FROND),
FOOD("桉树叶",       3,  1,  1, 0, VEGGY,   1, CLR_GREEN,
                                                          EUCALYPTUS_LEAF),
FOOD("苹果",                15,  1,  2, 0, VEGGY,  50, CLR_RED, APPLE),
FOOD("橙子",               10,  1,  2, 0, VEGGY,  80, CLR_ORANGE, ORANGE),
FOOD("梨",                 10,  1,  2, 0, VEGGY,  50, CLR_BRIGHT_GREEN,
                                                          PEAR),
FOOD("瓜",                10,  1,  5, 0, VEGGY, 100, CLR_BRIGHT_GREEN,
                                                          MELON),
FOOD("香蕉",               10,  1,  2, 0, VEGGY,  80, CLR_YELLOW, BANANA),
FOOD("胡萝卜",               15,  1,  2, 0, VEGGY,  50, CLR_ORANGE, CARROT),
FOOD("狼毒草枝",    7,  1,  1, 0, VEGGY,  40, CLR_GREEN,
                                                          SPRIG_OF_WOLFSBANE),
FOOD("蒜瓣",       7,  1,  1, 0, VEGGY,  40, CLR_WHITE,
                                                          CLOVE_OF_GARLIC),
/* name of slime mold is changed based on player's OPTION=fruit:something
   and bones data might have differently named ones from prior games */
FOOD("黏菌",           75,  1,  5, 0, VEGGY, 250, HI_ORGANIC,
                                                          SLIME_MOLD),

/* people food */
FOOD("蜂王浆块",   0,  1,  2, 0, VEGGY, 200, CLR_YELLOW,
                                                        LUMP_OF_ROYAL_JELLY),
FOOD("奶油派",            25,  1, 10, 0, VEGGY, 100, CLR_WHITE, CREAM_PIE),
FOOD("糖果棒",            13,  1,  2, 0, VEGGY, 100, CLR_BRIGHT_BLUE,
                                                                CANDY_BAR),
FOOD("幸运签饼",       55,  1,  1, 0, VEGGY,  40, CLR_YELLOW,
                                                              FORTUNE_COOKIE),
FOOD("煎饼",              25,  2,  2, 0, VEGGY, 200, CLR_YELLOW, PANCAKE),
FOOD("兰巴斯干粮",         20,  2,  5, 0, VEGGY, 800, CLR_WHITE,
                                                                LEMBAS_WAFER),
FOOD("饼干口粮",          20,  3, 15, 0, VEGGY, 600, HI_ORGANIC,
                                                                CRAM_RATION),
FOOD("食物口粮",         380,  5, 20, 0, VEGGY, 800, HI_ORGANIC,
                                                                FOOD_RATION),
FOOD("K口粮",              0,  1, 10, 0, VEGGY, 400, HI_ORGANIC, K_RATION),
FOOD("C口粮",              0,  1, 10, 0, VEGGY, 300, HI_ORGANIC, C_RATION),
/* tins have type specified by obj->spe (+1 for spinach, other implies
   flesh; negative specifies preparation method {homemade,boiled,&c})
   and by obj->corpsenm (type of monster flesh) */
FOOD("罐头",                  75,  0, 10, 1, METAL,   0, HI_METAL, TIN),
#undef FOOD

/* potions ... */
#define POTION(name,desc,mgc,power,prob,cost,color,sn) \
    OBJECT(OBJ(name, desc),                                             \
           BITS(0, 1, 0, 0, mgc, 0, 0, 0, 0, 0, 0, P_NONE, GLASS),      \
           power, POTION_CLASS, prob, 0, 20, cost, 0, 0, 0, 0, 10, color, sn)
POTION("增益能力","红宝石色",  1, 0, 40, 300, CLR_RED,
                                                        POT_GAIN_ABILITY),
POTION("恢复能力","粉色",  1, 0, 40, 100, CLR_BRIGHT_MAGENTA,
                                                        POT_RESTORE_ABILITY),
POTION("混乱","橙色",  1, CONFUSION, 40, 100, CLR_ORANGE,
                                                        POT_CONFUSION),
POTION("失明","黄色",  1, BLINDED, 30, 150, CLR_YELLOW,
                                                        POT_BLINDNESS),
POTION("麻痹","翡翠色",  1, 0, 40, 300, CLR_BRIGHT_GREEN,
                                                        POT_PARALYSIS),
POTION("加速","深绿色",  1, FAST, 40, 200, CLR_GREEN,
                                                        POT_SPEED),
POTION("浮空","青色",  1, LEVITATION, 40, 200, CLR_CYAN,
                                                        POT_LEVITATION),
POTION("幻觉","天蓝色",  1, HALLUC, 30, 100, CLR_CYAN,
                                                        POT_HALLUCINATION),
POTION("隐身","亮蓝色",  1, INVIS, 40, 150, CLR_BRIGHT_BLUE,
                                                        POT_INVISIBILITY),
POTION("看见隐形","洋红色",  1, SEE_INVIS, 40, 50, CLR_MAGENTA,
                                                        POT_SEE_INVISIBLE),
POTION("治疗","紫红色",  1, 0, 115, 20, CLR_MAGENTA,
                                                        POT_HEALING),
POTION("强力治疗","紫褐色",  1, 0, 45, 100, CLR_RED,
                                                        POT_EXTRA_HEALING),
POTION("升级","乳白色",  1, 0, 20, 300, CLR_WHITE,
                                                        POT_GAIN_LEVEL),
POTION("启迪","漩涡色",  1, 0, 20, 200, CLR_BROWN,
                                                        POT_ENLIGHTENMENT),
POTION("怪物探测","气泡色",  1, 0, 40, 150, CLR_WHITE,
                                                        POT_MONSTER_DETECTION),
POTION("物品探测","烟雾色",  1, 0, 40, 150, CLR_GRAY,
                                                        POT_OBJECT_DETECTION),
POTION("补充能量","浑浊色",  1, 0, 40, 150, CLR_WHITE,
                                                        POT_GAIN_ENERGY),
POTION("睡眠","沸腾色",  1, 0, 40, 100, CLR_GRAY,
                                                        POT_SLEEPING),
POTION("完全治疗","黑色",  1, 0, 10, 200, CLR_BLACK,
                                                        POT_FULL_HEALING),
POTION("变形","金色",  1, 0, 10, 200, CLR_YELLOW,
                                                        POT_POLYMORPH),
POTION("酒","棕色",  0, 0, 40,  50, CLR_BROWN,
                                                        POT_BOOZE),
POTION("疾病","冒泡色",  0, 0, 40,  50, CLR_CYAN,
                                                        POT_SICKNESS),
POTION("果汁","暗色",  0, 0, 40,  50, CLR_BLACK,
                                                        POT_FRUIT_JUICE),
POTION("酸液","白色",  0, 0, 10, 250, CLR_WHITE,
                                                        POT_ACID),
POTION("油","暗浊色",  0, 0, 30, 250, CLR_BROWN,
                                                        POT_OIL),
/* fixed description
 */
POTION("水","透明色",  0, 0, 80, 100, CLR_CYAN,
                                                        POT_WATER),
#undef POTION

/* scrolls ... */
#define SCROLL(name,text,mgc,prob,cost,sn) \
    OBJECT(OBJ(name, text),                                           \
           BITS(0, 1, 0, 0, mgc, 0, 0, 0, 0, 0, 0, P_NONE, PAPER),    \
           0, SCROLL_CLASS, prob, 0, 5, cost, 0, 0, 0, 0, 6, \
           HI_PAPER, sn)
SCROLL("附魔防具","ZELGO MER",  1,  63,  80,
                                                        SCR_ENCHANT_ARMOR),
SCROLL("摧毁防具","JUYED AWK YACC",  1,  45, 100,
                                                        SCR_DESTROY_ARMOR),
SCROLL("混乱怪物","NR 9",  1,  53, 100,
                                                        SCR_CONFUSE_MONSTER),
SCROLL("恐吓怪物","XIXAXA XOXAXA XUXAXA",  1,  35, 100,
                                                        SCR_SCARE_MONSTER),
SCROLL("解除诅咒","PRATYAVAYAH",  1,  65,  80,
                                                        SCR_REMOVE_CURSE),
SCROLL("附魔武器","DAIYEN FOOELS",  1,  80,  60,
                                                        SCR_ENCHANT_WEAPON),
SCROLL("创造怪物","LEP GEX VEN ZEA",  1,  45, 200,
                                                        SCR_CREATE_MONSTER),
SCROLL("驯服","PRIRUTSENIE",  1,  15, 200,
                                                        SCR_TAMING),
SCROLL("灭绝","ELBIB YLOH",  1,  15, 300,
                                                        SCR_GENOCIDE),
SCROLL("光亮","VERR YED HORRE",  1,  90,  50,
                                                        SCR_LIGHT),
SCROLL("传送","VENZAR BORGAVVE",  1,  55, 100,
                                                        SCR_TELEPORTATION),
SCROLL("金币探测","THARR",  1,  33, 100,
                                                        SCR_GOLD_DETECTION),
SCROLL("食物探测","YUM YUM",  1,  25, 100,
                                                        SCR_FOOD_DETECTION),
SCROLL("鉴定","KERNOD WEL",  1, 180,  20,
                                                        SCR_IDENTIFY),
SCROLL("魔法地图","ELAM EBOW",  1,  45, 100,
                                                        SCR_MAGIC_MAPPING),
SCROLL("失忆","DUAM XNAHT",  1,  35, 200,
                                                        SCR_AMNESIA),
SCROLL("火焰","ANDOVA BEGARIN",  1,  30, 100,
                                                        SCR_FIRE),
SCROLL("大地","KIRJE",  1,  18, 200,
                                                        SCR_EARTH),
SCROLL("惩罚","VE FORBRYDERNE",  1,  15, 300,
                                                        SCR_PUNISHMENT),
SCROLL("充能","HACKEM MUCHE",  1,  15, 300,
                                                        SCR_CHARGING),
SCROLL("臭云","VELOX NEB",  1,  15, 300,
                                                        SCR_STINKING_CLOUD),
    /* Extra descriptions, shuffled into use at start of new game.
     * Code in win/share/tilemap.c depends on SCR_STINKING_CLOUD preceding
     * these and on how many of them there are.  If a real scroll gets added
     * after stinking cloud or the number of extra descriptions changes,
     * tilemap.c must be modified to match.  Mgc,Prob,Cost are superfluous.
     * SC values must be distinct but are only used by 'nethack --dumpenums'.
     */
#define XTRA_SCROLL_LABEL(text, sn) SCROLL(NoDes, text, 1, 0, 100, sn)
XTRA_SCROLL_LABEL(     "FOOBIE BLETCH", SC01),
XTRA_SCROLL_LABEL(             "TEMOV", SC02),
XTRA_SCROLL_LABEL(        "GARVEN DEH", SC03),
XTRA_SCROLL_LABEL(           "READ ME", SC04),
XTRA_SCROLL_LABEL(     "ETAOIN SHRDLU", SC05),
XTRA_SCROLL_LABEL(       "LOREM IPSUM", SC06),
XTRA_SCROLL_LABEL(             "FNORD", SC07), /* Illuminati */
XTRA_SCROLL_LABEL(           "KO BATE", SC08), /* Kurd Lasswitz */
XTRA_SCROLL_LABEL(     "ABRA KA DABRA", SC09), /* traditional incantation */
XTRA_SCROLL_LABEL(      "ASHPD SODALG", SC10), /* Portal */
XTRA_SCROLL_LABEL(           "ZLORFIK", SC11), /* Zak McKracken */
XTRA_SCROLL_LABEL(     "GNIK SISI VLE", SC12), /* Zak McKracken */
XTRA_SCROLL_LABEL(   "HAPAX LEGOMENON", SC13),
XTRA_SCROLL_LABEL( "EIRIS SAZUN IDISI", SC14), /* Merseburg Incantations */
XTRA_SCROLL_LABEL(   "PHOL ENDE WODAN", SC15), /* Merseburg Incantations */
XTRA_SCROLL_LABEL(             "GHOTI", SC16), /* pronounced as 'fish',
                                                * George Bernard Shaw */
XTRA_SCROLL_LABEL("MAPIRO MAHAMA DIROMAT", SC17), /* Wizardry */
XTRA_SCROLL_LABEL( "VAS CORP BET MANI", SC18), /* Ultima */
XTRA_SCROLL_LABEL(           "XOR OTA", SC19), /* Aarne Haapakoski */
XTRA_SCROLL_LABEL("STRC PRST SKRZ KRK", SC20), /* Czech and Slovak
                                                * tongue-twister */
#undef XTRA_SCROLL_LABEL
    /* These must come last because they have special fixed descriptions.
     */
#ifdef MAIL_STRUCTURES
SCROLL("邮件","盖戳的",  0,   0,   0, SCR_MAIL),
#endif
SCROLL("空白纸","无标签",  0,  28,  60, SCR_BLANK_PAPER),
#undef SCROLL

/* spellbooks ... */
    /* Expanding beyond 52 spells would require changes in spellcasting
     * or imposition of a limit on number of spells hero can know because
     * they are currently assigned successive letters, a-zA-Z, when learned.
     * [The existing spell sorting capability could conceivably be extended
     * to enable moving spells from beyond Z to within it, bumping others
     * out in the process, allowing more than 52 spells be known but keeping
     * only 52 be castable at any given time.]
     */
#define SPELL(name,desc,sub,prob,delay,level,mgc,dir,color,sn)  \
    OBJECT(OBJ(name, desc),                                             \
           BITS(0, 0, 0, 0, mgc, 0, 0, 0, 0, 0, dir, sub, PAPER),       \
           0, SPBOOK_CLASS, prob, delay, 50, level * 100,               \
           0, 0, 0, level, 20, color, sn)
/* Spellbook description normally refers to book covers (primarily color).
   Parchment and vellum would never be used for such, but rather than
   eliminate those, finagle their definitions to refer to the pages
   rather than the cover.  They are made from animal skin (typically of
   a goat or sheep) and books using them for pages generally need heavy
   covers with straps or clamps to tightly close the book in order to
   keep the pages flat.  (However, a wooden cover might itself be covered
   by a sheet of parchment, making this become less of an exception.  Also,
   changing the internal composition from paper to leather makes eating a
   parchment or vellum spellbook break vegetarian conduct, as it should.) */
#define PAPER LEATHER /* override enum for use in SPELL() expansion */
SPELL("挖掘","羊皮纸",
      P_MATTER_SPELL,      20,  6, 5, 1, RAY, HI_LEATHER, SPE_DIG),
MARKER(FIRST_SPELL, SPE_DIG)
/* magic missile ... finger of death must be in this order; see buzz() */
SPELL("魔法飞弹","牛皮纸",
      P_ATTACK_SPELL,      45,  2, 2, 1, RAY, HI_LEATHER, SPE_MAGIC_MISSILE),
#undef PAPER /* revert to normal material */
SPELL("火球术","粗糙",
      P_ATTACK_SPELL,      20,  4, 4, 1, RAY, HI_PAPER, SPE_FIREBALL),
SPELL("寒冰锥","卷角",
      P_ATTACK_SPELL,      10,  7, 4, 1, RAY, HI_PAPER, SPE_CONE_OF_COLD),
SPELL("睡眠","斑点",
      P_ENCHANTMENT_SPELL, 30,  1, 3, 1, RAY, HI_PAPER, SPE_SLEEP),
SPELL("死亡一指","污迹",
      P_ATTACK_SPELL,       5, 10, 7, 1, RAY, HI_PAPER, SPE_FINGER_OF_DEATH),
SPELL("光亮","布面",
      P_DIVINATION_SPELL,  45,  1, 1, 1, NODIR, HI_CLOTH, SPE_LIGHT),
SPELL("侦测怪物","皮质",
      P_DIVINATION_SPELL,  43,  1, 1, 1, NODIR, HI_LEATHER,
                                                        SPE_DETECT_MONSTERS),
SPELL("治疗","白色",
      P_HEALING_SPELL,     40,  2, 1, 1, IMMEDIATE, CLR_WHITE,
                                                        SPE_HEALING),
SPELL("开锁术","粉色",
      P_MATTER_SPELL,      25,  1, 1, 1, IMMEDIATE, CLR_BRIGHT_MAGENTA,
                                                        SPE_KNOCK),
SPELL("力场弹","红色",
      P_ATTACK_SPELL,      30,  2, 1, 1, IMMEDIATE, CLR_RED,
                                                        SPE_FORCE_BOLT),
SPELL("混乱怪物","橙色",
      P_ENCHANTMENT_SPELL, 49,  2, 1, 1, IMMEDIATE, CLR_ORANGE,
                                                        SPE_CONFUSE_MONSTER),
SPELL("治愈失明","黄色",
      P_HEALING_SPELL,     25,  2, 2, 1, IMMEDIATE, CLR_YELLOW,
                                                        SPE_CURE_BLINDNESS),
SPELL("吸取生命","天鹅绒",
      P_ATTACK_SPELL,      10,  2, 2, 1, IMMEDIATE, CLR_MAGENTA,
                                                        SPE_DRAIN_LIFE),
SPELL("减速怪物","浅绿色",
      P_ENCHANTMENT_SPELL, 30,  2, 2, 1, IMMEDIATE, CLR_BRIGHT_GREEN,
                                                        SPE_SLOW_MONSTER),
SPELL("法师锁","深绿色",
      P_MATTER_SPELL,      25,  3, 2, 1, IMMEDIATE, CLR_GREEN,
                                                        SPE_WIZARD_LOCK),
SPELL("创造怪物","青绿色",
      P_CLERIC_SPELL,      35,  3, 2, 1, NODIR, CLR_BRIGHT_CYAN,
                                                        SPE_CREATE_MONSTER),
SPELL("探测食物","青色",
      P_DIVINATION_SPELL,  30,  3, 2, 1, NODIR, CLR_CYAN,
                                                        SPE_DETECT_FOOD),
SPELL("恐惧术","浅蓝色",
      P_ENCHANTMENT_SPELL, 25,  3, 3, 1, NODIR, CLR_BRIGHT_BLUE,
                                                        SPE_CAUSE_FEAR),
SPELL("洞察术","深蓝色",
      P_DIVINATION_SPELL,  15,  3, 3, 1, NODIR, CLR_BLUE,
                                                        SPE_CLAIRVOYANCE),
SPELL("治愈疾病","靛蓝色",
      P_HEALING_SPELL,     32,  3, 3, 1, NODIR, CLR_BLUE,
                                                        SPE_CURE_SICKNESS),
SPELL("魅惑怪物","洋红色",
      P_ENCHANTMENT_SPELL, 20,  3, 5, 1, IMMEDIATE, CLR_MAGENTA,
                                                        SPE_CHARM_MONSTER),
SPELL("自我加速","紫色",
      P_ESCAPE_SPELL,      33,  4, 3, 1, NODIR, CLR_MAGENTA,
                                                        SPE_HASTE_SELF),
SPELL("侦测隐形","紫罗兰色",
      P_DIVINATION_SPELL,  20,  4, 3, 1, NODIR, CLR_MAGENTA,
                                                        SPE_DETECT_UNSEEN),
SPELL("浮空","棕褐色",
      P_ESCAPE_SPELL,      20,  4, 4, 1, NODIR, CLR_BROWN,
                                                        SPE_LEVITATION),
SPELL("强力治疗","格子纹",
      P_HEALING_SPELL,     27,  5, 3, 1, IMMEDIATE, CLR_GREEN,
                                                        SPE_EXTRA_HEALING),
SPELL("恢复能力","浅棕色",
      P_HEALING_SPELL,     25,  5, 4, 1, NODIR, CLR_BROWN,
                                                        SPE_RESTORE_ABILITY),
SPELL("隐身","深棕色",
      P_ESCAPE_SPELL,      20,  5, 4, 1, NODIR, CLR_BROWN,
                                                        SPE_INVISIBILITY),
SPELL("探测财宝","灰色",
      P_DIVINATION_SPELL,  20,  5, 4, 1, NODIR, CLR_GRAY,
                                                        SPE_DETECT_TREASURE),
SPELL("解除诅咒","皱褶",
      P_CLERIC_SPELL,      25,  5, 3, 1, NODIR, HI_PAPER,
                                                        SPE_REMOVE_CURSE),
SPELL("魔法地图","灰尘",
      P_DIVINATION_SPELL,  18,  7, 5, 1, NODIR, HI_PAPER,
                                                        SPE_MAGIC_MAPPING),
SPELL("鉴定","青铜色",
      P_DIVINATION_SPELL,  20,  6, 3, 1, NODIR, HI_COPPER,
                                                        SPE_IDENTIFY),
SPELL("驱散亡灵","铜色",
      P_CLERIC_SPELL,      16,  8, 6, 1, IMMEDIATE, HI_COPPER,
                                                        SPE_TURN_UNDEAD),
SPELL("变形","银色",
      P_MATTER_SPELL,      10,  8, 6, 1, IMMEDIATE, HI_SILVER,
                                                        SPE_POLYMORPH),
SPELL("传送远离","金色",
      P_ESCAPE_SPELL,      15,  6, 6, 1, IMMEDIATE, HI_GOLD,
                                                        SPE_TELEPORT_AWAY),
SPELL("创造魔宠","闪烁",
      P_CLERIC_SPELL,      10,  7, 6, 1, NODIR, CLR_WHITE,
                                                        SPE_CREATE_FAMILIAR),
SPELL("消除术","光亮",
      P_MATTER_SPELL,      15,  8, 7, 1, IMMEDIATE, CLR_WHITE,
                                                        SPE_CANCELLATION),
SPELL("防护","黯淡",
      P_CLERIC_SPELL,      18,  3, 1, 1, NODIR, HI_PAPER,
                                                        SPE_PROTECTION),
SPELL("跳跃术","薄",
      P_ESCAPE_SPELL,      20,  3, 1, 1, IMMEDIATE, HI_PAPER,
                                                        SPE_JUMPING),
SPELL("石化为肉","厚",
      P_HEALING_SPELL,     15,  1, 3, 1, IMMEDIATE, HI_PAPER,
                                                        SPE_STONE_TO_FLESH),
SPELL("连锁闪电","棋盘格",
      P_ATTACK_SPELL,      25,  4, 2, 1, NODIR, CLR_GRAY,
                                                        SPE_CHAIN_LIGHTNING),

#if 0 /* DEFERRED */
/* from slash'em, create a tame critter which explodes when attacking,
   damaging adjacent creatures--friend or foe--and dying in the process */
SPELL("火焰球","帆布",
      P_MATTER_SPELL,      20,  2, 1, 1, NODIR, CLR_BROWN,
                                                        SPE_FLAME_SPHERE),
SPELL("冰冻球","精装",
      P_MATTER_SPELL,      20,  2, 1, 1, NODIR, CLR_BROWN,
                                                        SPE_FREEZE_SPHERE),
#endif
/* books with fixed descriptions
 */
SPELL("空白纸","素色", P_NONE, 18, 0, 0, 0, 0, HI_PAPER,
                                                        SPE_BLANK_PAPER),
/* LAST_SPELL is used to calculate MAXSPELL, allocation size of spl_book[];
   by including blank paper, which has no actual spell, we ensure that
   even if hero learns every spell, spl_book[] will have at least one
   unused slot at end; an unused slot is needed for use as terminator */
MARKER(LAST_SPELL, SPE_BLANK_PAPER)
/* tribute book added in 3.6 */
OBJECT(OBJ("小说","平装"),
       BITS(0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, P_NONE, PAPER),
       0, SPBOOK_CLASS, 1, 0, 10, 20, 0, 0, 0, 1, 20, CLR_BRIGHT_BLUE,
                                                        SPE_NOVEL),
/* a special, one of a kind, spellbook */
OBJECT(OBJ("死者之书","莎草纸"),
       BITS(0, 0, 1, 0, 1, 0, 1, 1, 0, 0, 0, P_NONE, PAPER),
       0, SPBOOK_CLASS, 0, 0, 50, 10000, 0, 0, 0, 7, 20, HI_PAPER,
                                                        SPE_BOOK_OF_THE_DEAD),
#undef SPELL

/* wands ... */
#define WAND(name,typ,prob,cost,mgc,dir,metal,color,sn) \
    OBJECT(OBJ(name, typ),                                              \
           BITS(0, 0, 1, 0, mgc, 1, 0, 0, 0, 0, dir, P_NONE, metal),    \
           0, WAND_CLASS, prob, 0, 7, cost, 0, 0, 0, 0, 30, color, sn)
WAND("光亮","玻璃", 95, 100, 1, NODIR, GLASS, HI_GLASS,
                                                            WAN_LIGHT),
WAND("侦测暗门",
                        "轻木", 50, 150, 1, NODIR, WOOD, HI_WOOD,
                                                    WAN_SECRET_DOOR_DETECTION),
WAND("启迪","水晶", 15, 150, 1, NODIR, GLASS, HI_GLASS,
                                                    WAN_ENLIGHTENMENT),
WAND("创造怪物","枫木", 50, 200, 1, NODIR, WOOD, HI_WOOD,
                                                    WAN_CREATE_MONSTER),
WAND("许愿","松木",  5, 500, 1, NODIR, WOOD, HI_WOOD,
                                                    WAN_WISHING),
WAND("停滞","红木", 45, 150, 1, NODIR, WOOD, CLR_RED,
                                                    WAN_STASIS),
WAND("无效果","橡木", 25, 100, 0, IMMEDIATE, WOOD, HI_WOOD,
                                                    WAN_NOTHING),
WAND("击打","乌木", 30, 150, 1, IMMEDIATE, WOOD, HI_WOOD,
                                                    WAN_STRIKING),
WAND("隐形","大理石", 45, 150, 1, IMMEDIATE, MINERAL, HI_MINERAL,
                                                    WAN_MAKE_INVISIBLE),
WAND("减速怪物","锡", 50, 150, 1, IMMEDIATE, METAL, HI_METAL,
                                                    WAN_SLOW_MONSTER),
WAND("加速怪物","黄铜", 50, 150, 1, IMMEDIATE, COPPER, HI_COPPER,
                                                    WAN_SPEED_MONSTER),
WAND("驱散亡灵","铜", 50, 150, 1, IMMEDIATE, COPPER, HI_COPPER,
                                                    WAN_UNDEAD_TURNING),
WAND("变形","银", 45, 200, 1, IMMEDIATE, SILVER, HI_SILVER,
                                                    WAN_POLYMORPH),
WAND("消除术","铂金", 45, 200, 1, IMMEDIATE, PLATINUM, CLR_WHITE,
                                                    WAN_CANCELLATION),
WAND("传送","铱", 45, 200, 1, IMMEDIATE, METAL,
                                     CLR_BRIGHT_CYAN, WAN_TELEPORTATION),
WAND("开启","锌", 30, 150, 1, IMMEDIATE, METAL, HI_METAL,
                                                    WAN_OPENING),
WAND("锁定","铝", 30, 150, 1, IMMEDIATE, METAL, HI_METAL,
                                                    WAN_LOCKING),
WAND("探查","铀", 30, 150, 1, IMMEDIATE, METAL, HI_METAL,
                                                    WAN_PROBING),
WAND("挖掘","铁", 40, 150, 1, RAY, IRON, HI_METAL,
                                                    WAN_DIGGING),
/* magic missile ... lightning must be in this order; see buzz() */
WAND("魔法飞弹","钢", 50, 150, 1, RAY, IRON, HI_METAL,
                                                    WAN_MAGIC_MISSILE),
WAND("火焰","六角形", 40, 175, 1, RAY, IRON, HI_METAL,
                                                    WAN_FIRE),
WAND("寒冰","短", 40, 175, 1, RAY, IRON, HI_METAL,
                                                    WAN_COLD),
WAND("睡眠","符文", 50, 175, 1, RAY, IRON, HI_METAL,
                                                    WAN_SLEEP),
WAND("死亡","长",  5, 500, 1, RAY, IRON, HI_METAL,
                                                    WAN_DEATH),
WAND("闪电","弯曲", 40, 175, 1, RAY, IRON, HI_METAL,
                                                    WAN_LIGHTNING),
/* extra descriptions, shuffled into use at start of new game */
WAND(NoDes,             "分叉的",  0, 150, 1, 0, WOOD, HI_WOOD, WAN1),
WAND(NoDes,             "带刺的",  0, 150, 1, 0, IRON, HI_METAL, WAN2),
WAND(NoDes,            "镶宝石的",  0, 150, 1, 0, IRON, HI_MINERAL, WAN3),
#undef WAND

/* coins ... - so far, gold is all there is */
#define COIN(name,prob,metal,worth,sn) \
    OBJECT(OBJ(name, NoDes),                                         \
           BITS(1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, P_NONE, metal),    \
           0, COIN_CLASS, prob, 0, 1, worth, 0, 0, 0, 0, 0, HI_GOLD, sn)
COIN("金币", 1000, GOLD, 1, GOLD_PIECE),
#undef COIN

/* gems ... - includes stones and rocks but not boulders */
#define GEM(name,desc,prob,wt,gval,nutr,mohs,glass,color,sn) \
    OBJECT(OBJ(name, desc),                                             \
           BITS(0, 1, 0, 0, 0, 0, 0, 0, 0,                              \
                HARDGEM(mohs), 0, -P_SLING, glass),                     \
           0, GEM_CLASS, prob, 0, wt, gval, 3, 3, 0, 0, nutr, color, sn)
#define ROCK(name,desc,kn,prob,wt,gval,sdam,ldam,mgc,nutr,mohs,glass,colr,sn) \
    OBJECT(OBJ(name, desc),                                             \
           BITS(kn, 1, 0, 0, mgc, 0, 0, 0, 0,                           \
                HARDGEM(mohs), 0, -P_SLING, glass),                     \
           0, GEM_CLASS, prob, 0, wt, gval, sdam, ldam, 0, 0, nutr, colr, sn)
GEM("双锂水晶","白色",  2, 1, 4500, 15,  5, GEMSTONE, CLR_WHITE,
                                                        DILITHIUM_CRYSTAL),
MARKER(FIRST_REAL_GEM, DILITHIUM_CRYSTAL)
GEM("钻石","白色",  3, 1, 4000, 15, 10, GEMSTONE, CLR_WHITE,
                                                        DIAMOND),
GEM("红宝石","红色",  4, 1, 3500, 15,  9, GEMSTONE, CLR_RED,
                                                        RUBY),
GEM("红锆石","橙色",  3, 1, 3250, 15,  9, GEMSTONE, CLR_ORANGE,
                                                        JACINTH),
GEM("蓝宝石","蓝色",  4, 1, 3000, 15,  9, GEMSTONE, CLR_BLUE,
                                                        SAPPHIRE),
GEM("黑蛋白石","黑色",  3, 1, 2500, 15,  8, GEMSTONE, CLR_BLACK,
                                                        BLACK_OPAL),
GEM("绿宝石","绿色",  5, 1, 2500, 15,  8, GEMSTONE, CLR_GREEN,
                                                        EMERALD),
GEM("绿松石","绿色",  6, 1, 2000, 15,  6, GEMSTONE, CLR_GREEN,
                                                        TURQUOISE),
GEM("黄水晶","黄色",  4, 1, 1500, 15,  6, GEMSTONE, CLR_YELLOW,
                                                        CITRINE),
GEM("海蓝宝石","绿色",  6, 1, 1500, 15,  8, GEMSTONE, CLR_GREEN,
                                                        AQUAMARINE),
GEM("琥珀","黄棕色",  8, 1, 1000, 15,  2, GEMSTONE, CLR_BROWN,
                                                        AMBER),
GEM("黄玉","黄棕色", 10, 1,  900, 15,  8, GEMSTONE, CLR_BROWN,
                                                        TOPAZ),
GEM("黑玉","黑色",  6, 1,  850, 15,  7, GEMSTONE, CLR_BLACK,
                                                        JET),
GEM("蛋白石","白色", 12, 1,  800, 15,  6, GEMSTONE, CLR_WHITE,
                                                        OPAL),
GEM("金绿宝石","黄色",  8, 1,  700, 15,  5, GEMSTONE, CLR_YELLOW,
                                                        CHRYSOBERYL),
GEM("石榴石","红色", 12, 1,  700, 15,  7, GEMSTONE, CLR_RED,
                                                        GARNET),
GEM("紫水晶","紫罗兰色", 14, 1,  600, 15,  7, GEMSTONE, CLR_MAGENTA,
                                                        AMETHYST),
GEM("碧玉","红色", 15, 1,  500, 15,  7, GEMSTONE, CLR_RED,
                                                        JASPER),
GEM("萤石","紫罗兰色", 15, 1,  400, 15,  4, GEMSTONE, CLR_MAGENTA,
                                                        FLUORITE),
GEM("黑曜石","黑色",  9, 1,  200, 15,  6, GEMSTONE, CLR_BLACK,
                                                        OBSIDIAN),
GEM("玛瑙","橙色", 12, 1,  200, 15,  6, GEMSTONE, CLR_ORANGE,
                                                        AGATE),
GEM("翡翠","绿色", 10, 1,  300, 15,  6, GEMSTONE, CLR_GREEN,
                                                        JADE),
MARKER(LAST_REAL_GEM, JADE)
GEM("无价值的白色玻璃片","白色",
    77, 1, 0, 6, 5, GLASS, CLR_WHITE, WORTHLESS_WHITE_GLASS),
MARKER(FIRST_GLASS_GEM, WORTHLESS_WHITE_GLASS)
GEM("无价值的蓝色玻璃片","蓝色",
    77, 1, 0, 6, 5, GLASS, CLR_BLUE, WORTHLESS_BLUE_GLASS),
GEM("无价值的红色玻璃片","红色",
    77, 1, 0, 6, 5, GLASS, CLR_RED, WORTHLESS_RED_GLASS),
GEM("无价值的黄棕色玻璃片","黄棕色",
    77, 1, 0, 6, 5, GLASS, CLR_BROWN, WORTHLESS_YELLOWBROWN_GLASS),
GEM("无价值的橙色玻璃片","橙色",
    76, 1, 0, 6, 5, GLASS, CLR_ORANGE, WORTHLESS_ORANGE_GLASS),
GEM("无价值的黄色玻璃片","黄色",
    77, 1, 0, 6, 5, GLASS, CLR_YELLOW, WORTHLESS_YELLOW_GLASS),
GEM("无价值的黑色玻璃片","黑色",
    76, 1, 0, 6, 5, GLASS, CLR_BLACK, WORTHLESS_BLACK_GLASS),
GEM("无价值的绿色玻璃片","绿色",
    77, 1, 0, 6, 5, GLASS, CLR_GREEN, WORTHLESS_GREEN_GLASS),
GEM("无价值的紫色玻璃片","紫罗兰色",
    77, 1, 0, 6, 5, GLASS, CLR_MAGENTA, WORTHLESS_VIOLET_GLASS),
MARKER(LAST_GLASS_GEM, WORTHLESS_VIOLET_GLASS)

/* Placement note: there is a wishable subrange for
 * "gray stones" in the o_ranges[] array in objnam.c
 * that is currently everything between luckstones and flint
 * (inclusive).
 */
ROCK("幸运石","灰色",  0,  10,  10, 60, 3, 3, 1, 10, 7, MINERAL, CLR_GRAY,
                                                                    LUCKSTONE),
ROCK("重石","灰色",  0,  10, 500,  1, 3, 3, 1, 10, 6, MINERAL, CLR_GRAY,
                                                                    LOADSTONE),
ROCK("试金石","灰色", 0,   8,  10, 45, 3, 3, 1, 10, 6, MINERAL, CLR_GRAY,
                                                                  TOUCHSTONE),
ROCK("燧石","灰色",      0,  10,  10,  1, 6, 6, 0, 10, 7, MINERAL, CLR_GRAY,
                                                                    FLINT),
ROCK("石头",NoDes,         1, 100,  10,  0, 3, 3, 0, 10, 7, MINERAL, CLR_GRAY,
                                                                    ROCK),
#undef GEM
#undef ROCK

/* miscellaneous ... */
/* Note: boulders and rocks are not normally created at random; the
 * probabilities only come into effect when you try to polymorph them.
 * Boulders weigh more than MAX_CARR_CAP; statues use corpsenm to take
 * on a specific type and may act as containers (both affect weight).
 */
OBJECT(OBJ("巨石",NoDes),
       BITS(1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, P_NONE, MINERAL), 0,
       ROCK_CLASS, 100, 0, 6000, 0, 20, 20, 0, 0, 2000, HI_MINERAL, BOULDER),
OBJECT(OBJ("雕像",NoDes),
       BITS(1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, P_NONE, MINERAL), 0,
       ROCK_CLASS, 900, 0, 2500, 0, 20, 20, 0, 0, 2500, CLR_WHITE, STATUE),

OBJECT(OBJ("重铁球",NoDes),
       BITS(1, 0, 0, 0, 0, 0, 0, 0, 0, 0, WHACK, P_NONE, IRON), 0,
       BALL_CLASS, 1000, 0, 480, 10, 25, 25, 0, 0, 200, HI_METAL,
                                                            HEAVY_IRON_BALL),
        /* +d4 when "very heavy" */
OBJECT(OBJ("铁链",NoDes),
       BITS(1, 0, 0, 0, 0, 0, 0, 0, 0, 0, WHACK, P_NONE, IRON), 0,
       CHAIN_CLASS, 1000, 0, 120, 0, 4, 4, 0, 0, 200, HI_METAL, IRON_CHAIN),
        /* +1 both l & s */

/* Venom is normally a transitory missile (spit by various creatures)
 * but can be wished for in wizard mode so could occur in bones data.
 */
OBJECT(OBJ("致盲毒液","毒液"),
       BITS(0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, P_NONE, LIQUID), 0,
       VENOM_CLASS, 500, 0, 1, 0, 0, 0, 0, 0, 0, HI_ORGANIC, BLINDING_VENOM),
OBJECT(OBJ("酸性毒液","毒液"),
       BITS(0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, P_NONE, LIQUID), 0,
       VENOM_CLASS, 500, 0, 1, 0, 6, 6, 0, 0, 0, HI_ORGANIC, ACID_VENOM),
        /* +d6 small or large */

#if defined(OBJECTS_DESCR_INIT) || defined(OBJECTS_INIT)
/* fencepost, the deadly Array Terminator -- name [1st arg] *must* be NULL */
OBJECT(OBJ(NoDes, NoDes),
       BITS(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, P_NONE, 0), 0,
       ILLOBJ_CLASS, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
#undef BITS
#endif

#undef OBJ
#undef OBJECT
#undef MARKER
#undef HARDGEM
#undef NoDes

/*objects.c*/
