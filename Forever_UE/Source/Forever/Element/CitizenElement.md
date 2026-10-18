# CitizenElement.h / .cpp

## 职责

一个citizen在场景里显形时对应的Actor，基类是`AForeverCharacter`（和玩家角色共用基类，带
摄像机/移动参数/Enhanced Input绑定），这样市民既能被AI/Tick静止摆放，也能在被玩家`Possess`
之后直接用同一套输入/移动逻辑操控。和`ABuildingElement`同一套"每个Core实例对应一个强类型
Actor子类"模式（Actor持有指向Core对象的裸指针+指向共享框架组件的`TWeakObjectPtr`），区别
是不常驻：`ACitizenElement`按玩家距离由`UForeverPopulaceFrameworkComponent::TickComponent`
流式`SpawnActor`/`Destroy`——人口规模通常比建筑数量大得多，流式管理是默认选择，见
`Source/Forever/Framework/ForeverPopulaceFrameworkComponent.md`。

`UForeverPopulaceFrameworkComponent::TickComponent`判定"该显形"时`SpawnActor<
ACitizenElement>()`+立刻调用`Init(citizen, this)`；判定"该隐藏"时先读走当前世界坐标写回
`Citizen`，再`Destroy()`这个Actor。

## 关键设计

### 数据结构：每个Core实例对应一个强类型Actor子类

`UForeverPopulaceFrameworkComponent`自己维护`TMap<Citizen*, TObjectPtr<ACitizenElement>>
activeInstances`做映射，`ACitizenElement`不需要额外维护任何名字表——好处是每个citizen的
碰撞盒/网格/AI状态都能自然地长在它自己的Actor上。

### 移动模式跟着占有状态切换：构造函数`MOVE_None`，被占有切`MOVE_Walking`

未被占有的市民默认静止（`GetCharacterMovement()->SetMovementMode(MOVE_None)`），避免
`CharacterMovementComponent`自己的重力/地面检测把`Init()`摆好的位置挪走：

```cpp
virtual void PossessedBy(AController* NewController) override; // Super先增Input Mapping，再切MOVE_Walking
virtual void UnPossessed() override;                            // 先切回MOVE_None，再Super减Input Mapping
```

`ACitizenElement`自己只管`CharacterMovementComponent`的开关；Input Mapping Context的
增删完全不在这里处理，交给`Super`（`AForeverCharacter::PossessedBy`/`UnPossessed`）——
这样不管当前占有的是初始`ADefaultPawn`/`AForeverCharacter`还是某个`ACitizenElement`，
输入映射的增删逻辑只有一份，见`[[memory:possession_driven_input_context]]`。

### `nearbyCitizens`：静态列表，供`LogNearbyRelationships`调试用

静态成员`TArray<TWeakObjectPtr<ACitizenElement>> nearbyCitizens`记录"当前被占有对象（玩家
pawn，也可能是另一个citizen）附近的市民"——`OnOverlapBegin`/`OnOverlapEnd`维护这份名单。
之所以是静态成员而不是每个Actor各自的实例状态：`LogNearbyRelationships()`是静态方法，不知道
当前被占有的具体是哪个子类实例，只能通过一个全局入口去问"附近有没有市民"；用
`TWeakObjectPtr`是因为市民会被流式`Destroy()`，名单里的引用要能安全感知失效。

`ACitizenElement::LogNearbyRelationships()`遍历`nearbyCitizens`里仍然有效的条目，对每个人
调`Citizen::GetAcquaintances()`/`GetExperiences()`，把熟人关系强度（6个`RELATION_TYPE`
字段）+人际关系历史记录（按`Experience::GetCategory()`分派，见`experience.md`）拼成一行
描述，通过`UE_LOG(LogTemp, Log, ...)`打进log（数据量可能较大，不走屏幕调试消息，事后翻
`Saved/Logs/Forever.log`查）。目前没有任何按键绑定它，方法保留供调试用，见
`Player/ForeverCharacter.md`"T键上下车切换"一节（T键现在指向上下车，不指向这个方法）。

