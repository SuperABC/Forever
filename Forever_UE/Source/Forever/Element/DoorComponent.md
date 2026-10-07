# DoorComponent.h / .cpp

## 职责

`UForeverDoorComponent`：一扇真正放了门扇的门在场景里的渲染+交互表现，绑定一个Core端
`Door*`。挂在"所属对象的渲染Actor"上——建筑门/房间门挂`ABuildingElement`（近景LOD生成/
清理，见`BuildingElement.md`"门"一节），园区门挂`UForeverZoneFrameworkComponent`的owner
（常驻，不随LOD）。负责：门扇/门框网格生成、触发盒、开关动画、身份判定+门禁拒绝、
MeetOption交互选项。

## 关键设计

### 坐标约定：组件自身用绝对世界坐标摆位，和父级Actor的本地坐标系无关

`Init()`里直接`SetWorldLocation`/`SetWorldRotation`，不是相对父级Attach点算offset——
`Door`的几何(`GetX/Y/Z/GetYaw()`)本来就是Core算好的绝对地图坐标(乘`DOOR_WORLD_SCALE`
换算成UE单位)，不需要关心挂在哪个Actor下面、那个Actor自己的Transform是什么。
`Door::GetYaw()`是墙体**外法线**方向，这个组件自己的`local X`要对齐资产约定的"沿墙"
方向，和法线正好相差90度——`componentYawDegrees = RadiansToDegrees(door->GetYaw()) -
90`，不是直接拿GetYaw()当组件朝向用。

### 门扇枢轴(pivot)和可视网格(mesh)分离，动画只改枢轴

每扇门扇是`USceneComponent`(枢轴)+挂在它下面的`UStaticMeshComponent`(可视网格)。
`AlignMeshToBounds()`在**创建时**按mesh资产的实际包围盒(`UStaticMesh::GetBounds()`)
算出一次性的缩放+本地offset，让网格的包围盒精确填满目标区间——不管资产的pivot在哪
(几何中心/边缘/随便哪里)都能摆对，这是为了兼容这次用的占位Cube(pivot在几何中心，和
"平开门pivot在铰链边底部/推拉门pivot在底边中点"的正式资产约定不一致)。开关动画
(`TickComponent`)只在`closedTransform`/`openTransform`两个预先算好的枢轴相对Transform
之间按`SmoothStep`插值`SetRelativeTransform`，从不重新计算mesh的缩放/offset——枢轴的
旋转/平移中心和网格资产自身的pivot约定完全解耦。

### Slide / Swing 的枢轴摆位

- **Slide**（建筑门/园区门用）：枢轴本地X=门扇"静止位"（单扇=0，覆盖整个门洞；双扇=
  各自半个门宽的中心），开门时枢轴沿本地X平移`leafWidth*openAmount`（单扇方向看
  `Door::IsFlippedSide()`；双扇固定对称往外滑）。
- **Swing**（房间门用）：枢轴本地X=铰链所在的洞口边缘（单扇看`IsFlippedSide()`选左右边；
  双扇两扇各自铰链在洞口两端外侧），mesh的包围盒中心offset到"离铰链半个扇宽"处（沿洞口
  内侧延伸），开门时枢轴绕本地Z转`openAmount`度（默认90度）。

### 已知简化：开门摆动方向这次固定，不按"远离触发者"动态判断

原始设计稿想要"转动方向在开门那一刻确定：朝远离触发者的一侧开，避免门扇扫到人"，这次
简化成固定往本地+Y方向转（不看是谁触发的）——单扇门洞穿到人的概率本来就低(门禁触发
+触发盒本身有1.5地图单位的缓冲)，这个简化不影响核心验证目标(门会不会开/关、门禁判定对不
对)，如果实测发现真的会扫到人，再回来加"按触发者方向动态翻转openTransform.GetRotation()
符号"这几行代码，不需要重新设计。

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
`DOOR_CLOSE_DELAY_SECONDS`(1秒)再关，`TickComponent`只在动画进行中/有未完成的开关请求时
跑(全关且没有待处理请求时`SetComponentTickEnabled(false)`省性能)。门禁拒绝通行的Actor
**不会**被加进`passers`(门不会为它开)，但如果这扇门有`Script`，MeetOption选项照常弹出
给靠近的玩家——拒绝通行和拒绝对话是两件独立的事。

## 依赖关系

- 依赖：`Source/Core/map/door.h`（`Door`几何/外观/门禁/交互）、
  `Source/Core/populace/citizen.h`（`Citizen`，身份判定用）、
  `Source/Forever/Element/CitizenElement.h`/`VehicleElement.h`（身份判定）、
  `Source/Forever/UI/MeetOptionWidget.h`（交互选项UI）。
- 被谁依赖：`Source/Forever/Element/BuildingElement.h`/`.cpp`（建筑门/房间门，近景LOD
  生成/清理）、`Source/Forever/Framework/ForeverZoneFrameworkComponent.h`/`.cpp`（园区
  大门，常驻生成）。

## 待办/后续阶段

- 真实资产到位后(见`door_system_plan.md`第七节)，`frameMesh`/`mesh`路径从Basic层配置
  里换成真实路径即可，这个组件的代码不需要改任何一行。
- "朝远离触发者一侧开"这个动态判断——见上面"已知简化"一节。
- 门扇运动中碰撞保持开启(`ETeleportType::None`)，如果实测发现会把人推飞，改成动画期间
  关闭门扇碰撞(`SetCollisionEnabled(ECollisionEnabled::NoCollision)`，动画结束再恢复)。
