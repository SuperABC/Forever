# ModPanelWidget.h / ModPanelWidget.cpp

## 职责

`UModPanelWidget`：配置界面的**Mod screen**，三栏布局。

## 关键设计

- 左列`DllList`：已加载的全部dll(根目录)路径，删除=`Bridge::RemoveDllPath`，与`AddDllPathButton`一起管理`Config::GetDllPaths()`。
- 中列`ConceptList`：按`GetModConceptDescriptors()`顺序在`NativeConstruct`里动态生成一次的concept按钮(`UConceptButtonWidget`)，这张表运行期不变。
- 右列`ModList`：点中列某按钮后(`SelectConcept`)清空重建，显示该concept下全部已发现mod id的勾选框(`UModCheckRowWidget`)；`selectedConceptKey`为空(刚打开)时右列保持空白。
- 刷新：`RefreshModList`(任意勾选切换后，只刷右列)；`RefreshDllList`(dll路径增删后刷左列，因为"至少一个id启用"的DLL集合可能变，调用方自行决定是否同时刷右列)。
- Blueprint侧分别指定三个行类：`PathRowClass`(与Story/Resource共用)、`ConceptButtonClass`、`ModCheckRowClass`；行间距统一用`ScrollBoxUtils`。

## 依赖关系

- 依赖：`PathRowWidget`、`ConceptButtonWidget`、`ModCheckRowWidget`、`ScrollBoxUtils`、`ForeverConfigBridgeSubsystem`。
- 被谁依赖：`ConfigMenuWidget.cpp`。

## 待办/后续阶段

- 无。
