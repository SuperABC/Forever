# AirplaneElement.h / AirplaneElement.cpp / AirplaneWheel.h / AirplaneWheel.cpp

## 职责

可驾驶飞机：停在机场停机位里、玩家走近选"驾驶"就能开的那架。路线上的飞机是运动学的`ATransitVehicleElement`，和它无关（两者共用`PlaneVehicle`这个车型，靠`vehicle_plane.script`的`on_route`区分谁提供"驾驶"选项）。

## 关键设计

- **继承`AVehicleElement`**：白拿Chaos轮子(转向/滚动/手刹)、骨骼网格+物理、`Init`、近身"驾驶"选项、`ExitVehicle`、`Tick`写回位置、`LoadClass<AVehicleElement>(blueprintPath)`那条生成/按距离流式链路，`ForeverTrafficFrameworkComponent`一行没改。构造函数里把轮子改成3个：`[0]鼻轮(UAirplaneWheelNose，转向)`、`[1][2]主轮(UAirplaneWheelMain，手刹/刹车)`——蓝图`WheelSetups`的`BoneName`按这个顺序填。轮子都不带动力(`bAffectedByEngine=false`)，Chaos油门恒为0；关掉了Chaos自带的`DragCoefficient/DownforceCoefficient`，免得和手写阻力叠加。
- **输入**：`SetupPlayerInputComponent`绕过基类(直接调`APawn::`)，自己绑：`IA_Move`(WASD)→`MoveAirplane`，`IA_Look/IA_MouseLook`→环视，`ExitVehicle`(Q)/`Handbrake`(Space)沿用，新增`ThrottleUp/ThrottleDown`(滚轮，`ForeverKeyBindingSubsystem`，每格`powerStep`=5)。W=机头向下，S=机头抬升(`pitchCmd=-Y`)，D/A=向右/向左滚转(`rollCmd=X`)，**A/D同时**也给`SetSteeringInput`：地面轮子着地转向有效、滚转被忽略；空中轮子悬空转向无效、滚转有效，互不影响(`IsAirborne()`=没有任何轮子着地，轮子没配好时当作在空中)。
- **飞行力学**(每帧`Tick`，对`carMesh`施加**加速度**：`AddForce/AddTorqueInRadians`的`bAccelChange=true`，忽略质量，系数和飞机质量无关，单位cm/s²与rad/s²)：
  - 推力：机头方向，大小`power/100*maxThrustAccel`；
  - 阻力：`-k|v|v`，`k=maxThrustAccel/topSpeed²`(满动力时推力=阻力，所以`topSpeed`就是满动力极速)；
  - 升力：沿机体"上"，`g*(vF/stallSpeed)²`，`vF`是机头方向速度分量，`stallSpeed`时刚好=重力，封顶`maxLiftAccel`；倒飞时朝下，要推杆才能保持高度；
  - 速度朝机头对齐(`alignRate`，空中，权限随空速增长)：让飞机"贴着机头飞"，抬头就爬升；
  - 风标(`weathervaneAccel`，空中)：机头朝速度方向转，倾斜转弯时机头跟着转向；
  - 舵面角加速度：俯仰`pitchAccel`、滚转`rollAccel`，权限随空速线性增长到`controlSpeed`(低速几乎打不动舵)；
  - 角速度阻尼(机体局部三轴，`pitch/roll/yawDamping`)：松杆后旋转自己停下，稳定角速度≈角加速度/阻尼。
  力和力矩都按机体局部轴合成、不用欧拉角，所以任意姿态(360度筋斗、倒飞、滚转)都成立。方向约定由`ω×r`推出：绕`-右轴`=机头抬升，绕`-前轴`=右翼下沉；若试飞发现方向反了，改`Tick`里这两处符号。
- **视角**：追尾视角，弹簧臂`bUsePawnControlRotation=false`，跟着机身姿态一起转(翻滚/特技时画面跟着转)；鼠标环视只改弹簧臂相对旋转(`lookYaw/lookPitch`)，无操作`recenterDelay`(10)秒后才以`recenterSpeed`(0.3，较慢)回正；按住C(`FreeLook`)视为操作中，不回正。弹簧臂长/高度是组件默认值，蓝图里可改。
- **无人时**：`BeginPlay`拉手刹+刹车拉住(停机位里不会滚走)；`PossessedBy`松开并清零动力；`UnPossessed`清零动力/舵量(基类拉满刹车)；无人且速度几乎为0时`Tick`不施力，让刚体能睡眠。
- **HUD**：玩家控制时`Tick`用`AddOnScreenDebugMessage(210, 0.1s, ...)`在左上角显示`动力 xx%  速度 xxx km/h  高度 xxx m`(和游戏时钟同一做法；高度是向下射线(`ECC_WorldStatic`)到地面的距离，打不到地退回世界Z)。
- **重心**：`centerOfMassOffset`(蓝图Details的`Airplane|Mass`，机体局部cm，+X=机头)在`BeginPlay`里通过`SetCenterOfMass`叠加到物理资产算出的重心上，默认往机头方向300cm——重心靠后会把鼻轮翘起来。也可以直接在物理资产编辑器里选Root body，改Physics下的`COM Nudge`，两处别重复设置。
- **`AirplaneWheel`**：`UAirplaneWheelNose`(半径25、转向40度)/`UAirplaneWheelMain`(半径30、手刹/刹车)，半径是估值；改半径的办法是在编辑器里基于这两个类各建一个蓝图子类改`WheelRadius`，再填进飞机蓝图的`WheelSetups[i].WheelClass`。

