# vehicle_boat.h / vehicle_boat.cpp

## 职责

`BoatVehicle`(`vehicle_boat`)：vehicle域的默认实现之一，可驾驶的船(目前是一艘拖网渔船)。

## 关键设计

- 载具种类`boat`(`GetCategory()`)：港口的船位(`building_port`，`AssignStockedParking(...,{"boat"})`)开局由`Traffic::InitStockedVehicles`各停一艘；普通车位不收`boat`。
- `blueprintPath`=`/Game/3rdParty/Trawler/Actor/BP_BoatElement.BP_BoatElement_C`，蓝图继承`ABoatElement`(见`Forever/Element/BoatElement.md`)，由用户在UE里按文档做；蓝图没做好时`SpawnCar`的`LoadClass`失败只打警告，船不生成。
- 没有`transitMeshPath`/航线：`boardable=false`、`capacity=0`，暂时没有公交船。
- 脚本：`scriptModName="script_empty"`，`milestoneNames={"vehicle_boat"}`(`Resource/Story/vehicle_boat.script`)，只有"驾驶"选项(进入`enter_vehicle`)。上船/下船走载具通用链路(`ApplyEnterVehicle`/`ExitVehicle`)，下船后船留在水上(`ParkPlayerVehicle`在不在房间里时是空操作)。
- `size`=(2500,700,800)、`exitOffset`=(0,-500,300)(左舷外、抬高)都是占位估计值，按实际船调。
- `GetName()`必须全局唯一：static计数器+id+现拼，计数器存档见`Dependence/common/mod_state.md`。

## 依赖关系

- 依赖：`traffic/vehicle_mod.h`。
- 被谁依赖：`basic.cpp`注册(依赖`script_empty`)；`Forever`层`ABoatElement`读取字段。
