# Brogue CE 中文版

保留 Brogue 原版 ASCII 地牢、动态火焰标题和键鼠操作的简体中文版本，基于 CE 1.15.1。

[下载 Release](https://github.com/Drafffffff/BrogueCE-zh/releases) · [掌机移植分支](https://github.com/Drafffffff/BrogueCE-zh/tree/移植)

| 分支 | 内容 |
| --- | --- |
| **汉化**（当前） | 原版桌面游戏 + 中文文本、UTF-8 排版、中文字体；Mac / Linux 键鼠操作 |
| **移植** | 包含汉化，再增加 RG34XX / NextUI、全手柄操作、掌机镜头与阅读面板 |

## 下载与安装

- **Mac Apple Silicon**：下载 `macOS-arm64.zip`，将 `Brogue CE 中文.app` 拖入 Applications。当前预编译包要求 **macOS 26 或以上**；已打包动态库，无需 Homebrew。应用为本地签名，未经过 Apple 公证。源码可以在满足依赖的其他 macOS 版本上自行编译。
- **Linux x86_64 / Bazzite**：下载 `Linux-x86_64.tar.gz`，解压 `Brogue` 到 `~/Games/`，执行 `~/Games/Brogue/launch.sh`。保留原版键鼠操作，默认全屏；程序使用系统 SDL2、glibc、FreeType，并附 SDL2_image / SDL2_ttf。已在 Bazzite 44 验证，其他发行版需要相应运行库。
- **Steam**：Linux 包包含封面、竖图、Hero、大透明 Logo 和图标。也可单独下载 `Steam-Artwork.zip`。退出 Steam 后运行 `python3 ~/Games/Brogue/install-steam.py`；有多个 Steam 账户时使用 `--config` 指定账户的 `config` 目录。也可手动添加 `launch.sh` 为非 Steam 游戏并设置图片。
- **RG34XX**：切换到 `移植` 分支，使用独立的 NextUI Release。

## 键鼠操作

方向键 / `hjklyubn` 移动；`s` 搜索，`z` 等待，`x` 自动探索；`i` 背包，`m` 菜单，`M` 消息历史，`?` 帮助，`S` 保存并退出。地牢底部及物品动作按钮显示真实快捷键。

瞄准和观察时，**Tab 下一目标，Ctrl+Tab 上一目标**。保留原版目标筛选规则：投掷卷轴等普通物品不会通过 Tab 自动选择怪物；可以用方向键或鼠标指定位置。Shift+Tab 留给 Steam Overlay。

Mac 存档：`~/Library/Application Support/Brogue/Brogue CE/`。Linux 启动器存档：`${XDG_DATA_HOME:-~/.local/share}/brogue-ce-zh/`。更新时替换程序目录，存档位于独立目录。

## 汉化与验证范围

已覆盖标题、菜单、物品名与详情、物品目录、怪物名字与说明、命中和伤害估计、变异与能力、常用战斗 / 地形事件、状态栏和帮助。药水名称使用公开外观色。不会翻译随机卷轴符文、种子、文件名和用户输入的名称。

本次修复怪物详情 / 睡眠状态 / 坠落提示 / 投掷药水结果 / 物品目录漏译，以及桌面软件渲染关闭中文窗口后留下半字残影的问题。检查了 1,809 组怪物、变异、盟友及俘虏说明；同种子中英文数值一致。仍可能存在未触达场景的漏译，首发标记为预发布，欢迎附截图报告。

![物品目录](docs/images/discoveries.png)

## 从源码构建

源码已经包含汉化，无需再次运行适配生成器。Linux 安装 C 编译器、Make、SDL2、SDL2_image 和 SDL2_ttf 开发包后：

```sh
make -j4 RELEASE=YES
cd bin
./brogue
```

macOS 安装同名开发依赖后使用 `make -B MAC_APP=YES RELEASE=YES`，应用打包脚本见 `packaging/`。默认中文，设置 `BROGUE_LANG=en` 可对照原版英文；`BROGUE_CJK_FONT` 可指定字体路径。

上游、许可证与字体来源见 [UPSTREAM.md](UPSTREAM.md) 和 [LICENSE.txt](LICENSE.txt)。

## macOS SDL2 与 Terminal（2026-10-15）

本次提供两个原版键鼠汉化版本：SDL2 图形版保留火焰标题与原版界面；Terminal 版保留 ASCII 地牢并支持中文双宽字符、16 / 256 / 真彩色终端。两者共享 `~/Library/Application Support/Brogue/Brogue CE/` 存档，避免同时打开同一局。预编译包仅支持 Apple Silicon，SDL2 包最低系统版本见 Release。

Terminal 包解压后双击 `Brogue Terminal.command`；终端至少需要 100 列、34 行。也可在已有终端中运行 `Brogue Terminal/launch.sh`。终端中的组合键和鼠标能力取决于终端模拟器；Steam Ctrl+Tab 修改主要针对 SDL2 版本。

从源码构建 Terminal：`make -B -j4 TERMINAL=YES GRAPHICS=NO MAC_APP=NO RELEASE=YES`，需要 ncurses 开发库。Linux 可安装 `libncursesw5-dev` 并在构建时指定 `LDLIBS=-lncursesw`；Mac 使用系统 ncurses，无需 SDL 或中文字体文件。