### 占位资产：`SKM_Manny_Simple`

和`AForeverCharacter.cpp`同款软路径（`/Game/Asset/Characters/Mannequins/Meshes/
SKM_Manny_Simple`），`ConstructorHelpers::FObjectFinder`在构造函数里加载，`GetMesh()->
SetRelativeLocation`用`-GetCapsuleComponent()->GetScaledCapsuleHalfHeight()`让mesh的
脚底正好落在capsule底部。动画蓝图也照抄`AForeverCharacter.cpp`挂了`ABP_Unarmed`——
`USkeletalMeshComponent`没有`AnimInstance`时会一直显示bind pose（T-pose），挂了之后至少
有一个正常待机姿势。当前阶段用UE默认小白人占位，以后换真实资产。

### 3D坐标"留空/首次随机/此后复用"——在这里落地，不在`Citizen`

`Citizen`（Core）只存储`hasPosition`+`posX/Y/Z`（地图单位），真正的判断/写入逻辑在
`Init()`里：
- `citizen->HasPosition()`为true：直接读`GetPosition()`转UE坐标（`*CITIZEN_WORLD_SCALE`）
  摆放，不再随机。
- 否则：用`Room::GetPosX/PosY()`（Floor局部坐标）+本文件维护的一份
  `ComputeCitizenWorldPosition`（和`BuildingElement.cpp`的`ComputeWorldPosition`公式
  一样，按既有约定各自维护一份，不额外抽公共头——**但函数名不能取成一样的**，见下
  "匿名namespace命名坑"）换算房间中心世界坐标，X/Y各加随机抖动（±0.2地图单位），Z用
  `Building::GetFloorBaseZ(room->GetLayer())`**再加一个`CITIZEN_GROUND_SLAB_THICKNESS`
  （20 UE单位）**——`BuildingElement.cpp`把Ground slab的中心摆在"`floorBaseZ`对应的楼层
  底部+半个slab厚度"，真正能站人的地板表面比`floorBaseZ`换算出来的"楼层底部"还要高一个完整
  slab厚度，不加这一段会正好陷进地板slab里那么深。最后再加一个capsule半高——
  `SetActorLocation`摆的是capsule中心，脚底(mesh)要正好落在楼板表面上。算出来**立刻**
  `citizen->SetPosition(...)`写回——"首次随机、此后复用"的记录在`Init()`这一步就完成，不用
  等销毁时才写。

### 匿名namespace命名坑：Forever模块是unity build，"各文件互不可见"这个假设不成立

`BuildingElement.cpp`/`ForeverZoneFrameworkComponent.cpp`等文件各自维护一份同名helper的
前提，是"不同翻译单元的匿名namespace不能跨文件共用"——这在**普通MSBuild项目**
（`Core`/`Dependence`/`Basic`）里成立，但**在UBT管理的`Forever`模块里不成立**：UBT的
adaptive unity build会把一批`.cpp`拼进一个生成的`Module.Forever.cpp`一起编译，被拼在一起
的文件的匿名namespace因此变成了同一个翻译单元，如果两个文件里出现同名同签名的匿名
namespace函数会直接报重定义编译错误。这个错误具体在哪次编译触发取决于UBT当次的"最近改动"
排除名单，**和你最后一次修改的代码可能完全无关**，容易误判。

教训：每个文件按约定各自维护一份小helper这个模式本身没问题（避免过早抽象/增加不必要的头
文件依赖），但**函数名必须在整个`Forever`模块内唯一**，不能只保证同一个文件内不重复；本
文件的版本因此叫`ComputeCitizenWorldPosition`，不是`ComputeWorldPosition`。

流式销毁时（`UForeverPopulaceFrameworkComponent::TickComponent`里），走远到销毁距离之外
会先读`GetActorLocation()`转回地图单位写回`Citizen`，再`Destroy()`——保证"反之直接在记录
的位置出现"：不管这个citizen被销毁过多少次，只要`hasPosition`已经为true，重新生成时永远
用最后一次的真实位置，不再触发随机抖动。

