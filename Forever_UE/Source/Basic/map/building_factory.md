# building_plant.h/.cpp / room_plant.h/.cpp / component_plant.h

## 职责

`FactoryBuilding`/`FactoryRoom`/`FactoryComponent`是Building/Room/Component域新增的
"工厂"默认内容，从`Source/Basic/README.md`共用文档独立出来，和`building_shop.md`同一批
新增，为society/industry域后续的job/manufacture系统做准备。照抄老工程
`E:\Projects\Forever_UE\Source\Basic\map\building_basic.h/.cpp`（`FactoryBuilding`）、
`room_basic.h/.cpp`（`FactoryRoom`）、`component_basic.h/.cpp`（`FactoryComponent`）的
对应部分——这次**不**迁移`ExperienceManufacture`/`ExperienceProduct`（Industry域）,
Industry域目前还是空骨架，留到`PHASE4_PLAN.md`真正点名迁移时再做。

## 文件命名（避免和Xxx注册表头文件冲突）

这一组文件名是`building_plant.h`/`component_plant.h`/`room_plant.h`，**不是**
`building_factory.h`/`component_factory.h`/`room_factory.h`——`Source/Dependence/map/
building_factory.h`（`BuildingFactory`注册表类）、`component_factory.h`
（`ComponentFactory`）、`room_factory.h`（`RoomFactory`）已经占用了这三个相对路径。
`Source/Basic/basic.cpp`里`#include "map/building_factory.h"`等三行本意引用的是
Dependence这三个注册表头文件；如果Basic目录下也放一份同名文件，取决于include目录搜索
顺序，有可能被意外遮蔽导致这三行`#include`解析到错误的文件、编译失败（`BuildingFactory`/
`ComponentFactory`/`RoomFactory`类型未定义）。类名/`GetId()`仍然是`FactoryBuilding`/
`"building_factory"`等，只有文件名避开了冲突。

## FactoryRoom（`room_plant.h/.cpp`）

`isManufacture=true; manufactureTypes={"experience"};`——照抄老工程
`FactoryRoom::ConfigRoom`。`"experience"`这个生产类型对应的`ExperienceManufacture`/
`ExperienceProduct`属于Industry域，这次不迁移，`manufactureTypes`这个占位字段先带上
真实字符串，目前没有任何调用方读取它。

## FactoryComponent（`component_plant.h`）

trivial占位，和`ResidenceComponent`/`ShopComponent`同样极简
（`GetId()="component_factory"`）。

## FactoryBuilding（`building_plant.h/.cpp`）

和`ResidenceBuilding`/`ShopBuilding`一样是"一个本体独占一个mod实例"模型。

- `GetPower(AREA_TYPE)`：照抄老工程`GetPowers()`——只在`AREA_INDUSTRIAL_HIGH/MIDDLE/LOW`
  上返回`1.f`，其余`0.f`。
- `RandomAcreage()`：`2000.f * powf(1.f + GetRandom(1000)/1000.f*1.f, 2)`；
  `GetAcreageMin()=4000.f`；`GetAcreageMax()=16000.f`——公式基数`2000`和min常量`4000`
  不一致是老工程本来的数值（`RandomAcreage`理论最小值趋近`2000`，小于`GetAcreageMin`
  声称的`4000`），照抄不做"修正"。
- `Assign()`：空实现，走权重CDF随机填充路径。
- `Layout()`：完整照抄老工程`FactoryBuilding::LayoutBuilding`（`building_basic.cpp:
  460-490`），复用新工程`Resource/Layouts/`下已有的`preset_single_room_*`模板：
  - 方向：按`quad`长宽比选`FACE_NORTH`（`sizeX>sizeY`）或`FACE_WEST`（否则）+
    `GetRandom(2)`；对应那条边长`>4`时`basements=1`。老工程这里用一个局部变量
    `direction`覆盖同名成员、从未真正回写方向（疑似遗留bug）——这次新API的`direction`
    引用参数就是唯一的输出，所以这个偏好方向要真正生效，同`ResidenceBuilding`/
    `ShopBuilding`一样，只有`boundaryRoads`里确实有这个方向的路时才采用，`direction<0`
    时先按老规矩从`boundaryRoads`随机挑一个有路的方向兜底。
  - `basements>0`：地下室`preset_single_room_pg+`（`ParkingRoom`，和`ShopBuilding`共用
    同一个`ParkingRoom`类，定义在`room_shop.h`）→ 1楼`preset_single_room_fg-`；否则
    只有1楼`preset_single_room_fg`。1楼都放1个`FactoryRoom`。
  - `footprint`固定0.6x0.6（照抄`PlaceConstruction`），`layers`应用户要求显式固定为1
    （地上1层，不依赖`BuildingMod`基类默认值——老工程没有显式设置，靠基类默认值凑出同样
    的1层，这次显式赋值，行为不变），`floorHeights`统一`0.6f`（照抄老工程`height`字段）,
    `FloorAssetSpec.wallMaterial="/Game/Asset/Materials/White.White"`。`basements`是否
    有地下室由`quad`尺寸判定，和地上层数无关。

## 依赖关系

- 依赖：`Source/Dependence/map/building_mod.h`/`room_mod.h`/`component_mod.h`、
  `common/utility.h`（`GetRandom`）、`room_shop.h`（`ParkingRoom`定义在那里，Shop/Factory
  共用）。
- 被谁依赖：`Source/Basic/basic.cpp`（`RegisterModBuildings`/`RegisterModComponents`/
  `RegisterModRooms`三组注册函数）。

## 验证结果

一次PIE/`-game`地图生成实测（临时`UE_LOG`统计，验证完已删除）：`AREA_TYPE`分布
`AREA_OFFICIAL_HIGH`(井字中心)恰好1个，`AREA_RESIDENTIAL_HIGH`/`AREA_COMMERCIAL_HIGH`/
`AREA_INDUSTRIAL_HIGH`三者分别14/17/16个（1:1:1随机分配，见`roadnet_basic.md`）；
`Building`类型统计`building_shop`91个、`building_residence`377个、`building_factory`
104个，全部成功生成，无崩溃。
