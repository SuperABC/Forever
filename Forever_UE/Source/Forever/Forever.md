# Forever.h / Forever.cpp

## 职责

`Forever`游戏模块的入口：`FForeverGameModule`(`IMPLEMENT_PRIMARY_GAME_MODULE`)，在每个`World`初始化之前**关掉UE自带的导航系统**。

## 关键设计

- 这个项目所有导航(车行/行人寻路)都由Core的导航图实现，不用UE的`NavigationSystem`/NavMesh。`DefaultEngine.ini`里已把`NavigationSystemConfigClassName`设成`NullNavSysConfig`，但那只影响新建关卡——已有关卡(`Menu`/`World`/`Editor.umap`)的`WorldSettings`里序列化着一份`NavigationSystemModuleConfig`。
- 所以`StartupModule`里注册`FWorldDelegates::OnPreWorldInitialization`(`UWorld::InitWorld`里它先于`AddNavigationSystemToWorld`)，给`WorldSettings`挂一个`UNullNavSysConfig`覆盖(`RF_Transient`)。之后所有创建导航系统的路径(编辑器/PIE/游戏)读到的都是空配置：不创建导航系统、不维护导航八叉树、不会因为增删组件/实例去刷新导航数据。`WorldSettings`已有覆盖就不重复挂。
- `ShutdownModule`里注销委托。

## Forever.Build.cs

- 公共依赖：`Core`/`CoreUObject`/`Engine`/`InputCore`/`EnhancedInput`/`UMG`/`ProceduralMeshComponent`/`Water`/`PhysicsCore`/`ChaosVehicles`(真实车辆物理，见`Element/VehicleElement.md`)/`Niagara`。私有依赖：`AIModule`/`NavigationSystem`(市民`AAIController`，脚本路径不依赖NavMesh)、`Slate`/`SlateCore`(`SectionSpeakingWidget::NativeOnKeyDown`直接用`FReply`/`FKeyEvent`，不链`SlateCore`会缺符号)、`DesktopPlatform`(`Mod/ForeverDialogUtils`的原生文件对话框)。
- 开了`bEnableExceptions`/`bUseRTTI`(与Core/Dependence链接的纯C++代码用到)。
- 模块根目录加进`PublicIncludePaths`：本模块按子文件夹(`Player/Input/...`)组织，没有Public/Private三段式，靠这个才能写`"Player/Xxx.h"`式跨目录include。
- 手动链接`x64/Release/Dependence.lib`/`Core.lib`，并把`Source/Dependence`、`Source/Core`、`Source/Core/common`加进include路径(`Basic`不链接，运行时作为dll被发现)。见`CONVENTIONS.md`。UE的任何构建配置(Debug/Development/Shipping)都链接Release的lib(`Forever.Build.cs`写死`x64/Release`)，因此`Framework.sln`必须先编`Release|x64`再编UE模块。

## 依赖关系

- 依赖：UE `Modules`/`Engine`/`AI/NavigationSystemConfig`。
- 被谁依赖：UE模块系统。

## 待办/后续阶段

- 无。
