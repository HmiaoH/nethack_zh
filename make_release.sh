#!/bin/bash
set -e
PKG=nethack-zh-$(date +%Y%m%d)

rm -rf "$PKG" "$PKG.tar.gz" "/tmp/$PKG"

# 复制必要数据文件
mkdir -p "$PKG"
cp /Users/chenjiahao/nethackdir/nethack "$PKG/"
cp /Users/chenjiahao/nethackdir/recover "$PKG/"
cp /Users/chenjiahao/nethackdir/nhdat "$PKG/"
cp /Users/chenjiahao/nethackdir/symbols "$PKG/"
cp /Users/chenjiahao/nethackdir/license "$PKG/"
cp /Users/chenjiahao/nethackdir/sysconf "$PKG/"
# 预编译包是单人使用，允许任何用户进入开发者模式（-D）
sed -i '' 's/^WIZARDS=.*/WIZARDS=*/' "$PKG/sysconf"
# 运行时需要的空文件和存档目录
touch "$PKG/perm" "$PKG/record" "$PKG/logfile" "$PKG/xlogfile" "$PKG/livelog"
mkdir -p "$PKG/save"

# 创建启动脚本（自动设置环境）
cat > "$PKG/nethack.sh" << 'SCRIPT'
#!/bin/bash
DIR="$(cd "$(dirname "$0")" && pwd)"
export HACKDIR="$DIR"
cd "$HACKDIR"

# 首次运行：游戏目录在编译时写死，若不存在就把它软链接到解压目录
SYSCONF_PATH=$(strings "$HACKDIR/nethack" | grep "/sysconf$" | head -1)
if [ -n "$SYSCONF_PATH" ] && [ ! -f "$SYSCONF_PATH" ]; then
    SYSCONF_DIR=$(dirname "$SYSCONF_PATH")
    if [ -e "$SYSCONF_DIR" ] || [ -L "$SYSCONF_DIR" ]; then
        echo "$SYSCONF_DIR 已存在但其中没有 sysconf，请先移走它再运行。"
        exit 1
    fi
    echo "首次运行，需要创建软链接 $SYSCONF_DIR -> $HACKDIR"
    PARENT=$(dirname "$SYSCONF_DIR")
    if [ -w "$PARENT" ]; then
        ln -s "$HACKDIR" "$SYSCONF_DIR"
    else
        echo "需要管理员权限，请输入密码："
        sudo mkdir -p "$PARENT" && sudo ln -s "$HACKDIR" "$SYSCONF_DIR"
    fi
    echo "配置完成！（移动解压目录后需删除该软链接再重新运行）"
fi

exec "$HACKDIR/nethack" "$@"
SCRIPT
chmod +x "$PKG/nethack.sh"

# 说明文件
cat > "$PKG/README_汉化版.txt" << INSTALL
NetHack 5.0 汉化版（macOS ARM64）
==================================

运行：
  cd 解压目录 && ./nethack.sh
  首次运行会创建一个软链接，可能需要输入密码。

开发者模式（调试/作弊）：
  ./nethack.sh -D

curses 界面：
  NETHACKOPTIONS=windowtype:curses ./nethack.sh
  或在 ~/.nethackrc 中加入 OPTIONS=windowtype:curses

终端需使用 UTF-8 编码，字体需能显示中文。

汉化内容：
  物品、怪物、神器、游戏消息、状态栏、帮助、百科、任务对话、
  结算界面，tty 与 curses 界面

源码：
  https://github.com/HmiaoH/nethack_zh

构建日期：$(date '+%Y-%m-%d')
INSTALL

tar czf "$PKG.tar.gz" "$PKG"
echo "已创建: $PKG.tar.gz ($(du -h "$PKG.tar.gz" | cut -f1))"
rm -rf "$PKG"
