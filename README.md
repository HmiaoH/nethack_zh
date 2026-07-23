# NetHack 5.0 汉化版

NetHack 5.0 的中文汉化插件项目。基于官方 [NetHack 5.0](https://github.com/NetHack/NetHack) 源码，通过 `-DZHLANG` 编译选项实现全中文游戏体验。

## 汉化范围

| 模块 | 内容 |
|------|------|
| 物品 | 全部 ~500 个物品名称及描述 |
| 怪物 | 全部 ~500 个怪物名称 |
| 状态栏 | 力/敏/体/智/慧/魅、生命/魔力/防御/经验/时 |
| 库存 | 祝福的/诅咒的/未诅咒的、已穿戴/已挥舞/箭袋中等 |
| 帮助 | 全部帮助文档及菜单 |
| 操作菜单 | 物品动作、按键绑定说明 |
| 游戏消息 | 全部 130+ C 源文件中的提示文本 |
| 任务对话 | quest.lua 中文翻译 |
| 死亡界面 | 墓碑、分数、再见语等 |
| 职业称号 | 全部 13 个职业 × 9 级称号 |

## 快速运行（macOS ARM64 预编译包）

下载 [Release](https://github.com/HmiaoH/nethack_zh/releases) 中的 `nethack-zh-*.tar.gz`：

```bash
tar xzf nethack-zh-*.tar.gz
cd nethack-zh-*
./nethack.sh          # 开始游戏
./nethack.sh -D       # 开发者模式（调试/作弊）
```

## 从源码构建

```bash
git clone --recursive https://github.com/HmiaoH/nethack_zh.git
cd nethack_zh
```

### macOS

```bash
cd sys/unix && sh setup.sh hints/macos.500 && cd ../..
make fetch-Lua && make all && make install
/usr/local/bin/nethack      # 或 ~/bin/nethack
```

### Linux

```bash
cd sys/unix && sh setup.sh hints/linux.500 && cd ../..
make fetch-Lua && make all && make install
nethack
```

### 清理与重编译

```bash
make spotless        # 清除所有编译产物
```

汉化通过 `sys/unix/hints/include/chinese.500` 中的 `-DZHLANG` 编译标志启用。macOS 和 Linux hints 文件默认已开启。如只需英文版，从对应 hints 文件中移除 `chinese` 即可。

## 技术方案

采用双数组方案：英文名用于内部标识符和 Lua 脚本查找，中文名通过 `obj_descr_zh[]` / `mons_zh[]` 并行数组用于屏幕显示。详细说明见 `AGENTS.md`。

## 许可证

继承 NetHack [原始许可证](dat/license)。

## 致谢

- [NetHack DevTeam](https://www.nethack.org/) — 近四十年的经典
- 汉化工作由 [OpenCode](https://opencode.ai) 辅助完成
