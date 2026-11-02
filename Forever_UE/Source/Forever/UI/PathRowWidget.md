# PathRowWidget.h / PathRowWidget.cpp

## 职责

`UPathRowWidget`：**一行=一个路径+删除按钮**，Story screen(脚本列表)、Resource screen(资源文件夹列表)、Mod screen(dll路径)共用，替代老工程`PathItem`。

## 关键设计

- `SetupRow(path, onDelete)`传入删除回调，由拥有者决定具体调哪个`Config` mutator；本类不知道自己是哪种行，所以不需要为不同用途各写一个子类。
- Blueprint子类只摆命名`PathLabel`(TextBlock)和`DeleteButton`(Button)。

## 依赖关系

- 依赖：UMG。
- 被谁依赖：`ModPanelWidget.cpp`、`ResourcePanelWidget.cpp`、`StoryPanelWidget.cpp`。

## 待办/后续阶段

- 无。
