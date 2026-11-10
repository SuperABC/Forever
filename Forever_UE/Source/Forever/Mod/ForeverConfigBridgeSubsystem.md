# ForeverConfigBridgeSubsystem

## 职责

游戏启动配置界面(Story/Mod/Resource三screen)到`Config`静态方法的Blueprint桥接层(`UGameInstanceSubsystem`)。绝大部分方法是`FString`<->`std::string`的薄包装；业务逻辑只有依赖闭包计算、`ValidateAndStartGame`的聚合校验和配置存取三处。

## 关键设计

- **做成`UGameInstanceSubsystem`而不是Actor**：不用在Menu关卡里放置Actor，且`OpenLevel`不会销毁它(`ValidateAndStartGame`在`EnsureModsRegistered()`和切关卡前后都要存活)，和`UForeverModSubsystem`同一套模式。
- **依赖闭包`ComputeRequiredModIds()`**：当前"必需启用"的mod id集合 = 全部Story screen脚本的`mod_dependences`(无条件计入，Story脚本没有单独的启用开关) ∪ 每个**已启用**mod id自己声明的`Config::GetModDependences`(按modId粒度)。key=被依赖的mod id，value=依赖方(mod id或`剧情脚本 <文件名>`，去重)。`GetModRows`据此给每行填`bLocked`/`Dependents`(锁定行整行变灰且不能取消勾选；未启用但被依赖的行标红)，`IsModLocked`直接查它。锁定判定**不看concept**，只认mod id字符串，和`Registry::CheckModRegistered`同一套匹配规则，所以`IsModLocked`的`ConceptKey`参数不影响结果。
- **`ValidateAndStartGame`**：先`Config::RescanFiles()`刷新资源目录里的`.layout`/`.script`缓存；再校验Story脚本依赖和每个已启用mod的依赖是否都在启用集合里(`Config::IsModIdEnabled`)，失败原因拼进`OutFailureMessage`，**失败时不改任何状态**；全部通过才`WriteConfig`写回`config.json`(只在这一刻保存，中途编辑不自动保存)、`UForeverModSubsystem::EnsureModsRegistered()`、再经`UFadeSubsystem::OpenWorldLevel`先变黑再`OpenLevel(World)`(没有FadeSubsystem时直接`OpenLevel`)。
- **配置存取**：
  - `SaveConfigWithDialog`：弹保存对话框，把当前内存配置`WriteConfig`到所选json，不校验依赖，不动`config.json`。取消时返回false且error为空。
  - `LoadConfigWithDialog`：弹打开对话框，`UForeverModSubsystem::ReloadConfigFrom(所选文件)`，成功后立即`WriteConfig`覆盖`config.json`(写绝对路径，所选文件里的相对路径按它自己所在目录解析)。文件无效时配置恢复为`config.json`并返回error"配置文件无效或为空"。成功后调用方需刷新三个面板。
  - 游戏实际生效的配置固定是`config.json`。
- `SelectFolder`/`SelectFile`：原生文件夹/文件选择器(`IDesktopPlatform`，父窗口见`ForeverDialogUtils`)；返回false表示用户取消，`OutPath`不变。
- 菜单按钮`SaveConfigButton`/`LoadConfigButton`是`BindWidgetOptional`，需在`WBP_ConfigMenu`里手工放置同名Button。

## 依赖关系

- 依赖：`Config`(`Source/Core/common/config.h`)、`Script::GetModDependences`、`UForeverModSubsystem`、`UFadeSubsystem`、`ForeverDialogUtils`。
- 被谁依赖：配置界面三个screen的Blueprint/Widget。

## 待办/后续阶段

- `IsModLocked`是蓝图可调用接口，每次调用都重算整个依赖闭包(`ComputeRequiredModIds`)；若蓝图界面逐行调用，行数多时可把闭包缓存成员并在模组列表变化/刷新处失效(C++侧调用方只有`GetModRows`，已只算一次)。

## 路网/取名必选校验
`ValidateAndStartGame`在依赖校验之后检查`Roadnets`/`Names`各自启用数恰好为1（0个或多个都加进失败信息），menu输出框显示并拦截开局。Mod行UI里已选中的单选项勾选框禁用，只能选别的切换（`UModCheckRowWidget`）。

## 主线剧情mod
`GetEnabledScriptMods()`/`GetMainStory()`/`SetMainStory(id)`：main_story只能是已启用的script mod。`ValidateAndStartGame`校验main_story非空且在script_mods里，否则在menu输出框报错。

`GetEnabledScriptMods`只返回当前dll提供的已启用script mod；`GetMainStory`在所选mod被取消启用或不在当前dll里时返回空，所以开局报错是“没有选择主线剧情mod”而不是“未启用”。