## 蓝图制作(UE5.7，不用Blender)

素材用主工程里的`Content/3rdParty/Low-poly_Airplane/StaticMeshes/low_poly_airplane`(不要用`/Test/Airplane`，那是独立mod插件的内容)。
1. 静态网格确认Nanite关闭，机头原本朝+Y；Modeling Mode→Xform→Transform烘焙：Rotation Z=-90、Uniform Scale=0.15(和路线飞机一致)，Accept。
2. 右键静态网格→Convert to Skeletal Mesh(Skeleton选Create New，Root在包围盒中心)。
3. 骨骼网格里启用Editing Tools，选Root按Ctrl+N新建三根骨并F2改名：`Phys_Wheel_Nose`、`Phys_Wheel_L`、`Phys_Wheel_R`，用Gizmo移到轮心(不刷权重也行，只是轮子模型不会转)。
4. Create Physics Asset：基元类型盒体，**取消"默认禁用碰撞"**；只保留Root一个body，Physics Type=Simulated，质量约1500kg，阻尼0。
5. 蓝图：`3rdParty/Low-poly_Airplane/Actor/BP_VehiclePlane`，父类`AirplaneElement`；`CarMesh`的Skeletal Mesh设为上面的骨骼网格(Anim Class不要改)；`Vehicle Movement`的`Wheel Setups`三项的`Bone Name`依次填鼻轮、左主轮、右主轮骨名；Details的`Airplane|Flight`/`Airplane|Control`分类调手感。路径要和`PlaneVehicle::blueprintPath`(`/Game/3rdParty/Low-poly_Airplane/Actor/BP_VehiclePlane.BP_VehiclePlane_C`)一致。
6. 蓝图没做好之前飞机不生成(`LoadClass`失败只打警告)，其余不受影响。

## 依赖关系

- **不弹跳**：`BeginPlay`给`carMesh`设恢复系数0、合并模式Min的运行时物理材质；轮子`SuspensionDampingRatio=1.2`(过阻尼)，高空坠落不会被弹起。
- 依赖：`VehicleElement`(基类)、`AirplaneWheel`、`ForeverKeyBindingSubsystem`(ThrottleUp/Down/ExitVehicle/Handbrake)。
- 被谁依赖：`PlaneVehicle::blueprintPath`指向的蓝图以它为父类。

## 待办/后续阶段

- 飞行系数(`maxThrustAccel`/`topSpeed`/`stallSpeed`/力矩/阻尼)是估值，需要试飞后调手感；起飞/降落/特技都没在游戏里验证过。
- 轮子没有刷权重时不会随转动画；起落架收放、襟翼、油量、碰撞损毁都没做。

- **升力配平**：基础升力=`g*min((前向速度/stallSpeed)²,1)`，封顶在重力，速度再高松杆也平飞；多余升力只来自迎角(`aoaLiftGain`)，所以拉杆才爬升。之前升力随速度平方无限增长(到`maxLiftAccel`)，高速时净向上→速度向上→风标把机头也带上去，正反馈越飞越抬头。
- **环视范围**：yaw不限制(规整到±180，可看正后方)，pitch ±80度。
- **起飞速度**：`stallSpeed`默认4500cm/s(约160km/h)，升力按它的平方缩放，滑行初期升力很小，速度接近它才托得起飞机。
- **负动力**：动力范围`minPower`(默认-50)~100，负值=沿机头反方向的推力(`power*0.01*maxThrustAccel`直接为负)，用来减速和地面倒车；HUD直接显示负百分数。
- **推重比与失速**：`maxThrustAccel`默认800(约0.8g)，满动力也托不住垂直上爬，速度会一路掉；之前1200(>1g)垂直爬还能加速/保速，不合理。新增失速：空中前向速度<`stallSpeed`时，机头受`stallPitchAccel`(1.5 rad/s²)×(1−速度/stallSpeed)的低头力矩(轴=机头×向下，垂直向上时补低头偏置)，垂直爬到速度耗尽会自己翻下来。起飞滑跑因此更长。
