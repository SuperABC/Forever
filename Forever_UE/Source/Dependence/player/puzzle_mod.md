# puzzle_mod.h / puzzle_mod.cpp

## 职责

`PuzzleMod`：小游戏Mod基类。`Init`对应"小游戏进入时初始化一次"，`Loop`对应"每帧更新一次"。

## 关键设计

- **画布由宿主创建**：`Canvas`的创建和定尺寸交给UE侧宿主(`UPuzzleWidget`)，`Init`只接收一个已经`Init()`好尺寸的`Canvas*`，用`canvas->GetWidth()`/`GetHeight()`布局初始状态；不持有所有权。
- **`SetParam(param)`**：剧情脚本`start_puzzle`的`param`字段传给这一局的**动态**启动参数，在`Init`之前调一次，默认忽略；每次启动可以不同，让剧情控制小游戏表现，格式由各小游戏自己约定。它和`config.json`里`puzzle_mods`的静态`args`(创建mod时传给创建函数、整个游戏期间固定)不是一回事；动态参数不进存档(谜题进行中本就不能存档)。
- **`Loop`返回int**：0表示继续，非0表示这一局结束；每帧读取画布里宿主注入的键鼠事件、推进逻辑、把画面画到画布上，`ms`是距上一帧的毫秒数。
- 带存档钩子，默认不存任何东西，见`common/mod_state.md`。

## 依赖关系

- 依赖：`common/mod_state.h`、`player/canvas.h`、`common/handle.h`。
- 被谁依赖：`puzzle_factory.h`创建它；`Core/player/puzzle.h`消费它；具体实现在`Source/Basic/player/puzzle_basic.*`和`Forever_Mod/Wxdj`等外部mod里。

## 待办/后续阶段

- 无。
