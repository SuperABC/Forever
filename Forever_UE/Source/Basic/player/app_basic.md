# app_basic.h / app_basic.cpp

## 职责

手机系统的两个测试App：`BounceApp`(`app_bounce`，画布里反弹的方块)和`NotesApp`(`app_notes`，硬编码几条便签的列表->详情两级页面)。

## 关键设计

- **`BounceApp`**：纯`Loop`驱动的位置积分，用来验证`AppMod::Init`(随机初始位置/方向)和`Refresh`(从后台切回来后继续弹，不能被`Phone`重置)；单页App，`Back`留空实现。
- **`NotesApp`**：用来验证`Back`(详情返回列表，已经在列表则不做事)、鼠标点击列表项、以及`Refresh`(从后台切回来停在离开前的页面/选中项，不跳回列表)。数据直接硬编码在`.cpp`里，不做JSON数据文件+配置路径查询，够验证`Phone`状态机就行。
- `GetName()`必须全局唯一：static计数器+id+`GetName`里现拼。

## 依赖关系

- 依赖：`player/app_mod.h`(`Canvas`)。
- 被谁依赖：`basic.cpp`注册；`Core/player/phone.h`通过`AppFactory`枚举桌面图标。

## 待办/后续阶段

- 无。
