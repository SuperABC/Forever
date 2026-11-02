# InventoryButtonStyle.h

## 职责

头文件内联的`FInvButtonStyleState`：背包按钮在`HitTestInvisible`下**手动复现悬停/按下样式**。没有`.cpp`。

## 关键设计

- 背包里的按钮被设成`HitTestInvisible`好让鼠标事件落到面板/行自己身上做拖拽检测，代价是`UButton`自带悬停/按下样式失效。
- `Capture`在设成`HitTestInvisible`之前、也是第一次`Apply`之前调用，记下WBP里配置的原始样式；`Apply(button, state)`把样式的`Normal`换成它自己的`Hovered`/`Pressed`(按下时含`PressedPadding`)。用的是WBP配置的样式，不是写死的颜色。

## 依赖关系

- 依赖：UMG `UButton`。
- 被谁依赖：`AssetButtonWidget.h`、`InventoryWidget.h`。

## 待办/后续阶段

- 无。
