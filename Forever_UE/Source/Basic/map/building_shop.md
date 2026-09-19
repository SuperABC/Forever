# building_shop.h/.cpp / room_shop.h/.cpp / component_shop.h

## 职责

`ShopBuilding`/`ShopRoom`/`WarehouseRoom`/`ParkingRoom`/`ShopComponent`是Building/Room/
Component域新增的"商店"默认内容，从`Source/Basic/README.md`共用文档独立出来（该文档自己
约定的"业务接口补齐后独立成文"规则，和`terrain_basic.md`/`roadnet_basic.md`同一个先例）。
照抄老工程`E:\Projects\Forever_UE\Source\Basic\map\building_basic.h/.cpp`
（`ShopBuilding`）、`room_basic.h/.cpp`（`ShopRoom`/`WarehouseRoom`/`ParkingRoom`）、
`component_basic.h/.cpp`（`ShopComponent`）的对应部分，为society域后续的job系统（商店
提供工作岗位）做准备——这次**不**迁移`ShopSalerJob`/`ShopOrganization`，Society域这两个
概念目前还是空骨架（只有`GetType/GetName/ApplyArgs`），留到`PHASE4_PLAN.md`真正点名迁移
Society域时再做。

## ShopRoom / WarehouseRoom / ParkingRoom（`room_shop.h/.cpp`）

- `ShopRoom`：`isWorkspace=true; workspaceCapacity=100;`——照抄老工程
  `ShopRoom::ConfigRoom`。`workspaceCapacity`这个"工位数"占位字段这次带上真实数值，真正
  被Job系统消费留到Society域迁移时再接，目前没有任何调用方读取它。
- `WarehouseRoom`：`isStorage=true; storageConfig={{"shop",100.f}};`——照抄老工程
  `WarehouseRoom::ConfigRoom`。只有`ShopBuilding`用到（地下室+每层过道铺unused acreage），
  不单独开文件，装在`room_shop.h`里。
- `ParkingRoom`：应用户要求正常迁移（不跳过），但保持trivial——`RoomMod`基类不需要加
  `isParking`/`parkingSpaces`这类新字段，`ParkingRoom`不设置任何`isXxx`标记，只提供
  `"room_parking"`类型id给`AssignRoom`用，不处理车辆/车位数量。`ShopBuilding`和
  `FactoryBuilding`（见`building_factory.md`）共用同一个`ParkingRoom`类。

## ShopComponent（`component_shop.h`）

trivial占位，和`ResidenceComponent`同样极简（`GetId()="component_shop"`），没有
`InitComponent`逻辑（老工程`ShopComponent::InitComponent`本来就是空实现）。

## ShopBuilding（`building_shop.h/.cpp`）

和`ResidenceBuilding`一样是"一个本体独占一个mod实例"模型：`RandomAcreage`/
`GetAcreageMin`/`GetAcreageMax`/`GetPower`/`Assign`全部是不需要实例的static方法。

- `GetPower(AREA_TYPE)`：照抄老工程`GetPowers()`——只在`AREA_COMMERCIAL_HIGH/MIDDLE/LOW`
  上返回`1.f`，其余`0.f`。
- `RandomAcreage()`：`4000.f * powf(1.f + GetRandom(1000)/1000.f*1.f, 2)`；
  `GetAcreageMin()=4000.f`；`GetAcreageMax()=16000.f`——数值照抄老工程。
- `Assign()`：空实现，走权重CDF随机填充路径（`FillRemainder`），不显式占位。
- `Layout()`：完整照抄老工程`ShopBuilding::LayoutBuilding`的两个acreage分支
  （`building_basic.cpp:330-406`），复用新工程`Resource/Layouts/`下已有的
  `preset_lobby_linear_*`/`preset_circle_double_*`模板（文件名和老工程一致，直接可用）：
  - `quad.GetAcreage() < 6000`：2层（`layers=2`固定值，不像`ResidenceBuilding`那样按
    acreage随机分档），`preset_lobby_linear_fg+`（1楼，`ShopRoom`+`WarehouseRoom`过道）→
    `preset_lobby_linear_fu-`（2楼，纯`WarehouseRoom`）。中间那个`for (i=1;i<layers-1;i++)`
    循环体在`layers=2`时是no-op（`1<1`为假），照抄老工程原始写法保留，不简化。
  - 否则：`basements=1`，地下室`GetRandom(2)`随机选`preset_circle_double_pg+`
    （`ParkingRoom`）或`preset_circle_double_bg+`（5间`WarehouseRoom`）——这个随机分支
    完整保留，不删减。1楼`preset_circle_double_fg+-`（2间`ShopRoom`+4间`WarehouseRoom`），
    2楼`preset_circle_double_fu+-`（5间`WarehouseRoom`）。
  - `AssignFloor`参数顺序对齐新API`(level, templateName, face, assets)`（老工程是
    `(level, direction, templateName)`），`AssignRoom`/`ArrangeRow`参数顺序和老工程一致，
    不用改。
  - `direction<0`（`FillRemainder`落地）时先从`boundaryRoads`里随机挑一个有真实边界路的
    方向兜底，再用老工程`PlaceConstruction`算出的"偏好方向"（按`quad`长宽比选
    `FACE_WEST/FACE_NORTH`）覆盖——但只有偏好方向在`boundaryRoads`里确实有非空条目时才
    采用，否则保留兜底方向，照抄`ResidenceBuilding::Layout`同款保护（避免行人/车辆导航
    "outside"端点查不到路），不照抄老工程"直接覆盖、不检查"的写法。
  - `footprint`占地比例照抄`PlaceConstruction`公式（`quad`任一边小于6时按
    `(边长-2.4)/边长`收缩，否则0.6）。
  - 每层`FloorAssetSpec.wallMaterial="/Game/Asset/Materials/White.White"`（照抄老工程
    `wallTexture`），`floorHeights`统一`0.4f`（照抄老工程单一`height`字段的语义）。

## 依赖关系

- 依赖：`Source/Dependence/map/building_mod.h`/`room_mod.h`/`component_mod.h`
  （`BuildingMod`/`RoomMod`/`ComponentMod`基类）、`common/utility.h`（`GetRandom`）。
- 被谁依赖：`Source/Basic/basic.cpp`（`RegisterModBuildings`/`RegisterModComponents`/
  `RegisterModRooms`三组注册函数，和`ResidenceXxx`/`FactoryXxx`一起注册）。

## 命名说明

文件名是`building_shop.h`（不是`building_factory.h`那种可能冲突的命名）——这一组本身
没有命名冲突，冲突问题在`FactoryBuilding`那一侧，见`building_factory.md`。