### `WalkTo`：沿现成路径点走，不用AIController/NavMesh

`UForeverPopulaceFrameworkComponent::RequestWalk`发现某个citizen当前有对应
`ACitizenElement`时会调`WalkTo(waypoints, destination)`——路径点已经由`Map::
FindPedestrianPath`（Dijkstra）算好，这里只负责"沿着这串世界坐标走过去"：
`SetMovementMode(MOVE_Walking)`+`SetActorTickEnabled(true)`（构造函数里
`PrimaryActorTick.bCanEverTick=true`但默认`SetActorTickEnabled(false)`——绝大多数
citizen静止不动，只有真正在走路时才需要每帧开销），`Tick()`里每帧朝
`pendingWaypoints[waypointIndex]`方向`AddMovementInput`（只判水平距离，阈值
`kWaypointArrivalThresholdUU`=80 UE单位，到达即前进到下一个路径点），全部走完切回
`MOVE_None`+关闭Tick+回调`framework->NotifyArrived(citizen, walkDestination)`更新
`Citizen::SetCurrentRoom`。**不用`AIController`/`NavMesh`**——路径已经是Core侧算好的现成
数据，`WalkTo`纯粹是沿点插值前进+转向，复用`CharacterMovement`只是为了保留碰撞/坡度处理。

**构造函数里必须显式打开`bRunPhysicsWithNoController`**——`UCharacterMovementComponent::
TickComponent`默认只有`Pawn::Controller`非空时才会调用`PerformMovement()`，这个项目没有
`AIController`，`WalkTo`带着走路的citizen按定义不会同时被玩家占有（`RequestWalk`发现被
占有会整个跳过调度），`Controller`因此永远是`nullptr`——不开这个开关的话`AddMovementInput`
是纯空操作，`MovementMode`显示`MOVE_Walking`、`Tick`每帧都在调用，但速度/位置纹丝不动，
市民会永久冻结在走路起点。

### `TeleportToRoom`/`ComputeRoomLandingSpot`：寻路失败时，可见的Actor也必须跟着挪

`RequestWalk`寻路失败（起点/终点没有导航节点，或图不连通）但这个citizen当前**有已生成的
`ACitizenElement`**时，不能像"没有Actor"那种情况一样只改`Citizen::SetCurrentRoom`——那样
Core状态已经"到家"了，但这个可见的Actor完全没人碰过，会一直冻结在原地不动。
`TeleportToRoom(Room* destination)`把Actor本身也瞬移过去：`SetActorLocation`+同步
`citizen->SetCurrentRoom(destination)`+`citizen->SetPosition(...)`。

落地位置的计算（房间中心+随机抖动+楼层高度换算，见上"3D坐标"一节）提炼成私有辅助
`ComputeRoomLandingSpot(Room* room, float& outWorldX, float& outWorldY, float& outWorldZ)`，
`Init()`换房间后首次实例化的分支和这里共用同一份实现。`room`为空或反查不到
`GetParentBuilding()`时返回`false`，调用方保留原有坐标不变——和`ComputeLogicalPosition`
同一条规则：**必须从`room->GetParentBuilding()`反查building，不能用
`citizen->GetBuilding()`**（那是"家"building，换房间之后不再保证同一栋楼），见
`ForeverPopulaceFrameworkComponent.md`。

### 靠近检测碰撞盒——和流式生成/销毁的距离判定是两回事

`proximityBox`是纯UE层的装饰性判定，用固定的UE单位常量（不走地图单位换算），和
`UForeverPopulaceFrameworkComponent`的`citizenSpawnDistance`/`citizenDespawnDistance`
（决定"这个citizen该不该存在于场景里"）完全独立——一个citizen存在于场景里之后，玩家还要
再靠得更近才会触发这个盒子的Overlap（"接近/离开"屏幕日志）。`BuildProximityBox()`挂在
`RootComponent`（`ACharacter`自带的`CapsuleComponent`）上，和`ABuildingElement::
BuildCollisionBox()`同一套写法：`"Trigger"`碰撞profile，`OnComponentBeginOverlap`/`End`
只使用`Init()`时预先烘焙好的`collisionLabel`（`FString`，从`citizen->GetName()`产出），
绝不在回调里解引用`citizen`；`OtherActor`按惯例过滤成只认
`UGameplayStatics::GetPlayerPawn(GetWorld(),0)`。

