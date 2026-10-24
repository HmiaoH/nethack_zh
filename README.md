# NetHack 5.0 汉化

这是一个自己闲着没事干想顺畅入坑NetHack的无聊小工作。

既然做都做完了，不妨就发出来给大家一起玩一玩。
此版本不定期更新，也不大可能长期维护～

基于官方 [NetHack 5.0](https://github.com/NetHack/NetHack) 源码，通过 `-DZHLANG` 编译选项实现全中文游戏体验。

## 汉化范围

| 模块 | 内容 | 实现位置 |
|------|------|----------|
| 物品 | 全部约 500 个物品名称及未知描述 | `include/objects_zh.h` |
| 怪物 | 全部约 500 个怪物名称 | `include/monsters_zh.h` |
| 状态栏 | 力/敏/体/智/慧/魅、生命/魔力/防御/经验/回合、头衔与阵营 | `src/botl.c` 等 |
| 库存 | 祝福的/诅咒的/未诅咒的、附魔、已穿戴/已挥舞/箭袋中等 | `src/objnam.c` 等 |
| 游戏消息 | 主要消息源 C 文件的提示文本；未覆盖的少数冷门消息仍为英文 | `TR()` 宏与 `ZHLANG` 分支 |
| 帮助 | help / hh / cmdhelp / keyhelp / history / opthelp / optmenu / usagehlp / wizhelp | `dat/*_zh` |
| 操作菜单 | 扩展命令名与说明、按键说明 | `src/cmd.c`、`dat/cmdhelp_zh` |
| 任务对话 | 任务文本与 `%` 变量替换 | `dat/quest_zh.lua`、`src/questpgr.c` |
| 关卡文字 | 新手教程与各关卡 Lua 脚本里的玩家可见文字 | `dat/msgs_zh.lua` |
| 地牢名 | `#overview`、死亡原因、任务 `%Z`、状态栏、计分板显示的地牢名 | `dname_zh()`（`src/dungeon.c`） |
| 神器 | 神器中文显示名；许愿同时接受中英文名 | `include/artilist_zh.h`、`src/artifact.c` |
| 随机文本 | 饼干签、神谕、墓碑、地面刻字、幻觉怪物名 | `dat/*_zh.txt` / `dat/*_zh.tru` / `dat/*_zh.fal` |
| 死亡界面 | 死亡原因、DYWYPI、墓碑、计分板 | `src/end.c` 等 |
| 职业称号 | 全部 13 个职业 × 9 级称号 | `src/role.c` |

> 说明：内部标识符（Lua 脚本中的物品名/怪物名、地牢名、配置取值如
> `role:Valkyrie`、存档与 bones 文件名、`data.base` 词条键）始终保持英文；
> 中文只用于显示。英文版（不定义 `ZHLANG`）的行为与上游 NetHack 5.0 一致。

## 快速运行（macOS ARM64 预编译包）

下载 [Release](https://github.com/HmiaoH/nethack_zh/releases) 中的 `nethack-zh-*.tar.gz`：

```bash
tar xzf nethack-zh-*.tar.gz
cd nethack-zh-*
./nethack.sh          # 首次运行会提示输入密码完成配置
./nethack.sh -D       # 开发者模式（调试/作弊）
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
/usr/local/bin/nethack      # 或 ~/bin/nethack
```

### Linux

```bash
cd sys/unix && sh setup.sh hints/linux.500 && cd ../..
make fetch-Lua && make all && make install
```

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

汉化通过 `sys/unix/hints/include/chinese.500` 中的 `-DZHLANG` 编译标志启用；该文件是唯一添加 `-DZHLANG` 的地方。macOS 和 Linux hints 文件默认已包含它。

**切换到英文版**：编辑对应 hints 文件（`sys/unix/hints/macOS.500` 或 `linux.500`），
1. 从 `HINTSINCLNAMES` 列表中删掉 `chinese`；
2. 删掉（或注释掉）`#-INCLUDE chinese.500` 这一行。

然后重新 `sh setup.sh` 并编译即可。此时编译日志中不再出现 `-DZHLANG`，游戏读取英文数据文件（`dat/epitaph` 等），行为与上游 NetHack 5.0 相同。

修改 `sys/unix/Makefile.top`、`sys/unix/Makefile.dat` 等构建模板后，需要重新执行 `sh setup.sh`（必要时先 `make clean`）来重新生成根目录的 `Makefile` 和各子目录的 `Makefile`。

## 技术方案

采用双数组方案：英文名用于内部标识符和 Lua 脚本查找，中文名通过 `obj_descr_zh[]` / `mons_zh[]` 并行数组用于屏幕显示。详细说明见 `AGENTS.md`。

## 许可证

继承 NetHack [原始许可证](dat/license)。

## 致谢

- [NetHack DevTeam](https://www.nethack.org/) — 近四十年的经典
- 汉化工作由 [OpenCode](https://opencode.ai) 辅助完成
