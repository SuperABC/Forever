# ForeverKeyBindingSubsystem

## 职责
把单键触发的输入动作的按键绑定从C++/资源里解放出来,改为读取外部文本配置文件。覆盖跳跃/视角切换/冲刺/载具手刹&下车/MeetOption对话选项/武器/手机/背包/地图等全部单键动作。

## 关键设计
- 只覆盖**数字单键动作**,不覆盖Move/Look这类模拟量+多按键组合映射(WASD四向、鼠标XY)——这类映射的Modifier/Trigger设置复杂,继续放在`IMC_Default`/`IMC_MouseLook`资源里由编辑器authored。没有做交互式设置UI。
- 动作(`UInputAction`)和映射(`UInputMappingContext`)都是`NewObject`出来的**运行时对象**,不对应内容浏览器里的任何`.uasset`。好处是新增/修改按键不需要碰编辑器资源,坏处是这些动作在编辑器里不可见、不能被蓝图或Details面板直接引用,只能通过`GetAction(FName)`按名字取。`GetKey(FName)`返回某个绑定读配置后实际生效的按键,给UI Only模式下自己处理按键的Widget用。
- 配置文件路径是`Forever_UE/Resource/Config/KeyBindings.ini`,不在UE标准`Project/Config`目录下,用`FConfigFile::Read`直接读文件内容,不走`GConfig`全局单例注册。
- 键名解析用`FKey(FName(keyNameString))` + `IsValid()`校验;缺配置项或键名无效都静默回退到硬编码默认键并打`Warning`日志。
- `UInputMappingContext::MapKey`每次调用都是"新增一条映射"而不是"查找并更新已有映射",所以在全新构造的`bindingContext`上直接为每个动作调用一次即可。
- 同一个物理键可以映射到多个不同的`UInputAction`而不冲突(如`Jump`和`Handbrake`都默认绑空格键,分别只在`AForeverCharacter`/`AVehicleElement`各自的`SetupPlayerInputComponent`里绑定消费)。

- `ThrowItem`(默认数字键3)：投掷物。动态Boolean Action没有Hold触发器，短按/长按由`AForeverCharacter`自己判断：`Started`记真实时间，`Tick`里按住超过`longPressThreshold`就弹出选择列表(长按)，没到就松开算短按切换(松开才触发，有一点延迟)。
  在`Started`记时间、`Completed`比较`longPressThreshold`区分(短按松开时才触发，有一点延迟)。

- `Identify`(默认鼠标中键)：识别模式，按住进入、松开退出，绑在`AForeverPlayerController`上，见`Player/ForeverIdentifyComponent.md`。

## 依赖关系
- 依赖:`Forever_UE/Resource/Config/KeyBindings.ini`(找不到该文件时全部用硬编码默认值,不报错)。
- 被谁依赖:
  - `AForeverCharacter::SetupPlayerInputComponent`(`Jump`/`ToggleView`/`Sprint`/`MeetOptionUp`/`MeetOptionDown`/`MeetOptionSelect`/`FireWeapon`/`ReloadWeapon`/`SwitchWeapon1`/`SwitchWeapon2`/`AimWeapon`)。
  - `AForeverPlayerController::BeginPlay`/`SetupInputComponent`(整份`GetBindingContext()`常驻加入Enhanced Input子系统;`TogglePhone`/`ToggleInventory`/`ToggleMap`/`CloseMap`/`TogglePause`绑定,见`Player/ForeverPlayerController.md`)。
  - `UForeverPausePanelWidget::NativeOnKeyDown`(用`GetKey("TogglePause")`取配置后的实际按键——UI Only模式下Enhanced Input不触发,只能自己比对按键)。
  - `AVehicleElement::SetupPlayerInputComponent`(`Handbrake`/`ExitVehicle`)。
  - `AAirplaneElement::SetupPlayerInputComponent`(`ThrottleUp`/`ThrottleDown`/`FreeLook`=C)。
- Mod可扩展点:"可绑定动作"注册表(`GetBindingDefinitions()`)是硬编码在本类里的静态列表,新增数字按键动作时在这里加一条`{名字, 默认键}`即可,不需要改配置文件解析逻辑。

## 待办/后续阶段
- 若后续需要让玩家在游戏内改键并持久化,需要在这个类基础上加"运行时改键+写回配置文件"的能力,当前只有"启动时读取一次"。
- Move/Look的模拟量组合按键配置化,留待有实际需求时再设计。

## ThrottleUp / ThrottleDown

飞机动力：默认`MouseScrollUp/MouseScrollDown`，和`MeetOptionUp/Down`共用滚轮键。只在`AAirplaneElement::SetupPlayerInputComponent`上绑定(`Started`，每格一次，加减`powerStep`)，别的Pawn不绑，所以不冲突，见`Forever/Element/AirplaneElement.md`。
