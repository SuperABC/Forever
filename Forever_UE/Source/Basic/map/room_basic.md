# room_basic.h/.cpp

## 职责

Room域的默认实现，合在同一份`room_basic.h/.cpp`里(每个concept统一一份`<concept>_basic.h/.cpp`，不按业务场景拆文件)。`SetProperty()`里按类型声明房间的功能属性(`isResidence`/`isWorkspace`/`isStorage`/`isManufacture`/`isParking`及对应容量、房租、停车位、门规格)，没声明的属性保持`RoomMod`基类默认值。

| 类型 | id | 属性 |
| --- | --- | --- |
| `ResidenceRoom` | `room_residence` | `isResidence`，`residenceCapacity=1`，房租2000(每日20元)，挂`entity_event`脚本 |
| `OfficeRoom` | `room_office` | `isWorkspace`，`workspaceCapacity=5`，房租2000；`OfficeBuilding`(三档密度)的房间，没有挂脚本的门 |
| `ShopRoom` | `room_shop` | `isWorkspace`，`workspaceCapacity=5`（满编；月结可超编到2倍），房租2000 |
| `WarehouseRoom` | `room_warehouse` | `isStorage`，`storageType="storage_shop"`；另有管理员工位`isWorkspace`，`workspaceCapacity=1`，房租2000 |
| `ParkingRoom` | `room_parking` | `isParking`，房间正中心一个车位 |
| `TrainStationRoom` | `room_train_station` | 无任何功能标志，纯普通房间(候车厅) |
| `AirportRoom` | `room_airport` | `isParking`，两个停机位 |
| `FarmRoom`/`RanchRoom`/`FoodFactoryRoom` | `room_farm`/`room_ranch`/`room_food_factory` | `isManufacture`，`manufactureTypes`分别是`manufacture_farm`/`manufacture_ranch`/`manufacture_food_factory`；`isWorkspace`，容量20，房租2000 |

## 关键设计

- **`ResidenceRoom`**：每间住宅room=1个名额，多人合住靠`Map::Checkin()`的名额池逻辑，不靠这个capacity强制。
- **`ResidenceRoom`暂时不登记任何`doorSpecs`**：layout 里 single/row 的门洞(`home`/`room` 标签)没有门规格，`ResolveDoorSpec` 返回 nullptr，只是墙上的缺口，不生成门。要放门时在 `SetProperty()` 里登记对应标签(网格/动画蓝图路径直接写在里面)。其余房间类型仍用 `doorSpecs["room"]`。
- **`ShopRoom`的门有名字"商店大门"、挂`door_shop_entrance`脚本**(开局加"敲门"选项，靠近会弹交互选项)。挂在房间门而不是`ShopBuilding::doorSpecs["entrance"]`上，是因为商店用的一楼模板(`preset_lobby_linear_fg+`/`preset_circle_double_fg+-`)里门标签全是`room`，没有`entrance`，`ShopBuilding`的`entrance`规格对不上任何门。
- 带`isWorkspace`的房间(商店/仓库/工坊)，Organization按容纳量一半定员。
- **`ParkingRoom`**：车位种类`{"car","bus","truck"}`，供`Traffic::Init()`在这里生成预置车辆，见`Source/Core/traffic/traffic.md`；不想要某种车就从这个列表里删。`ShopBuilding`和`FactoryBuilding`共用同一个`ParkingRoom`类。
- **`AirportRoom`的停机位**用房间局部坐标系(u轴沿正面墙、v轴朝正面，原点在房间角点，见`ParkingSpot`注释/`room.md`)，房间按朝向旋转时车位跟着转。u轴ratio相对房间角点：`kLeftAlong=-0.15`/`kRightAlong=1.15`落在房间沿正面墙两端的外面(0/1是房间边缘)，再各向外挪`kExtraOffset`(绝对距离，地图单位，不受房间尺寸影响)；v轴`0.5`和房间同一条中线。`rotationDegrees=180`即背对路、机头朝向房间深处的跑道。位置是估计值，常量在`AirportRoom::SetProperty`里，按实际机翼宽度、和跑道/楼体的间距调。
- 三种工坊房间由`FactoryBuilding`等概率随机选一种；生产类型是`ManufactureMod`的id，Industry按它为房间生成`Manufacture`。

## 依赖关系

- 依赖：`Source/Dependence/map/room_mod.h`(`RoomMod`基类)。
- 被谁依赖：`Source/Basic/basic.cpp`(`RegisterModRooms`注册全部类型，`ShopRoom`依赖`script_empty`做门脚本)；`building_basic.cpp`里各`Layout()`按房间类型调`ArrangeRow`/`AssignRoom`；Society域`ShopSalerJob`依赖`ShopRoom`。

## 待办/后续阶段

- 无。
