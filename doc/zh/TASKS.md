# NetHack 5.0 汉化：修复与补齐任务书

> 读者：执行修复的 AI / 开发者。
> 配套文件：[ACCEPTANCE.md](ACCEPTANCE.md)（验收规范）、`test/zh/`（验收工具）。
> 基线：提交 `b3072466c`。上游最后一个未汉化提交：`c2a332168`。

---

## 0. 开工前必读

### 0.1 验收方式

每个任务都对应一个或多个**检查项 ID**（如 `A1-no-format-warnings`）。验收工具会自动跑这些检查：

```bash
python3 test/zh/zh_accept.py --interim            # 阶段性自检
python3 test/zh/zh_accept.py                      # 最终验收（门禁 E 用最终阈值）
python3 test/zh/zh_accept.py --require A,B        # 只要求部分门禁
```

这条命令会：
1. 把工作区复制到临时目录，分别编译中文版和英文版；
2. 以巫师模式启动游戏，跑 `test/zh/zh_accept.lua` 和上游的 `test/testwish.lua`；
3. 检查终端原始输出、跑静态检查，最后输出一张报告表。

一轮约 3–5 分钟。只改了脚本或静态内容时，可以加 `--no-build` 复用上次的编译结果。

**交付前，你负责的门禁必须在本地全绿。**

### 0.2 红线（违反任何一条，验收直接不通过）

1. **不得修改验收工具让它变绿。** `test/zh/`、`doc/zh/ACCEPTANCE.md` 由验收方维护。如果你认为某条检查本身有误，在交付说明里写明理由，由验收方决定。
2. **英文版行为不变。** 所有改动要么放在 `ZHLANG` 条件下，要么对语言中立。英文版要能编译通过，`test/testwish.lua` 要通过，而且不能新增警告。
3. **内部标识符一律保持英文。** 包括：Lua 脚本里的物品名和怪物名、`sp_lev` 的查找、配置项取值（`role:Valkyrie`）、地牢名（`dname_to_dnum("The Gnomish Mines")`）、存档和 bones 文件。中文**只用于显示**。
4. **不得新增"把 `#ifdef` 写在宏参数里"的写法**（clang 的 `-Wembedded-directive` 警告，属于未定义行为）。
5. **格式串的参数个数和类型必须与英文原句完全一致。** 中文语序不同时，调整的是格式串里文字的位置，不能删参数，也不能多留参数。

### 0.3 提交规范

- 一次提交只做一个任务 ID。标题格式：`zh: <ID> <简述>`，标题 ≤50 字符，正文每行 ≤72 字符（遵循仓库 `Contributing.md`）。
- 提交正文写明：改了什么、对应哪些检查项、本地 `zh_accept.py` 的结果。

### 0.4 推荐顺序

```
A（崩溃与构建）→ B（语法层）→ C（名称查找）→ D（TTY 显示）
                                         ↘ E（剩余字符串，可分批并行）
                                         ↘ F（数据文件，可与 E 并行）
```

E 的前提是 E0（`TR` 宏）先落地。F 和 E 改动的文件不重叠，可以并行。

---

## 门禁 A：崩溃、构建、回归

### A1 修复格式串错误　`A1-no-format-warnings` `A1-no-new-warnings` `A1-en-build-clean`

**现状**（`b3072466c` 下实测编译警告）：

| 位置 | 问题 | 后果 |
|---|---|---|
| `src/dothrow.c:637-640` | 参数顺序错位：`%d` 收到字符串、`%s` 收到整数 | 多发射击被打断时**段错误** |
| `src/mkobj.c:1756` | `"%s%s%s"` 只给 2 个参数 | 光源亮度变化时未定义行为 |
| `src/end.c:1800` | `%ld` 收到 `plur()` 返回的字符串，后面还有多余参数 | 结算界面回合数显示成垃圾值 |
| `src/music.c:1113,1124,1132` | `%d` 收到 `plur()`，有多余参数；`You_hear("听到…")` 里"听到"重复 | 数字显示错乱 |
| `src/polyself.c:2254` | `"% s"` | 未定义行为 |
| `src/end.c:1699,1743,1791` | 多余参数 | 信息丢失 |
| `src/dbridge.c:470`、`src/dogmove.c:388,1365`、`src/dokick.c:748,765`、`src/minion.c:347`、`src/mthrowu.c:944`、`src/pickup.c:3410`、`src/wield.c:1309` | 多余参数（怪物名、代词等被丢掉） | 信息丢失 |
| `src/do_name.c:284,356`、`src/invent.c:1944,1946,2623` | `#ifdef` 写在宏参数里 | 未定义行为 |

