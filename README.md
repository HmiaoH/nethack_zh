# NetHack 5.0 汉化

这是一个自己闲着没事干想顺畅入坑NetHack的无聊小工作。

既然做都做完了，不妨就发出来给大家一起玩一玩。
汉化几乎100%；包括几乎所有界面～
但不保证完全0英文

基于官方 [NetHack 5.0](https://github.com/NetHack/NetHack) 源码，通过 `-DZHLANG` 编译选项实现全中文游戏体验。

## 汉化范围

| 模块 | 内容 | 实现位置 |
|------|------|----------|
| 物品 | 全部约 480 个物品名称及未鉴定时的外观描述 | `include/objects_zh.h` |
| 怪物 | 全部约 390 种怪物名称（含男女两种称呼） | `include/monsters_zh.h` |
| 神器 | 神器中文名；许愿时中英文名都能用 | `include/artilist_zh.h` |
| 游戏消息 | 全部 C 源文件里玩家能看到的消息、提示和菜单 | `TR()` 宏，见下文 |
| 状态栏 | 力/敏/体/智/慧/魅、生命/魔力/防御/经验/层、头衔与阵营 | `src/botl.c` |
| 物品栏 | 祝福/诅咒状态、附魔、已穿戴/已挥舞/箭袋中、商店价格等 | `src/objnam.c` |
| 许愿 | 中英文都能用，比如"2祝福的+3银匕首"、"豺狼的尸体" | `src/objnam.c` |
| 帮助 | 游戏内 `?` 打开的全部帮助：命令、按键、选项、历史等 | `dat/*_zh` |
| 百科 | 用 `;` 或 `/` 查看怪物、物品时显示的全部 515 个条目，可以用中文名查询 | `dat/data_zh.base` |
| 任务对话 | 全部职业的任务剧情，包括其中的称谓、代词等变量 | `dat/quest_zh.lua` |
| 关卡文字 | 新手教程和各关卡里的提示、刻字 | `dat/msgs_zh.lua` |
| 地名与地形 | 地牢名、远观时的地形/陷阱/怪物类别说明 | `src/dungeon.c`、`src/pager.c` |
| 随机文本 | 饼干签、神谕、墓碑、地面刻字、幻觉时的怪物名 | `dat/*_zh.txt` 等 |
| 结算界面 | 死亡原因、遗物鉴定、墓碑、计分板 | `src/end.c` 等 |
| 职业 | 13 个职业的名称和全部头衔；武士的日式物品名（胁差、兵粮丸……） | `src/role.c`、`src/objnam.c` |
| 界面 | 默认的 tty 界面和可选的 **curses 界面**都支持中文显示和输入 | `win/tty/`、`win/curses/` |

> 说明：
> - 内部标识符一律保持英文：Lua 脚本里的物品名和怪物名、地牢名、配置项取值（如 `role:Valkyrie`）、存档和遗骨文件、百科词条的键。中文只用于显示，所以原有的配置文件可以照常使用。
> - 英文版（不定义 `ZHLANG`）的行为和上游 NetHack 5.0 一致。
> - Qt、X11 两个图形界面和游戏手册 `doc/Guidebook` 没有汉化。

## 快速运行（macOS ARM64 预编译包）

下载 [Release](https://github.com/HmiaoH/nethack_zh/releases) 中的 `nethack-zh-*.tar.gz`：

```bash
tar xzf nethack-zh-*.tar.gz
cd nethack-zh-*
./nethack.sh          # 首次运行会提示输入密码完成配置
./nethack.sh -D       # 开发者模式（调试/作弊）
```

终端需要使用 UTF-8 编码，字体要能显示中文。

## 切换到 curses 界面

默认是传统的 tty 界面。curses 界面有分栏窗口、常驻物品栏和鼠标支持，想用的话在 `~/.nethackrc` 里加一行：

```
OPTIONS=windowtype:curses
```

或者临时用环境变量：

```bash
NETHACKOPTIONS=windowtype:curses ./nethack.sh
```

## 从源码构建

```bash
git clone --recursive https://github.com/HmiaoH/nethack_zh.git
cd nethack_zh
```

### macOS

```bash
cd sys/unix && sh setup.sh hints/macOS.500 && cd ../..
make fetch-Lua && make all && make install
~/bin/nethack
```

### Linux

```bash
cd sys/unix && sh setup.sh hints/linux.500 && cd ../..
make fetch-Lua && make all && make install
```

curses 界面需要宽字符版的 ncurses（`libncursesw`，Debian/Ubuntu 上是 `libncurses-dev`）。

### Windows

使用 **MSYS2/MinGW64**：

```bash
cd sys/windows
mingw32-make -f GNUmakefile install
```

或使用 **Visual Studio 2022**：打开 `sys/windows/vs/NetHack.sln`，选择 `Release` 配置直接编译。项目默认已启用 `ZHLANG` 预处理宏。

### 清理与重编译

```bash
make spotless        # 清除所有编译产物
```

修改 `sys/unix/Makefile.top`、`sys/unix/Makefile.dat` 等构建模板后，需要重新执行 `sh setup.sh`（必要时先 `make clean`）来重新生成各级 `Makefile`。

### 构建选项

- **英文版**：编辑 `sys/unix/hints/macOS.500` 或 `linux.500`，从 `HINTSINCLNAMES` 列表里删掉 `chinese`，再删掉（或注释掉）`#-INCLUDE chinese.500` 这一行，然后重新 `sh setup.sh` 并编译。`sys/unix/hints/include/chinese.500` 是唯一添加 `-DZHLANG` 的地方。
- **不编 curses 界面**：`make WANT_WIN_CURSES=`。

## 技术方案

- **双表**：物品、怪物、神器的英文表用于内部查找、Lua 脚本和存档，中文表（`obj_descr_zh[]`、`mons_zh[]` 等）只用于显示。
- **`TR(英文, 中文)` 宏**：每条消息的中英文写在一起，按是否定义 `ZHLANG` 取其中之一，英文版的代码路径不受影响。
- **语法层**：英文的复数、冠词、所有格等处理在遇到中文时不生效，避免出现"箭s""the 长剑"这类拼接。
- **显示层**：消息和提示在输出前统一整理中文标点和空格；tty 和 curses 界面按显示宽度（汉字占 2 列）排版，换行和输入回显都不会截断汉字。

更多细节见 `AGENTS.md`。

## 许可证

继承 NetHack [原始许可证](dat/license)。

## 致谢

距离上一个版本的更新已经过去了九个月。
在这一段时间中，AI的智力以一个我完全无法想象的速度攀升。以至于只要知道技术基础，加以合适的提示词引导，就能做出几乎任何想要的后端效果。
就近期而言，无论是层出不穷的3D化模组和各色高完成度汉化，无不昭示着这或许就是游戏玩家最好的时代。
但对于还在上学的我而言，可能这就是最坏的时代了。无论什么样的大作业小作业小组作业，第一反应不是去查资料，而是去想要怎么和AI表述清楚我的想法。次数多到自己都开始觉得自己“学风不正，滥竽充数”了。

但是不管怎么说，对于这个项目而言，我还是一个游戏玩家而非学生。
所以我在这里还是要感谢Codex、Claude和DeepSeek。没有他们，或许这个游戏的完全汉化真的要等到大道都磨灭了。

- 汉化工作由 [OpenCode](https://opencode.ai) 和 [Claude Code](https://claude.com/claude-code) 主要完成

- [NetHack DevTeam](https://www.nethack.org/) — 近四十年的经典
