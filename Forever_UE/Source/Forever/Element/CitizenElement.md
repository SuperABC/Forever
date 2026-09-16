# CitizenElement.h / .cpp

## 职责

一个citizen在场景里显形时对应的Actor。和`ABuildingElement`同一套"每个Core实例对应一个
强类型Actor子类"模式（Actor自己持有指向Core对象的裸指针+指向共享框架组件的
`TWeakObjectPtr`），区别只有一点：`ABuildingElement`开局为每栋building常驻生成一个，
`ACitizenElement`按玩家距离由`UForeverPopulaceFrameworkComponent::TickComponent`流式
`SpawnActor`/`Destroy`，不常驻——这是这次会话和用户确认过的架构决定：人口规模通常比建筑
数量大得多，流式管理是更保险的默认选择，见`Source/Forever/Framework/
ForeverPopulaceFrameworkComponent.md`。

`UForeverPopulaceFrameworkComponent::TickComponent`判定"该显形"时`SpawnActor<
ACitizenElement>()`+立刻调用`Init(citizen, this)`；判定"该隐藏"时先读走当前世界坐标写回
`Citizen`，再`Destroy()`这个Actor。

## 关键设计

### 数据结构：为什么不照抄老工程`APopulaceBase`+`TMap<FString,AActor*>`

老工程用`APopulaceBase`单例+`TMap<FString, AActor*> personInstances`+Blueprint里真正
`SpawnActor`，C++侧完全没有"一个Person对应一个具名Actor子类"的强类型设计，真正的Actor
类型/生成逻辑埋在Blueprint里看不到。这次工程从`Building`域开始就确立了"每个Core实例对应
一个强类型Actor子类"这个模式（好处是每个citizen的碰撞盒/网格/未来AI状态都能自然地长在它
自己的Actor上，不需要再维护一个"名字→Actor"的旁路查找表），这次延续同一个模式，只是
生成/销毁的时机换成了按距离流式（`UForeverPopulaceFrameworkComponent`自己维护
`TMap<Citizen*, TObjectPtr<ACitizenElement>> activeInstances`做这个映射，不需要
`ACitizenElement`自己额外维护任何名字表）。

### 基类是`ACharacter`，不是纯`AActor`（第二版修正）

第一版`ACitizenElement`是`AActor`+自建的`USceneComponent elementRoot`+
`USkeletalMeshComponent bodyMesh`，只把Mobility设成`Movable`留了个"以后能走"的口子——
被用户指出这不够，"citizen不能只是静态骨架，要是一个Character才行"。修正成
`ACitizenElement : public ACharacter`，和玩家角色`AForeverCharacter`同一个基类，自带
`CapsuleComponent`（root）/`SkeletalMeshComponent`（`GetMesh()`）/
`CharacterMovementComponent`（`GetCharacterMovement()`）——为将来的行走AI预留好完整组件
骨架，不是等真的要加移动时再把`AActor`整个换成`ACharacter`重新搭一遍。这次仍然不驱动任何
移动：构造函数里显式`GetCharacterMovement()->SetMovementMode(MOVE_None)`，避免
`CharacterMovementComponent`自己的重力/地面检测把`Init()`摆好的位置挪走；以后加AI移动
时改回`MOVE_Walking`即可，不需要动组件骨架。

### 占位资产：`SKM_Manny_Simple`

和`AForeverCharacter.cpp`同款软路径（`/Game/Asset/Characters/Mannequins/Meshes/
SKM_Manny_Simple`），`ConstructorHelpers::FObjectFinder`在构造函数里加载，`GetMesh()->
SetRelativeLocation`用`-GetCapsuleComponent()->GetScaledCapsuleHalfHeight()`让mesh的
脚底正好落在capsule底部（和`AForeverCharacter.cpp`硬编码`-96.f`是同一个道理，这里动态
取值不依赖某个特定的capsule尺寸配置）——用户明确要求"当前阶段就暂时都用UE默认的小白人，
但后面是会替换成别的资产的"。**动画蓝图也照抄`AForeverCharacter.cpp`挂了同一个
`ABP_Unarmed`**（第二版补入）——第一版没挂任何`AnimInstance`，`USkeletalMeshComponent`
没有`AnimInstance`时会一直显示bind pose，用户看到的是"T-pose插在地里"，T-pose就是这个
原因（"插在地里"是另一个独立的Z坐标bug，见下）。挂了`ABP_Unarmed`之后至少有一个正常的
待机姿势，不需要等真正的移动AI才能看起来像个人。