**要求：**
- 以上全部修掉。参数被丢掉的，要把对应信息补回中文句子里，**不要用删参数的办法消除警告**。
- 汉化过程中只在中文版出现的 unused 变量警告也一并清掉（`ball.c:925 pullmsg`、`do_wear.c:8,10`、`end.c:63 ends`、`iactions.c:356`、`invent.c:1766,4297,4299`、`lock.c:876`、`mhitm.c:1285,1487`、`mhitu.c:127`、`mthrowu.c:1050`、`wield.c:81,82`、`wizcmds.c:1073`）。修完 A5 之后，验收工具会拿中文版的警告和真正的英文版对比，多出来的都算失败。

### A2 内部按名字查找必须用英文表　`A2-appear-as-lookup` `A2-appear-as-run`

**现状：**
- `src/sp_lev.c:2032` 用 `OBJ_NAME()`（中文）去匹配 Lua 里的 `appear_as="obj:luckstone"`。
- 实测在巫师模式里会报 `create_monster: can't find object "luckstone"`，矿坑底层 `minend-1.lua` 必定触发。

**要求：**
- 改用 `obj_descr[objects[i].oc_name_idx].oc_name`（英文）。
- **逐一审计**全部 `OBJ_NAME` / `OBJ_DESCR` / `pmname` / `mons_zh` 的使用，按"显示用"和"标识用"分类。标识用的一律改回英文表。
- 已知需要核实的位置：`src/options.c:8344`（水果名）、`src/write.c:176,189`（写卷轴）、`src/objnam.c:2721`、`src/spell.c:2186`（法术排序用中文排序也可以，但要说明）、`src/nhlobj.c`、`src/nhlua.c` 的 `nhl_int_to_obj_name`（这两个见 C1）。
- 在交付说明里附上审计清单（文件:行 → 分类 → 处理方式）。

### A3 Windows 打包中文数据文件　`A3-GNUmakefile` `A3-Makefile.nmake` `A3-files.props`

**现状：** `help_zh`、`hh_zh`、`cmdhelp_zh`、`keyhelp_zh`、`history_zh`、`opthelp_zh`、`optmenu_zh`、`usagehlp_zh`、`wizhelp_zh` 都没有加进 DLB 列表，所以 Windows 上帮助菜单打不开。

**要求：** 补进 `sys/windows/GNUmakefile`（第 1020 行起的 `$(DLBLST)` 规则）、`sys/windows/Makefile.nmake`、`sys/windows/vs/files.props`。F4 新增的数据文件也要同步加进去。

### A4 恢复 `wintty.c` 的字形输出逻辑　`A4-wintty-glyph-path`

**现状：** `win/tty/wintty.c` 的 `process_text_window` 删掉了上游在行首处理 `g_putch` / `end_glyphout()` 的分支，而且没有加 `ZHLANG` 条件，英文版也受影响（DECgraphics / IBM 符号集）。

**要求：** 恢复上游逻辑。中文 UTF-8 文本需要放行的话，只对"本身就是合法 UTF-8 多字节序列"的情况放行，不要整段删除。

### A5 英文版可以真正编出来　`A5-en-build-is-english`

**现状：** `sys/unix/hints/include/compiler.500` 里硬编码了 `CFLAGS+=-DZHLANG`，所以 README 说的"从 hints 里去掉 chinese 就是英文版"做不到。`chinese.500` 里的 `ZHLANG` 变量也没有任何地方使用。

**要求：**
- 删掉 `compiler.500` 里的那一行，只由 `chinese.500` 负责加 `-DZHLANG`。
- 清理 `chinese.500` 里没用的变量。
- 确认去掉 `chinese` 后，编译日志里不再出现 `-DZHLANG`。

### A6 运行时没有 impossible()　`A6-no-impossible`

验收流程中，终端输出不能出现任何"程序出错了"或 `Program in disorder`。完成 A2 和 C1 后应该自然满足。

---

## 门禁 B：英文语法层中文化

### B1 语法辅助函数　`B1-plural` `B1-singular` `B1-possessive` `B1-article` `B1-en-*`

**现状（实测）：** `makeplural("箭")` 返回 `"箭's"`，`s_suffix("长剑")` 返回 `"长剑's"`。`the()` 会给中文名加 `"the "`，`plur()` 返回 `"s"`。

**要求：** 在 `ZHLANG` 下，当参数含非 ASCII 字符时：

| 函数 | 位置 | 期望行为 |
|---|---|---|
| `makeplural`、`makesingular` | `src/objnam.c` | 原样返回 |
| `s_suffix` | `src/hacklib.c` | 末尾加"的" |
| `the()`、`The()` | `src/objnam.c` | 不加冠词 |
| `just_an` | `src/objnam.c` | 已处理，保持 |
| `plur()` | 宏，`include/hack.h` 一带 | 中文版返回空串（它只用于拼英文复数，中文语境不需要） |

