# ForeverDialogUtils.h / .cpp

## 职责

系统文件对话框的小工具（`namespace ForeverDialog`）：父窗口句柄、打开文件、保存文件。原来
`GetDialogParentWindowHandle()` 是 `ForeverConfigBridgeSubsystem.cpp` 里的匿名函数，存档也要弹对话框，所以
抽成共享函数。

## 关键设计

- 都用 `IDesktopPlatform`（`Forever.Build.cs` 已经依赖 `DesktopPlatform`）；没有活跃顶层窗口时父窗口传 nullptr。
- `SaveFile` 不检查文件是否已存在，由系统对话框自己提示覆盖。

## 依赖关系

- 被谁依赖：`ForeverConfigBridgeSubsystem`（SelectFolder/SelectFile）、`ForeverSaveSubsystem`（保存/加载对话框）。
