# InventoryWidget / AssetButtonWidget

背包面板，全部靠拖拽操作；规则和搬运见 `InventoryTransfer.md`。

- 区域：5 个槽位按钮（`LeftShoulderButton`/`RightShoulderButton`/`LeftHandButton`/`RightHandButton`/`BackPackButton`）、容器列表（`BodyList`/`BodyLabel`，显示选中槽位里容器的内容）、房间列表（`RoomList`/`RoomLabel`，不在房间时隐藏）、附近列表（`NearbyList`/`NearbyLabel`，BindWidgetOptional）。可选 `MessageText` 显示失败原因，没放则回退成屏幕调试信息。只保留 `UseButton` 和 `CloseButton`。
- 槽位按钮和行内的 `Button` 构造时设为 `HitTestInvisible`（`SButton` 会吞掉按下事件，拖拽和双击到不了上层），鼠标事件由面板按几何命中判断槽位、由行自己处理。槽位点击（不拖动）= 让容器列表显示该槽位容器的内容；槽位按钮文字显示 `原标签: 物品`（取按钮第一个 TextBlock 子控件）。
- 拖拽：按下并拖动开始；不按 Ctrl 拖 1 个，按住 Ctrl（拖拽开始时取）拖整个堆叠；拖拽框只显示物品名和数量。松手后 `PerformDrop` 按鼠标位置确定目标区域，调用 `InventoryTransfer::Transfer`。
- 附近：`UForeverAssetFrameworkComponent::CollectNearby` 用球形物理重叠查询（半径 `kNearbyRadiusCm`=10m，对象类型 PhysicsBody）收集玩家周围、归 `Player::worldAssets` 持有的物品，按距离排序；只在打开面板和每次操作之后收集，不每帧收集。列表行对象池复用（`FillList`）。
- 进入的子容器栈在每次刷新前用 `TrimStack` 校验，拖拽移走/销毁容器后自动截断。
- 编辑器手工步骤：`WBP_Inventory` 里加 `NearbyList`（ScrollBox）、`NearbyLabel`（TextBlock），可选 `MessageText`；删掉 `DropButton`/`PickButton`/`ResetButton`（代码不再绑定）。

## 补充（拖拽框、悬停样式、槽位文字）
- **槽位拖拽不走 UMG 的 DragDropOperation**：`FUMGDragDropOp` 的拖拽框会在 150ms 内从"发起拖拽的控件左上角"插值滑向鼠标，槽位拖拽是整个面板发起的，框就会从屏幕左上角飞过来。所以槽位由面板自己捕获鼠标（`CaptureMouse`），超过拖动阈值后用 `AssetButtonClass` 的一行做一个贴着鼠标的框（`CreateGhost/UpdateGhost`），松手在 `NativeOnMouseButtonUp` 里 `PerformDrop`。列表行同理：行按下时只通知面板（`BeginRowPress`），之后单击（`UAssetButtonWidget::Click`）和拖拽都由面板处理，槽位与列表行用同一套逻辑，不再使用 UMG 的 DragDropOperation。
- **悬停/按下样式**：槽位按钮和行内 Button 都是 HitTestInvisible，原生样式失效；`InventoryButtonStyle.h` 的 `FInvButtonStyleState` 记下 WBP 里配置的按钮样式，按状态把 Normal 换成 Hovered/Pressed 复现默认响应。
- **槽位文字**：有物品只显示物品内容（`类型 xN [容器]`），空槽位才显示 WBP 里原来的部位标签。
- **手只收数量为 1 的物品**：`InventoryTransfer::Transfer` 与 `Player::AddByPath` 都检查。