英文参数的行为必须和原来完全一样（`B1-en-*` 检查）。

### B2 输出里不出现中英语法拼接　`B2-no-mixed-grammar`

验收工具会扫描整个游戏会话的终端输出。只要出现"汉字后跟 `'s` / `s` / `es`"或"`the` / `a` / `an` 后跟汉字"，就判为失败。

**实测已发现：**
- 物品栏里的 `2 未诅咒的 鉴定卷轴's`（B1 修好后应消失）；
- 状态栏的 `Wizard the 见习`（`src/botl.c` 用 `"%s the %s"` 拼头衔；中文建议写成"见习 Wizard"或"见习者 Wizard"）；
- 同样在状态栏：阵营显示为英文 `Neutral`，也要翻译。

另外清理这几处：
- `src/objnam.c` 的"（副手武器%s；未挥舞）"去掉 `plur`；
- `src/do_name.c` 的 `do_oname`：提示语中英混杂（"你想命名" + `these/this`）；
- 所有中文分支里调用的 `vtense()` / `otense()` / `makeplural()`（它们返回的是英文动词变形）。

### B3 任务对话的变量替换　`B3-questpgr`

**现状：** `src/questpgr.c` 的 `convert_arg` 和修饰符处理完全没有改。`quest_zh.lua` 里会混进 `sister`、`see`、`chaotic`、`The Dungeons of Doom`、`he/his`、`'s`、`a/an` 等英文。

**要求（`ZHLANG` 下）：**

| 变量 | 中文输出 |
|---|---|
| `%s` | 姐妹 / 兄弟 |
| `%S` | 女儿 / 儿子 |
| `%x` | 看见 / 感知到 |
| `%C` / `%N` / `%L` | 混沌 / 中立 / 守序 |
| `%Z` | 走 F1 的地牢名显示映射 |
| `%o` / `%O` | 走 F5 的神器中文名；`%O` 的"截断 of"逻辑改成中文规则 |

修饰符（见 `convert_line` / `qtext_pronoun`）：
- `h/H`、`i/I`（主格、宾格代词）→ 他/她/它（神器的复数代词也用"它们"）；
- `j/J`（所有格代词）→ 他的/她的/它的；
- `s/S`（所有格后缀）→ 加"的"（B1 修好 `s_suffix` 后自动满足）；
- `A/a`（冠词）→ 空（`just_an` 已处理）；
- `p/P`（复数）→ 原样（B1 修好 `makeplural` 后自动满足）；
- `C`（首字母大写）、`t`（去掉 "the "）→ 对中文无操作。

**附带：** `quest_zh.lua` 的 `%l` / `%o` / `%n` 个数和原文不一致（原文 159/256/274，译文 158/255/275），请逐条核对是漏了还是故意的。

### B4 `You()` 系列的主语统一

**现状：** `src/pline.c` 把 `You()` 等函数的前缀改成了空串。结果有的译文自带"你"，有的不带，至少 38 处出现"你你"或缺主语。

**要求：** 二选一：
- **推荐：** `You()` 的前缀统一为"你"，再用脚本去掉译文开头重复的"你"；
- 保持前缀为空，但全部译文统一自带"你"。

在交付说明里写明采用哪种。验收方会抽查。

---

## 门禁 C：名称查找（许愿、配置、Lua 接口）

### C1 Lua 接口返回英文标识符　`C1-otyp-name` `C1-int-to-objname`

**现状：** `src/nhlobj.c:266,268` 的 `otyp_name` / `otyp_descr` 和 `src/nhlua.c:1213` 的 `nh.int_to_objname` 返回的是中文，导致上游 Lua 脚本和测试全部失效。

**要求：** 改为直接读英文表 `obj_descr[...]`。如果需要，可以另加 `otyp_name_zh` 字段。

### C2 中文版里英文许愿照常工作　`C2-en-wish` `C2-en-wish-corpse` `C2-zh-build-testwish` `C2-en-build-testwish`

**要求：** 中文版和英文版都要通过上游的 `test/testwish.lua`（约 120 条英文许愿，原封不动）。

### C3 中文许愿　`C3-zh-wish*`

**要求：** `readobjnam`（`src/objnam.c`）支持中文输入。最低要求：

| 输入 | 期望结果 |
|---|---|
| `银匕首` | `silver dagger` |
| `2 祝福的 +3 银匕首` / `2祝福的+3银匕首` | 数量 2、祝福、+3。空格可有可无 |
| `诅咒的灰龙鳞甲` | 诅咒 |
| `豺狼尸体` / `豺狼的尸体` | 豺狼的尸体（`name_to_mon` 要能查 `mons_zh`） |

