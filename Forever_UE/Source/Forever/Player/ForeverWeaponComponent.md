# ForeverWeaponComponent.h / .cpp

## 职责

挂在`AForeverCharacter`上的武器组件——玩家自己的初始角色和任何被`ChangeControlChange`
切换过去操控的citizen都共用同一个基类，因此都天然带一份（这次没有为"哪些角色能开枪"单独
加开关，见"设计取舍"一节）。负责开火判定/伤害/换弹/切枪这条MVP核心链路，来自
`weapon_system_plan.md`这份设计文档（已删除，读过一次并按其设计落地，这次会话按用户
选定的范围做了裁剪，见下）。

## 这次落地的范围（用户选定）

只做核心武器机制：`WeaponMod`/`WeaponFactory`+`Player`/`Citizen`补生命值字段+hitscan
开火/伤害/击杀+换弹夹+数字键切枪，命中citizen致死会广播一次`CitizenDeceaseEvent`（照抄
`UForeverStoryFrameworkComponent::OptionDialog`的写法单独写一次，不做成通用机制，见下）。
**不做**：通缉值(`ChangeWantedChange`)、警察`JobMod`、反抗小游戏、
`ArrestPlayerChange`/`ReleasePlayerChange`——这些需要先补一个"把任意运行时Event广播给
剧情脚本匹配"的通用机制（现在只有`BroadcastGameStart`这一个特化入口）+一个从零设计的
警察`JobMod`，工作量明显大于武器本身，留给用户下一轮单独提出。

## 关键设计

### 高频/低频两条路径——和weapon_system_plan.md原始设计一致

**高频路径（每次开火/每次命中）**：`Fire()`直接调`LineTraceSingleByChannel`+
`WeaponMod::ComputeDamage()`+`Citizen::TakeDamage()`，纯C++调用，不经过
Story/Change/Event——一场火拼可能一秒钟打好几发，都走Story的`Script::MatchEvent`
（对`mainScript`当前所有`actives`里程碑做一遍完整匹配）是明显的性能陷阱，参照这次会话
早前`Populace::Tick`每天9点/12点集中调度导致卡顿的教训（见`ForeverPopulaceFrameworkComponent.md`
"为什么、怎么做、怎么保证安全"一节）。

**低频/叙事阈值路径（这次只有一个）**：命中导致Citizen死亡时，广播一次
`CitizenDeceaseEvent`——这是唯一"值得让剧情系统知道"的时刻。

### 为什么不做成通用的"广播任意Event"机制

`UForeverStoryFrameworkComponent::BroadcastCitizenDecease`是照抄`OptionDialog`的写法
单独写的一个新方法，不是先抽象出一个"把任意`Event*`广播给任意`Script*`"的通用接口再
在它上面实现——原因：
1. 现在只有两个"构造一个具体Event、广播给具体脚本"的调用点(`BroadcastGameStart`广播
   `GameStartEvent`、`OptionDialog`广播`OptionDialogEvent`)，加上这次的第三个
   (`BroadcastCitizenDecease`广播`CitizenDeceaseEvent`)，三份代码结构相似但每次要广播
   的对象集合不完全一样(`BroadcastGameStart`额外广播给所有Organization/Job/Scheduler
   脚本，`OptionDialog`额外广播给目标citizen自己的Scheduler脚本，
   `BroadcastCitizenDecease`只广播给`mainScript`)——现在没有第四个用例之前，抽象成通用
   接口猜的形状大概率不对，先攒够案例更稳妥。
2. 用户明确把"通缉值→警察JobMod反应"这条需要真正通用广播机制的链路排除在这次范围外
   （见上"这次落地的范围"），这次唯一的广播需求(`CitizenDeceaseEvent`只需要匹配
   `mainScript`)用照抄现有写法就能满足，不需要提前建这个抽象。

### 为什么`WeaponMod`没有`Fire()`/`ApplyRecoil()`虚方法

