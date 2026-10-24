# NetHack 5.0 汉化：验收规范

> 本文件和 `test/zh/` 由验收方维护，执行方不得修改（见 [TASKS.md](TASKS.md) §0.2）。
> 验收分两部分：**自动门禁**（`zh_accept.py`）和**人工抽查**（本文 §3）。两部分都通过才算验收通过。

## 1. 运行方法

```bash
python3 test/zh/zh_accept.py --report /tmp/zh-report.md
```

| 参数 | 作用 |
|---|---|
| `--interim` | 阶段性自检：门禁 E 只要求总数不超过基线 |
| `--require A,B` | 只要求指定的门禁通过（退出码按它计算） |
| `--no-build` | 复用上次的编译结果，只重跑游戏内检查和静态检查 |
| `--work DIR` | 工作目录，默认 `/tmp/nethack-zh-accept`。编译日志、`tty-zh.txt` / `tty-en.txt`（终端文本）和 `*.raw`（原始字节）都在这里 |

支持 macOS 和 Linux。前提：Python 3.9+；首次运行需要联网下载 Lua 5.4.8，之后会缓存在工作目录里。

脚本的流程：
1. 用 `git ls-files -co --exclude-standard` 复制当前工作区（包括未提交的改动），分别编译**中文版**和**英文版**。英文版的做法和 README 一致：从 hints 里去掉 `chinese`。
2. 安装到各自树内的 `playground/`，开启巫师模式，在伪终端（80×24，UTF-8）里自动操作游戏：
   - `#wizloadlua zh_accept`（`test/zh/zh_accept.lua`，只在中文版跑）
   - `#wizloadlua testwish`（上游 `test/testwish.lua`，两个版本都跑）
   - `#overview`、`i`（物品栏）
3. 分析终端输出、编译日志，做静态检查，跑 `scan_untranslated.py`。

## 2. 自动门禁

**最终验收必须 A–F 全部 PASS。**

| 门禁 | 检查项 | 判定标准 |
|---|---|---|
| **A** 崩溃·构建·回归 | `A0-build-zh/en` | 两个版本都编译、安装成功 |
| | `A1-no-format-warnings` | 中文版没有 `-Wformat*` 和 `-Wembedded-directive` 警告 |
| | `A1-no-new-warnings` | 中文版没有英文版之外的新警告（按 文件+警告文本 比较） |
| | `A1-en-build-clean` | 英文版没有格式类警告 |
| | `A2-appear-as-lookup` / `A2-appear-as-run` | `des.monster{appear_as="obj:luckstone"}` 不报 `can't find object` |
| | `A3-*` | Windows 三套构建脚本都包含 `*_zh` 数据文件 |
| | `A4-wintty-glyph-path` | `process_text_window` 保留上游的 `end_glyphout()` 分支 |
| | `A5-en-build-is-english` | 英文版编译日志里没有 `-DZHLANG` |
| | `A6-no-impossible` | 整个会话没有 impossible()，也没有 Lua 报错 |
| | `A7-en-inventory-headers` | 英文版物品栏的分类标题依次是 `Weapons`、`Armor`、`Comestibles`（女武神开局） |
| | `A8-en-text-matches-upstream` | 英文版每个 `src/*.c` 预处理后的英文字符串集合与上游 `c2a332168` 完全一致（相邻字面量合并后比较；有意的改动写进 `zh_accept.py` 的 `EN_ALLOWED` 白名单，由验收方批准） |
| **B** 语法层 | `B1-*` | 见 `zh_accept.lua`：中文的复数、单数、所有格、冠词正确，英文行为不变 |
| | `B2-no-mixed-grammar` | 终端输出里没有"汉字+`'s`/`s`/`es`"，也没有"`the`/`a`/`an`+汉字" |
| | `B3-questpgr` | `convert_arg` 有 `ZHLANG` 处理。这只是存在性检查，具体输出看 §3 的人工抽查 |
| **C** 名称查找 | `C1-*` | Lua 接口返回英文标识符 |
| | `C2-*` | 英文许愿正常；两个版本都通过上游 `testwish.lua` |
| | `C3-*` | 中文许愿：基本名、带前缀、无空格、诅咒、"X尸体"/"X的尸体" |
| | `C4-role-option` | `NETHACKOPTIONS=role:Valkyrie` 不报错 |
| | `C5-unique-zh-names` | 不同的英文物品名不共用同一个中文名 |
| | `B5-no-embedded-english` | 没有小写英文单词紧贴汉字（如 `你的strengthis`、`human女武神`）。首字母大写的专名不计 |
| **D** TTY | `D1-utf8-stream` | 终端原始字节是完全合法的 UTF-8（长中文消息换行时不截断字符） |
| | `D2-status-line` | 按 80×24、汉字占 2 列渲染后，状态栏每个字段都带数值（`敏:12`、`生命:16(16)`、`防御:6`…） |
| | `D4-getlin-echo` | 在许愿提示里输入中文，回显的字节流是合法 UTF-8 |
| **E** 字符串 | `E0-TR-macro` | 存在 `TR(en, zh)` 宏 |
| | `E1-untranslated-total` | 最终：≤ 基线的 10%（3986 → **≤398**，按 v3 规则，见下）。`--interim` 下：≤ 基线 |
| | `E2-untranslated-per-file` | 最终：每个文件 ≤ 15 处 |
| **F** 数据 | `F1-dungeon-zh-not-copy` | 不存在和 `dungeon.lua` 一模一样的 `dungeon_zh.lua` |
| | `F2-overview-dungeon-names` | `#overview` 里不出现 `Dungeons of Doom` |
| | `F3-tutorial` / `F6-lua-messages` | 共享 Lua 脚本里所有玩家可见的英文字面量，都能在 `dat/msgs_zh.lua` 里查到 |
| | `F4-random-text` | 6 个随机文本文件都有 `_zh` 版本 |
| | `F5-artifacts` | 神器名已翻译 |

