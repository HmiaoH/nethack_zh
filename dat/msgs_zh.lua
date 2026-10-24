-- NetHack 5.0  msgs_zh.lua
-- NetHack may be freely redistributed.  See license for details.
--
-- English -> Chinese message table for the ZHLANG build.
--
-- The Lua level and tutorial scripts (dat/*.lua) are shared between the
-- English and the Chinese build, so their player-visible strings must not
-- be translated in place.  Instead this table maps the English original
-- to its Chinese translation; the ZHLANG run-time hook looks the string up
-- before it is displayed and falls back to the original English when a
-- string is not present here.
--
-- Entries whose key is a fragment of a composed string (for example
-- "Use '", which the scripts concatenate with a key name) are matched as
-- the longest prefix of the composed text; everything after the matched
-- prefix is kept as is.
--
-- The file is a plain Lua chunk whose only return value is the table.

return {
    ["X marks the spot."] = "X 标记此处。",
    ["Elbereth"] = "Elbereth",
    ["What a strange feeling!"] = "多么奇怪的感觉！",
    ["You notice that there is no gravity here."] = "你注意到这里没有重力。",
    ["You arrive on the Astral Plane!"] = "你来到了星界！",
    ["Here the High Temple of %d is located."] = "这里坐落着 %d 的高等神殿。",
    ["You sense alarm, hostility, and excitement in the air!"] = "你感觉到空气中弥漫着警觉、敌意与兴奋！",
    ["Well done, mortal!"] = "干得好，凡人！",
    ["But now thou must face the final Test..."] = "但现在你必须面对最终的考验……",
    ["Prove thyself worthy or perish!"] = "证明你的价值，否则就灭亡吧！",
    ["Failed to open dgl extrainfo file: "] = "无法打开 dgl 额外信息文件：",
    ["Looks like you're getting hungry.  You'll starve to death, unless you eat something."] = "看来你开始饿了。除非吃点东西，否则你会饿死。",
    ["Comestibles are eaten with '"] = "食物用 '",
    ["Warning: themeroom '"] = "警告：主题房间 '",
    ["Warning: themeroom fill '"] = "警告：主题房间填充 '",
    ["Warning: fill '"] = "警告：填充 '",
    ["Dig"] = "挖",
    ["Note: Outside the tutorial, Ctrl-key combinations are shown prefixed with a caret, like '^"] = "注意：教程之外，Ctrl 组合键会以插入符号为前缀显示，例如 '^",
    ["Move around with "] = "移动用 ",
    ["Move diagonally with "] = "斜向移动用 ",
    ["Knights can jump with '"] = "骑士可以用 '",
    ["Some actions may require multiple tries before succeeding"] = "有些动作可能需要多次尝试才能成功",
    ["Open the door by moving into it"] = "朝门的方向移动即可开门",
    ["Close the door with '"] = "关门用 '",
    ["You can leave the tutorial via the magic portal."] = "你可以通过魔法传送门离开教程。",
    ["This door is locked. Kick it with '"] = "这扇门锁住了。踢开它用 '",
    ["Look around the map with '"] = "查看地图用 '",
    ["Use '"] = "使用 '",
    ["Wrong secret"] = "暗号错误",
    ["Behind this door is a dark corridor"] = "这扇门后是一条黑暗的走廊",
    ["There are four traps next to you! Search for them."] = "你旁边有四个陷阱！搜索它们。",
    ["Some traps can be disabled with '"] = "有些陷阱可以用 '",
    ["Pick up items with '"] = "拾取物品用 '",
    ["Wear armor with '"] = "穿护甲用 '",
    ["Wield weapons with '"] = "挥舞武器用 '",
    ["Hit monsters by walking into them."] = "走向怪物即可攻击它们。",
    ["Now you know the very basics. You can leave the tutorial via the magic portal."] = "现在你已掌握最基本的知识。你可以通过魔法传送门离开教程。",
    ["Step into this portal to leave the tutorial"] = "踏入这个传送门即可离开教程",
    ["Push boulders by moving into them"] = "朝巨石移动即可推动它们",
    ["Take off armor with '"] = "脱下护甲用 '",
    ["Some items have shuffled descriptions, different each game"] = "有些物品的描述每局都会打乱",
    ["Pick up this scroll, read it with '"] = "拾起这卷卷轴，阅读用 '",
    ["Another magic portal, a way to leave this tutorial"] = "另一个魔法传送门，离开本教程的途径",
    ["Avoid being burdened, it slows you down"] = "避免负重，那会拖慢你的速度",
    ["Drop items with '"] = "丢弃物品用 '",
    ["You can drop partial stacks by prefixing the item slot letter with a number"] = "在物品栏字母前加数字可以丢下部分堆叠",
    ["Throw items with '"] = "投掷物品用 '",
    ["Missiles, such as rocks, work better when fired from appropriate launcher"] = "石块等投掷物用合适的发射器射出效果更好",
    ["Wield the sling"] = "挥舞投石索",
    ["Firing launches items from your quiver; Use '"] = "发射会把箭袋中的物品射出去；用 '",
    ["You can wait a turn with '"] = "你可以用 '",
    ["You loot containers with '"] = "搜刮容器用 '",
    ["Containers can also be emptied with '"] = "也可以用 '",
    ["Magic wands are used with '"] = "使用魔杖用 '",
    ["You can run by prefixing a movement key with '"] = "在移动键前加 '",
    ["Travel across the level with '"] = "跨越关卡用 '",
    ["Swap weapons quickly with '"] = "快速切换武器用 '",
    ["Put on accessories with '"] = "佩戴饰品用 '",
    ["Remove accessories with '"] = "摘下饰品用 '",
    ["UNDER CONSTRUCTION"] = "施工中",
    ["Can't get through?  You're carrying too much."] = "过不去？你带的东西太多了。",
    ["Spellcasting"] = "施法",
    ["Unfortunately you don't have enough energy to cast spells."] = "很遗憾，你的能量不足以施放法术。",
    ["Pick up the spellbook with '"] = "拾起法术书用 '",
    ["Read the spellbook with '"] = "阅读法术书用 '",
    ["You \"quaff\" potions with '"] = "你「喝下」药水用 '",
    ["You find yourself suspended in an air bubble surrounded by water."] = "你发现自己悬在一个被水包围的气泡中。",
}