**实现建议：**
- 先把中文前缀（祝福的 / 诅咒的 / 未诅咒的 / 被腐蚀的 / 涂油的 / 已上锁的 / 空的 …）规范化成英文 token；
- 再把中文物品名和怪物名反查成英文名；
- 最后交给原有的英文解析流程。

这样英文路径不需要改动。灭绝（`do_genocide`）和变形的提示同样应该能接受中文怪物名，验收方会手动抽查。

### C4 配置项接受英文值　`C4-role-option`

**现状（实测）：** `NETHACKOPTIONS=role:Valkyrie` 会报 `Unknown role 'Valkyrie'`，因为职业名被翻译了，选项解析拿中文去比较。玩家现有的 `.nethackrc` 会全部失效。

**要求：** `role` / `race` / `gender` / `align` 的解析同时接受英文（必须）和中文（可选）。

另外，角色选择流程里的英文提示（`Shall I pick ...`、`Is this ok?`）和 `你是女人法师` 这种不通顺的说法，也在本任务里一起处理。

### C5 中文名不能撞名　`C5-unique-zh-names`

**现状：** `yumi` 和 `bow` 都译成了"弓"。

**要求：** 把 yumi 改成"和弓"（或"日式弓"）。两个不同的英文物品名不能共用同一个中文名；戒指/魔杖与魔法书同名的情况（睡眠、挖掘、驱散亡灵）除外，英文里本来就同名。

### C6 百科查询（手动验收）

**现状：** `/` 和 `;` 用中文名去查 `data.base`，永远查不到词条。

**要求：** `src/pager.c` 的 `checkfile` 先把中文名反查成英文键，再去查。

---

## 门禁 D：TTY 中文显示

### D1 换行不能截断汉字　`D1-utf8-stream`

**现状（实测）：** 一条较长的中文消息在 `win/tty/topl.c` 的 `update_topl` 里按字节数和空格断行，中文没有空格，就在多字节字符中间被切开，终端输出非法 UTF-8。

**要求：** 只在 UTF-8 字符边界上断行。中文没有空格可断时，按显示宽度硬断。

### D2 按显示宽度计算列（手动验收）

**要求：** 加一个 `u8_width()` 函数（ASCII 算 1 列，CJK 和全角字符算 2 列），用在：
- `topl.c` 的"能否续在同一行"判断（`n0 + strlen(toplines) + 3 < CO - 8`）；
- `curx` 的累加（`topl.c`、`wintty.c:1169` 等）；
- 菜单列对齐、状态栏截断、`nhl_text`（`src/nhlua.c:841` 按 76 字节切行）。

验收方会在 80×24 终端里截图检查物品栏、状态栏和长消息。

### D3 死亡消息判断

**现状：** `topl.c` 用 `strncmp(bp, "You die", 7)` 判断死亡消息。死亡消息翻译之后（见 E 第 1 批），这个判断会失效，死亡提示可能被跳过。

**要求：** 改成在 `done()` 流程里设置一个标志位，不再比较字符串。

---

## 门禁 E：补齐 C 源码中的剩余字符串

### E0 引入 `TR` 宏　`E0-TR-macro`

在 `include/global.h`（或 `hack.h`）里加：

```c
#ifdef ZHLANG
#define TR(en, zh) zh
#else
#define TR(en, zh) en
#endif
```

**规则：**
- 中英两句**格式参数的个数、顺序、类型完全相同**时，必须用 `TR`：`pline(TR("You die...", "你死了……"));`
- 参数顺序必须不同时，才保留 `#ifdef ZHLANG / #else / #endif`，而且整条语句写在分支里，不能拆进宏参数。
- **可选（推荐）：** 写脚本把现有约 3771 个简单 `#ifdef` 块批量转成 `TR`，能减少约一半代码量，以后合并上游也更省事。

### E1 / E2 数量阈值　`E1-untranslated-total` `E2-untranslated-per-file`

扫描工具：`python3 test/zh/scan_untranslated.py`。加 `--list hits.txt` 会输出逐条清单。

| | 数值 |
|---|---|
| 基线（`b3072466c`） | **2798 处** |
| 最终要求：总数 | ≤ **279**（基线的 10%） |
| 最终要求：单个文件 | ≤ **15** 处 |

只在巫师模式或调试路径出现的字符串，可以在同一行加注释 `/* NOTRANS */` 豁免。验收方会审查全部 `NOTRANS`，玩家正常游戏能看到的字符串不得豁免。

**分批顺序**（按玩家见到的频率排）：

