# app_mod.h / app_mod.cpp

## 职责

`AppMod`："手机里的一个App"的Mod扩展接口，四个生命周期方法`Init`/`Loop`/`Back`/`Refresh`。

## 关键设计

- 都是真正的虚函数实例方法：Factory创建一个真实的`AppMod*`实例，`Phone::AppEntry`直接持有一个`App`(Core包装类)实例，多个App互不干扰。和`PuzzleMod`同一个模式。
- **画布是共享的**：所有App共用`Phone`持有的同一块画布，`Init`拿到的`Canvas*`只用来读`GetWidth()`/`GetHeight()`布局初始状态(画布分辨率是动态的，见`Canvas::Resize`，不能硬编码)，不持有所有权。
- **`Loop`不返回int**：没有App需要单方面结束整个Phone，退出/切换完全由Phone自己的Home/Back/任务列表逻辑决定。
- **`Back`**：收到"返回"操作(底部工具栏返回按钮/Backspace)时调用，多级页面的App在这里弹出自己的导航栈，单页App留空。
- **`Refresh`**：从后台切回前台时调用，不重新`Init`，只按已有状态重画一次(恢复被切走前的页面/滚动位置)。

## 依赖关系

- 依赖：`common/mod_state.h`、`player/canvas.h`、`common/handle.h`(`PostHandle`，向Core发起查询)。
- 被谁依赖：`app_factory.h`创建它；`Core/player/app.h`/`phone.h`消费它；具体实现在`Source/Basic/player/app_basic.*`。

## 待办/后续阶段

- 无。
