/* NetHack 5.0  artilist_zh.h
 * Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985.
 * NetHack may be freely redistributed.  See license for details.
 *
 * Chinese display names for the artifacts defined in artilist.h.
 *
 * This file is only included by src/artifact.c for a ZHLANG build, inside
 * the initializer of the artifact_names_zh[] array.  The entries are
 * line-aligned with the A() entries in artilist.h: the leading empty
 * string is the dummy element #0, then come the artifacts in the same
 * order as artilist.h, and the final 0 terminates the array.
 *
 * The names are for display only.  Everything that looks an artifact up
 * (artifact_name(), exist_artifact(), save files, bones) keeps using the
 * English names from artilist.h; artifact_name_zh() translates a Chinese
 * name back to the canonical English one for wishes.
 *
 * Chinese article wording: the English names that start with "The " are
 * stored without any equivalent, because Chinese does not need one.
 */

#ifdef ARTI_ZH_NAMES
    "",                             /* dummy element #0 */
    "埃克斯卡利伯",                 /* Excalibur */
    "风暴使者",                     /* Stormbringer */
    "妙尔尼尔",                     /* Mjollnir */
    "劈砍者",                       /* Cleaver */
    "狰狞之牙",                     /* Grimtooth */
    "兽咬剑",                       /* Orcrist */
    "刺针",                         /* Sting */
    "破魔之刃",                     /* Magicbane */
    "霜刃",                         /* Frost Brand */
    "火刃",                         /* Fire Brand */
    "屠龙者",                       /* Dragonbane */
    "屠魔者",                       /* Demonbane */
    "屠狼者",                       /* Werebane */
    "格雷斯万迪尔",                 /* Grayswandir */
    "巨人杀手",                     /* Giantslayer */
    "食人魔粉碎者",                 /* Ogresmasher */
    "巨魔克星",                     /* Trollsbane */
    "斩首之刃",                     /* Vorpal Blade */
    "窃笑刀",                       /* Snickersnee */
    "太阳剑",                       /* Sunsword */
    "侦测宝珠",                     /* The Orb of Detection */
    "阿里曼之心",                   /* The Heart of Ahriman */
    "力量权杖",                     /* The Sceptre of Might */
#if 0 /* OBSOLETE -- mirrors the #if 0 that excludes this entry in
         artilist.h (former Elf quest artifact) */
    "西方之地的帕兰提尔",           /* The Palantir of Westernesse */
#endif
    "阿斯克勒庇俄斯之杖",           /* The Staff of Aesculapius */
    "梅林的魔镜",                   /* The Magic Mirror of Merlin */
    "异界之眼",                     /* The Eyes of the Overworld */
    "圣洁法冠",                     /* The Mitre of Holiness */
    "狄安娜长弓",                   /* The Longbow of Diana */
    "盗贼大师之钥",                 /* The Master Key of Thievery */
    "村正妖刀",                     /* The Tsurugi of Muramasa */
    "伊尔多白金特快卡",             /* The Platinum Yendorian Express Card */
    "命运宝珠",                     /* The Orb of Fate */
    "埃塞俄比亚之眼",               /* The Eye of the Aethiopica */
    0                               /* terminator */
#endif /* ARTI_ZH_NAMES */
