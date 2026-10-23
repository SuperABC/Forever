# ForeverFadeWidget / ForeverFadeSubsystem（开局黑屏）

盖住整个画面的全黑遮罩：**按下开始/加载按钮的那一帧立刻变黑**，一直黑到World关卡世界就绪，再整体渐变透明并移除。
以后要在这个界面加加载进度条，布局直接在WBP里加即可。

## 为什么要有Subsystem

`OpenLevel(World)`会同步加载很久、期间画面不刷新；黑屏如果在World关卡里才创建，按下按钮后要等加载完才看到。所以：

- `UForeverFadeSubsystem`（GameInstanceSubsystem）持有控件，并用`GameViewportClient::AddViewportWidgetContent`
  挂到视口上——不属于任何关卡，Menu→World切换过程中一直盖着；
- `OpenWorldLevel()`先`ShowBlack()`，再用`FTSTicker`等3帧让黑屏真正渲染出来，才调`OpenLevel`。
  开始游戏（`UForeverConfigBridgeSubsystem::ValidateAndStartGame`）和读档（`UForeverSaveSubsystem::RequestLoad`，
  菜单/暂停面板共用）都走它；校验失败路径不会变黑。
- Menu/World两个Controller在`BeginPlay`里`RegisterWidgetClass`登记`fadeWidgetClass`（类在各自BP里指定）；
  World的Controller还会`EnsureBlack()`——编辑器里直接PIE在World.umap上（没经过按钮）时补一个黑屏。

## 就绪条件（新游戏、读档共用）

`AForeverFrameworkActor::IsInitialGenerationDone()` **且** `UForeverBuildingFrameworkComponent::IsLodSettled()`。

- 新游戏：`BeginPlay`里整段生成流程走完，意味着`EnsureStoryGenerated`里的`BroadcastGameStart`已经同步
  响应完所有game_start（包括`change_control`占有市民）；
- 读档：`RestoreControlledPawn`/`RestoreWorldAssets`走完，人物已恢复；
- 两种情况占有市民时都调了`RequestFreezeUntilLodSettled()`，`IsLodSettled()`为真即附近建筑LOD已建完。
- 脚本里的对话/定时器是异步的，不算"响应完"，不会拖住黑屏。
- 兜底：进入World后真实时间等待超过`maxWaitSeconds`（默认15s）也开始渐隐，避免没有可操控市民、LOD冻结
  从未请求时永远黑屏。控件感知World更换会重新计时，关卡加载时间不计入。

渐隐用`NativeTick`的`InDeltaTime`（真实时间）——等待期间世界被`SetGlobalTimeDilation(0)`冻结。

## 蓝图约定

C++只写逻辑，外观在`WBP_Fade`（父类`UForeverFadeWidget`）里设计：

1. 放一个全屏、纯黑的Border/Image（锚点铺满Canvas）。无`BindWidget`依赖，淡出作用在控件根的`RenderOpacity`上。
2. 在**`BP_ForeverMenuController`和`BP_ForeverPlayerController`两个**类默认值的`fadeWidgetClass`里都指定`WBP_Fade`
   （没指定只打警告；两个指定不同的类时以后登记的为准，旧控件作废）。
3. 类默认值可调：`fadeDuration`（渐隐秒数，默认1.5）、`maxWaitSeconds`（兜底，默认15）。

控件`HitTestInvisible`，不拦截输入；ZOrder 100（高于PausePanel的10，低于背包拖拽ghost的1000）。
