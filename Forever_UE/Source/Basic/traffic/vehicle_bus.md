# vehicle_bus.h / vehicle_bus.cpp

## 职责

`BusVehicle`(`vehicle_bus`)：vehicle域的默认实现之一。

## 关键设计

- **两类车型**：停车场里的可驾驶车(`blueprintPath`，继承`AVehicleElement`的蓝图，Chaos物理，骨骼网格/轮骨`Phys_Wheel_FL/FR/BL/BR`在蓝图里设，这里只存资源路径，见`Forever/Element/VehicleElement.md`)和线路车辆(`transitMeshPath`，`ATransitVehicleElement`运动学显示)。公交、飞机两种都有，同一个车型两种用法；火车只有线路版本。`sizeX/Y/Z`是模型包围盒(UE单位)。
- **脚本**：`scriptModName="script_empty"`(纯透传)，"靠近弹出上车/驾驶"完全靠`milestoneNames`触发：小汽车和货车用`vehicle_basic`，公交`vehicle_bus`、火车`vehicle_train`(照抄公交去掉"驾驶")、飞机`vehicle_plane`，均在`Resource/Story/`下。
- **载具种类**：每个类有`static GetCategory()`(`car`/`bus`/`truck`/`train`/`plane`)，`basic.cpp`注册时传给`VehicleFactory::RegisterVehicle`，停车位按种类过滤(`room_*.cpp`的停车位种类含`car`/`bus`/`truck`)。
- **`exitOffset*`**是车身局部坐标(车头+X)的下车点，全部是估计值，进游戏看着不对再调。
- `GetName()`必须全局唯一：static计数器+id+现拼，计数器存档见`Dependence/common/mod_state.md`。

### BusVehicle

`sizeX/Y/Z`=786/312/347(PAZ_652包围盒)，`transitMeshPath`=`/Game/3rdParty/PAZ_652/StaticMeshes/SKM_paz_652.SKM_paz_652`(骨骼网格，车头+X，原点在车身中心地面)、`blueprintPath`=`/Game/3rdParty/PAZ_652/Actor/BP_BusElement.BP_BusElement_C`(停车场驾驶用，CarMesh同一个骨骼网格)，`milestoneNames={"vehicle_bus"}`，`exitOffset`=(250,250,100)(右侧前门)，`drivable`/`boardable`=true。`capacity=30`。

## 依赖关系

- 依赖：`traffic/vehicle_mod.h`。
- 被谁依赖：`basic.cpp`注册(都依赖`script_empty`)；`BusRoute`/`TrainRoute`/`AirRoute`按`vehicleType`引用；`Forever`层`AVehicleElement`/`ATransitVehicleElement`/`APlaneElement`读取字段。

## 待办/后续阶段

- 各车型的`exitOffset`和火车模型朝向是估计值，待进游戏调整。
