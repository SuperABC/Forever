# ForeverConfigBridgeSubsystem

配置界面(Story/Mod/Resource)到 `Config` 静态方法的薄包装。

## 配置管理

- `SaveConfigWithDialog`：弹保存对话框，把当前内存配置 `WriteConfig` 到所选 json。不校验依赖，不动 config.json。取消时返回 false 且 error 为空。
- `LoadConfigWithDialog`：弹打开对话框，`UForeverModSubsystem::ReloadConfigFrom(所选文件)`，成功后立即 `WriteConfig` 覆盖 config.json(写绝对路径，所选文件里的相对路径按它自己所在目录解析)。文件无效时配置恢复为 config.json 并返回 error "配置文件无效或为空"。
- 游戏实际生效的配置仍固定为 config.json；`ValidateAndStartGame` 校验通过后照旧写回它。
- 菜单按钮 `SaveConfigButton` / `LoadConfigButton` 是 `BindWidgetOptional`，需在 `WBP_ConfigMenu` 里手工放置同名 Button。
