# puzzle_basic.h / puzzle_basic.cpp

## 职责

`TetrisPuzzle`(`puzzle_tetris`)：俄罗斯方块，作为新`Canvas`图形/输入API的落地测试重新实现(不照抄老工程)。

## 关键设计

- 标准10x20棋盘，7种方块**用一份基础形状+按外接正方形边长的通用旋转公式`(x,y)->(size-1-y,x)`**实现，不需要为每种方块存4个旋转态的查表数据。
- **布局随画布尺寸变化**：`RecomputeLayout`按画布尺寸重算格子边长/棋盘偏移/侧栏位置，`Init`首次调一次，之后画布被`Resize`时通过`Canvas::ResizeFunc`回调再调一次。`ResizeFunc`是裸函数指针，不能直接指向成员函数，所以用`static OnCanvasResize(userData, w, h)`带上`this`转发。
- `Loop`返回0继续、非0结束(游戏结束)。键鼠事件从画布的队列里消费。
- `GetName()`必须全局唯一：static计数器+id+`GetName`里现拼。

## 依赖关系

- 依赖：`player/puzzle_mod.h`(`Canvas`)。
- 被谁依赖：`basic.cpp`注册；`Core/player/puzzle.h`消费，`Source/Forever/UI/PuzzleWidget`驱动。

## 待办/后续阶段

- 无。
