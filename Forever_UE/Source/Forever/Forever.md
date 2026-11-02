# Forever.h / Forever.cpp

## 职责

`Forever`游戏模块的入口：`FForeverGameModule`(`IMPLEMENT_PRIMARY_GAME_MODULE`)，在每个`World`初始化之前**关掉UE自带的导航系统**。

## 关键设计

- 这个项目所有导航(车行/行人寻路)都由Core的导航图实现，不用UE的`NavigationSystem`/NavMesh。`DefaultEngine.ini`里已把`NavigationSystemConfigClassName`设成`NullNavSysConfig`，但那只影响新建关卡——已有关卡(`Menu`/`World`/`Editor.umap`)的`WorldSettings`里序列化着一份`NavigationSystemModuleConfig`。
- 所以`StartupModule`里注册`FWorldDelegates::OnPreWorldInitialization`(`UWorld::InitWorld`里它先于`AddNavigationSystemToWorld`)，给`WorldSettings`挂一个`UNullNavSysConfig`覆盖(`RF_Transient`)。之后所有创建导航系统的路径(编辑器/PIE/游戏)读到的都是空配置：不创建导航系统、不维护导航八叉树、不会因为增删组件/实例去刷新导航数据。`WorldSettings`已有覆盖就不重复挂。
- `ShutdownModule`里注销委托。

## 依赖关系

- 依赖：UE `Modules`/`Engine`/`AI/NavigationSystemConfig`。
- 被谁依赖：UE模块系统。

## 待办/后续阶段

- 无。