原始设计文档把这两个方法留成"签名待定"的虚方法占位，理由是"要接UE的碰撞查询/相机接口，
Dependence层看不到UE类型"。这次落地时改成：`WeaponMod`只留纯数据字段
（`damage`/`maxRange`/`baseSpread`/`fireRate`/`magazineCapacity`/`reloadDuration`等）
+一个纯数学的`ComputeDamage(float distance)`虚方法（只用Core类型，和`BuildingMod::
Layout()`只用`Quad`/`Road*`这类Core类型做参数是同一个约定），真正的开火判定/相机取值/
碰撞查询整个放在这个组件里直接做。这样不需要发明一套"Dependence层声明、Forever层实现"
的跨层虚方法签名，`WeaponMod`不留任何"以后再定"的半成品。

### 命中判定：ECC_Pawn，不是ECC_Visibility（实测踩过的坑）

`Fire()`第一版用的是`ECC_Visibility`通道，理由(当时)是"静态几何和`ACharacter`默认Capsule
预设对这个通道都是Block"——**这个假设是错的，且是实测验证出来的**：这个项目的
`Config/DefaultEngine.ini`用的是UE5原版内置的碰撞预设表（`Source/Forever`自己没有改过
这份配置），其中`[/Script/Engine.CollisionProfile]`节的`"Pawn"`预设（`ACharacter`默认
Capsule就是用这个预设）显式把`CustomResponses`写成`((Channel="Visibility",
Response=ECR_Ignore))`——**Pawn预设对Visibility通道是Ignore，不是Block**。用
`ECC_Visibility`做`LineTraceSingleByChannel`的结果是：打墙正常停下（墙体slab用
`BlockAll`/`BlockAllDynamic`预设，对没有专门覆盖的通道一律默认Block），但打citizen会
直接穿过去（Capsule对Visibility通道是Ignore），且因为`bHit`是`false`，`Fire()`里连
"命中"分支都不会进，什么消息也不会打印——这正是用户实测反馈的现象。

改用`ECC_Pawn`通道解决：`BlockAll`/`BlockAllDynamic`对"没有专门覆盖的通道"一律默认
Block（所以墙照样能挡住），`Pawn`预设对`Pawn`通道本身也没有写进`CustomResponses`里
覆盖成Ignore（所以走默认响应，会被判定为Block）——两边都正常挡住，不需要改
`DefaultEngine.ini`/新增自定义碰撞通道。**这是一个通用的坑，不只是这次武器系统才会
碰到**：这个项目里任何"要不要针对Pawn做碰撞查询"的新代码，都不能想当然地假设UE的
`ECC_Visibility`通道会挡住Pawn——这份`DefaultEngine.ini`的默认配置明确不是这样。

**只有打中`ACitizenElement`才算"命中"（第二版明确要求）**：`Fire()`里`Cast<
ACitizenElement>(hit.GetActor())`失败（打中墙、打中别的什么东西、或者射线全程没碰到
任何东西）在伤害判定这一层完全等价——都不产生任何效果，直接`return`，不打印任何"命中"
消息、不计算`ComputeDamage()`。第一版曾经对任意命中的Actor都打印一条"命中 X，伤害 Y"
的debug消息，即使X是一面墙——这条消息在那种情况下意义不明（墙又不会真的掉血），这次
去掉了：debug消息只在真的命中一个还活着的citizen、且真的调用了`TakeDamage()`之后才打。

### 子弹轨迹：`DrawDebugLine`，碰撞检测用摄像机射线，视觉起点是枪的位置

`Fire()`每次开火都画一条`DrawDebugLine`——**不管这次算不算"命中"**（上一节的"只有打中
citizen才算命中"只影响伤害判定，不影响这条线画不画）：`LineTraceSingleByChannel`真的
碰到东西（不管是墙还是citizen）就画到`hit.ImpactPoint`，全程没碰到任何东西就画到
`maxRange`处的射线终点——子弹视觉上应该飞到障碍物为止，不能因为"打中的不是citizen、
不算命中"就让轨迹线穿墙画到很远的地方。用`DrawDebugLine`（黄色，**5秒后消失**，不是
`bPersistentLines`常驻——原本给的0.15秒太短，PIE里几乎来不及看清弹道，用户明确要求
调长）而不是真正的粒子特效/`NiagaraSystem`——这次没有`muzzleFlashEffectPath`/
`impactEffectPath`对应的任何VFX资产，纯debug线段先验证弹道方向/散射(`baseSpread`)
手感是否合理，PIE里能直接看见每一发子弹往哪飞。

