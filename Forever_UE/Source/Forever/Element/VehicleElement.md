# VehicleElement

## 职责

`AVehicleElement`：一辆车在场景里对应的Actor。照抄引擎自带Chaos Vehicle
（`AWheeledVehiclePawn`+`UChaosWheeledVehicleMovementComponent`）真实物理驾驶手感
（真实的悬挂弹簧、轮胎摩擦圆、引擎/变速箱扭矩曲线）。不直接继承`AWheeledVehiclePawn`
（它固定用`USkeletalMeshComponent`存在私有字段里，没有开放子类覆盖的方式），而是自己
组合`USkeletalMeshComponent`+`UChaosWheeledVehicleMovementComponent`+相机/输入，效果
等价。

历史：这套Chaos插件方案是最初的实现；本项目中途一度整体重写成自研简化物理
（`UStaticMeshComponent`+手写`AddForce`），目的是摆脱插件依赖，但实测驾驶手感（尤其
转弯/摩擦力）明显不如真实悬挂/轮胎模拟，最终决定改回直接使用插件本身，接受"车身必须是
带骨骼的资产"这个前提，把工作量放在教用户怎么给自己的车模型绑骨骼上（见本文档最后一节）。

## 骨骼网格：为什么车身不能是普通静态网格

`UChaosWheeledVehicleMovementComponent::CanCreateVehicle()`硬性要求每个轮子都绑定一个
**真实存在的骨骼名**（`BoneName==NAME_None`直接拒绝创建整台车）。悬挂系统靠这个骨骼名
在骨骼网格的T-pose上查出弹簧安装点（`LocateBoneOffset()`/`GetWheelRestingPosition()`），
这个查找只发生在初始化/重建阶段，不是每帧都查，但没有骨骼就完全无从谈起。

要换成自己的车模型，需要给它做骨骼绑定——见本文档最后一节的教程。

## 骨骼网格必须在组件注册之前就绑好——踩过的坑

最初想法是运行时动态`LoadObject`（和这个项目其他资源一样"mod自己的字段、Core只读
转发"的模式），在`Init()`里赋值。实测这条路走不通：

1. **`SpawnActor()`在`Init()`跑之前就已经完成了组件注册**——`AActor::
   PostSpawnInitialize()`会无条件调用`RegisterAllComponents()`（不受`bDeferConstruction`
   影响，`SpawnActorDeferred`也只是推迟`ExecuteConstruction`/`PostActorConstruction`
   这两步，`RegisterAllComponents()`在这两步之前就已经跑完，见`Actor.cpp::
   PostSpawnInitialize()`）。`carMesh`这时还没有骨骼网格，
   `UChaosWheeledVehicleMovementComponent`第一次（也是唯一一次）尝试创建物理车辆时，
   `LocateBoneOffset()`发现`Mesh->GetSkinnedAsset()`是空，四个轮子的弹簧位置全部退化
   成同一个原点——触发引擎自己的ensure（"Expected skinned asset when locating bone
   offset"）+`LogVehicleUtility`的"Spring configuration is invalid"警告，车辆物理
   相当于没建成功。
2. 事后在`Init()`里补`SetSkeletalMesh(mesh)`+`vehicleMovement->RecreatePhysicsState()`，
   试图"重来一次"——没用，还是同样的ensure/警告。
3. **最终结论：骨骼网格必须在构造函数里就绑好**——**这是Chaos WheeledVehicle这套
   机制本身的限制，不是这个类自己的设计选择**。而`SpawnActor<T>()`不支持给构造函数
   传自定义参数，所以"用哪个具体外观"这件事没法在生成之后（`Init()`）或构造函数内部
   读某个运行时参数来决定，只能靠"生成之前就选好用哪个`UClass`"——见下一节。

## 多车型共存：`AVehicleElement`是C++基类，具体车型是蓝图子类

这个类本身**不写死任何具体车型的外观**——`carMesh`不在构造函数里赋值
`SkeletalMesh`，`vehicleMovement->WheelSetups`的四个`BoneName`也留空，只把
`WheelClass`（前/前/后/后）这类所有车型大概率共用的部分配好。具体每种车型（轿车、
卡车……）是**继承`AVehicleElement`的蓝图(Blueprint)子类**：在蓝图编辑器的
Components面板选中`CarMesh`，把`Skeletal Mesh`设成这个车型自己的骨骼网格资产；
选中`VehicleMovement`，展开`Wheel Setups`，四个元素的`Bone Name`填这个资产实际的
骨骼名。这些都是引擎自带的`EditAnywhere`属性，不需要写任何C++代码。

