# CoreGuard.h / CoreGuard.cpp

## 职责

Core/mod抛出的C++异常在UE这一层的"会话边界"：保证异常不会穿出Forever模块进到UE引擎的栈帧里，并且在接住之后把这一局
干净地结束——回到菜单，把原因显示出来。异常体系本身（三类异常、`ErrorScope`、`ErrorReport`）见
`Source/Dependence/common/error.md`。

## 关键设计

### 为什么必须在Forever模块里接住

UE引擎自己的代码编译时没开异常。异常一旦穿进引擎的栈帧，那些栈帧的清理代码不会执行（锁不释放、计数不回退），引擎状态就坏了；
在编辑器里是整个编辑器崩。所以每个"引擎直接调进来、里面会调Core"的函数，都要在自己的函数体里把异常接住。

### 接住之后：这一局结束

Core里大量用裸`new`/`delete`，异常从中间抛出时世界停在半更新状态（比如账本记了一半），不能再Tick。所以：

1. `FailSession`把这一局标记为失败（只有第一次生效），打一条Error日志（类别`LogForeverGuard`，内容是
   `ErrorReport::ToText()`：类别、消息、抛出位置、当时在哪个阶段/mod/文件）。
2. 之后所有`CoreGuard::Run`/`CORE_GUARD_BEGIN`看到失败标记直接跳过，不再碰世界。
3. 回到游戏线程后（`AsyncTask(GameThread)`，所以线程池里接到的异常也走这条路）：撤掉黑屏遮罩、`OpenLevel(Menu)`。
   关卡销毁时`AForeverFrameworkActor::EndPlay`照常释放Core各域、卸载mod dll。
4. 菜单的`UConfigMenuWidget::NativeConstruct`用`ConsumePendingMessage()`取走错误文字，显示在`FailureMessage`上。

想要"跳过这一步继续玩"，不能靠这一层：要在更靠里的地方用`GuardData`接（一条脚本指令、一个里程碑的触发条件），见`error.md`。

### 入口

- `CoreGuard::Run(where, lambda)`：跑一段会调Core的代码。
- `CORE_GUARD_BEGIN("Class::Func")` … `CORE_GUARD_END`：把一个返回`void`的函数体整个包起来，不用改成lambda，里面的`return`
  照常用。
- `CoreGuard::RunAlways` / `CORE_GUARD_ALWAYS_BEGIN`：这一局已经失败也照样跑。收尾函数用（`EndPlay`、`NativeDestruct`）。
- **可以嵌套，只有最外层接异常**（线程局部的深度计数）。里层如果接了，外层会带着半更新的世界把剩下的代码跑完；现在里层
  发现自己不是最外层就原样往外抛，异常一直展开到最靠近引擎的那一层。
- 父类调用（`Super::Tick`、`Super::BeginPlay`……）放在保护**外面**：引擎要求它们必须被调到。

### 现在包了哪些地方

- `AForeverFrameworkActor`的`BeginPlay`、`Tick`、`EndPlay`。世界生成的各个阶段、时间步进、跳时间都在`Tick`里。
- 所有`#include`了Core头文件的.cpp里的`Tick`/`TickComponent`/`NativeTick`（Controller、Element、各Framework组件、
  Player组件、几个Widget）。
- `UForeverStoryFrameworkComponent`的全部`void`函数：脚本事件的总入口，UI点选项、输入、触发盒最终都从这里进脚本。
- `UPuzzleWidget`、`UPhoneWidget`的全部`void`函数：它们每帧直接跑mod的界面代码。
- 线程池里的行人寻路任务（`UForeverPopulaceFrameworkComponent`）。
- 菜单里的几个操作不走"这一局结束"，而是用函数级try块把异常变成一条错误提示：`UForeverSaveSubsystem::SaveToFile`/
  `RequestLoad`、`UForeverConfigBridgeSubsystem::ValidateAndStartGame`/`SaveConfigWithDialog`/`LoadConfigWithDialog`。

### 没有逐个包的地方

输入回调、UI按钮回调、碰撞回调、定时器回调没有逐个包：它们进脚本都经过上面的`UForeverStoryFrameworkComponent`，直接调的
Core接口基本是不抛异常的取值函数。**新写一个引擎直接调用的函数、里面又调了可能抛异常的Core接口（脚本、表达式、layout、
mod）时，要自己加上`CORE_GUARD_BEGIN`/`CORE_GUARD_END`。**

### 接不住的

空指针解引用、除零、栈溢出不是C++异常，这套东西接不住，也不打算接。

### 静态量

失败标记、待显示的文字、指向本局`AForeverFrameworkActor`的弱引用是模块级静态量，活得比关卡久——所以只存文字和弱引用，
不存任何Core/mod指针（`CONVENTIONS.md`"mod dll的生命周期"）。

## 依赖关系

- 依赖：`common/error.h`、`UFadeSubsystem::Dismiss`、`AForeverFrameworkActor`（只用`GetWorld`）。
- 被谁依赖：上面"现在包了哪些地方"列的所有文件；`UConfigMenuWidget`（显示错误文字）。
