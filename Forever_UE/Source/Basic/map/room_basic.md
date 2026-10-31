# room_basic.h/.cpp

## 职责

`ResidenceRoom`/`ShopRoom`/`WarehouseRoom`/`ParkingRoom`/`FactoryRoom`是Room域五个默认
实现，合在同一份`room_basic.h/.cpp`里(每个concept统一一份`<concept>_basic.h/.cpp`，不按
业务场景拆文件)。

## ResidenceRoom

`isResidential=true; residentialCapacity=1;`——每间住宅room=1个名额，多人合住靠
`Map::Checkin()`的名额池逻辑，不靠这个capacity强制。`workspace`/`storage`/`manufacture`
这3类占位属性保持`RoomMod`基类默认值(`false`/空)不动。

## ShopRoom / WarehouseRoom

- `ShopRoom`：`isWorkspace=true; workspaceCapacity=100;`。`workspaceCapacity`这个"工位数"
  占位字段带真实数值，真正被Job系统消费留到Society域接入时再做，目前没有任何调用方读取它。
  门(`doorSpecs["room"]`)有名字"商店大门"、挂`door_shop_entrance`脚本(开局加"敲门"选项)，靠近会弹交互
  选项。挂在房间门上而不是`ShopBuilding::doorSpecs["entrance"]`，是因为商店用的一楼模板
  (`preset_lobby_linear_fg+`/`preset_circle_double_fg+-`)里门标签全是`room`，没有`entrance`。
- `WarehouseRoom`：`isStorage=true; storageConfig={{"shop",100.f}};`。只有`ShopBuilding`
  用到(地下室+每层过道铺unused acreage)。

## ParkingRoom

`isParking=true; parkingSpots={{{0.5f,0.f,0.5f,0.f},0.f,{"car","bus"}}};`——房间正中心一个车位，朝向跟
房间一致，供`Traffic::Init()`在这里生成预置车辆，见`Source/Core/traffic/traffic.md`。
`ShopBuilding`和`FactoryBuilding`(见`building_basic.md`)共用同一个`ParkingRoom`类。

## FactoryRoom

`isManufacture=true; manufactureTypes={"experience"};`。`"experience"`这个生产类型对应的
`ExperienceManufacture`/`ExperienceProduct`属于Industry域，目前不接入，`manufactureTypes`
这个占位字段先带上真实字符串，目前没有任何调用方读取它。

## 依赖关系

- 依赖：`Source/Dependence/map/room_mod.h`(`RoomMod`基类)。
- 被谁依赖：`Source/Basic/basic.cpp`(`RegisterModRooms`注册五个类型)、
  `building_basic.cpp`(`ResidenceBuilding`/`ShopBuilding`/`FactoryBuilding::Layout()`按
  房间类型调`ArrangeRow`/`AssignRoom`)。

## AirportRoom

`SetProperty()`：`isParking=true`，两个停机位（种类`{"plane"}`）：用房间局部坐标系（u轴沿正面墙、v轴朝正面，原点在房间角点，见`ParkingSpot`注释/`room.md`），房间按朝向旋转时车位跟着转。u轴ratio相对房间角点，`-0.15`/`1.15`落在房间沿正面墙两端外面（0/1是房间边缘），v轴`0.5`和房间同一条中线，rotationDegrees=0表示朝向房间正面(朝路)，停机位设成180度即背对路、机头朝向房间深处的跑道。位置是估计值，常量`kLeftAlong`/`kRightAlong`(ratio)和`kExtraOffset`(绝对距离，地图单位)在`AirportRoom::SetProperty`里，不合适直接调（机翼宽、和跑道/楼体的间距按实际飞机调）。
