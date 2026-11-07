# ForeverMenuController

## 职责
主菜单/前端场景用的PlayerController。`BeginPlay`创建主菜单Widget、加入视口、切到UI-only输入模式、显示鼠标。

## 关键设计
- `BeginPlay`先强制`CollectGarbage`再`ForeverMemoryReport::Log(..., "menu")`:每次进入菜单(开局或从游戏退回)打一行内存快照,反复进出几轮对比即可看出有没有泄漏,见`Framework/ForeverMemoryReport.md`。
- `fadeWidgetClass`(`WBP_Fade`,在`BP_ForeverMenuController`类默认值里指定)只登记给`UFadeSubsystem`:按下开始/加载按钮的那一刻就要变黑(`OpenLevel`要同步加载很久),所以菜单关卡也要登记;未设置只警告,不会立刻变黑。
- `CreateWidget`→`AddToViewport`→`UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(this, widget, EMouseLockMode::DoNotLock, false)`→`bShowMouseCursor = true`。
- `startMenuWidgetClass`(`TSubclassOf<UUserWidget>`,`EditDefaultsOnly`)不在构造函数里用`ConstructorHelpers::FClassFinder`填默认值,而是在`BP_ForeverMenuController`的类默认值里指定;`BeginPlay`里判空+警告日志,不会崩溃,只是不会弹出菜单。
- 不管理输入映射上下文(`AddMappingContext`等)——菜单场景不需要移动/视角这类玩法输入。

## 依赖关系
- 被`AForeverMenuGameMode`的`PlayerControllerClass`引用。
- 依赖`UMG`模块、`UFadeSubsystem`/`UFadeWidget`(`UI/`)、`ForeverMemoryReport`。

## 待办/后续阶段
- 无。
