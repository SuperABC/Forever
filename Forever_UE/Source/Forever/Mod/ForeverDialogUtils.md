# ForeverDialogUtils.h / .cpp

## 职责

系统文件对话框的小工具（`namespace ForeverDialog`）：父窗口句柄、打开文件、保存文件，配置界面和存档共用。

## 关键设计

- 对话框返回的路径统一转成绝对路径（可能是相对引擎 Binaries 的）。
- 都用 `IDesktopPlatform`（`Forever.Build.cs` 已经依赖 `DesktopPlatform`）；没有活跃顶层窗口时父窗口传 nullptr。
- `SaveFile` 不检查文件是否已存在，由系统对话框自己提示覆盖。

## 依赖关系

- 被谁依赖：`ForeverConfigBridgeSubsystem`（SelectFolder/SelectFile）、`ForeverSaveSubsystem`（保存/加载对话框）。