**碰撞检测和视觉线段的起点这次故意拆成两个不同的点**：`LineTraceSingleByChannel`的
`start`仍然是摄像机位置（命中判定依据，不能变——见下"开火起点"一节的历史反复），但
`DrawDebugLine`画的线段起点改成`weaponMesh->GetComponentLocation()`（枪的位置），
终点仍然是碰撞检测算出来的`trailEnd`。这样命中判定的精确性（从瞄准点发射，保证"看哪打
哪"）和子弹轨迹的视觉合理性（子弹应该看起来是从枪口飞出去的，不是从眼睛里飞出去的）
分开满足，互不影响——两者用的是同一条`LineTraceSingleByChannel`结果，只是`DrawDebugLine`
调用时换了个起点参数，不需要跑第二次射线检测。`weaponMesh`为空时(还没有真正的枪械
mesh资产，见"资产状态"一节)退化成`character->GetActorLocation()`。

**开火起点：摄像机位置（曾经改成枪口位置，又被用户要求改回来）**：中间有一版把
`Fire()`的起点从摄像机(眼睛)位置换成"枪的位置"（按`weaponMesh`的`Muzzle`socket→
`weaponMesh`本身位置→角色骨骼`gripSocketName`socket→`GetActorLocation()`这个优先级
链取值），但用户明确要求撤回，改回最初的`camera->GetComponentLocation()`。撤回的
具体原因这次没有细问，只按字面要求执行；如果以后又要换回枪口起点，上面那条优先级链的
设计思路仍然成立，直接照抄`git log`里这次改动之前的版本即可，不需要重新设计。

### 死亡处理：`Citizen::IsDead()`是单向状态，不会复活

`Citizen`/`Player`（Core）这次各自新增`health`/`maxHealth`+`TakeDamage`/`Heal`/
`IsDead()`，这次没有死亡后果系统（复活/game over），`IsDead()`一旦为true不会变回
false。`UForeverPopulaceFrameworkComponent`新增`HandleCitizenDeath(Citizen*)`——
`Destroy()`掉对应的`ACitizenElement`（如果当前有）、从`activeInstances`摘除；同时
`TickComponent`的流式生成循环、`FindOrSpawnCitizenByName`都补了`IsDead()`跳过判断，
死掉的citizen不会再被生成/重新生成。`Player::TakeDamage`这次没有任何调用方——这次没有
会反击的NPC/敌人，这个方法是为将来"citizen/警察向玩家开火"预留的，见"待办"一节。

### 每次被占有都会重新配一把满弹匣的手枪——已知简化

`AForeverCharacter::PossessedBy`里无条件调一次
`weaponComponent->EquipWeapon(TEXT("weapon_pistol"))`，这样玩家自己的初始角色、或者
`ChangeControlChange`换过去的任意citizen，一被占有就能立刻测开火/换弹/切枪，不需要
额外的UI/背包流程。代价：如果同一个角色被取消占有又重新占有（比如切到另一个citizen
体验一圈之后又切回来），武器/弹药状态不会保留，会被重置成默认手枪满弹匣——这次没有
按角色持久化武器状态，见"待办"一节。

### 瞄准：复用现有cameraBoom/followCamera插值，不新增第三个摄像机

用户明确要求"仿照大部分射击游戏"的越肩瞄准——默认鼠标右键，按住瞄准/松开取消，过渡必须
平滑，且瞄准后子弹要从瞄准摄像机位置发出。实现放在`AForeverCharacter`
（`ForeverCharacter.h/.cpp`），不在这个组件里：

- 不新建第三个`UCameraComponent`。直接在现有`cameraBoom`（`USpringArmComponent`）上插值
  `SocketOffset`/`TargetArmLength`——瞄准目标值`aimSocketOffset`(默认`(0,60,40)`，Y轴正值
  偏向右肩、Z轴正值略微升高)/`aimArmLength`(默认150)，取消瞄准目标值
  `defaultSocketOffset`(零)/`defaultArmLength`(默认400，构造函数里原字面量`400.0f`这次
  改存成这个成员变量，供`Tick()`插值时做"回到哪"的目标)。
- 新增`AForeverCharacter::Tick(float DeltaTime)`（构造函数里补
  `PrimaryActorTick.bCanEverTick = true`，这个类之前没有override过`Tick`），每帧用
  `FMath::VInterpTo`/`FMath::FInterpTo`把`cameraBoom`当前值朝目标值推进一步
  （插值速度`aimTransitionSpeed`，默认10），不是`StartAim`/`StopAim`里直接赋值瞬间跳变
  ——这样满足"过程平滑"的要求。
