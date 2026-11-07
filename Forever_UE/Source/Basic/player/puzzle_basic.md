# puzzle_basic.h / puzzle_basic.cpp

## 职责

两个基础小游戏：`TetrisPuzzle`(`puzzle_tetris`，俄罗斯方块，新`Canvas`图形/输入API的落地测试)和`AnyKeyPuzzle`(`puzzle_anykey`，按任意键退出的占位小游戏)。

## 关键设计

- `Loop`返回`nullptr`继续，返回非空`JsonValue*`(puzzle自己持有的结果对象，根是object)表示结束；键鼠事件从画布的队列里消费。
- **`TetrisPuzzle`**
  - 标准10x20棋盘，7种方块用一份基础形状+按外接正方形边长的通用旋转公式`(x,y)->(size-1-y,x)`实现，不需要为每种方块存4个旋转态的查表数据；旋转带简单踢墙(原位、左1、右1)，O方块不转。
  - 计分：一次清`n`行得`n*n*100*level`，每10行升一级；重力间隔`max(100, 800-level*60)`毫秒。
  - 键盘：方向键/WASD移动、上/W旋转、空格硬降、`P`暂停、ESC退出；ESC任何时候都直接结束并返回`{score, completed}`；俄罗斯方块没有通关概念，`completed`=是否堆到顶自然结束(true)，中途ESC/EXIT退出为false。游戏结束后只响应ESC/EXIT按钮。
  - **屏幕按键**(鼠标/触摸)：画布上画一组按键，点击等价于键盘(左/旋转/右/下/硬降/暂停/退出)，让没有键盘的设备(手机网页)也能完整操作；键盘路径并存。左/右/下可连发(按住250ms后每90ms一次)。暂停中点击按键以外任意位置即继续。
  - **布局随画布尺寸变化**：`RecomputeLayout`按画布尺寸重算格子边长/棋盘偏移/侧栏位置；"棋盘+侧栏(+按键面板)"作为整体在画布内居中。竖屏(宽<高x1.2，手机)按键放棋盘下方两排；横屏按键放侧栏右边的两列面板。`Init`首次调一次，之后画布被`Resize`时经`Canvas::ResizeFunc`回调再调。`ResizeFunc`是裸函数指针，不能指向成员函数，所以用`static OnCanvasResize(userData, w, h)`带上`this`转发。
- **`AnyKeyPuzzle`**：只显示脚本通过`start_puzzle`的param传来的一行提示文字(+可选配图`puzzle_anykey_icon.png`，经`LoadModImage`从`Resource/Mod/Basic/`加载，加载不到只显示文字)，收到任意按键按下或鼠标点击就返回空object。`Init`先清键鼠缓冲，避免打开瞬间还按着的键(比如点选项的那一下)被当成退出。
- 另有一批小游戏(躲避方块/节奏大师/广场舞/德扑/打字追逐/汉堡店/多色试管/黄金矿工/平衡楼房/零花钱大作战)，见`puzzle_games.md`，公共工具在`puzzle_common.h`。
- `GetName()`必须全局唯一：static计数器+id+`GetName`里现拼。

## 依赖关系

- 依赖：`player/puzzle_mod.h`(`Canvas`、`PostHandle`)、`common/utility.h`(`GetRandom`)。
- 被谁依赖：`basic.cpp`注册；`Core/player/puzzle.h`消费，`Source/Forever/UI/PuzzleWidget`驱动。

## 待办/后续阶段

- 无。