这样设计能绕开上一节的限制，是因为**蓝图子类的属性覆盖在蓝图编辑器里保存资产的那一刻
就已经烘焙进了这个蓝图类自己的CDO**，早于任何运行时`SpawnActor`调用——这是标准的、
UE官方推荐的"一个C++基类+多个蓝图子类"车辆配置模式（很多商业游戏的多车型系统都是
这么做的），不是这个项目发明的特例。

`VehicleMod`（见`vehicle_mod.h`）只存一个字符串字段`blueprintPath`，指向某个具体的
蓝图子类资源路径（比如`/Game/Blueprint/Vehicle/BP_VehicleBasic.BP_VehicleBasic_C`——
注意蓝图生成类的对象名固定加`_C`后缀）。`UForeverTrafficFrameworkComponent::
ToggleVehicle`的上车分支在调用`SpawnActor`**之前**，先用
`vehicle->GetBlueprintPath()`+`LoadClass<AVehicleElement>()`拿到具体该用哪个蓝图
类，再拿这个类去`SpawnActor`（原来传的是`AVehicleElement::StaticClass()`）——这时
已经通过`traffic->CreateVehicle(...)`拿到了具体的`Vehicle`实例，不需要像早前那版
"构造函数里现读现造一个临时mod"那样绕弯子。`LoadClass`失败（蓝图资产还没创建、路径
写错）会打警告+销毁这个`Vehicle`+直接返回，不会崩溃，见
`ForeverTrafficFrameworkComponent.cpp`。

**新增一种车型完全不需要碰`Forever`这个UE模块的代码**：做一个继承`AVehicleElement`
的新蓝图（换骨骼网格资产、改轮子骨骼名，具体步骤见本文档最后一节）+ 在`Basic.dll`
（或将来别的mod dll）里注册一个新的`VehicleMod`子类指向这个蓝图路径——只需要
`Basic`本来就是运行时加载的DLL这一层重新编译，没有车型数量上限。

## `bSimulatePhysics`：为什么和`AWheeledVehiclePawn`原版不一样

`AWheeledVehiclePawn`构造函数里`Mesh->BodyInstance.bSimulatePhysics = false;`——这是
对的，但前提是配套的物理资产（Physics Asset）本身把底盘骨骼的`PhysicsType`设成了
`Simulated`，组件级别这个开关会被物理资产per-bone的设置覆盖，`false`只是"没有资产
覆盖时的默认值"。

`SportsCar_PhysicsAsset`是给另一套"Modular Vehicle"系统准备的，底盘骨骼在那份资产里
大概率是`Kinematic`/`Default`（继承组件默认）——实测确认：`bSimulatePhysics`留`false`
时车辆能正常创建（骨骼/轮子位置都对，没有ensure警告），但**完全不响应油门/转向，Z坐标
也不受重力影响**——因为`FChaosVehicleManagerAsyncCallback`每帧更新前会检查
`Handle->ObjectState() != Chaos::EObjectStateType::Dynamic`，不是Dynamic状态直接跳过
整个模拟。这里改成`carMesh->BodyInstance.bSimulatePhysics = true;`强制底盘是动态刚体，
绕开"借用的资产不是为这套系统设计的"这个限制——这是明确偏离`AWheeledVehiclePawn`参考
实现的一处。

**同样要注意**：这里用的是**直接写字段**（`carMesh->BodyInstance.bSimulatePhysics =
true;`），不是调用`SetSimulatePhysics()`函数——这个函数内部会立刻查物理材质
（`GetSimplePhysicalMaterial`），而`AActor`的C++构造函数除了`SpawnActor`时会跑，
引擎启动/模块加载构建CDO时也会跑一次，那时候`GEngine`还没初始化，调用会报
"GEngine not initialized! Cannot call this during native CDO construction"。

## 生成时的碰撞检测——`AlwaysSpawn`