### `EndPlay`安全性——和`ABuildingElement`同一套原则

`ACitizenElement`/`UForeverPopulaceFrameworkComponent`/`AForeverFrameworkActor`是三个
独立的Actor/组件，`EndPlay`调用顺序不保证谁先谁后。`ACitizenElement`的安全性设计不依赖
这个顺序：只保证"自己的`EndPlay`一跑完，自己不会再解引用`citizen`"，所以`EndPlay`只把
`citizen`置空就足够安全（不在这里把当前位置写回`Citizen`——那个写回只在受控的运行时销毁
路径里由`UForeverPopulaceFrameworkComponent`主动做，关卡卸载时`Citizen`/`Populace`可能
已经先被析构了，见`ForeverFrameworkActor.md`）。

- **交互选项运行中变化**：`add_option`/`remove_option`执行后，`AForeverFrameworkActor::RefreshInteractOptions`按名字找到场景里的这个对象调`RefreshOptions()`：重新烘焙`cachedOptions`快照；玩家正站在旁边时用`UMeetOptionWidget::ReplaceOptions`把屏幕上的选项整组换掉。Overlap回调仍然只读快照、不解引用Core对象。

## 依赖关系

- 依赖：`Player/ForeverCharacter.h`（基类）、`Components/CapsuleComponent.h`/
  `GameFramework/CharacterMovementComponent.h`（`ACharacter`自带组件，
  `PossessedBy`/`UnPossessed`切换`MOVE_Walking`/`MOVE_None`）、
  `Source/Forever/Framework/ForeverPopulaceFrameworkComponent.h`（`framework`弱引用
  回调；`FindOrSpawnCitizenByName`会强制生成/复用`ACitizenElement`，见该文件.md）、
  `Source/Core/populace/citizen.h`（`Citizen`）、`map/building.h`/`map/room.h`
  （Core侧数据）、`Components/SkeletalMeshComponent.h`（占位mesh）、
  `Components/BoxComponent.h`（碰撞盒）、`Kismet/GameplayStatics.h`
  （`GetPlayerPawn`）、`Engine/Engine.h`（`GEngine->AddOnScreenDebugMessage`，接近/
  离开提示用）、`Source/Core/populace/experience.h`/`school.h`、
  `Source/Core/society/organization.h`（`LogNearbyRelationships`按`Experience`
  类别分派打印用）。
- 被谁依赖：`UForeverPopulaceFrameworkComponent::TickComponent`/
  `FindOrSpawnCitizenByName`（`SpawnActor<ACitizenElement>()`+`Init()`+`Destroy()`）、
  `UForeverStoryFrameworkComponent::ApplyControlChange`（剧情`change_control`指定切换
  控制权，间接通过`FindOrSpawnCitizenByName`拿到`ACitizenElement*`后`Possess`——T键现在
  指向上下车，不走这条路径，`Possess`目前只由剧情脚本触发）。
  `LogNearbyRelationships()`目前没有任何按键绑定它。

## 待办/后续阶段

- 未被占有的市民已经能按Job调度（上下班）走动（见上"`WalkTo`"一节），但这仅限于"有明确
  调度触发"的场景——没有Job、或者Job没有产生调度的市民仍然是`MOVE_None`静止不动，通用的
  "没事做的时候到处逛逛"这类自由游走AI还没有。
- 真实资产替换`SKM_Manny_Simple`占位。
- 靠近检测碰撞盒尺寸（`CITIZEN_PROXIMITY_HALF_XY`/`_Z`）是按经验给的初始值，可能需要按
  真实资产的实际比例微调。
- `LogNearbyRelationships()`按`nearbyCitizens`原有顺序（进入范围的先后）遍历打印，不做
  "离玩家最近排前面"这类排序，后续如有需要可以加。
