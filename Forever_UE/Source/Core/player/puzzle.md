# puzzle.h / puzzle.cpp

## 职责

`Puzzle`：小游戏实体，**薄包装类**，持有`PuzzleMod*`+`PuzzleFactory*`，方法直接转发给mod。

## 关键设计

- 照抄`Core/populace/scheduler.h`那种"`Citizen`独占持有一个`Scheduler`"的写法，这里是`UPuzzleWidget`独占持有一个`Puzzle`：构造时`factory->CreatePuzzle(id)`、析构时`factory->DestroyPuzzle(mod)`，保证跨DLL的`new`/`delete`成对。
- `SetParam`必须在`Init`之前调用：转发剧情脚本`start_puzzle`的`param`动态启动参数，空串表示脚本没传。
- `Loop`返回`JsonValue*`：`nullptr`继续运行，非空退出且指针是结果(归mod所有，Puzzle销毁前有效，调用方只读，要保留就在销毁前展平/拷走)。画布由宿主创建并定好尺寸，`Puzzle`不持有所有权。
- 禁止默认构造。

## 依赖关系

- 依赖：`player/puzzle_mod.h`、`player/puzzle_factory.h`。
- 被谁依赖：`Source/Forever/UI/PuzzleWidget`；具体小游戏在`Source/Basic/player/puzzle_basic.*`和`Forever_Mod/Wxdj`等外部mod。

## 待办/后续阶段

- 无。
