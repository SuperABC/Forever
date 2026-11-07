# app_basic.h / app_basic.cpp

## 职责

手机系统的两个测试App：`BounceApp`(`app_bounce`，画布里反弹的方块)和`NotesApp`(`app_notes`，硬编码几条便签的列表->详情两级页面)。

## 关键设计

- **`BounceApp`**：纯`Loop`驱动的位置积分(速度240像素/秒定值，不随分辨率缩放)，用来验证`AppMod::Init`(随机初始位置、45度倍数方向)和`Refresh`(从后台切回来后继续弹，只按当前位置重画一次、不推进物理，不能被`Phone`重置)；单页App，`Back`留空实现。
- **`NotesApp`**：用来验证`Back`(详情返回列表，已经在列表则不做事，交给`Phone`决定是否离开App)、键盘上下/回车和鼠标点击列表项、以及`Refresh`(从后台切回来停在离开前的页面/选中项，不跳回列表)。数据直接硬编码在`.cpp`里，不做JSON数据文件+配置路径查询，够验证`Phone`状态机就行。
- 标题字号(App名字这一档)与`Phone`的"HOME"/"TASKS"统一用`height/17`，内容文字用`height/23`。
- `GetName()`必须全局唯一：static计数器+id+`GetName`里现拼，计数器存档见`Dependence/common/mod_state.md`。

## 依赖关系

- 依赖：`player/app_mod.h`、`player/canvas.h`、`common/utility.h`(`GetRandom`)。
- 被谁依赖：`basic.cpp`注册；`Core/player/phone.h`通过`AppFactory`枚举桌面图标。

## 待办/后续阶段

- 无。