### 2.0 扫描规则版本

- **v1**（初版）：`#ifdef ZHLANG` 分支里的内容一律视为已翻译。基线 2798 处。
- **v2**（首轮验收后修订）：
  - `ZHLANG` 分支照常扫描，里面仍是英文的字符串计为未翻译；
  - 新增 `paranoid_query()` 和 `nomovemsg` / `multi_reason` 赋值。
  
  用 v2 重算 `b3072466c`，基线为 3305 处。修订原因：首轮验收时，`sounds.c` 等文件里大量"中英分支内容完全相同"的假翻译，以及祈祷确认等提示，都没有被 v1 统计到。
- **v3**（第二轮验收后修订）：按**整条语句**扫描，不再只看调用名所在的那一行。
  - 语句里任何一个英文字面量都计入：续行上的整句，以及作为 `%s` 参数、三元表达式分支、`hliquid()` 参数传进中文句子的英文片段；
  - `TR()` 的英文参数不计入；语句里任意一行带 `NOTRANS` 即整条豁免。
  
  用 v3 重算 `b3072466c`，基线为 **3986 处**。修订原因：第二轮验收时，游戏里实测到 `As you read the scroll, it disappears.`、`feel a momentary chill.`（获得火焰抗性）、`这尝起来像liquid fire`、`你mispronounce了咒语`、`冷的water水龙头` 等，v2 都统计为 0。

### 2.1 基线（`b3072466c`，2026-10-24 实测）

| 门禁 | 结果 | 主要失败项 |
|---|---|---|
| A | FAIL | 20 条格式/宏内指令警告；`can't find object "luckstone"`；Windows 缺 `*_zh` 文件；wintty 回归；英文版仍带 `-DZHLANG` |
| B | FAIL | `箭's`、`长剑's`；终端里出现 `鉴定卷轴's`、`Wizard the 见习`；questpgr 未处理 |
| C | FAIL | Lua 接口返回中文；英文/中文许愿均失败；`role:Valkyrie` 报错；"弓"重名 |
| D | FAIL | 长消息在汉字中间断行 |
| E | FAIL | 无 `TR` 宏；未翻译 2798 处 |
| F | FAIL | `dungeon_zh.lua` 是原文副本；教程 55 条、关卡消息 25 条未翻；随机文本、神器名缺失 |

## 3. 人工抽查（验收方执行）

自动门禁全绿之后，验收方在 80×24 UTF-8 终端里用中文版逐项检查。每项记录为"通过 / 不通过 + 截图或说明"。

| # | 场景 | 操作 | 通过标准 |
|---|---|---|---|
| M1 | 开局 | 新建角色，走完角色选择 | 提示全中文；没有"女人法师"这类不通顺的说法 |
| M2 | 状态栏 | 观察底部两行 | 头衔、阵营、各项数值都是中文；不溢出 80 列，也不错位 |
| M3 | 物品栏 | `i`，并许愿得到多种物品 | 数量、祝福状态、附魔、"已穿戴/已挥舞"排列自然；列对齐；没有英文残留 |
| M4 | 长消息 | 触发连续多条消息 | 合并、换行、`--More--` 正常；没有乱码 |
| M5 | 战斗 | 和数种怪物战斗 | 命中、未命中、击杀消息通顺；主语"你"不缺也不重复（B4） |
| M6 | 商店 | 进商店、拿起商品、付款 | 店主对话和报价是中文；货币单位一致 |
| M7 | 祈祷/献祭 | `#pray`、`#offer` | 消息完整，没有英文 |
| M8 | 吃东西/饥饿 | 吃尸体，饿到虚弱 | 消息完整；饥饿状态在状态栏显示正确 |
| M9 | 许愿 | 用中文和英文各许 5 个愿（含神器、尸体、带前缀） | 都能得到正确的物品 |
| M10 | 灭绝/变形 | 灭绝卷轴、变形 | 中文和英文怪物名都接受 |
| M11 | 百科 | `;` 指向怪物，`/` 输入中文物品名 | 能显示 `data.base` 词条（C6） |
| M12 | 任务 | `#wizmakemap` 或传送到任务入口层，和首领对话 | 对话里没有 sister/see/he/'s 等英文，代词和称谓正确（B3） |
| M13 | 概览 | `^O` / `#overview` | 地牢名是中文（F2） |
| M14 | 教程 | 不加 `!tutorial` 开局，进入教程 | 地面刻字是中文，按键提示正确（F3） |
| M15 | 随机文本 | 吃饼干签，求神谕，看墓碑和地面刻字 | 中文内容正确显示（F4） |
| M16 | 死亡 | 巫师模式 `#wizkill` 自杀，或饿死 | "你死了……"出现；DYWYPI、墓碑、计分板全中文，回合数和金币数正确（A1、D3） |
| M17 | 英文版 | 用英文版走一遍 M1–M5 | 和上游行为一致，没有中文 |
| M18 | 代码审查 | `git diff b3072466c` | 遵守 TASKS.md §0.2 红线；`NOTRANS` 豁免合理；A2 审计清单完整；没有修改 `test/zh/` 和本文件 |

## 4. 验收结论格式

验收方给出：
1. `zh_accept.py` 报告的全文；
2. §3 每一项的结论；
3. 不通过项的清单：现象、复现步骤、对应的任务 ID。

**全部通过**，才建议合并和发布。
