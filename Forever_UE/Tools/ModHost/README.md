# ModHost

不依赖 UE、不链接 Core 的 puzzle / app 本地验证程序。加载 mod DLL(默认同目录的 `Basic.dll`),用和游戏里 `UPuzzleWidget` / `UPhoneWidget` 同样的方式驱动:

- puzzle:`Puzzle`(Core 的薄包装)+ 一个 `Canvas`;
- app:完整的 `Phone`(桌面、任务列表、底部 返回/Home/任务 三按钮),源码直接取自 `Source/Core/player/phone.cpp`,`common/registry.h` 由 `shim/` 提供最小垫片。

## 构建

`Forever_UE\Tools\ModHost\build.bat`(Release,产物 `Forever_UE\x64\Release\ModHost.exe`,需先有 `Dependence.lib` 和 `Basic.dll`)。

## 用法

```
ModHost.exe --list
ModHost.exe --puzzle puzzle_tetris --window            # 自己玩 puzzle
ModHost.exe --phone --window                           # 自己用手机(app)
ModHost.exe --phone --script steps.txt --out shots     # 脚本 + 截图(无界面)
ModHost.exe --dll X.dll --puzzle <id> [--param text] [--size 480x640]
```

## 脚本命令

`wait ms` · `frame n` · `key <名>` / `keydown` / `keyup`(left right up down esc enter tab backspace space 单字符) · `click x y` · `down x y` · `up` · `move x y` · `drag x1 y1 x2 y2` · `bar back|home|tasks` · `open_app N` · `tap_task N` · `close_task N` · `resize W H` · `shot 名字`(写 PNG) · `expect_finished` / `expect_running`。`#` 开头为注释。

## 约定

新增的 puzzle / app 必须能只靠鼠标(点击/拖动)完成键盘能做的全部操作,以便在没有键盘的手机网页(`Tools/ModWeb`)里使用。
