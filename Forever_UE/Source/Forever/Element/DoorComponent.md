# DoorComponent.h / .cpp

## 职责

`UForeverDoorComponent`：一扇真正放了门扇的门在场景里的渲染+交互表现，绑定一个Core端
`Door*`。挂在框架Actor上，由`UForeverDoorFrameworkComponent`按玩家距离流式生成/销毁（建筑门/房间门/园区门一视同仁，和建筑LOD无关，见`ForeverDoorFrameworkComponent.md`）。负责：骨骼门网格生成、挡板、触发盒、开合度插值并喂给动画蓝图、身份判定+门禁拒绝、
MeetOption交互选项。

## 关键设计

### 坐标约定：组件自身用绝对世界坐标摆位，和父级Actor的本地坐标系无关

`Init()`里直接`SetWorldLocation`/`SetWorldRotation`，不是相对父级Attach点算offset——
`Door`的几何(`GetX/Y/Z/GetYaw()`)本来就是Core算好的绝对地图坐标(乘`DOOR_WORLD_SCALE`
换算成UE单位)，不需要关心挂在哪个Actor下面、那个Actor自己的Transform是什么。
`Door::GetYaw()`是墙体**外法线**方向，这个组件自己的`local X`要对齐资产约定的"沿墙"
方向，和法线正好相差90度——`componentYawDegrees = RadiansToDegrees(door->GetYaw()) -
90`，不是直接拿GetYaw()当组件朝向用。

### 门是骨骼网格，Core/组件只给一个"开合度"

`DoorSpec.mesh`是**骨骼网格**软路径（门框和门扇在同一个资产里，会动的部分绑在骨上），`DoorSpec.animClass`是动画蓝图类（父类必须是`UDoorAnimInstance`）。组件自己的职责只有：算`openProgress`(0=全关,1=全开，按`openSeconds`线性插值)，每帧写进动画实例的`OpenProgress`/`OpenSide`，**模型怎么动完全由动画蓝图决定**（AnimGraph里用Transform (Modify) Bone或Sequence Evaluator读`OpenProgress`）。组件里没有任何Slide/Swing/铰链代码；`animClass`为空或加载失败回退到`UDoorAnimInstance`基类，门不动（日志告警）。动画实例不是`UDoorAnimInstance`（蓝图父类选错）时只告警一次。

### 摆位：X、Y等比缩放，Z独立缩放

`BuildSkeletalDoor`按`USkeletalMesh::GetBounds()`（ref-pose包围盒）：X、Y用同一个系数`门宽/包围盒宽`（门厚跟着门宽等比，所以绕Z的平开旋转不会被拉变形），Z单独缩放到门高；包围盒中心落在组件本地`(0,0,门高/2)`，不管资产pivot在哪。代价：很宽的门（园区车行门）门厚也会按比例变厚。网格自己`NoCollision`。

### 挡板（blocker）

骨骼网格不带碰撞，门关着挡人靠一个`UBoxComponent`挡板（宽×`DOOR_BLOCKER_THICKNESS`(10cm，和墙同厚)×高，`BlockAllDynamic`，隐藏）。`openProgress < 0.5`时开启，之后关闭，只在状态变化时切换`SetCollisionEnabled`。

### 门永远往远离触发者的一侧开（`OpenSide`）

组件本地+Y就是墙法线方向（local X=沿墙）。门**从全关状态**被一个`CanPass()`通过的Actor触发时（`!bWantsOpen && openProgress≈0`），`ChooseOpenSide`把它的位置变换到组件本地坐标：在+Y一侧`OpenSide=-1`，反之`+1`，写进动画实例的`OpenSide`。平开门的动画蓝图可以用它决定转动方向（从房间外靠近就往房间里推开），推拉门忽略即可。门已经开着、或者正在关但还没关严时不改，避免门在半空中反向翻转。

### 身份判定：`ResolvePasserIdentity`

- 玩家当前pawn(不管是初始角色还是`ChangeControlChange`换过去的citizen) → `isPlayer=true`
  （**判断顺序很重要**：先比较`actor==GetPlayerPawn()`，再才尝试`Cast<ACitizenElement>`——
  被玩家占有的citizen应该按`isPlayer`走，不是按普通citizen身份走）。
