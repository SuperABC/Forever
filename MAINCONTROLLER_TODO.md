# MainController 迁移待办

## 背景

旧蓝图`Blueprint/Player/MainController`的dump文件(`.dump/MainController.txt`,1334行)已经拿到,但内容分析后发现它不是"视角/输入基础"范畴的东西,而是**覆盖多个后续系统的全局热键中枢**——绝大部分节点调用的函数/Widget在当前新工程里都还不存在。阶段1(视角与输入基础)决定完全跳过这个文件的迁移,只把`AForeverPlayerController`留空壳(见`Forever_UE/Source/Forever/Player/ForeverPlayerController.md`),这份文档记录热键清单和各自的依赖,留给阶段4/5对应系统就绪后再回来实现,避免届时要重新打开dump文件、从头分析一遍。

`.dump/MainController.txt`本身不要删除,后续实现时仍需要打开它逐条核对具体连线(下面的清单是按函数调用和节点顺序推断的功能归属,**没有逐条追踪pin连线**,精确的执行顺序/分支条件要在实现时重新确认)。

## 热键清单

| 按键 | 推断功能 | 涉及函数/资源 | 阻塞依赖 |
|---|---|---|---|
| `Tab` | 暂停菜单开关 | `GlobalBase::GlobalPause`、`PausePanel`(UMG,`UpdateGuide`函数)、`bShowMouseCursor`、`SetInputMode_UIOnlyEx` | `GlobalBase`内核接口(阶段4)、`PausePanel` UI(阶段5) |
| `M` | 地图面板开关 | `GlobalBase::DrawMap`、`Forever.CanvasBuffer`(`InitCanvas`/`ApplyImage`,UE模块层类,不是内核) | `GlobalBase::DrawMap`(阶段4 Map系统)、`CanvasBuffer`(需要新建,UI相关) |
| `P` | 打开手机(Phone)界面 | `GlobalBase::InitPhone`、`/Game/Blueprint/UI/PhoneFrame` Widget、`CanvasBuffer` | `GlobalBase::InitPhone`(阶段4)、`PhoneFrame` UI(阶段5) |
| `Q` | 退出载具 + 刷新导航调试贴图 | `Traffic_C::QuitVehicle`(Framework Traffic Actor)、自身`VisualizeNavigation`函数、`CanvasBuffer::GetTexture`/`ApplyImage` | `Traffic` Framework Actor(阶段2骨架+阶段4 Traffic系统)、下面的`VisualizeNavigation` |
| `B` | 背包(Bag)面板开关 | `GlobalBase::GlobalPause`、`BagPanel::InitPanel`(UMG) | `GlobalBase`(阶段4)、`BagPanel` UI(阶段5) |
| `N` | 切换导航可视化调试 | 自身`VisualizeNavigation`函数(见下) | 同`VisualizeNavigation` |
| `MouseScrollUp`/`MouseScrollDown` | 对话选项(`MeetOption`)焦点上/下移 | **已实现**：`UMeetOptionWidget::FocusUp`/`FocusDown`，`AForeverCharacter::MeetOptionFocusUp`/`MeetOptionFocusDown`转发 | 无 |
| `F` | 对话选项确认选中 | **已实现**：`UMeetOptionWidget::ClickFocus`，`AForeverCharacter::MeetOptionSelect`转发 | 无 |

## 其余函数(非按键直接触发,是Function/事件)

- **`ScriptMessage`**(蓝图自定义函数,BlueprintCallable,供外部调用):内部调用`StoryBase::ScriptMessage`,先`GetActorOfClass`拿到Story相关Actor。依赖`StoryBase`内核接口(阶段4 Story系统)。
- **`VisualizeNavigation`**(蓝图自定义函数):一段较复杂的debug可视化逻辑——调用`RoadnetBase::GetNavigations()`拿导航路径点,做向量数学(点乘/叉乘/夹角/法线等),按`SwitchString`分支,`SpawnActorFromClass`生成多个箭头/标记网格(`SetStaticMesh`+`SetMobility`)沿路径摆放,最后写入`CanvasBuffer`纹理。依赖`RoadnetBase`内核接口(阶段4 Map/Traffic系统)和多个占位网格资产。这是一个开发调试工具,不是玩法核心,优先级可以放低。

## 需要的支撑类/系统一览

- Core内核:`GlobalBase`(GlobalPause/DrawMap/InitPhone)、`StoryBase`(ScriptMessage)、`RoadnetBase`(GetNavigations)——均属阶段4范围。
- Framework Actor:`Traffic`(QuitVehicle)——阶段2搭骨架、阶段4填Traffic系统逻辑。
- UE模块层新类:`CanvasBuffer`(把内核画的图/数据转成UMG可显示的纹理,`InitCanvas`/`ApplyImage`/`GetTexture`),当前完全不存在,需要新建。
- UMG Widget:`PausePanel`、`BagPanel`、`PhoneFrame`、`MeetOption`——均属阶段5 UI迁移范围。

## 建议

等到阶段4对应系统(Map/Traffic/Story等)和阶段5对应UI都迁移完成后,再回来把这份清单里的热键一条条加回`AForeverPlayerController`(或者按`REFACTOR_PLAN.md`阶段2的Actor收敛设计,加到对应子系统里),按键本身应该走阶段1已经搭好的`UForeverKeyBindingSubsystem`配置化框架,而不是重新写死。
