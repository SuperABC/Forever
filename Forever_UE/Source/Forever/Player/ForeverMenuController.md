# ForeverMenuController

## 职责
主菜单/前端场景用的PlayerController。对应旧蓝图`Blueprint/Player/MenuController`(dump确认整个EventGraph只有`ReceiveBeginPlay`一条链路,没有其他逻辑)。`BeginPlay`创建主菜单Widget、加入视口、切到UI-only输入模式、显示鼠标。

## 关键设计
- 完全照旧蓝图dump迁移:`CreateWidget`→`AddToViewport`→`UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(this, widget, EMouseLockMode::DoNotLock, false)`→`bShowMouseCursor = true`。
- `startMenuWidgetClass`(`TSubclassOf<UUserWidget>`,`EditDefaultsOnly`)**故意不在构造函数里用`ConstructorHelpers::FClassFinder`填默认值**,因为旧蓝图引用的`/Game/Blueprint/UI/StartMenu.StartMenu_C`这个UMG Widget属于阶段5(UI迁移)范围,新工程里还不存在。`BeginPlay`里做了`nullptr`判空+警告日志,不会崩溃,只是不会弹出菜单。
- 没有像`ForeverCharacter`那样管理输入映射上下文(`AddMappingContext`等)——dump里`MenuController`完全没有涉及Enhanced Input,菜单场景不需要移动/视角这类玩法输入。

## 依赖关系
- 被`AForeverMenuGameMode`的`PlayerControllerClass`引用。
- 依赖`UMG`模块(`Forever.Build.cs`已加入`PublicDependencyModuleNames`)。

## 待办/后续阶段
- 阶段5:`Blueprint/UI/StartMenu`迁移为C++驱动的UMG Widget后,需要在编辑器里把此类默认对象的`startMenuWidgetClass`指向新Widget蓝图。
