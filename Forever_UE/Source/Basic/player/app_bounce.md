# app_bounce.h / app_bounce.cpp

## 职责

`BounceApp`(`app_bounce`)：app域的默认实现之一。

## 关键设计

- **`BounceApp`**：纯`Loop`驱动的位置积分(速度240像素/秒定值，不随分辨率缩放)，用来验证`AppMod::Init`(随机初始位置、45度倍数方向)和`Refresh`(从后台切回来后继续弹，只按当前位置重画一次、不推进物理，不能被`Phone`重置)；单页App，`Back`留空实现。
- 标题字号(App名字这一档)与`Phone`的"HOME"/"TASKS"统一用`height/17`，内容文字用`height/23`。
- `GetName()`必须全局唯一：static计数器+id+`GetName`里现拼，计数器存档见`Dependence/common/mod_state.md`。

## 依赖关系

- 依赖：`player/app_mod.h`、`player/canvas.h`、`common/utility.h`(`GetRandom`)。
- 被谁依赖：`basic.cpp`注册；`Core/player/phone.h`通过`AppFactory`枚举桌面图标。

## 待办/后续阶段

- 无。
