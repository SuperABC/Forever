# ForeverMenuController

## 职责
主菜单/前端场景用的PlayerController。`BeginPlay`创建主菜单Widget、加入视口、切到UI-only输入模式、显示鼠标。

## 关键设计
- `CreateWidget`→`AddToViewport`→`UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(this, widget, EMouseLockMode::DoNotLock, false)`→`bShowMouseCursor = true`。
- `startMenuWidgetClass`(`TSubclassOf<UUserWidget>`,`EditDefaultsOnly`)没有在构造函数里用`ConstructorHelpers::FClassFinder`填默认值,因为对应的UMG Widget资产尚未建立;`BeginPlay`里做了`nullptr`判空+警告日志,不会崩溃,只是不会弹出菜单。
- 不管理输入映射上下文(`AddMappingContext`等)——菜单场景不需要移动/视角这类玩法输入。

## 依赖关系
- 被`AForeverMenuGameMode`的`PlayerControllerClass`引用。
- 依赖`UMG`模块(`Forever.Build.cs`已加入`PublicDependencyModuleNames`)。

## 待办/后续阶段
- UI就位后,需要在编辑器里把此类默认对象的`startMenuWidgetClass`指向对应Widget蓝图。