- `ACitizenElement`（非玩家占有）→ `GetCitizen()`。
- `AVehicleElement`：`IsPlayerControlled()`为true→`isPlayer=true`；否则读
  `GetPreviousPawn()`转`ACitizenElement`拿司机身份；空车(`GetPreviousPawn()`为空，
  从没被开过)→`(nullptr,false)`，只有`Open`的门会给它开。
- 其余(公交载具等) → `(nullptr,false)`。

### 开关状态：`passers`集合+延迟关门

进入触发盒且`Door::CanPass()`通过的Actor才加进`passers`；非空就开，清空后
门是流式生成的，生成时玩家/市民/载具可能已在盒内：生成后静默刷新重叠记录，只对盒内的Pawn手动补一次BeginOverlap（不能让静态物体也触发，否则门全开）。
`DOOR_CLOSE_DELAY_SECONDS`(1秒)再关，`TickComponent`只在动画进行中/有未完成的开关请求时
跑；全关且没有待处理请求时多Tick一帧让动画实例评估一次0，随后`bPauseAnims=true`并`SetComponentTickEnabled(false)`省性能，下次`UpdateOpenState`唤醒。门禁拒绝通行的Actor
**不会**被加进`passers`(门不会为它开)，但如果这扇门有`Script`，MeetOption选项照常弹出
给靠近的玩家——拒绝通行和拒绝对话是两件独立的事。

- **交互选项运行中变化**：`add_option`/`remove_option`执行后，`AForeverFrameworkActor::RefreshInteractOptions`按名字找到场景里的这个对象调`RefreshOptions()`：重新烘焙`cachedOptions`快照；玩家正站在旁边时用`UMeetOptionWidget::ReplaceOptions`把屏幕上的选项整组换掉。Overlap回调仍然只读快照、不解引用Core对象。

- **`FindForDoor`**：静态登记表`componentsByDoor`(Door* -> 组件)，`Init`时登记、`OnComponentDestroyed`时移除；门组件按距离流式创建/销毁，没生成时查不到。

## 依赖关系

- 依赖：`Source/Core/map/door.h`（`Door`几何/外观/门禁/交互）、
  `Source/Core/populace/citizen.h`（`Citizen`，身份判定用）、
  `Source/Forever/Element/CitizenElement.h`/`VehicleElement.h`（身份判定）、
  `Source/Forever/Element/DoorAnimInstance.h`（动画实例基类，见`DoorAnimInstance.md`）、
  `Source/Forever/UI/MeetOptionWidget.h`（交互选项UI）。
- 被谁依赖：`Source/Forever/Framework/ForeverDoorFrameworkComponent.h`/`.cpp`（全部门的流式生成/销毁）。

## 待办/后续阶段

- 骨骼组件+动画实例比静态网格贵；每帧生成/销毁数量由`ForeverDoorFrameworkComponent`的`maxSpawnsPerFrame`(4)/`maxDespawnsPerFrame`(8)限流，PIE看卡顿再调。
- 所有门（entrance/room/home/园区门）目前共用同一个测试门资产，见`Basic/map/door_assets.h`；之后按尺寸/用途分资产。
- NPC寻路不按门的开合/门禁绕路；挡板关着时NPC靠触发盒开门通过。

## 包围盒

门用`UDoorMeshComponent`(继承骨骼网格组件)，`CalcBounds`返回固定的局部盒：网格自带范围，Y方向加上`2*extent.X`(门扇绕铰链转开最远扫到的距离，两个方向)。
不能用默认的每帧按动画/Physics Asset重算：门扇转开后盒子跟不上姿势，门会被可见性/遮挡剔除成一帧有一帧无；也不能用`SetBoundsScale`等比放大(X也跟着放大几十倍，包围球过大，阴影/灯光开销让帧率腰斩)。

## 销毁时必须显式销毁子组件（"有的门打不开"的根因）
`DestroyComponent()`销毁父`UForeverDoorComponent`时，挂在它下面的门网格/挡板/触发盒不会被一起销毁，而是被提升挂走，残留在原位。
门流式反复生成/销毁后，同一门洞位置会叠着多个关着的旧门网格(实测最多11个，场景里1000+个)，新门开了旧门仍挡着，表现为"门不动、进不去"。
`OnComponentDestroyed`里显式销毁所有子组件并置空`doorMesh`。
