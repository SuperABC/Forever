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

### 命中判定：ECC_Visibility，不是ECC_Pawn

`Fire()`用`ECC_Visibility`通道做`LineTraceSingleByChannel`——静态几何（墙体slab，见
`BuildingElement.cpp`的`SpawnCube`）和`ACharacter`默认Capsule碰撞预设对这个通道都是
Block，子弹因此会先打到墙就停下、不会穿墙命中站在墙后面的citizen，不需要额外配置新的
碰撞通道/改`DefaultEngine.ini`。如果用`ECC_Pawn`，静态几何不会挡住射线，子弹会直接
穿墙命中。

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
  实际手感需要在PIE里试。
- 武器mesh资产（`firstPersonMeshPath`）需要用户下载导入后手动填路径，见"资产状态"一节。