### 3D坐标"留空/首次随机/此后复用"——在这里落地，不在`Citizen`

`Citizen`（Core）只存储`hasPosition`+`posX/Y/Z`（地图单位），真正的判断/写入逻辑在
`Init()`里：
- `citizen->HasPosition()`为true：直接读`GetPosition()`转UE坐标（`*CITIZEN_WORLD_SCALE`）
  摆放，不再随机。
- 否则：用`Room::GetPosX/PosY()`（Floor局部坐标）+本文件自己维护的一份
  `ComputeCitizenWorldPosition`（和`BuildingElement.cpp`的`ComputeWorldPosition`公式
  完全一样，按本文件既有约定各自维护一份，不额外抽公共头——**但函数名不能取成一样的**，
  见下"匿名namespace命名坑"）换算房间中心世界坐标，X/Y各加`(GetRandom(11)/10.f-0.5f)*0.4f`
  随机抖动（老工程
  原公式，抖动范围±0.2地图单位），Z用`Building::GetFloorBaseZ(room->GetLayer())`（比
  老工程"layer*楼层固定高度"的粗糙算法更准，这栋楼各层高度本来就不均匀）**再加一个
  `CITIZEN_GROUND_SLAB_THICKNESS`（20 UE单位）**——`BuildingElement.cpp`把Ground slab
  的中心摆在"`floorBaseZ`对应的楼层底部+半个slab厚度"，也就是说真正能站人的地板表面
  （slab顶面）比`floorBaseZ`换算出来的"楼层底部"还要高一个完整slab厚度，不加这一段会
  正好陷进地板slab里那么深（实测踩过这个坑：citizen在地图里插在地里，深度和这个厚度量级
  吻合）。最后再加一个capsule半高——`SetActorLocation`摆的是capsule中心，脚底(mesh)要
  正好落在楼板表面上，capsule中心必须比楼板表面高`GetScaledCapsuleHalfHeight()`。算出来
  **立刻**`citizen->SetPosition(...)`写回——"首次随机、此后复用"的记录在`Init()`这一步
  就完成，不用等销毁时才写。

### 匿名namespace命名坑：Forever模块是unity build，"各文件互不可见"这个假设不成立

`BuildingElement.cpp`/`ForeverZoneFrameworkComponent.cpp`等文件里的匿名namespace注释
一直写"不同翻译单元的匿名namespace不能跨文件共用，所以各自维护一份"——这句话背后的假设
（每个`.cpp`独立编译成自己的翻译单元，匿名namespace天然只在本文件可见）在**普通MSBuild
项目**（`Core`/`Dependence`/`Basic`）里成立，但**在UBT管理的`Forever`模块里不成立**：
UBT的adaptive unity build会把一批`.cpp`直接`#include`拼进一个生成的`Module.Forever.cpp`
一起编译，被拼在一起的几个文件的匿名namespace因此变成了**同一个真正的翻译单元**，如果
两个文件里出现了同名同签名的匿名namespace函数（比如这次`CitizenElement.cpp`最初也叫
`ComputeWorldPosition`，和`BuildingElement.cpp`撞了），会直接报重定义编译错误——只是
UBT的"adaptive"策略平时只把"最近改动过的文件"从unity blob里排除、单独编译，所以只要
还在改这两个文件，它们大概率不会被拼到一起，问题不会暴露；一旦某次编译时这批文件都不在
"最近改动"名单里（比如这次纯粹因为`git checkout`切了一次分支，working set缓存被
UBT判定要重算），adaptive排除名单变化，两个文件第一次被拼进同一个unity blob就编译
失败了——**编译错误可能和你最后一次修改的代码完全无关**，很容易误判成别的问题。
教训：这次架构里"每个文件按约定各自维护一份小helper"这个模式本身没问题（避免过早抽象/
增加不必要的头文件依赖），但**函数名必须在整个`Forever`模块内唯一**，不能只保证同一个
文件内不重复；本文件的版本因此叫`ComputeCitizenWorldPosition`，不是
`ComputeWorldPosition`。