`carMesh`是有真实碰撞的骨骼网格（`Vehicle_ProfileName`碰撞预设），车辆生成的位置就是
当前pawn自己所在的位置，两者碰撞体在那一刻必然重叠。默认的`SpawnActor`碰撞检测会因为
"生成点被占用"直接拒绝生成（返回`nullptr`）。
`UForeverTrafficFrameworkComponent::ToggleVehicle`的上车分支用
`FActorSpawnParameters::SpawnCollisionHandlingOverride =
ESpawnActorCollisionHandlingMethod::AlwaysSpawn`跳过这次检测——马上就要隐藏+关掉原
pawn的碰撞了，生成那一刻的短暂重叠不需要真的处理。

## `Move`：转发给`UChaosVehicleMovementComponent`的输入接口——油门/刹车是两个独立的量

```cpp
void AVehicleElement::Move(const FInputActionValue& value) {
	const FVector2D movement = value.Get<FVector2D>();
	if (movement.Y >= 0.f) {
		vehicleMovement->SetThrottleInput(movement.Y); // W
		vehicleMovement->SetBrakeInput(0.f);
	} else {
		vehicleMovement->SetThrottleInput(0.f);
		vehicleMovement->SetBrakeInput(-movement.Y);   // S
	}
	vehicleMovement->SetSteeringInput(movement.X);         // A/D
}
```

**最初的版本把`movement.Y`直接原样传给`SetThrottleInput`（S键=负油门），结果车完全没法
倒车**——`UChaosVehicleMovementComponent`默认`bReverseAsBrake=true`，油门/刹车被设计成
像真车的两个独立踏板，不是一个正负号共用的单轴：换挡逻辑只看`RawBrakeInput`是否大于0来
判断"该挂倒挡了"。现在按`movement.Y`的正负分别转发到`SetThrottleInput`/`SetBrakeInput`：
车速较高时按S是正常刹车，刹停之后继续按住S才会挂倒挡、真正往后开——这是
`CalcThrottleBrakeInput()`里`bReverseAsBrake`分支自己的设计，不是这个类额外发明的规则。

真实的引擎/变速箱/悬挂/轮胎摩擦力模拟接管"前后加速度"和"转向"的手感（包括速度相关的
转向灵敏度衰减、悬挂压缩带来的过弯侧倾、轮胎抓地力带来的自然不侧滑），这个函数本身不
再做任何运动学计算。`StopMove`绑在`ETriggerEvent::Completed`/`Canceled`——WSAD松开后
Enhanced Input的`Triggered`事件不会再触发，不显式归零的话`SetThrottleInput`/
`SetBrakeInput`/`SetSteeringInput`会停留在松手前的最后一个值上。

## 手刹：空格键

`ForeverKeyBindingSubsystem`新增了一条`{ TEXT("Handbrake"), EKeys::SpaceBar }`绑定
定义，和`ACitizenElement`用的`"Jump"`共用同一个物理键（空格）——两者是不同的
`UInputAction`对象，只会被各自的Pawn在`SetupPlayerInputComponent()`里显式绑定消费，
同一个键同时映射到多个动作在Enhanced Input里不冲突。`Handbrake`/`StopHandbrake`绑在
`Started`/`Completed`+`Canceled`，直接转发`vehicleMovement->SetHandbrakeInput(true/
false)`——手刹是纯粹的按下/松开两态开关，不像`Move`那样有个连续的模拟量，不需要走
`Triggered`事件。四个轮子里只有`VehicleWheelRear`(`bAffectedByHandbrake=true`)会真正
响应手刹力矩，`VehicleWheelFront`保持`false`。

## 相机默认卡在车底——`cameraBoom`需要一个高度偏移

车辆骨骼网格的组件原点大概率在车轮接地点附近（常见的车辆骨骼摆法），`cameraBoom`
`SetupAttachment(RootComponent)`之后如果不加相对位置偏移，摄像机环绕的中心就是"地面
高度"而不是"车身高度"，车旋转/后退时镜头会缩进车身底盘以下。构造函数里
`cameraBoom->SetRelativeLocation(FVector(0.f, 0.f, 50.f))`把环绕中心抬高一点作为
所有车型共用的默认值——具体某个车型的车身比例差异较大（比如卡车比轿车高很多）的话，
`RelativeLocation`同样是`EditAnywhere`属性，可以在对应的蓝图子类里针对性覆盖，不需要
改这个基类。

## 下车姿态只继承车身的Yaw，不继承Pitch/Roll——翻车后视角别跟着歪