| 批次 | 文件（基线条数） |
|---|---|
| 1 | `hack.c`(152)、`eat.c`(136)、`pray.c`(133)、`shk.c`(169)、`invent.c`(86)；以及 `end.c` / `hack.c` / `eat.c` 里的 `You die...` 死亡流程 |
| 2 | `trap.c`(91)、`read.c`(102)、`zap.c`(110)、`apply.c`(76)、`mon.c`(75)、`uhitm.c`(68)、`do.c`(73)、`pickup.c`(53)、`potion.c`(58) |
| 3 | `insight.c`(367：`^X` 属性、`#conduct`、成就)、`options.c`(81)、`pager.c`(97)、`botl.c`(62)、`objnam.c`(76)、`cmd.c`(63)、`dungeon.c`(44：`#overview`) |
| 4 | 其余长尾文件；菜单标题和选项（`end_menu` / `add_menu`，如 `o_init.c` 的 "Pick an object type to name"） |

**翻译约定：**
- 标点用全角（，。！？：；「」……）。物品名、怪物名、地名不加引号。
- 物品和怪物统一走 `xname` / `doname` / `mon_nam` 等函数输出，不要硬编码中文名。
- 程度副词等措辞参考已有译文（`objects_zh.h`、`monsters_zh.h`），同一个术语前后要一致。

---

## 门禁 F：数据文件

### F1 / F2 地牢名　`F1-dungeon-zh-not-copy` `F2-overview-dungeon-names`

**现状：** `dat/dungeon_zh.lua` 和 `dungeon.lua` 一字不差。`#overview` 显示的是 `The Dungeons of Doom`。

**要求：**
- 删除 `dungeon_zh.lua`，`dungeon.c` 恢复只读 `dungeon.lua`（地牢名是标识符，见红线 3）。
- 在 `dungeon.c` 里加一个显示用的映射函数（如 `dname_zh(const char *)`），用在 `#overview`、死亡原因里的"死于…"、`%Z`、计分板等所有显示地牢名的地方。
- 同步修改 Unix 和 Windows 的构建列表。

### F3 / F6 Lua 关卡里的文字　`F3-tutorial` `F6-lua-messages`

**现状：** 新手教程 `tut-1.lua` / `tut-2.lua` 的 55 条刻字全是英文，这是新玩家最先看到的内容。其他关卡（`astral`、`castle`、`themerms`、`*-loca` 等）还有 25 条消息是英文。

**要求：** 关卡脚本是中英两版共用的，**不能原地翻译**。做法是：
- 新建 `dat/msgs_zh.lua`，返回一张"英文原文 → 中文"的表：
  ```lua
  return {
     ["You arrive on the Astral Plane!"] = "你来到了星界！",
     ["Move around with "] = "移动方向键：",
     ...
  }
  ```
- 在 `ZHLANG` 下，`des.message`、`nh.pline`、`nh.verbalize`、`des.engraving` 的 `text` 输出前先查这张表，查不到就原样输出。
- 拼接出来的字符串（`"Move around with " .. movekeys`）按片段建表。
- 验收工具会提取所有非注释的英文字面量，逐条检查是否在表中。
- 记得把 `msgs_zh.lua` 加进 Unix 和 Windows 的构建列表。

### F4 随机文本　`F4-random-text`

**要求：** 新建 `dat/rumors_zh.tru`、`rumors_zh.fal`、`oracles_zh.txt`、`engrave_zh.txt`、`epitaph_zh.txt`、`bogusmon_zh.txt`，并且：
- 接入 `makedefs`（非 ASCII 过滤已关闭）、`Makefile.top`、`dat/Makefile` 和 Windows 构建；
- 在 `include/global.h` 的 `ZHLANG` 区块里切换对应的文件名；
- 生成的文件仍然要满足 `makedefs` 的定长填充要求，装好后游戏内能读出（饼干签、神谕、墓碑、地面刻字、幻觉怪物名）。

### F5 神器名　`F5-artifacts`

**要求：** 参照 `objects_zh.h` 的做法，为 `include/artilist.h` 加一份中文显示名表，内部查找仍用英文。神器许愿要同时接受中英文（与 C3 一致）。

### F7 百科 `data.base`（可选，不设门禁）

**要求：** 词条名保留英文，配合 C6 的反查使用。正文可以翻译成 `data_zh.base`，引文保留原文或附上出处。

---

## 第二轮整改（2026-10-24 首轮验收后新增）

首轮验收结论是**不通过**：A、C、F 通过，B、D、E 不通过。

验收工具在首轮验收中修订了三处：
- 扫描规则升级到 v2（见 ACCEPTANCE.md §2.0）；
- 新增 `B5`、`D2`、`D4` 三项检查；
- 修复了 Lua 结果的解析。

下面每个问题都已经在验收工具或 80×24 渲染截图里实测复现。

### R1 状态栏数值被覆盖　`D2-status-line`（严重）