流式销毁时（`UForeverPopulaceFrameworkComponent::TickComponent`里），走远到销毁距离之外
会先读`GetActorLocation()`转回地图单位写回`Citizen`，再`Destroy()`——这保证"反之直接在
记录的位置出现"：不管这个citizen被销毁过多少次，只要`hasPosition`已经为true，重新生成
时永远用最后一次的真实位置，不再触发随机抖动。

### 靠近检测碰撞盒——和流式生成/销毁的距离判定是两回事

`proximityBox`是纯UE层的装饰性判定，用固定的UE单位常量（不走地图单位换算），和
`UForeverPopulaceFrameworkComponent`的`citizenSpawnDistance`/`citizenDespawnDistance`
（决定"这个citizen该不该存在于场景里"）完全独立——一个citizen存在于场景里之后，玩家
还要再靠得更近才会触发这个盒子的Overlap（"接近/离开"屏幕日志）。`BuildProximityBox()`挂在`RootComponent`（`ACharacter`自带的`CapsuleComponent`）上，和
`ABuildingElement::BuildCollisionBox()`同一套写法：`"Trigger"`碰撞profile，
`OnComponentBeginOverlap`/`End`只用`Init()`时预先烘焙好的`collisionLabel`（`FString`，
从`citizen->GetName()`产出），绝不在回调里解引用`citizen`；`OtherActor`按现有惯例过滤成
只认`UGameplayStatics::GetPlayerPawn(GetWorld(),0)`。

### `EndPlay`安全性——和`ABuildingElement`同一套原则

`ACitizenElement`/`UForeverPopulaceFrameworkComponent`/`AForeverFrameworkActor`是三个
独立的Actor/组件，`EndPlay`调用顺序不保证谁先谁后。`ACitizenElement`的安全性设计不依赖
这个顺序：只保证"自己的`EndPlay`一跑完，自己不会再解引用`citizen`"，所以`EndPlay`只把
`citizen`置空就足够安全（不尝试在这里把当前位置写回`Citizen`——那个写回只在受控的运行时
销毁路径里由`UForeverPopulaceFrameworkComponent`主动做，关卡卸载时`Citizen`/`Populace`
可能已经先被析构了，见`ForeverFrameworkActor.md`）。

## 依赖关系

- 依赖：`GameFramework/Character.h`（基类）、`Components/CapsuleComponent.h`/
  `GameFramework/CharacterMovementComponent.h`（`ACharacter`自带组件）、
  `Source/Forever/Framework/ForeverPopulaceFrameworkComponent.h`（`framework`弱引用
  回调）、`Source/Core/populace/citizen.h`（`Citizen`）、`map/building.h`/`map/room.h`
  （Core侧数据）、`Components/SkeletalMeshComponent.h`（占位mesh）、
  `Components/BoxComponent.h`（碰撞盒）、`Kismet/GameplayStatics.h`
  （`GetPlayerPawn`）、`Engine/Engine.h`（`GEngine->AddOnScreenDebugMessage`）。
- 被谁依赖：`UForeverPopulaceFrameworkComponent::TickComponent`
  （`SpawnActor<ACitizenElement>()`+`Init()`+`Destroy()`）。

## 待办/后续阶段

- 真正的AI行为（走动/工作/日程驱动的移动）——这次已经是`ACharacter`+完整
  `CharacterMovementComponent`骨架，`MOVE_None`改`MOVE_Walking`即可接入，但移动逻辑
  本身、寻路都是后续任务。
- 真实资产替换`SKM_Manny_Simple`占位。
- 靠近检测碰撞盒尺寸（`CITIZEN_PROXIMITY_HALF_XY`/`_Z`）是按经验给的初始值，可能需要按
  真实资产的实际比例微调。
