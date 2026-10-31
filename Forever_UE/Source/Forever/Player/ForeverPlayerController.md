# ForeverPlayerController

## 职责
第三人称玩家的PlayerController。承载一批常驻UI Widget(MeetOption对话选项/剧情对话SectionSpeaking/SectionOption/Puzzle/Phone/Inventory/Map/PausePanel)和不跟随被占有Pawn变化的全局热键(`P`开手机、`B`开背包、`M`开地图、`Esc`关地图、`Tab`开暂停界面)。

## 关键设计
- `BeginPlay`里显式`SetInputMode(FInputModeGameOnly())`+`bShowMouseCursor = false`——不能假设这是空白状态:主菜单关卡的`AForeverMenuController::BeginPlay`会把输入模式设成UI Only+显示鼠标,这个状态挂在ViewportClient/LocalPlayer身上,不会因为旧Controller被`OpenLevel`销毁就自动还原,必须显式重置。
- `UForeverKeyBindingSubsystem`的动态Mapping Context在这里(`BeginPlay`)常驻添加一次,不放在Pawn的`PossessedBy`里——`TogglePhone`等热键要求任何时候都生效(包括开局还操控着`ADefaultPawn`、以及`ChangeControlChange`换人/换乘载具的瞬间),Controller是整局游戏唯一不随换Pawn而变的宿主。Pawn侧(`AForeverCharacter`/`AVehicleElement`)只保留各自专属的移动/视角Context增删。
- 8个Widget(MeetOption/SectionSpeaking/SectionOption/Puzzle/Phone/Inventory/Map/PausePanel)在`BeginPlay`里统一用`CreateWidget`+`AddToViewport`常驻创建,`xxxWidgetClass`未配置时跳过该Widget的创建并打警告日志,不影响其余Widget,也不会崩溃。创建后的显示/隐藏由各自的业务逻辑驱动(SectionSpeaking/SectionOption初始手动Collapse;其余几个自己在构造/`NativeConstruct`里默认不显示)。
- `TogglePhone`/`ToggleInventory`/`ToggleMap`/`HandleCloseMap`只是到对应Widget方法的一行转发。
- `TogglePause`:地图/手机/背包/谜题/剧情对话任何一个可见时不打开暂停界面(它们各自占着UI Only输入和键盘焦点);暂停界面打开后的关闭由`UPausePanelWidget`自己的`NativeOnKeyDown`处理,见`UI/PausePanelWidget.md`。

## 旧MainController热键

旧蓝图`Blueprint/Player/MainController`(dump文件`.dump/MainController.txt`)的热键已经全部迁移。按键都走`UForeverKeyBindingSubsystem`的配置(`Resource/Config/KeyBindings.ini`)，不写死：
- `P`开手机、`B`开背包、`M`开地图、`Esc`关地图、`Tab`开暂停界面：绑在这个Controller上(`SetupInputComponent`)。玩家会通过`ChangeControlChange`换人操控、换乘载具，Controller是整局游戏里唯一不随换Pawn而变的宿主，所以这些"任何时候都要生效"的热键放在这里，动态Context也在`BeginPlay`里常驻添加。
- 滚轮上下(`MeetOptionUp/Down`)+`F`(`MeetOptionSelect`)：对话选项，由`AForeverCharacter::SetupPlayerInputComponent`转发给这个Controller持有的`MeetOptionWidget`。
- `Q`下车：`AVehicleElement`自己的`ExitVehicle`。
- `N`(导航调试可视化)：旧蓝图里是手动开关，新实现改成`UForeverRoadnetFrameworkComponent`常驻生成的调试网格，不再需要按键。

- **投掷物选择列表**(`throwListWidget`，长按3)：跟其它常驻Widget同一套创建模式(ZOrder 5)，`throwListWidgetClass`在`BP_ForeverPlayerController`里指定为`WBP_ThrowList`，没指定会警告并跳过(长按3不弹列表)。`TogglePause`在列表打开时拒绝(和地图/手机/背包等同样的理由)。见`UI/ThrowListWidget.md`。
- **初始化时机(`InitializeOnce`)**：输入模式重置、Mapping Context、全部常驻Widget的创建都在幂等的`InitializeOnce()`里。打包版里`AForeverFrameworkActor::BeginPlay`(含game_start广播)先于Controller的`BeginPlay`，所以FrameworkActor在生成世界之前先调一次，`BeginPlay`里再调一次(已初始化就跳过)——否则game_start里的`start_puzzle`/对话拿到的Widget是空的被静默跳过，Controller迟到的"输入重置成Game Only"还会盖掉谜题设的UI Only(2026-10-24发现：wxdj开局谜题不弹)。
- **开局黑屏**：控件由`UFadeSubsystem`持有(跨关卡)，Controller只在`BeginPlay`最后把`fadeWidgetClass`(在`BP_ForeverPlayerController`里指定为`WBP_Fade`)登记给它并`EnsureBlack()`；没指定会警告。见`UI/FadeWidget.md`。

- **识别模式**(`identifyComponent`，按住鼠标中键)：`Identify`热键的`Started`/`Completed`绑在这里转给`UForeverIdentifyComponent`(挂在Controller上)，换人操控/上下载具时Controller不变，所以最稳。Controller因此有了构造函数(`CreateDefaultSubobject`)。见`Player/ForeverIdentifyComponent.md`。

## 依赖关系
- 被`AForeverPlayerGameMode`的`PlayerControllerClass`引用。
- 依赖`Input/ForeverKeyBindingSubsystem`(`TogglePhone`/`ToggleInventory`/`ToggleMap`/`CloseMap`动态Action)。
- 依赖`UI/{MeetOptionWidget,SectionSpeakingWidget,SectionOptionWidget,PuzzleWidget,PhoneWidget,InventoryWidget,MapWidget,PausePanelWidget}`。
- 被`AForeverCharacter::MeetOptionFocusUp/Down/Select`(取`GetMeetOptionWidget()`)、`UForeverStoryFrameworkComponent::ShowCurrentItem`(取`GetSectionSpeakingWidget()`/`GetSectionOptionWidget()`)等依赖。

## 待办/后续阶段
- 无(旧`MainController`热键已全部迁移,见上面"旧MainController热键"一节)。