**现象：** 状态栏渲染为 `力:18/02 敏 体:18 智: 慧:8 魅: 中立` 和 `生命(16) 魔力(2) 防御`，多项数值不见了。

**根因：** [wintty.c:5049](../../win/tty/wintty.c) 的 `tty_putstatusfield` 每输出一个**字节**就让 `curx++`，一个汉字被记成 3 列，实际只占 2 列。下一个字段定位时，tty 用退格（`\b`）往回移，把刚写好的数值盖掉了。原始字节流里可以看到 `敏:12\x08\x08 体`。

**要求：** `curx` 和 `cw->curx` 按显示宽度累加，`cw->data` 的写入位置也要同步。改完后检查 `tty_curs` 不再输出多余的退格。

### R2 getlin 中文回显被拆开　`D4-getlin-echo`（严重）

**现象：** 在许愿、命名、刻字等提示里输入中文时，tty 按**字节**逐个回显，每个字节之间还插入光标显示/隐藏的转义序列（`\xe9 ESC[?12l ESC[?25h ESC[?25l \x93 …`），真实终端上会显示成乱码。

**要求：** 回显时按完整的 UTF-8 序列一次输出；退格要删掉整个字符，并按 2 列回退光标（`win/tty/getline.c`）。

### R3 假翻译：`ZHLANG` 分支照抄英文　`E1` / `E2`

扫描器 v2 下还剩 **507 处**，上限是 330。其中 8 个文件超过 15 处：

| 文件 | 剩余 |
|---|---|
| `mhitu.c`（怪物攻击，最高频） | 95 |
| `timeout.c` | 67 |
| `artifact.c` | 52 |
| `steed.c` | 50 |
| `muse.c` | 49 |
| `vault.c` | 37 |
| `cmd.c` | 35 |
| `sounds.c` | 35 |

这些大多是早期提交里"`#ifdef ZHLANG` 分支和 `#else` 分支内容一模一样"的写法，例如 `sounds.c:305` 的 `the footsteps of a guard on patrol.`。

**要求：** 补成真正的中文，能改成 `TR` 的一律改成 `TR`。可以用 `python3 test/zh/scan_untranslated.py --list hits.txt` 拿到逐条清单。

### R4 `You_feel` / `You_cant` 前缀重复（48 处）

**现象：** `You_feel()` 的前缀是"你感到"，`You_cant()` 的是"你不能"。而 48 处译文本身又以"感到/感觉/觉得"或"不能/无法"开头，结果显示为"你感到感到内疚。"、"你不能无法拾起…"。后者是双重否定，意思直接反了。

例子：`read.c:1891`、`eat.c:1443`、`pickup.c:1887`、`wield.c:922`、`engrave.c:551`。

**要求：** 去掉这些译文开头重复的动词；或者让 `zh_you_prefix()` 在译文已带同义动词时不再加前缀。二选一，但要全部覆盖。

### R5 片段拼接出的中英混杂　`B5-no-embedded-english`

**实测：**
- `^X` 属性页：`You are 见习，1 级的human女武神`、`你的strengthis 18/02`、`You have 4/16 点生命值`、`You are in the Dungeons of Doom`、`You can't 安全地祈祷`；
- 吃东西：`这里is蛇鸡兽 尸体；要eatit吗？`、`尝起来okay.`、`尸体是gamey.`；
- 地面刻字：`Something被刻在这里的floor上。`。

**原因：** 这些句子是用 `enl_msg()` / `you_are()` 等函数加上 `"is"`、`"eat"`、`"it"` 这类短词片段拼起来的。片段太短，扫描器统计不到，但游戏里就是这样显示的。

**要求：** `src/insight.c` 的 `enl_msg` 系列（前缀 `You are` / `You have` / `You can` / `Your … is`）、`src/eat.c` 的 `floorfood` 提示和口感形容词、`src/engrave.c` 的读刻字提示、`src/pager.c` 的 `farlook` 提示，都要整句中文化。英文属性名（strength 等）也要翻译。

### R6 运行时仍是英文的提示（人工抽查发现）

