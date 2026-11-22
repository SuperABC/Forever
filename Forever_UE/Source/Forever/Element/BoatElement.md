# BoatElement.h / BoatElement.cpp

## 职责

可驾驶的船：停在港口船位(`building_port`)里、玩家走近选"驾驶"就能开的那艘(目前是拖网渔船)。船型是`BoatVehicle`(`vehicle_boat`，类别`boat`)，没有航线/公交船。

## 关键设计

- **继承`AVehicleElement`**：复用骨骼网格+物理、`Init`、近身"驾驶"选项、`ExitVehicle`、`Tick`把位姿写回Core(存档/重生用)、`LoadClass<AVehicleElement>(blueprintPath)`的流式生成链路，`ForeverTrafficFrameworkComponent`一行没改。和飞机一样绕过基类输入。构造里把Chaos轮式载具的`WheelSetups`清空(船没有轮子)，`BeginPlay`里关掉该组件的Tick，`carMesh`的轮子动画类置空。**风险**：Chaos轮式载具0轮子是否报错/崩溃要进PIE验证；不行的话退路是让基类对`vehicleMovement`的调用全部判空(基类部分调用已经判了`WheelSetups.Num()`)。
- **水面高度**：和人物游泳同一套——`ForeverWater::QuerySurface`(`Water/WaterQuery.h`，角色的`QueryWater`也走它)：世界坐标除以1000得地图格，`Map::GetWater`有没有水+水位，`+ForeverWater::WaveHeight`(波浪目前恒0)，水底取`Map::SampleHeight`。没水格子=没有水的作用(不浮、无阻力、推力为0)，船会落到地形上静止。
- **浮力**(`Tick`每帧，无论有没有人开，所以停着的船浮得住)：对每个浮点(`floatPoints`，船体局部cm；空=按骨骼网格包围盒在船底自动生成6个点：船头/船尾/左右各2)取世界位置查水面，`depth=水面-点高度`；每点加速度`=(g*clamp(depth/draft,0,maxSubmergeRatio) - floatDamping*点垂直速度)/N`(不为负)，乘质量后`AddForceAtLocation`向上。N个点都沉入`draft`时合力刚好=重力，所以平衡时浮点沉在水下`draft`(吃水)；多点之间的差给出俯仰/横滚恢复力矩。`floatDamping`小于临界值(≈2√(g/draft))船落水后会多弹几下。
- **排水量**(`displacementMassKg`，2026-11-22)：浮力只跟排开的水的体积(沉入深度)有关，和船多重无关，所以浮力弹簧按排水量算：浮点都沉入`draft`时总浮力=`displacementMassKg`的重量，平衡吃水≈`draft×质量/排水量`——改轻船会浮得更高。0(默认)=取当前质量，即旧行为(吃水恒为`draft`，改质量不影响)。用法：先进PIE看日志`Boat mass: ... kg`得到当前质量，填进`displacementMassKg`，再把船质量(`CarMesh`的Mass Scale或物理资产)调轻。推力/阻力/转向仍是加速度制，和质量无关。
- **输入**：`IA_Move`(WASD)→`MoveBoat`：W/S=油门(`Y`，前进/后退)，A/D=转向(`X`)；鼠标环视沿用基类`Look`(相机跟控制器旋转，不转船)；Q退出。没有手刹/滚轮油门，不需要新按键。
- **推进**：只有螺旋桨(推力点`activeThrustPoint`，自动=船尾船底，`bCustomThrustPoint`可自定义)在水里才有推力，按入水程度缩放。大小`mass*maxThrustAccel*油门`(倒车乘`reverseThrustScale`)。水平分量沿船头方向，作用点的前后/左右在船尾，但**沿船体上下方向的力臂只保留`thrustLeverRatio`(默认0.15)**——真放在船底的话力臂≈重心离船底的高度，大船会被推得船头竖起来翻过去(2026-11-22实测)；**前进时再加一个向上分量**`thrust*thrustUpRatio`(默认0.35；0.2时实测起伏不够)，沿**世界上方向**(不跟船体倾斜走，否则船头一抬它就变成往后推，正反馈翻船)、作用在重心(不产生俯仰)，船体倾斜越大越弱、超过`maxLiftTilt`(25度)为0。浮点出水、螺旋桨出水推力变小，落回水里再托起——快艇一跳一跳。`thrustUpRatio=0`不跳。
- **起伏(靠推力力臂)**(2026-11-22)：不额外加周期力。推力作用在船尾螺旋桨处，沿船体上下方向的力臂=`thrustLeverRatio`×(推力点离重心的高度)，默认0.6：前进时推力产生抬头力矩 → 船头翘起 → 船尾浮点入水更深/船头浮点出水 → 浮力分布变化、倾斜增大后恢复力矩把船头压回去 → 再被推力抬起，循环就是一起一伏。起伏太小就加大`thrustLeverRatio`(1=真在船底)，太猛/翻船就减小或加大`uprightAccel`。`thrustUpRatio`默认改为0(额外的恒定向上分量只会抬高平衡吃水，不产生起伏；之前试过加周期向上脉冲，已删除)。
- **转向**：目标角速度`=maxYawRate*转向*clamp(沿船头速度/steerFullSpeed,-1,1)`——停着(速度0)完全转不了，慢速只能慢转，快了转得快，倒车方向反过来；绕世界Z的角加速度`=(目标-当前)*yawResponse`(UE里绕+Z的角速度=向右转)。另有**防翻船**：把船体"上"向世界上方向回正的角加速度`uprightAccel`(默认4，叉积幅值=sin倾斜角)和俯仰/横滚角速度阻尼`rollPitchDamping`，强度取`max(入水程度,0.3)`——船头一抬、跳起来时浮点出水，浮力给不出恢复力矩，所以空中也保留一部分。
- **阻力**：水中、按入水程度缩放，纵向`forwardDrag/forwardDragQuad`小、横向`lateralDrag/lateralDragQuad`大。转弯过快时船头和速度方向夹角大 → 横向速度分量大 → 横向阻力把速度吃掉，船停下来——这个行为靠大横向阻力涌现，没有特殊分支。所有力都是加速度×质量，系数和船的质量无关；单位cm/s²、rad/s²。**所有系数都是估值，试开后调(同飞机)**。
- **浪花**(纯C++程序化，没有Niagara/新资产依赖，仿`ForeverExplosionEffect`)：构造里建32个`UStaticMeshComponent`(`/Engine/BasicShapes/Sphere`+`/Game/Asset/Effects/Explosion/ExplosionSmoke`材质，`SetAbsolute`世界空间，无碰撞/阴影，默认隐藏)；`BeginPlay`给每个建一个动态材质实例(参数`Color`/`Opacity`)。`UpdateWake`：速度超过`wakeMinSpeed`且螺旋桨在水里时按速度比例发射(`wakeRate`×速度比例，起点=推力点+`wakeOffset`、高度取水面)，粒子初速度=向船尾后方+左右散开+上抬，重力下落，落回水面/寿命(`wakeLife`)到就回收，直径由小变大(`wakeSize`)、不透明度先升后降(`wakeOpacity`)。粒子池满了丢掉这一批。
- **重心**：构造里把基类的`centerOfMassOffset`默认设成(0,0,-50)(重心下移50cm)；物理资产是包住船体的一个盒子，重心在盒子中央，比真实船高，高速转弯/被托起时容易侧翻。基类`BeginPlay`把它叠加到物理资产算出的重心上(`SetCenterOfMass`)；蓝图Class Defaults的`Mass → Center Of Mass Offset`里可以改，Z越负重心越低。推力作用点的力臂按实际重心位置算，所以跟着变。
- **HUD**：玩家控制时`AddOnScreenDebugMessage(211, ...)`显示速度km/h。
- **无人时**：`UnPossessed`清零油门/转向；浮力和阻力照常，船停在水上不会飘走也不会沉。读档/重生：位姿由基类每帧写回Core，`GenerateVehicles`用保存的位姿(只有yaw)重新生成，船从保存位置落回水面。
- 约定：骨骼网格**船头朝+X**，原点在船体中心附近。