- `StartAim()`/`StopAim()`本身极简，只翻转`bIsAiming`标志位，真正的运动全在`Tick()`里。
- 输入绑定：`AimWeapon`（默认`EKeys::RightMouseButton`，`ForeverKeyBindingSubsystem.cpp`
  的`GetBindingDefinitions()`新增一行），`Started`→`StartAim`，`Completed`→`StopAim`，
  和`FireWeapon`的Started/Completed绑定同一个模式。

**"子弹从瞄准相机位置发出"这个要求为什么不需要改`ForeverWeaponComponent.cpp`一行代码**：
`Fire()`本来就读`character->GetFollowCamera()->GetComponentLocation()`做第三人称开火
起点（见上"开火起点"一节）——`followCamera`本身没有动，动的是它父级`cameraBoom`的
`SocketOffset`/`TargetArmLength`，瞄准时`cameraBoom`把`followCamera`带到肩膀附近，
`GetComponentLocation()`取到的自然就是新位置，开火起点跟着自动生效。这是选择"复用现有
组件插值"而不是"新增专门的瞄准摄像机"的主要原因——后者还需要在`Fire()`里加一个"是否在
瞄准，用哪个摄像机"的分支，前者是零改动。
- 只对第三人称生效（第一人称`firstPersonCamera`没有挂在`cameraBoom`下，这次没有为
  第一人称单独做瞄准效果——按需求"这样角色本身不会挡住被瞄准的东西"本来就是第三人称视角
  才有的问题，第一人称视角本身已经不存在这个遮挡）。

### 瞄准时角色朝向跟着摄像机转，不跟着移动方向转

用户要求"人不能朝身后瞄准"——瞄准时角色必须正对着准星指向的方向，不管此刻在往哪个方向
移动。复用第一人称视角本来就有的同一套朝向开关（`bUseControllerRotationYaw`+
`UCharacterMovementComponent::bOrientRotationToMovement`，`ToggleCameraView()`按
`bIsFirstPerson`切这两个值），新增`AForeverCharacter::UpdateRotationMode()`统一计算
`bool bFaceCamera = bIsFirstPerson || bIsAiming`——第一人称、或者瞄准中(不管第几人称)，
都用"朝向跟摄像机走"；否则用"朝向跟移动方向走"。`ToggleCameraView`/`StartAim`/`StopAim`
三处都改成调这个函数，而不是各自维护一份判断——避免"第一人称+瞄准同时结束"这种组合状态
被算错。

### DrawDebugLine视觉起点改成枪的位置，碰撞检测射线不变

用户要求碰撞检测继续用摄像机位置/方向的射线（保证"看哪打哪"的精确命中判定），但画出来
的子弹轨迹线段视觉起点改成枪的位置（`weaponMesh`的世界坐标），不再是摄像机位置——两者
用同一次`LineTraceSingleByChannel`结果，只是`DrawDebugLine`调用时把起点参数换成
`weaponMesh->GetComponentLocation()`(为空时退化成`character->GetActorLocation()`)，
终点仍然是碰撞检测算出的`trailEnd`，不需要跑第二次射线检测，见"子弹轨迹"一节。

### 挂载点本地偏移(`attachOffsetX/Y/Z`)：在`WeaponMod`里定义，不是硬编码在Forever层

