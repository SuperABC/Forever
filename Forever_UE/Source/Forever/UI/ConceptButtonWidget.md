# ConceptButtonWidget.h / ConceptButtonWidget.cpp

## 职责

`UConceptButtonWidget`：Mod screen中间一列的**concept按钮**，一个按钮对应一个concept，点击后通知拥有者切换右侧显示哪个concept的mod列表。

## 关键设计

- 按钮不在Blueprint里手摆：`UModPanelWidget::NativeConstruct`按`Core/common/loader.h`的`GetModConceptDescriptors()`顺序动态生成(目前21个)，所以新增concept不用改蓝图。
- `SetupButton(conceptKey, onClicked)`传入回调，本类只转发"我被点了、我的key是什么"，不知道右列怎么刷新。
- Blueprint子类需要命名`ConceptLabel`(TextBlock)和`SelectButton`(Button)。

## 依赖关系

- 依赖：`loader.h`的concept描述；`ModPanelWidget`提供回调。
- 被谁依赖：`ModPanelWidget.cpp`。

## 待办/后续阶段

- 无。