`UForeverTrafficFrameworkComponent::ToggleVehicle`的下车分支只取车身的Yaw构造一个
`FRotator(0, yaw, 0)`给`previousPawn`——车翻了/侧倒之后车身的Pitch/Roll可能是任意
角度，人下车应该始终站直，不应该继承车身的翻滚姿态（实测反馈：翻车后下车，市民视角被
转了90度）。同时显式`controller->SetControlRotation(FRotator(0, yaw, 0))`——
`ControlRotation`存在`Controller`身上，不随`Possess`切换自动清零。

## 车轮滚动/转向动画——没有Animation Blueprint，纯C++重写`Evaluate()`

车轮骨骼只是物理悬挂用的挂点，本身不会因为物理创建成功就自动带动可见网格转起来——
`USkeletalMeshComponent`的渲染姿态由`AnimInstance`产出，没有`AnimInstance`（或
`AnimInstance`没有真正修改骨骼）就永远停在bind pose。

标准做法是配一个继承`ChaosVehicles`插件自带`UVehicleAnimationInstance`的Animation
Blueprint，在编辑器里拖一个`Wheel Handler`（`FAnimNode_WheelController`）节点。这个
项目没有走这条路——`SKM_SportsCar`自带的`SportsCar_AnimBP`是插件"Modular Vehicle"
系统自己的资产，和这里用的经典`UChaosWheeledVehicleMovementComponent`不兼容；从零
搭一个新的Animation Blueprint需要在编辑器里手工建图，这一阶段全部改动想保持纯C++。

`Source/Forever/Element/VehicleWheelAnimInstance.h/.cpp`：

- `UVehicleWheelAnimInstance : public UAnimInstance`：`NativeInitializeAnimation()`
  从`GetOwningActor()`（也就是这个`AVehicleElement`）上按类型找
  `UChaosWheeledVehicleMovementComponent`（和引擎自带`UVehicleAnimationInstance::
  NativeInitializeAnimation()`一样的写法，只是这里owner不是`AWheeledVehiclePawn`）。
- `FVehicleWheelAnimInstanceProxy : public FAnimInstanceProxy`：
  - `PreUpdate()`（游戏线程，每帧调用一次）：按`vehicleMovement->WheelSetups`遍历每个
    轮子，读`UChaosVehicleWheel::GetRotationAngle()`（滚动角度）/`GetSteerAngle()`
    （转向角度，非前轮恒为0）缓存成一份纯数据（`FVehicleWheelPose`：`FBoneReference`+
    两个float）。
  - `Evaluate(FPoseContext&)`：**重写引擎自带`FAnimInstanceProxy::Evaluate()`
    （`AnimInstanceProxy.h`里默认实现`{ return false; }`，是官方留给"完全代码驱动、
    不需要蓝图节点图"场景的正式扩展点）**，`ResetToRefPose()`之后，对每个缓存的轮子
    姿态用`FBoneReference`查骨骼在当前`FBoneContainer`里的`FCompactPoseBoneIndex`，
    把`FRotator(rotationAngle, steerAngle, 0)`转成的`FQuat`**后乘**到该骨骼原有的
    本地旋转上（`BoneTransform.GetRotation() * RollSteerQuat`）。**踩过的坑**：
    最初写成预乘（`RollSteerQuat * BoneTransform.GetRotation()`），实测轮子转起来
    七扭八歪——预乘是把这个旋转当成"父骨骼坐标系下的旋转"叠加，而"绕轮子自己的轴滚动/
    转向"这个概念应该表达在轮子骨骼自己的本地(bind pose)坐标系里，必须后乘：先用骨骼
    原有的旋转把"轮子自己的坐标系"变换到父骨骼坐标系，再在轮子自己的坐标系里叠加滚动/
    转向，顺序不能反。`FAnimNode_WheelController::EvaluateSkeletalControl_AnyThread`
    （`AnimNode_WheelController.cpp`）效果上做的是同一件事，只是它工作在组件空间，
    需要先`ConvertCSTransformToBoneSpace`把变换重新表达到骨骼自己的坐标系里才能这样
    叠加，再`ConvertBoneSpaceTransformToCS`转回去；这里的`Evaluate()`本来就是本地
    （父骨骼空间）的`FPoseContext`，不需要那一趟来回转换，但叠加顺序（后乘）是同一个
    道理，不能图省事写成预乘。