实测发现`weaponMesh`挂在`gripSocketName`留空时的Root上，会直接出现在角色身体正中心
(卡在了胯部)——因为`SpawnWeaponMesh()`只做了`SetupAttachment`，没有加任何本地偏移。
按用户要求，这个偏移量放进`WeaponMod`(`Source/Dependence/player/weapon_mod.h`)的
`attachOffsetX/Y/Z`三个float字段（角色本地坐标系，X前/Y右/Z上，单位cm），不是写死在
`ForeverWeaponComponent.cpp`里的常量——不同武器（比如手枪贴身、步枪需要更靠前/靠下的
持握点）以后可以给不同的值，不需要改Forever层代码。`SpawnWeaponMesh()`里
`SetupAttachment`之后紧跟一行`weaponMesh->SetRelativeLocation(FVector(attachOffsetX,
attachOffsetY, attachOffsetZ))`。`PistolWeapon`/`RifleWeapon`(`weapon_basic.cpp`)这次都
给了`(20, 20, 40)`——目测挪到右肩膀靠前一点的位置，具体数值需要在PIE里再调，只改这两个
构造函数里的三个数字即可，不需要碰任何其它代码。**这个偏移只对没有真实持枪socket的情况
生效**——以后换成真实的手部socket挂载后，socket本身的位置已经是"手该在哪"，这个偏移
字段可能需要归零或者重新调（因为它是叠加在挂载点之上的相对偏移，不是绝对世界坐标）。
`Source/Basic`是运行期DLL(见项目memory"Basic is a runtime DLL")，改完`weapon_basic.cpp`
只需要重新编译`Framework.sln`，不需要碰UBT/ForeverEditor那一侧。

**实测踩过的坑：加了偏移字段之后debugLine起点还是没变**——原因是`firstPersonMeshPath`
这次一直是空的(用户还没导入任何武器mesh，见"资产状态"一节)，`weaponMesh`因此永远是
`nullptr`，`Fire()`里`gunLocation`走的其实一直是"没有mesh"的退化分支，而退化分支最初
写的是纯`character->GetActorLocation()`——完全没有用上`attachOffsetX/Y/Z`，导致调
`WeaponMod`里的偏移数值画出来的线段起点纹丝不动。修复：退化分支也要应用这个偏移，
用角色当前朝向把本地偏移转成世界坐标再加到角色位置上（`character->GetActorRotation().
RotateVector(FVector(attachOffsetX, attachOffsetY, attachOffsetZ))`），效果等价于
"挂到Root上再SetRelativeLocation"这条路径。**这是一个通用的坑**：这个类里任何"有mesh用
mesh位置，没mesh退化成角色位置"的分支，都要确认退化分支有没有跟着应用同一套偏移/变换，
不能假设两条分支视觉上应该一致就直接漏掉其中一条。

**实测踩过的坑：瞄准在"被玩家占有的citizen"身上完全没反应，根源是Tick()被关掉了**——
右键能正常触发`StartAim`/`StopAim`(`bIsAiming`确实翻转)，但摄像机纹丝不动。逐步加
`UE_LOG`诊断+读`Saved/Logs/Forever.log`定位到：当时玩家占有的不是初始角色，而是一个
`ACitizenElement`(`Name=CitizenElement_0`)，而`ACitizenElement`的构造函数默认
`SetActorTickEnabled(false)`（绝大多数citizen静止不动，关Tick省性能，只有`WalkTo()`
带它走路的那段时间才会打开，见`CitizenElement.cpp`），`ACitizenElement::PossessedBy`
只切了`MovementMode`，没有把Tick重新打开——所以`AForeverCharacter::Tick()`(瞄准插值的
唯一入口)在这个实例上从始至终没被引擎调用过一次，`bIsAiming`翻转了但没有任何代码在
消费这个状态。**这个坑分两层，第一次修复只补了第一层，实测后发现还是不生效，才挖到
第二层**：
1. `ACitizenElement::PossessedBy`只切了`MovementMode`，没有把Tick重新打开——补一行
   `SetActorTickEnabled(true)`，`UnPossessed`里对称地补一行`SetActorTickEnabled(false)`
   (前提是`pendingWaypoints`已经走完，否则会打断`WalkTo`自己的Tick管理)。
2. 光补第1层还是没用——因为`ACitizenElement::Tick()`自己的第一行判断
   `if (waypointIndex >= pendingWaypoints.Num())`，对一个被玩家占有、没有任何AI
   `WalkTo()`路径点的citizen永远成立(`0 >= 0`)，会在`PossessedBy`打开Tick之后的
   下一帧就自己把Tick又关掉——`AForeverCharacter::Tick()`(瞄准插值)只会在被占有的
   那一帧真正跑一次，之后又永久停掉，右键瞄准在这之后按下`bIsAiming`会翻转但
   `Tick()`根本不会再被引擎调用。修复：这个分支和"到达终点"那个分支的
   `SetActorTickEnabled(false)`都加上`if (!IsPlayerControlled())`保护——AI控制的
   citizen走完路径照常关Tick省性能，但被玩家占有的citizen不会被这个逻辑误关。
   `UnPossessed`不需要对应调整，它本身就会在还给AI之前显式关Tick。

