# ModArgsDialogWidget.h / ModArgsDialogWidget.cpp

## 职责
`UModArgsDialogWidget`：某个mod参数的编辑弹窗。每个参数一行(一个原始token，如`--path=C:/x`、`--skip`)，可增、删、改；点确定把非空行用单个空格拼回整串参数并回调，取消不改。

## 关键设计
- **不依赖蓝图**：`Open()`里用`WidgetTree->ConstructWidget`动态搭：全屏半透明遮罩(`UBorder`)→居中固定大小面板→标题、`UScrollBox`参数行、"添加参数/取消/确定"按钮。依赖`UUserWidget`为纯C++类时自带的`WidgetTree`；`Open`必须在`AddToViewport`之前调。
- 每行的"删除"按钮用`UModArgRowProxy`(每行一个UObject)接回调：UMG按钮`OnClicked`是动态委托、不带发送者。
- 一行里误输入的空格按多个参数处理(Core的`ModArgs`按空白拆)，所以参数值不能含空格。
- 用法：`CreateWidget<UModArgsDialogWidget>(this, StaticClass())` → `Open(modId, 当前参数, 确定回调)` → `AddToViewport(1000)`。

## 依赖关系
- 被谁依赖：`ModCheckRowWidget`。
