# ThrowListButtonWidget

## 职责
投掷物选择列表(`UThrowListWidget`)里的一行：一组可投掷物，一个按钮加一行文字"资产id  x数量"。点击时通过C++委托`onSelected(type)`通知列表控件。

## 关键设计
- 逻辑在C++，外观在Blueprint子类`WBP_ThrowListButton`：需要名字对得上的`Button`(Button)和`Label`(TextBlock)。
- 当前激活项的外观由C++处理，不需要蓝图写逻辑：`Setup`里按是否激活把`Button`的底色设成`activeColor`/`inactiveColor`(`EditDefaultsOnly`，在`WBP_ThrowListButton`的类默认值里调)。曾经用`BlueprintImplementableEvent`让蓝图实现，但`Button`/`Label`是`protected`成员，蓝图图表里拿不到这两个变量，没法写；现在这两个成员也标了`BlueprintReadOnly`。
- 点击回调用原生`DECLARE_DELEGATE_OneParam`而不是动态委托——只有C++的列表控件绑定它，不用暴露给蓝图，也能直接带`type`参数。

## 依赖关系
- 被谁依赖：`UThrowListWidget`(`Open`里为每组投掷物创建一行并绑定`onSelected`)。