**这是一个通用的坑**：以后任何指望在`AForeverCharacter`及其子类身上用原生C++`Tick()`
的功能(不只是瞄准)，如果这个子类默认关闭Tick且有自己的`Tick()`覆写会按条件自我关闭
(目前只有`ACitizenElement`这么做)，光在`PossessedBy`里打开还不够，必须确认子类自己的
`Tick()`覆写里有没有"满足某条件就自己关掉"的逻辑、且这条逻辑有没有考虑到"被玩家占有"
这个例外。也不能只看`PrimaryActorTick.bCanEverTick`(这个从始至终是true，`bCanEverTick`
只表示"允许Tick"，不代表"这一刻真的在Tick"，真正决定这一刻要不要跑的是
`SetActorTickEnabled`/`IsActorTickEnabled()`这个运行时开关，且这个开关可能在同一帧内被
基类逻辑打开、又被派生类逻辑关掉)。

### 按住左键连续开火 + 弹药显示

`PistolWeapon`改成`fullAuto = true`（用户明确要求"按住左键连续开火"），配套弹匣从12发
放大到30发——这两个字段本来就在`WeaponMod`里可调，这次只是改具体数值，没有新加机制：
连发本身`StartFire()`/`TickComponent()`/`fireRate`冷却这套逻辑此前给`RifleWeapon`就已经
实现好了，`PistolWeapon`只是之前一直是`fullAuto = false`。

`Fire()`每次成功开火（真正扣了一发弹药之后）都在屏幕固定位置(`AddOnScreenDebugMessage`
用固定Key`200`，不是`-1`，所以是"刷新同一行"而不是往下堆叠新消息)打印
`"弹药: X/Y"`——`TickComponent()`里换弹完成、弹匣填满的那一刻也打印同一行，保持数字
随时准确。这次没有做成正式的UMG弹药UI，和其余"先用原生调用验证"的思路一致。

### 必须瞄准中才能开火

用户明确要求"不瞄准的时候进行射击操作没反应"——`Fire()`最开头(比`fireRate`冷却/弹药
检查还早)加了`if (!character || !character->IsAiming()) return;`，不瞄准时按/按住开火键
完全是空操作：不消耗弹药、不触发`fireRate`冷却计时、也不会打印"弹匣已空"（因为压根没
真正尝试开火）。这个检查必须放在最前面而不是放在原来"取得`character`指针"那一步之后——
如果放晚了，"没瞄准时疯狂点开火键"仍然会在真正没瞄准的情况下消耗弹药/占用冷却，只是
不产生子弹效果，行为上不对。全自动武器持续按住开火键但没有瞄准时，`TickComponent()`
里的`Fire()`调用会每帧走到这个检查就直接返回，一旦开始瞄准(`bWantsToFire`仍然是true)
会立刻恢复开火，不需要重新按一次开火键。

### 后坐力：仿PUBG手感，没有自动回正

`WeaponMod`新增`recoilPitchMin`/`recoilPitchMax`/`recoilYawMin`/`recoilYawMax`四个字段
（度）——垂直方向最初是固定值(`recoilPitch`)，用户要求"垂直和水平都要随机"之后改成和
水平同款的区间随机，两个方向每次开火都各自在自己的`[min, max]`里独立取一个随机值，不是
固定踢同样的角度。`recoilPitchMin/Max`都给正数(FRotator的Pitch约定"+Up")，保证垂直
方向永远是往上踢，不会随机抽到往下压枪口的结果。`Fire()`每次真正开火后（在这一发的
碰撞检测/轨迹算完之后，只影响"下一发"用到的朝向，不会回头改变这一发自己的命中判定），
直接改`Controller->GetControlRotation()`的Pitch(+`[recoilPitchMin, recoilPitchMax]`
区间随机值)/Yaw(+`[recoilYawMin, recoilYawMax]`区间随机值)再`SetControlRotation`回去。
**故意不用`AddControllerPitchInput`/`AddControllerYawInput`**
——这两个方法会经过`APlayerController::InputPitchScale`/`InputYawScale`缩放，符号不直观
(实测容易踢反方向)，直接改`ControlRotation`可以按UE文档写明的`FRotator`约定
(`Pitch: +Up/-Down`，`Yaw: +Right/-Left`)确定方向。**这次故意不做任何自动回正/衰减**
——PUBG本身也没有"松开鼠标后视角自动弹回开火前的朝向"这种机制，持续连发时准心会一直
往上/往两边走，直到玩家自己把鼠标往反方向拉压枪，这是特征不是bug。单发/连发都会踢，
因为两条路径最终都走同一个`Fire()`调用，不需要分别处理。`PistolWeapon`给的后坐力比
`RifleWeapon`轻（单手持握手枪 vs 双手端着全自动步枪连发），具体数值都是目测给的，见
`weapon_basic.cpp`，需要在PIE里试实际手感。