| 现象 | 位置 |
|---|---|
| 职业菜单第一项 `a - an Archeologist` | `src/role.c:31`：`#ifdef` 放错了位置，包住的是第一级头衔 Digger 而不是职业名。现在第一级头衔是"考古学家"，职业名仍是英文。两处都要修 |
| `That is a silly thing to 喝.` | `src/decl.c:43` 的 getobj 提示表 |
| `Are you sure you want to pray? [yn]` | `src/pray.c:2246`（`paranoid_query`） |
| `You finish your prayer.` | `src/pray.c:2298`（`nomovemsg`） |
| `选择a monster, object or location。`、farlook 的 `Tip:` 说明窗口 | `src/getpos.c` / `src/pager.c` |
| `It's a wall.` | `src/hack.c` 撞墙提示 |
| `Since you were in wizard mode, the score list will not be checked.` | `src/topten.c` |
| `Put "OPTIONS=!tutorial" in .nethackrc to skip this query.` | 教程询问菜单 |
| "Save bones?" 译成了"保存尸体吗？" | 意思不对，bones 指"遗骨文件"，建议译为"保存遗骨关卡吗？" |
| 物品栏 `（已穿戴）` 前多一个半角空格；提示语用半角 `?` 和句号 `.`（如 `你希望得到什么?`、`e - 2 银匕首.`） | 标点规范，见 E 的翻译约定 |
| 许愿得到神器后显示为 `长剑名为Excalibur` | 神器的中文名没有用在物品名显示上（F5 显示侧） |

### R7 玩家可见的 `NOTRANS` 豁免

130 处 `NOTRANS` 大多合理（配置文件格式、文件名、xlogfile 字段、调试输出）。以下几类是玩家能看到的，要改为翻译：
- `livelog_printf(LL_CONDUCT / LL_GENOCIDE / LL_DIVINEGIFT …)`：会出现在 `#chronicle` 里；
- `options.c` 选项菜单里显示的取值（`none`、`unknown`、`normal`、`%d top`…）；
- `botl.c` 状态高亮菜单里的属性名（`normal`、`dim`、`underline`、`inverse`）。

只写给文件、由程序解析的那些，保留豁免。

## 第三轮整改（2026-10-24 第二轮验收后新增）

第二轮验收结论仍是**不通过**，但进步很大：R1–R7 基本完成，状态栏、中文输入、属性页、职业菜单、教程、祈祷都已修好。

验收工具在第二轮验收中的修订：
- 扫描规则升级到 v3（按整条语句扫描，见 ACCEPTANCE.md §2.0）；
- 新增英文版回归检查 `A7`、`A8`。

### R8 英文版回归（违反红线 2）　`A7-en-inventory-headers` `A8-en-text-matches-upstream`（严重）

`A5` 修好之后，英文版才真正不带 `ZHLANG` 编译，下面这些问题因此第一次暴露出来：

| 现象 | 位置 | 原因 |
|---|---|---|
| 英文版物品栏的分类标题整体错位：武器上面是 `Illegal objects`，防具上面是 `Weapons`，食物上面是 `Tools` | `src/invent.c` 的 `names[]` | `#ifdef` 外面已经有一个 `0,`，`#else` 分支里又写了一个 `0,`。基线就有这个问题 |
| `The floating eye凝视着 the newt...` | `src/mhitm.c:837` | 拼接 `buf` 的 `Sprintf` 没有放进 `ZHLANG` 分支，两个版本都用了中文 |
| 结算界面的物品价值显示为 `(价值%ld %s)，` | `src/end.c:1771` | 同上 |
| `used 2 wishs` | `src/insight.c:3110` | 把 `"es"` 换成了 `plur()`；英文版必须保持 `"es"`，中文版传空串 |

**要求：** `A8` 必须为 0 处。如果确实需要改英文原文（比如修正上游的错别字），在交付说明里列出理由，由验收方加进白名单。

### R9 语句内的英文片段　`E1` / `E2`（v3 规则）

v3 规则下还剩 **1237 处**（基线 3986，上限 398）。主要有三类：

**1. 三元表达式里只翻了一个分支。** 常见的情况反而还是英文：
- `src/eat.c:1077`：`Hallucination ? TR(...) : "feel a momentary chill."`，获得火焰抗性时显示英文；
- `src/read.c:676`：`nodisappear ? TR(...) : "As you read the scroll, it disappears."`。

**2. 续行上的整句没翻译。**
- `src/topten.c:750` 的 `Since you were in %s mode, the score list will not be checked.`（R6 里已经点过名，第二轮仍未修）；
- `src/potion.c:989`、`src/do.c:1045` 等。

**3. 作为 `%s` 参数传进中文句子的英文片段。** 游戏里会显示成：
- `这尝起来像liquid fire`（`potion.c:886`）；
- `你mispronounce了咒语`（`read.c:686`）；
- `冷的water水龙头`、`被…的acid溅到了`（`hliquid()` 在中文版下原样返回英文，见 `do_name.c:1633`）；
- `无法猜出this区域的大小`（`cmd.c:1474`）；
- `"one "` / `"two "` 拼进"%s给了你%s%s买麦芽酒"（`priest.c:670`）；
- `vtense(..., "move")` / `"over"` / `"onto"` 等动词和介词拼进中文（`muse.c:3373` 一类）。