## 蓝图制作(UE5.7，用户在编辑器里做)

素材：`Content/3rdParty/Trawler/StaticMeshes/SKM_trawler`(骨骼网格)+`SKM_trawler_Physics`(物理资产)。
1. 确认骨骼网格船头朝+X、原点在船体中心偏船底(不对就像飞机那样用Modeling Mode→Xform→Transform烘焙旋转/缩放)。
2. 物理资产：只保留一个Box(或凸包)覆盖船体，`Physics Type=Simulated`，质量按船估(拖网渔船约8000kg量级，浮力质量无关)，线/角阻尼0；**取消"默认禁用碰撞"**。
3. 新建蓝图`/Game/3rdParty/Trawler/Actor/BP_BoatElement`，父类`BoatElement`：`CarMesh`的Skeletal Mesh/Physics Asset填上面的(不用设Anim Class)；`Vehicle Movement`的`Wheel Setups`**保持空**；Details里`Boat|Float`按船调`draft`(船底入水深度)，船体不规则时填`floatPoints`；`Boat|Drive`调推力/阻力/`thrustUpRatio`(跳的程度)，`bCustomThrustPoint`+`thrustPoint`指螺旋桨位置；`Boat|Steer`调转向；`Boat|Wake`调`wakeOffset`(船尾偏移)和浪花外观；`CameraBoom`的臂长/高度。路径要和`BoatVehicle::blueprintPath`一致。
4. 蓝图没做好之前船不生成(`LoadClass`失败只打警告)，其余不受影响。
5. 之后在布局编辑器里按真实船长调`preset_port_fg`的船位矩形，必要时改`BoatVehicle`的`size`/`exitOffset`。

## 依赖关系

- 依赖：`Element/VehicleElement.h`(基类)、`Water/WaterQuery.h`(水面)、`Framework/ForeverFrameworkActor.h`(取Core地图)、`Input/ForeverKeyBindingSubsystem.h`(`ExitVehicle`键)。
- 被谁依赖：`BoatVehicle::blueprintPath`指向的蓝图继承它。