`carMesh->SetAnimInstanceClass(UVehicleWheelAnimInstance::StaticClass())`和骨骼网格/
物理设置一样必须在构造函数里设。

## 车辆调参：`EngineSetup`/`DifferentialSetup`/`TransmissionSetup`/`SteeringSetup`

`DifferentialSetup`/`TransmissionSetup`/`SteeringSetup`保持
`UChaosWheeledVehicleMovementComponent`自己的`InitDefaults()`（后轮驱动+自动挡+速度
相关转向灵敏度，数值是引擎给的通用默认）。`EngineSetup.TorqueCurve`默认是**空曲线**
（`FRuntimeFloatCurve`没有任何key）——`FillEngineSetup()`会拿这条曲线的
`GetValueRange()`当分母做归一化，空曲线的`MaxVal`是0，直接除0出NaN扭矩。构造函数里
手写了一条形状普通的扭矩曲线（低转速扭矩小、中段最大、高转速回落一点），数值不是照抄
某个真实发动机，只是给个能跑起来的合理默认——如果加速手感不满意，这是第一个该调的参数。

`Source/Forever/Element/VehicleWheelFront.h/.cpp`/`VehicleWheelRear.h/.cpp`：继承
`UChaosVehicleWheel`，前轮`bAffectedBySteering=true`、后轮`bAffectedByHandbrake=true`，
照抄引擎Vehicle模板`TP_VehicleAdvWheelFront`/`Rear`的最小配置。

**踩过的坑：`bAffectedByEngine`不显式设的话默认是`false`，油门完全没反应**——
`UChaosVehicleWheel`基类构造函数没有给这个字段赋初值（bool零初始化=false），
`AxleType`也默认是`EAxleType::Undefined`。`SetupVehicle()`里只有
`AxleType != Undefined`时才会按`DifferentialSetup.DifferentialType`重新计算
`EngineEnabled`；`Undefined`时直接用`bAffectedByEngine`原值（也就是`false`）。
结果是：车辆创建完全正常（`GetNumWheels()`==4、没有任何ensure/警告），转向、手刹这些
也都正常，但不管给多大油门`GetForwardSpeed()`都纹丝不动——因为压根没有一个轮子的
`EngineEnabled`是true，油门的扭矩传不到任何轮子上。修法：在`VehicleWheelRear.cpp`
里显式`bAffectedByEngine = true;`（配合`DifferentialSetup`默认的`RearWheelDrive`），
`VehicleWheelFront.cpp`里显式写`bAffectedByEngine = false;`只是为了不留疑问。这是
这次自己拼装`WheelSetups`时才会踩的坑——如果是在编辑器里用UE自带的Vehicle模板蓝图，
`TP_VehicleAdvWheelFront/Rear`这两个蓝图资产里已经在细节面板上点好了这个勾选框，
不会有人踩到这个坑。

## 依赖关系

- 依赖：`Source/Core/traffic/vehicle.h`（`Vehicle`，不持有所有权，只用于挂载期间的
  transform同步）、`ChaosVehicles`模块（`UChaosWheeledVehicleMovementComponent`/
  `UChaosVehicleWheel`，见`Source/Forever/Forever.Build.cs`）、
  `Source/Forever/Element/VehicleWheelFront.h`/`VehicleWheelRear.h`、
  `Source/Forever/Element/VehicleWheelAnimInstance.h`/`.cpp`（`carMesh`的
  `AnimInstanceClass`，纯C++驱动车轮滚动/转向动画）、
  `Input/ForeverKeyBindingSubsystem.h`（`Test`/`Handbrake`动态`UInputAction`及其
  Mapping Context）、`Content/Blueprint/Player/Input/Actions/{IA_Move,IA_Look,
  IA_MouseLook}`、`Content/Blueprint/Player/Input/{IMC_Default,IMC_MouseLook}`
  （和`AForeverCharacter`共用同一份资源）。**不再直接依赖任何具体车型的骨骼网格
  资产**——那些由继承这个类的蓝图子类各自持有。
- 被谁依赖：`Source/Dependence/traffic/vehicle_mod.h`（`VehicleMod::blueprintPath`
  指向某个继承这个类的蓝图）、`Source/Forever/Framework/
  ForeverTrafficFrameworkComponent.cpp`（`ToggleVehicle`负责按`blueprintPath`
  `LoadClass`+`SpawnActor`(带`AlwaysSpawn`碰撞处理)+`Init`+`Destroy`）。

