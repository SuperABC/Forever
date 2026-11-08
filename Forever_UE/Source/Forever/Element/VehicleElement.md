# VehicleElement

> **新增一种载具**（静态网格模型 -> 骨骼网格体 -> 蓝图 -> Basic 注册）的完整流程见 `Forever_UE/Tools/VehicleRig/README.md`，脚本也在那个目录。

## 职责

`AVehicleElement`：一辆车在场景里对应的 Pawn，提供所有车型公用的驾驶(Chaos 真实悬挂/轮胎/引擎物理)、相机、输入、靠近弹选项逻辑，不写死任何具体车型的外观。不直接继承 `AWheeledVehiclePawn`(它把 `USkeletalMeshComponent` 存在私有字段里，子类没法覆盖)，而是自己组合 `USkeletalMeshComponent` + `UChaosWheeledVehicleMovementComponent` + 相机/输入，效果等价。

**不要改成自研简化物理(`UStaticMeshComponent` + 手写 `AddForce`)来摆脱 Chaos 依赖**——试过，转弯/摩擦手感明显不如真实悬挂/轮胎模拟。代价是车身必须是带骨骼的资产。

## 关键设计

- **车身必须是骨骼网格**：`CanCreateVehicle()` 要求每个轮子绑定真实存在的骨骼名(`BoneName==NAME_None` 直接拒绝创建)，悬挂靠骨骼在 T-pose 上查弹簧安装点。
- **骨骼网格必须在构造函数阶段绑好，不能在 `Init()` 里动态 `LoadObject`**：`SpawnActor` 在 `Init()` 之前就已 `RegisterAllComponents()`，`UChaosWheeledVehicleMovementComponent` 创建物理车辆只有这一次机会，那时 `GetSkinnedAsset()` 为空会触发 ensure("Expected skinned asset when locating bone offset")+"Spring configuration is invalid"，事后补 `SetSkeletalMesh`+`RecreatePhysicsState()` 也无效。`SpawnActor<T>` 又不能给构造函数传参，所以**具体车型是继承本类的蓝图子类**：蓝图保存时骨骼网格(`CarMesh`)和 `VehicleMovement` 的 `WheelSetups[i].BoneName` 烘焙进子类 CDO，早于任何运行时 `SpawnActor`。本类构造函数只配 `WheelClass`(前/前/后/后)等共用部分。
- **`VehicleMod::blueprintPath`** 指向某个蓝图子类(形如 `/Game/.../BP_X.BP_X_C`，生成类名固定带 `_C`)；`UForeverTrafficFrameworkComponent` 在 `SpawnActor` 之前用 `LoadClass<AVehicleElement>()` 取类，失败只打警告并销毁 `Vehicle`，不崩溃；生成用 `AlwaysSpawn`(车碰撞体与当前 pawn 必然重叠，默认碰撞检测会拒绝生成)。新增车型不需要改 `Forever` 模块，只需蓝图 + 在 `Basic.dll`/mod dll 里注册新的 `VehicleMod`。
- **`bSimulatePhysics` 偏离原版**：构造函数里**直接写** `carMesh->BodyInstance.bSimulatePhysics = true`，不调 `SetSimulatePhysics()`(后者查物理材质，CDO 构造时 `GEngine` 未初始化会报错)。必须 true：底盘骨骼在物理资产里不一定是 Simulated，留 false 车能创建但不响应油门、不受重力(Chaos 发现刚体非 Dynamic 就跳过整个模拟)。
- **预置车辆生成后 `Init()` 里 `WakeAllRigidBodies()`**：无人操控时刚体睡眠，会悬在半空直到第一次踩油门。
- **输入**：`Move` 按 `movement.Y` 正负分别转成油门/刹车(`SetDriveInput`)，**不能把 S 当负油门**：`bReverseAsBrake=true` 下油门/刹车是两个独立踏板，车速高时 S 是刹车，刹停后继续按才挂倒挡。`StopMove` 绑 `Completed`/`Canceled` 归零(`Triggered` 在输入回到 0 后不再触发)。`Handbrake`/`StopHandbrake`(默认空格，与市民 `Jump` 共键，各自只在自己的 Pawn 绑定)绑 `Started`/`Completed`+`Canceled`；四个轮子只有后轮 `bAffectedByHandbrake`。`ExitVehicle`(Q)转发给 `UForeverTrafficFrameworkComponent::ExitVehicle`。玩家 WASD 与 AI Controller(`AVehicleCommandController`)共用 `SetDriveInput`/`SetHandbrake`。
- **相机**：`cameraBoom` 相对原点抬高 50cm(骨骼原点通常在轮子接地点，不抬镜头会缩进底盘；过大则翻车时相机钻进地里)，个别车型可在蓝图子类覆盖。
- **下车姿态只继承 Yaw**：`ToggleVehicle` 下车分支用 `FRotator(0, yaw, 0)` 给 `previousPawn` 并显式 `SetControlRotation`(翻车后 Pitch/Roll 任意，人应站直；`ControlRotation` 存在 Controller 上，不随 Possess 清零)。
- **`UnPossessed`**：玩家驾驶(非搭乘)失去控制时清零油门/转向并拉满刹车(油门是持久状态，不清零会一直冲)；AI Controller 换手不做，以免打断 AI。
- **车轮动画**：`carMesh->SetAnimInstanceClass(UVehicleWheelAnimInstance)` 必须在构造函数里设，见 `VehicleWheelAnimInstance.md`。轮子类见 `VehicleWheelFront.md`/`VehicleWheelRear.md`(`bAffectedByEngine` 必须显式设置)。
- **引擎调参**：`DifferentialSetup`/`TransmissionSetup`/`SteeringSetup` 保持 `InitDefaults()`(后轮驱动+自动挡+速度相关转向)；`EngineSetup.TorqueCurve` 默认是空曲线(归一化时除 0 出 NaN)，构造函数手写了一条普通形状的曲线(0/1890/5730rpm 对应 400/500/400)，加速手感不满意先调这里。
- **交互选项**：`BuildProximityBox` 只在有脚本或有选项时建盒，之后选项出现由 `RefreshOptions` 补建；`category!="car"`(公交)触发盒取 `carMesh` 局部包围盒外扩(XY150/Z50)，轿车固定 250x250x120。Overlap 回调只读 `vehicleNameOnly`/`cachedOptions` 快照，不解引用 `Core` 对象；**只有玩家当前的 `AForeverCharacter` 走近才弹**(否则开车靠近另一辆车也会弹"上车")。`add_option`/`remove_option` 后 `AForeverFrameworkActor::RefreshInteractOptions` 调 `RefreshOptions()` 重新烘焙，玩家在旁边时用 `UMeetOptionWidget::ReplaceOptions` 整组换掉(搭乘中改为先 `RemoveName` 再加，因为 `ReplaceOptions` 在名下无按钮时是空操作)。`EnsureOptionsForPlayer(player)`：`ExitVehicle` 下一帧调用，玩家已在盒内就补选项(下车瞬间 Begin 事件时序不可靠，飞机实测过)。`EndPlay` 被销毁时撤掉屏幕上这辆车名下的选项。
- **视角始终摆正**：`PawnClientRestart` 里(不能在 `PossessedBy`：`APlayerController::OnPossess` 随后会 `SetControlRotation(车身姿态)` 覆盖)把控制旋转重置为 `(pitch -15, 车身yaw, roll 0)`，避免车歪/翻着上车时视角继承车身姿态；Look 只改 yaw/pitch，所以 roll 不会再出现。（弹簧臂原点偏移 50cm 仍跟车身转，翻车时仅有这点偏移。）
- **搭乘模式**(`SetRideMode(true)`，Possess 之前设置；线路公交近处让玩家当乘客)：`PossessedBy` 只加视角 Context，常驻显示选项(如停站的"下车")；`Move`/手刹/Q 忽略；`MeetOptionUp/Down/Select` 绑定(同 `ATransitVehicleElement`)；`UnPossessed` 撤选项并清标志。
- **运动学/物理切换**：`SetKinematicMode(true)` 清输入并关物理(位姿由 Controller 写)；`ResumePhysics(velocity)` 重新开启并 `DisableWheelBodies()`(`SetSimulatePhysics(true)` 会把 Chaos 特意关掉的轮子刚体重新打开)。`ComputeWheelbase()`/`GetFrontAxleLocation()`/`GetMaxSteerAngleDegrees()`(读 `WheelSetups[0]` 轮子类 CDO)/`GetForwardSpeed()`(运动学下为 0)供 Controller 用；约定 `WheelSetups[0..1]` 前轮、`[2..3]` 后轮。
- **重心与轮子半径**(蓝图按车型填，在 `BeginPlay` 应用而不是 `Init`，因为直接摆在关卡里的车不走 `Init`)：`centerOfMassOffset`(车身局部坐标，非零才 `SetCenterOfMass`，Z 取负=下移防侧翻；公交的下移 60cm 在 `BP_BusElement` 里填 `(0,0,-60)`，基类没有车型特判)；`wheelRadius`(>0 时对每个轮位 `SetWheelRadius`，因为 Chaos 建物理读的是轮子类 CDO 的半径，没法按实例改；填模型轮胎实际半径，只改半径，悬挂行程等仍读类默认值)。
- **Core 位姿同步**：`Tick` 把位姿写回 `Vehicle::SetTransform`，有 `Route` 的车(线路公交)用地图单位，除以 1000。
- **飞机子类**：`APlaneElement` 继承本类(复用 Chaos 轮子/骨骼网格/近身选项/上下机/存档位置)，换成 3 个轮子，不走本类 `SetupPlayerInputComponent`，见 `PlaneElement.md`。`ExitVehicle()`/`Handbrake()`/`StopHandbrake()` 和各输入成员都是 protected，供子类直接调用。

## 依赖关系

- 依赖：`Core/traffic/vehicle.h`(`Vehicle`，不持有所有权)、`ChaosVehicles` 模块、`VehicleWheelFront/Rear`、`VehicleWheelAnimInstance`、`Input/ForeverKeyBindingSubsystem`(`ExitVehicle`/`Handbrake`/`MeetOption*`)、`Content/Blueprint/Player/Input/{IA_Move,IA_Look,IA_MouseLook,IMC_Default,IMC_MouseLook}`(与 `AForeverCharacter` 共用)。不依赖任何具体车型的骨骼网格资产。
- 被谁依赖：`VehicleMod::blueprintPath`(指向蓝图子类)、`UForeverTrafficFrameworkComponent`(`LoadClass`+`SpawnActor`+`Init`+上下车)、`AVehicleCommandController`、`APlaneElement`/`ATransitVehicleElement` 的同类逻辑。

## 待办/后续阶段

- 无。
