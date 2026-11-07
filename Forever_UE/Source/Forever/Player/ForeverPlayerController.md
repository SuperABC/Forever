# ForeverPlayerController

## 职责
游玩关卡的PlayerController。常驻创建全部游玩期UI Widget(MeetOption对话选项/SectionSpeaking/SectionOption/BackgroundSpeaking剧情对话/Puzzle/Shop/Phone/Inventory/Map/PausePanel/ThrowList),承载不跟随被占有Pawn变化的全局热键(`P`开手机、`B`开背包、`M`开地图、`Esc`关地图、`Tab`开暂停界面、鼠标中键识别模式),并登记开局黑屏。

## 关键设计
- **初始化时机(`InitializeOnce`)**:输入模式重置、Mapping Context、全部常驻Widget的创建、黑屏登记都在幂等的`InitializeOnce()`里。打包版里`AForeverFrameworkActor::BeginPlay`(含game_start广播)先于Controller的`BeginPlay`,所以FrameworkActor在生成世界之前先调一次,`BeginPlay`里再调一次(已初始化就跳过)——否则game_start里的`start_puzzle`/对话拿到的Widget是空的被静默跳过,迟到的"输入重置成Game Only"还会盖掉谜题设的UI Only。
- **输入模式重置**:`InitializeOnce`显式`SetInputMode(FInputModeGameOnly())`+`bShowMouseCursor = false`——不能假设这是空白状态:主菜单关卡的`AForeverMenuController`会把输入模式设成UI Only+显示鼠标,这个状态挂在ViewportClient/LocalPlayer身上,不会因为旧Controller被`OpenLevel`销毁就自动还原。
- **全局热键放在Controller**:`UForeverKeyBindingSubsystem`的动态Mapping Context在这里常驻添加一次,热键在`SetupInputComponent`里绑定,按键走`Resource/Config/KeyBindings.ini`配置。玩家会通过`ChangeControlChange`换人操控、换乘载具,Controller是整局游戏唯一不随换Pawn而变的宿主,所以"任何时候都要生效"的热键(包括开局还操控着`ADefaultPawn`的那几帧)放在这里;Pawn侧(`AForeverCharacter`/`AVehicleElement`)只保留各自专属的移动/视角Context增删。
- **Widget创建**:统一`CreateWidget`+`AddToViewport`常驻创建;`xxxWidgetClass`(`EditDefaultsOnly`,在`BP_ForeverPlayerController`类默认值里指定)未配置时只跳过该Widget并打警告,不影响其余,不崩溃。显示/隐藏由各自业务驱动(SectionSpeaking/BackgroundSpeaking/SectionOption创建后手动`Hide*`,其余自己默认不显示)。`PausePanel`的ZOrder是10,`ThrowList`是5,其余默认,保证暂停界面盖在最上面。
- `TogglePhone`/`ToggleInventory`/`ToggleMap`/`HandleCloseMap`只是到对应Widget方法的一行转发。
- `TogglePause`:地图/手机/背包/谜题/商店/剧情对话/投掷物列表任何一个可见时不打开暂停界面(它们各自占着UI Only输入和键盘焦点);暂停界面打开后的关闭由`UPausePanelWidget`自己的`NativeOnKeyDown`处理,见`UI/PausePanelWidget.md`。
- **投掷物选择列表**(`throwListWidget`,长按3):由`AForeverCharacter`在长按时`Open`,见`UI/ThrowListWidget.md`。
- **开局黑屏**:控件由`UFadeSubsystem`持有(跨关卡),Controller只在`InitializeOnce`最后把`fadeWidgetClass`(`WBP_Fade`)登记给它并`EnsureBlack()`;没指定会警告。见`UI/FadeWidget.md`。
- **识别模式**(`identifyComponent`,按住鼠标中键):`Identify`热键的`Started`/`Completed`绑在这里转给`UForeverIdentifyComponent`(挂在Controller上)。Controller因此有构造函数(`CreateDefaultSubobject`)。见`ForeverIdentifyComponent.md`。
- **不在这个Controller的热键**:滚轮上下(`MeetOptionUp/Down`)+`F`(`MeetOptionSelect`)对话选项,由`AForeverCharacter::SetupPlayerInputComponent`转发给这里持有的`MeetOptionWidget`;`Q`下车是`AVehicleElement`自己的`ExitVehicle`。

## 依赖关系
- 被`AForeverPlayerGameMode`的`PlayerControllerClass`引用。
- 依赖`Input/ForeverKeyBindingSubsystem`(动态Action)、`UFadeSubsystem`、`UForeverIdentifyComponent`。
- 依赖`UI/`下各常驻Widget(`MeetOptionWidget`/`SectionSpeakingWidget`/`BackgroundSpeakingWidget`/`SectionOptionWidget`/`PuzzleWidget`/`ShopWidget`/`PhoneWidget`/`InventoryWidget`/`MapWidget`/`PausePanelWidget`/`ThrowListWidget`/`FadeWidget`)。
- 被`AForeverCharacter::MeetOptionFocusUp/Down/Select`(取`GetMeetOptionWidget()`)、`UForeverStoryFrameworkComponent`(取各对话/谜题/商店Widget)等依赖。

## 待办/后续阶段
- 无。
