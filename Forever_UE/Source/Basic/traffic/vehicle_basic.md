
## BusVehicle
`BusVehicle::SetProperty`：`transitMeshPath`=`/Game/3rdParty/PAZ_652/StaticMeshes/SKM_paz_652.SKM_paz_652`(骨骼网格，路线车用)、`blueprintPath`=`/Game/3rdParty/PAZ_652/Actor/BP_VehicleBus.BP_VehicleBus_C`(停车场驾驶用)，`milestoneNames={"vehicle_bus"}`(`Resource/Story/vehicle_bus.script`)，`exitOffset`=(250,250,100)(右侧前门，估计值)，`drivable`/`boardable`=true，`capacity`=30。

## TrainVehicle
`TrainVehicle::SetProperty`(2026-10-30)：`transitMeshPath`=`/Game/3rdParty/SciFiTram/StaticMeshes/SciFiTram.SciFiTram`(静态网格，长边沿X，包围盒约2134x399x577)，`milestoneNames={"vehicle_train"}`(`Resource/Story/vehicle_train.script`，照抄公交的脚本去掉"驾驶")，`exitOffset`=(0,320,100)(右侧车身中段，估计值)，`boardable`=true，`capacity`=80，`drivable`=false。模型的车头朝向和下车点都是估计值，进游戏看着不对再调`meshTransform.yawOffsetDegrees`/`exitOffset`。