**要求：**
- 三元表达式的每个分支都要 `TR`；
- 传进中文格式串的参数也要是中文：在调用处 `TR`，或者在 `hliquid()`、`otense()` / `vtense()` 这类函数的中文分支里返回中文；
- 菜单项（如 `"[sort spells]"`、`"Percentage threshold"`）也要翻译；
- 调试和巫师模式专用的字符串（如 `artifact.c` 的 `exists;`、`selvar.c` 的形状描述）可以在同一条语句里加 `/* NOTRANS */` 豁免，验收方会抽查。

逐条清单：`python3 test/zh/scan_untranslated.py --list hits.txt`。

### R10 措辞打磨（次要，不单独设门禁）

`^X` 属性页实测：
- `你右撇子。` 缺"是"；
- `你进入了地牢 4 回合前。` 语序不对，应为"你在 4 回合前进入了地牢"；
- `你正常 <897>。` 应为"你不饿"；
- `你曾被杀 一次` 多一个空格；
- `，与以下神祇敌对：` 换行后以逗号开头，应和上一句合并。

其他：
- 提示语末尾还有半角 `?`，如 `你希望得到什么?`；
- 物品显示为 `长剑名为埃克斯卡利伯`，建议改为"名为埃克斯卡利伯的长剑"，或者已识别的神器直接显示神器名。

## 第四轮整改（2026-10-24 第三轮验收后新增）

第三轮验收结论是**不通过，但只剩小范围问题**：
- 英文版回归（R8）已全部修好，英文字符串与上游完全一致；
- 语句级扫描从 1237 处降到 0（v3）。

验收工具在第三轮验收中的修订：
- 扫描规则升级到 v4（加入 `safe_qbuf`）；
- 新增 `E3-notrans-not-visible`，自动检查 `NOTRANS` 有没有被拿来豁免玩家能看到的英文。

### R11 用 `NOTRANS` 掩盖可见英文（阻塞）　`E3-notrans-not-visible`

TASKS §E 规定"玩家正常游戏能看到的字符串不得豁免"。本轮新增的 85 处 `NOTRANS` 里，有 22 处违反了这条。下面几处已在游戏里实测或从代码确认：

| 位置 | 游戏里显示为 |
|---|---|
| `potion.c:1592` 等 7 处 `Hallucination ? "normal" : "strange"` | 喝变形药水："你感到有点strange。"（已实测） |
| `timeout.c:1009` | 变形结束："你又恢复成normal的自己了。" |
| `potion.c:328/362/450/693` | 视觉变化消息里的 `sadder` / `happier` / `normal` / `peculiar` |
| `sounds.c:1421/1608` | "up边的人听不到你。" |
| `zap.c:3589` | "血boils，pool你的…"（中文模板 `"血%ss，%s你的%s"` 本身也要重写） |
| `zap.c:5146` | "…吸收了致命的ray！" |
| `pager.c:826/878`、`hack.c:2357` | 水下查看地形时的 `unknown`、`nothing` |
| `options.c:3521/3522/5082` | 选项菜单里显示的 `none` / `random` / `view`。只是显示值（`get_val`），写回配置用的是 `get_cnf_val`，可以放心翻译 |
| `spell.c:645` | `#chronicle` 里的 "became literate by reading %s"。R7 已经要求过翻译 livelog，这里是回退 |

**要求：** `E3` 为 0 处。真正只写给程序读的字符串可以保留 `NOTRANS`。

### R12 `safe_qbuf` 拼接的提示（建议）

v4 规则下还剩 15 处，在阈值以内，不阻塞。但其中几条非常常见，建议一并修掉：
- `do_name.c:735`：每次使用未鉴定物品后的 `Call 暗浊色药水:`（已实测）；
- `pickup.c`：`Pick up ...?`、`... is empty.  Do what with it?`、`Do what with ...`；
- `trap.c` / `lock.c`：`There is a box here.  Check it for traps?`、`Disarm this ...`；
- `apply.c`：`Attach ...`；
- `mhitu.c`：`"That ring looks pretty.  May I have it?"`。

### R13 小问题（不阻塞）

- `#chronicle`：`her的首次许愿`。livelog 里用了英文代词 `uhis()`，中文应写成"她的首次许愿"或省略代词；
- 卷轴名：`卷轴 标签为THARR`，多一个空格，建议写成"标签为 THARR 的卷轴"；
- `options.c:5885/6213`：`TR` 宏嵌套使用，是批量替换留下的，请清理成单层；
- 提交里误带了 `test/zh/__pycache__/*.pyc`，请删除，并把 `__pycache__/` 加进 `.gitignore`。

## 附：README 同步

全部完成后，按实际情况改写 `README.md` 的"汉化范围"一节，删掉"全部 130+ C 源文件"这类和实际不符的说法；构建说明里切换英文版的方法要与 A5 一致。
