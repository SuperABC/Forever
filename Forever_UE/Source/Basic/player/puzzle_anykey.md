# puzzle_anykey.h / puzzle_anykey.cpp

## 职责

`AnyKeyPuzzle`(`puzzle_anykey`)：puzzle域的默认实现之一。

## 关键设计

- `Loop`返回`nullptr`继续，返回非空`JsonValue*`(puzzle自己持有的结果对象，根是object)表示结束；键鼠事件从画布的队列里消费。
- **`AnyKeyPuzzle`**：只显示脚本通过`start_puzzle`的param传来的一行提示文字(+可选配图`puzzle_anykey_icon.png`，经`LoadModImage`从`Resource/Mod/Basic/`加载，加载不到只显示文字)，收到任意按键按下或鼠标点击就返回空object。`Init`先清键鼠缓冲，避免打开瞬间还按着的键(比如点选项的那一下)被当成退出。
- 另有一批小游戏(躲避方块/节奏大师/广场舞/德扑/打字追逐/汉堡店/多色试管/黄金矿工/平衡楼房/零花钱大作战)，见`puzzle_games.md`，公共工具在`puzzle_common.h`。
- `GetName()`必须全局唯一：static计数器+id+`GetName`里现拼。

## 依赖关系

- 依赖：`player/puzzle_mod.h`(`Canvas`、`PostHandle`)、`common/utility.h`(`GetRandom`)。
- 被谁依赖：`basic.cpp`注册；`Core/player/puzzle.h`消费，`Source/Forever/UI/PuzzleWidget`驱动。

## 待办/后续阶段

- 无。
