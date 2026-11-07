# vehicle_basic.h / vehicle_basic.cpp

## 职责

五个内置`VehicleMod`：`VehicleBasic`(`vehicle_basic`，小汽车)、`BusVehicle`(`vehicle_bus`)、`TruckVehicle`(`vehicle_truck`，ZIL-130货车)、`TrainVehicle`(`vehicle_train`)、`PlaneVehicle`(`vehicle_plane`)。

## 关键设计

- **两类车型**：停车场里的可驾驶车(`blueprintPath`，继承`AVehicleElement`的蓝图，Chaos物理，骨骼网格/轮骨`Phys_Wheel_FL/FR/BL/BR`在蓝图里设，这里只存资源路径，见`Forever/Element/VehicleElement.md`)和线路车辆(`transitMeshPath`，`ATransitVehicleElement`运动学显示)。公交、飞机两种都有，同一个车型两种用法；火车只有线路版本。`sizeX/Y/Z`是模型包围盒(UE单位)。
- **脚本**：`scriptModName="script_empty"`(纯透传)，"靠近弹出上车/驾驶"完全靠`milestoneNames`触发：小汽车和货车用`vehicle_basic`，公交`vehicle_bus`、火车`vehicle_train`(照抄公交去掉"驾驶")、飞机`vehicle_plane`，均在`Resource/Story/`下。
- **载具种类**：每个类有`static GetCategory()`(`car`/`bus`/`truck`/`train`/`plane`)，`basic.cpp`注册时传给`VehicleFactory::RegisterVehicle`，停车位按种类过滤(`room_basic.cpp`的停车位种类含`car`/`bus`/`truck`)。
- **`exitOffset*`**是车身局部坐标(车头+X)的下车点，全部是估计值，进游戏看着不对再调。
- `GetName()`必须全局唯一：static计数器+id+现拼，计数器存档见`Dependence/common/mod_state.md`。

### VehicleBasic
`blueprintPath`=`/Game/3rdParty/Cars_for_Arcade_Demolition_Racing_Games/Actor/BP_VehicleElement.BP_VehicleElement_C`，`milestoneNames={"vehicle_basic"}`，`drivable=true`。

### BusVehicle
`sizeX/Y/Z`=786/312/347(PAZ_652包围盒)，`transitMeshPath`=`/Game/3rdParty/PAZ_652/StaticMeshes/SKM_paz_652.SKM_paz_652`(骨骼网格，车头+X，原点在车身中心地面)、`blueprintPath`=`/Game/3rdParty/PAZ_652/Actor/BP_BusElement.BP_BusElement_C`(停车场驾驶用，CarMesh同一个骨骼网格)，`milestoneNames={"vehicle_bus"}`，`exitOffset`=(250,250,100)(右侧前门)，`drivable`/`boardable`=true。`capacity=30`。

### TruckVehicle
ZIL-130货车，只有停车场版本(和`VehicleBasic`同一条路线)。`blueprintPath`=`/Game/3rdParty/ZIL_130__body____Lowpoly/Actor/BP_TruckElement.BP_TruckElement_C`，`milestoneNames={"vehicle_basic"}`，`size`=(556,229,199)，`exitOffset`=(150,-220,100)(左侧驾驶室车门)，`drivable=true`。轮子半径/重心在`BP_TruckElement`里填(`wheelRadius`=42)。

### TrainVehicle
`transitMeshPath`=`/Game/3rdParty/SciFiTram/StaticMeshes/SciFiTram.SciFiTram`(静态网格，长边沿X，包围盒约2134x399x577)，`milestoneNames={"vehicle_train"}`，`exitOffset`=(0,320,100)(右侧车身中段)，`boardable=true`，`capacity=80`，`drivable=false`。模型车头朝向和下车点都是估计值，不对再调`meshTransform.yawOffsetDegrees`/`exitOffset`。

### PlaneVehicle
- 两种用法：线路上的飞机(`Route`生成，`ATransitVehicleElement`，用`transitMeshPath`=`/Test/Airplane/StaticMeshes/airplane.airplane`，资产来自`Forever_Mod/Test`的Plugin，运行时挂载成`/Test/...`包路径；`meshTransform.scale=0.15`，模型横着建所以`yawOffsetDegrees=-90`)和机场停机位里的飞机(`AirportRoom`车位，`APlaneElement`可驾驶，用`blueprintPath`=`/Game/3rdParty/Low-poly_Airplane/Actor/BP_PlaneElement.BP_PlaneElement_C`，见`Forever/Element/PlaneElement.md`)。`vehicle_plane.script`靠`$$self.on_route`区分：只有`on_route==0`的才提供"驾驶"选项。
- `size`=(3000,2500,800)，`drivable=true`。`exitOffset`=(-300,-200,200)：机身左后方，避开机翼；Z=200是因为飞机原点比地面低约9cm，Z=100时胶囊体底部陷进TerrainLod被挡(日志实测命中点Z=2)，抬高后人才能落地。

## 依赖关系

- 依赖：`traffic/vehicle_mod.h`。
- 被谁依赖：`basic.cpp`注册(都依赖`script_empty`)；`BusRoute`/`TrainRoute`/`AirRoute`按`vehicleType`引用；`Forever`层`AVehicleElement`/`ATransitVehicleElement`/`APlaneElement`读取字段。

## 待办/后续阶段

- 各车型的`exitOffset`和火车模型朝向是估计值，待进游戏调整。
