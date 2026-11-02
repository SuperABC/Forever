# AssetButtonWidget.h / AssetButtonWidget.cpp

## 职责

`UAssetButtonWidget`：背包列表里的**一行**(对应老蓝图`AssetButton_C`)，显示一个`Asset`的名字，支持选中、单击、双击进入容器、按下拖拽。

## 关键设计

- 内部`Button`只负责外观(选中高亮背景色)，构造时设成`HitTestInvisible`：`SButton`会吞掉按下事件，拖拽检测和原生双击都到不了这一层，所以鼠标事件直接落到这一行自己身上；按下只通知面板(`UInventoryWidget::BeginRowPress`)，由面板捕获鼠标统一区分单击/拖拽。
- 因为Button不再接收鼠标，悬停/按下样式靠`InventoryButtonStyle.h`手动复现；面板捕获鼠标期间收不到`MouseLeave`，所以松开后由面板调`FinishPress`按松开位置重算悬停。
- 单击(没拖动就松开)：普通/容器行=选中；"返回上一级"占位行(`SetupAsBack`，asset为空)=直接退出。双击(第二次点击在`kDoubleClickInterval`秒内)：容器行=进入。
- 行对象会被复用，所以每次刷新列表都重新`Setup`；`Setup`记下行所属列表(`EInvZone`)和所在容器(拖拽时作为源parent)。`asset`不持有所有权。
- `SetLabel`供拖拽框复用这个控件显示文字，不关联任何Asset。

## 依赖关系

- 依赖：`InventoryWidget`、`InventoryButtonStyle.h`、Core `Asset`。
- 被谁依赖：`UI/InventoryWidget.cpp`(整体拖拽流程见`InventoryWidget.md`)。

## 待办/后续阶段

- 无。