## 新增一种车型：怎么把静态网格做成带骨骼的`USkeletalMesh`+蓝图子类

第一步是3D建模/绑骨骼的工作，需要在Blender（免费、最常用）里手动完成，UE编辑器本身
没有"把静态网格自动转成带骨骼车辆"的功能。用户提供的静态小车模型
（`Content/3rdParty/Cars_for_Arcade_Demolition_Racing_Games/StaticMeshes/Car.uasset`）
没有骨骼、没有轮子分件，是这个流程要处理的典型例子。核心步骤：

1. **拿到原始可编辑模型文件**：`Car.uasset`是已经导入UE的静态网格，没法反向导出回
   可编辑的分件模型。需要找当初下载这个资源包时的原始源文件（通常是`.fbx`/`.blend`/
   `.obj`）。很多"arcade car pack"这类资源本身就带分开的车身/四个轮子网格，只是导入
   UE时被合并/只导入了合并版本——如果原始下载包里有分件版本，直接用；如果确实只有一个
   合并整体的模型，需要在Blender里手动把车身和四个轮子区域"分离"成独立的mesh对象
   （进入Edit Mode，框选轮子的面，按`P`键选`Selection`拆出来）。
2. **创建骨架（Armature）**：`Add` → `Armature`，建5根骨骼——1根根骨骼（车身/底盘用）+
   4根轮子骨骼（每根骨骼原点摆在对应轮子的几何中心，这个位置精度会直接影响悬挂/转向
   的观感）。
3. **绑定（Parenting）**：车身mesh整体选中后再Shift选中根骨骼，`Ctrl+P` →
   `Armature Deform` → `With Empty Groups`，然后在车身的Vertex Groups里把对应根骨骼
   的组权重设成1.0（车辆是刚体绑定，不需要平滑蒙皮/骨骼混合权重，比人物角色绑骨骼
   简单很多，每个mesh 100%属于一根骨骼即可）；每个轮子mesh同样方式各自`Parent`到
   对应的轮子骨骼。
4. **导出FBX**：`File` → `Export` → `FBX`，确认勾选了骨架/Armature数据一起导出。
5. **导入UE**：Content Browser里`Import`，导入选项里骨架网格（Skeletal Mesh）相关的
   选项要打开（不是Static Mesh），UE会自动生成骨骼网格+骨架(Skeleton)资产；勾选
   `Create Physics Asset`自动生成一份基础物理资产（之后可以在Physics Asset Editor
   里微调每个刚体的碰撞体形状）。
6. **记下导入后四个轮子骨骼的实际名字**（Blender里建骨骼时起的名字，导入UE后一般会
   保留）。
7. **新建一个继承`AVehicleElement`的蓝图**：Content Browser里右键→`Blueprint
   Class`，父类搜`AVehicleElement`（如果搜不到，确认`Forever`模块已经编译过这次
   改动），保存到比如`Content/Blueprint/Vehicle/BP_YourCar`。打开这个蓝图，
   Components面板选中`CarMesh`，把`Skeletal Mesh`设成第5步导入的骨骼网格；选中
   `VehicleMovement`，展开`Wheel Setups`，四个元素的`Bone Name`分别填第6步记下的
   骨骼名（下标顺序和C++构造函数里`WheelClass`指派的前前后后对应，不需要重新选
   `Wheel Class`）。编译+保存。
8. **在Basic.dll（或新mod dll）里注册一个新的`VehicleMod`子类**，`blueprintPath`
   指向第7步保存的蓝图（形如`/Game/Blueprint/Vehicle/BP_YourCar.BP_YourCar_C`——
   注意蓝图生成类的对象名固定加`_C`后缀），照抄`VehicleBasic`的结构——**只需要重新
   编译`Basic.dll`（`Source/Framework.sln`的`Basic`工程），不需要碰`Forever`这个
   UE模块**，见上面"多车型共存"一节。

这一步做完之后可以考虑把`ChaosModularVehicleExamples`这个插件从`Forever.uproject`里
禁用（只是用来提供`SKM_SportsCar`兜底测试车型，不再需要它之后没有别的依赖）。
