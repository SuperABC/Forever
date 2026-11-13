# app_notes.h / app_notes.cpp

## 职责

`NotesApp`(`app_notes`)：app域的默认实现之一。

## 关键设计

- **`NotesApp`**：用来验证`Back`(详情返回列表，已经在列表则不做事，交给`Phone`决定是否离开App)、键盘上下/回车和鼠标点击列表项、以及`Refresh`(从后台切回来停在离开前的页面/选中项，不跳回列表)。数据直接硬编码在`.cpp`里，不做JSON数据文件+配置路径查询，够验证`Phone`状态机就行。
- 标题字号(App名字这一档)与`Phone`的"HOME"/"TASKS"统一用`height/17`，内容文字用`height/23`。
- `GetName()`必须全局唯一：static计数器+id+`GetName`里现拼，计数器存档见`Dependence/common/mod_state.md`。

## 依赖关系

- 依赖：`player/app_mod.h`、`player/canvas.h`、`common/utility.h`(`GetRandom`)。
- 被谁依赖：`basic.cpp`注册；`Core/player/phone.h`通过`AppFactory`枚举桌面图标。

## 待办/后续阶段

- 无。
