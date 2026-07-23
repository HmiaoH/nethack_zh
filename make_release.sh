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

# 创建启动脚本（自动设置环境）
cat > "$PKG/nethack.sh" << 'SCRIPT'
#!/bin/bash
DIR="$(cd "$(dirname "$0")" && pwd)"
export HACKDIR="$DIR"
cd "$HACKDIR"
exec "$HACKDIR/nethack" "$@"
SCRIPT
chmod +x "$PKG/nethack.sh"

# 创建首次运行安装脚本
cat > "$PKG/install.sh" << 'SETUP'
#!/bin/bash
# 首次运行：创建 nethackdir 软链接（如果二进制硬编码了此路径）
DIR="$(cd "$(dirname "$0")" && pwd)"
TARGET=/Users/chenjiahao/nethackdir
if [ ! -d "$TARGET" ]; then
    sudo mkdir -p "$(dirname "$TARGET")" 2>/dev/null || true
    sudo ln -sf "$DIR" "$TARGET" 2>/dev/null || {
        echo "无法创建系统路径，请手动运行："
        echo "  cd '$DIR' && ./nethack.sh"
        exit 1
    }
    echo "安装完成！"
fi
echo "运行：cd '$DIR' ; ./nethack.sh"
SETUP
chmod +x "$PKG/install.sh"

# 说明文件
cat > "$PKG/README_汉化版.txt" << 'INSTALL'
NetHack 5.0 汉化版
==================

最简单运行方式：
  cd 解压目录 && ./nethack.sh

如果提示找不到数据文件，运行：
  ./install.sh

开发者模式（调试/作弊）：
  ./nethack.sh -D

汉化内容：
  全部物品名、怪物名、状态栏、帮助系统、游戏消息、任务对话、死亡界面

源文件及汉化补丁：
  git@github.com:NetHack/NetHack.git
  (参见 AGENTS.md 了解汉化插件详情)

构建日期：$(date '+%Y-%m-%d')
INSTALL

tar czf "$PKG.tar.gz" "$PKG"
echo "已创建: $PKG.tar.gz ($(du -h "$PKG.tar.gz" | cut -f1))"
rm -rf "$PKG"
