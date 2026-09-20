# room_basic.h/.cpp

## 职责

`ResidenceRoom`/`ShopRoom`/`WarehouseRoom`/`ParkingRoom`/`FactoryRoom`是Room域五个默认
内容，合并进同一份`room_basic.h/.cpp`(不再按residence/shop/plant各开一个文件，和
`terrain_basic.h/.cpp`里`OceanTerrain`/`MountainTerrain`合并的方式一样，见
`Source/Basic/README.md`)。

## ResidenceRoom

`isResidential=true; residentialCapacity=1;`——照抄老工程`ResidentialRoom::ConfigRoom`
的取值(每间住宅room=1个名额，多人合住靠`Map::Checkin()`的名额池逻辑，不靠这个capacity
强制)。`workspace`/`storage`/`manufacture`这3类占位属性保持`RoomMod`基类默认值(`false`/
空)不动。

## ShopRoom / WarehouseRoom

- `ShopRoom`：`isWorkspace=true; workspaceCapacity=100;`——照抄老工程
  `ShopRoom::ConfigRoom`。`workspaceCapacity`这个"工位数"占位字段这次带上真实数值，真正
  被Job系统消费留到Society域迁移时再接，目前没有任何调用方读取它。
- `WarehouseRoom`：`isStorage=true; storageConfig={{"shop",100.f}};`——照抄老工程
  `WarehouseRoom::ConfigRoom`。只有`ShopBuilding`用到（地下室+每层过道铺unused acreage）。

## ParkingRoom

应用户要求正常迁移（不跳过），但保持trivial——`RoomMod`基类不需要加`isParking`/
`parkingSpaces`这类新字段，`ParkingRoom`不设置任何`isXxx`标记，只提供`"room_parking"`
类型id给`AssignRoom`用，不处理车辆/车位数量。`ShopBuilding`和`FactoryBuilding`
（见`building_basic.md`）共用同一个`ParkingRoom`类。

## FactoryRoom

`isManufacture=true; manufactureTypes={"experience"};`——照抄老工程
`FactoryRoom::ConfigRoom`。`"experience"`这个生产类型对应的`ExperienceManufacture`/
`ExperienceProduct`属于Industry域，这次不迁移，`manufactureTypes`这个占位字段先带上
真实字符串，目前没有任何调用方读取它。

## 文件合并说明

原来这五个类型分别放在`room_residence.h/.cpp`、`room_shop.h/.cpp`（`ShopRoom`/
`WarehouseRoom`/`ParkingRoom`三个都在这一个文件里）、`room_plant.h/.cpp`
（`FactoryRoom`，文件名避开和`Source/Dependence/map/room_factory.h`的同名冲突）三个文件
里。这次合并成一份`room_basic.h/.cpp`之后，不再需要为了避让`_factory`后缀而单独起名，
和`terrain_basic`/`roadnet_basic`保持同一个组织方式：每个concept统一只对应一份
`<concept>_basic.h/.cpp`。

## 依赖关系

- 依赖：`Source/Dependence/map/room_mod.h`（`RoomMod`基类）。
- 被谁依赖：`Source/Basic/basic.cpp`（`RegisterModRooms`注册五个类型）、
  `building_basic.cpp`（`ResidenceBuilding`/`ShopBuilding`/`FactoryBuilding::Layout()`
  按房间类型调`ArrangeRow`/`AssignRoom`）。