### 瞄准时的准心

单独放进新的`AForeverHUD`类（`Source/Forever/Player/ForeverHUD.h/.cpp`），不在这个
组件/`ForeverCharacter`里画——原生`AHUD::DrawHUD()`+`DrawLine`画屏幕空间的十字，只在
`AForeverCharacter::IsAiming()`为true时画，用户明确说是"临时"效果，见`ForeverHUD.md`
完整设计。

## 资产状态

`PistolWeapon`/`RifleWeapon`（`Source/Basic/player/weapon_basic.h/.cpp`）的
`firstPersonMeshPath`这次留空——用户还没有下载导入武器模型（`weapon_system_plan.md`
推荐过两个免费资源：itch.io的Low Poly FPS Weapons Pack Lite、Epic官方Fab Free
Content）。`SpawnWeaponMesh()`发现路径为空会直接跳过挂mesh这一步，不影响开火/伤害/
换弹逻辑本身——可以先在PIE里测手感，等资产导入后把实际路径填进这两个类的构造函数
即可，不需要改其它任何代码。

## 依赖关系

- 依赖：`Source/Dependence/player/weapon_mod.h`/`weapon_factory.h`（`WeaponMod`纯数据+
  `ComputeDamage`）、`Source/Core/common/registry.h`（`Registry::Get().GetWeaponFactory()`）、
  `Source/Core/populace/citizen.h`（`TakeDamage`/`IsDead`）、
  `Source/Forever/Player/ForeverCharacter.h`（摄像机取值/`IsFirstPerson()`）、
  `Source/Forever/Element/CitizenElement.h`（`GetCitizen()`反查命中目标）、
  `Source/Forever/Framework/ForeverFrameworkActor.h`/
  `ForeverStoryFrameworkComponent.h`/`ForeverPopulaceFrameworkComponent.h`
  （死亡时广播Event+清理Actor）。
- 被谁依赖：`AForeverCharacter`（`CreateDefaultSubobject`+`StartFireWeapon`/
  `StopFireWeapon`/`ReloadWeapon`/`SwitchToWeapon1`/`SwitchToWeapon2`转发）。

## 待办/后续阶段

- 通缉值(`ChangeWantedChange`)/警察`JobMod`/反抗小游戏(`StartPuzzleChange`复用现成的
  `PuzzleMod`/`Canvas`/`PuzzleWidget`机制)/`ArrestPlayerChange`——需要先补"任意Event
  广播给剧情脚本"的通用机制，用户已明确排除在这次范围外。
- `Player::TakeDamage`没有任何调用方——等有会反击的NPC/敌人再接上。
- 武器状态不按角色持久化（每次被占有都重置成默认手枪满弹匣）。
- 弹药：`magazineCapacity`/`reloadDuration`照常生效，但`Reload()`无条件把弹匣填满，
  不检查/不消耗任何备弹（`ammoObjectId`/`maxReserveAmmo`这两个概念这次没有加进
  `WeaponMod`，等Asset/背包域真正做出来再补）。
- 具体弹道/后坐力/散射数值调优——`PistolWeapon`/`RifleWeapon`这次给的是随手定的测试值，
  实际手感需要在PIE里试（后坐力机制本身已实现，见"后坐力"一节，这里指的是数值调优）。
- 准心目前是固定尺寸的原生"+"（`AForeverHUD`），没有换成正式贴图资产、没有跟随
  `baseSpread`动态张开。
- 武器mesh资产（`firstPersonMeshPath`）需要用户下载导入后手动填路径，见"资产状态"一节。
