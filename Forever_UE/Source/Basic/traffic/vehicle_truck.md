# vehicle_truck.h / vehicle_truck.cpp

## 职责

`TruckVehicle`(`vehicle_truck`)：vehicle域的默认实现之一。

## 关键设计

- **两类车型**：停车场里的可驾驶车(`blueprintPath`，继承`AVehicleElement`的蓝图，Chaos物理，骨骼网格/轮骨`Phys_Wheel_FL/FR/BL/BR`在蓝图里设，这里只存资源路径，见`Forever/Element/VehicleElement.md`)和线路车辆(`transitMeshPath`，`ATransitVehicleElement`运动学显示)。公交、飞机两种都有，同一个车型两种用法；火车只有线路版本。`sizeX/Y/Z`是模型包围盒(UE单位)。
- **载具种类**：每个类有`static GetCategory()`(`car`/`bus`/`truck`/`train`/`plane`)，`basic.cpp`注册时传给`VehicleFactory::RegisterVehicle`，停车位按种类过滤(`room_*.cpp`的停车位种类含`car`/`bus`/`truck`)。
- **`exitOffset*`**是车身局部坐标(车头+X)的下车点，全部是估计值，进游戏看着不对再调。
- `GetName()`必须全局唯一：static计数器+id+现拼，计数器存档见`Dependence/common/mod_state.md`。

### TruckVehicle

ZIL-130货车，只有停车场版本(和`SedanVehicle`同一条路线)。`blueprintPath`=`/Game/3rdParty/ZIL_130__body____Lowpoly/Actor/BP_TruckElement.BP_TruckElement_C`，`milestoneNames={"vehicle_basic"}`，`size`=(556,229,199)，`exitOffset`=(150,-220,100)(左侧驾驶室车门)，`drivable=true`。轮子半径/重心在`BP_TruckElement`里填(`wheelRadius`=42)。

## 依赖关系

- 依赖：`traffic/vehicle_mod.h`。
- 被谁依赖：`basic.cpp`注册(都依赖`script_empty`)；`BusRoute`/`TrainRoute`/`AirRoute`按`vehicleType`引用；`Forever`层`AVehicleElement`/`ATransitVehicleElement`/`APlaneElement`读取字段。

## 待办/后续阶段

- 各车型的`exitOffset`和火车模型朝向是估计值，待进游戏调整。
