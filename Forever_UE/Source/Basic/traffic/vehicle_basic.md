
## BusVehicle
`BusVehicle::SetProperty`：`transitMeshPath`=`/Game/3rdParty/PAZ_652/StaticMeshes/SKM_paz_652.SKM_paz_652`(骨骼网格，路线车用)、`blueprintPath`=`/Game/3rdParty/PAZ_652/Actor/BP_BusElement.BP_BusElement_C`(停车场驾驶用)，`milestoneNames={"vehicle_bus"}`(`Resource/Story/vehicle_bus.script`)，`exitOffset`=(250,250,100)(右侧前门，估计值)，`drivable`/`boardable`=true，`capacity`=30。

## TrainVehicle
`TrainVehicle::SetProperty`(2026-10-30)：`transitMeshPath`=`/Game/3rdParty/SciFiTram/StaticMeshes/SciFiTram.SciFiTram`(静态网格，长边沿X，包围盒约2134x399x577)，`milestoneNames={"vehicle_train"}`(`Resource/Story/vehicle_train.script`，照抄公交的脚本去掉"驾驶")，`exitOffset`=(0,320,100)(右侧车身中段，估计值)，`boardable`=true，`capacity`=80，`drivable`=false。模型的车头朝向和下车点都是估计值，进游戏看着不对再调`meshTransform.yawOffsetDegrees`/`exitOffset`。

## 载具种类与PlaneVehicle的两种用法

- 每个类有`static const char* GetCategory()`（`VehicleBasic`="car"、`BusVehicle`="bus"、`TrainVehicle`="train"、`PlaneVehicle`="plane"），`basic.cpp`注册时传给`VehicleFactory::RegisterVehicle`；`VehicleMod`不再有`category`实例字段。
- `PlaneVehicle`和公交一样一个车型两种用法：线路上的飞机(`Route`生成，`ATransitVehicleElement`运动学显示，用`transitMeshPath`)和机场停机位里的飞机(`AirportRoom`车位，`APlaneElement`可驾驶，用`blueprintPath`)。`vehicle_plane.script`靠`$$self.on_route`区分：只有`on_route==0`的才提供"驾驶"选项，线路飞机不提供。
- `PlaneVehicle`的`exitOffset`=(-1000,-800,100)：机身左后方，避开机翼(之前(0,-400,100)在翼下，静止时下车路径被挡住)，估计值。
