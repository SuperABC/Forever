# ForeverIdentifyComponent

## 职责
识别模式：按住鼠标中键进入、松开退出。模式下，距离当前操控的 Pawn 一定范围内(默认 20m)的所有人物(包括被操控的角色)头上出现世界空间的文字标签，近大远小、始终正对摄像机。挂在 `AForeverPlayerController` 上，热键 `Identify`(默认鼠标中键)在 Controller 的 `SetupInputComponent` 里绑 `Started`/`Completed`——换人操控/上下载具时 Controller 不变，所以最稳。

## 关键设计
- **进入那一刻定下参与者和内容，之后不再更新**：`Enter()` 用 `UForeverPopulaceFrameworkComponent::ForEachActiveCitizen` 枚举当前已生成的人物，按 Pawn 位置(和人物流式生成同一个基准，所以范围内的人一定已经生成出来)筛出 `identifyRadius` 以内、未隐藏的，各调用一次 `BuildLines` 得到文字行，交给标签控件。模式期间不再查询，退出再进入才刷新。
- **`BuildLines(Citizen&)` 是唯一生成内容的地方**，目前只返回名字一行；以后加年龄/职业等只改这里。支持任意多行。
- **标签是挂在人物 Actor 上的 `UWidgetComponent`(世界空间)**：位置相对胶囊体顶端：胶囊半高 − `headBelowCapsuleTop`(头顶比胶囊顶端低约15cm) + `headMargin`(头顶上方留的缝，默认5cm)，不依赖动画和 socket；`SetPivot(0.5, 1.0)` 让控件底边中点对齐挂点，行数增加往上长，最后一行始终在头上方一点。人走动时标签跟着走；人被流式销毁时标签随 Actor 消失(弱引用持有，Tick 里清掉失效的)，不补建。开载具时角色被隐藏，标签随之隐藏。
- **描边粗细靠分辨率倍数调**：Slate 的描边宽度是整数像素(最小1)。`resolutionScale`把控件绘制尺寸和字号乘上去、世界缩放除下来，画面大小不变，字更精细、1像素的描边相对字形更细。渲染贴图面积按倍数平方增长，人多时别设太大。
- **始终正对摄像机**：Tick 里把所有标签的世界朝向设成 `MakeFromXZ(-相机前向, 相机上方向)`(平行于屏幕，不是"看向相机位置"——后者让屏幕边缘的标签倾斜)，只更新朝向。
- **文字用 UWidgetComponent 而不是 UTextRenderComponent**：后者默认字体没有 CJK 回退，人物名字是中文。
- 世界空间控件会被墙/物体挡住(深度测试)，隔墙看不到。
- **曝光**：项目白天曝光很重，世界里的 UI 亮度默认不够；`tintBrightness`(可 >1)乘在标签颜色上提亮。固定倍数在白天/夜里不能同时合适，如果白天仍然发暗，正确做法和爆炸特效一样：做一个小材质(`SlateUI` 贴图参数乘 Eye Adaptation Inverse)设给标签组件。
- 在 UI Only 面板(背包/地图等)打开时中键被屏蔽；如果按住中键时面板打开导致松开事件丢失，模式不会自动退出，再按一次中键松开即可。

## 依赖关系
- 依赖：`UForeverPopulaceFrameworkComponent::ForEachActiveCitizen`、`ACitizenElement::GetCitizen`、`UIdentifyTagWidget`、`APlayerCameraManager`。
- 被谁依赖：`AForeverPlayerController`(创建并转发热键)。

## 待办/后续
- 更多信息行(年龄、职业等)。
- 白天曝光材质(如果需要)。

## 名牌亮度跟着曝光走(2026-10-30)

昼夜曝光(见`Framework/ForeverSkyFrameworkComponent.md`"夜晚随游戏时间变暗")夜里固定在比白天亮得多的EV，固定的`tintBrightness=10000`会过曝。
现在`tintBrightness`是白天参考曝光(`referenceDayEV`)下的值，创建和识别期间每帧都乘`2^(当前EV-参考EV)`(`ComputeTintBrightness`)；拿不到天空组件时保持原值。
