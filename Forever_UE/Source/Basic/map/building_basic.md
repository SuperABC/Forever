# building_basic.h/.cpp

## 职责

Building域的七个默认实现，合在同一份`building_basic.h/.cpp`里(每个concept统一一份`<concept>_basic.h/.cpp`，不按业务场景拆文件)：`ResidenceBuilding`/`ShopBuilding`/`FactoryBuilding`(住宅/商店/工厂)、`TrainStationBuilding`/`AirportBuilding`(火车站/机场)、`ParkBuilding`(公园)、`MoharBuilding`(成品模型住宅)。

## 关键设计

- **"一个本体独占一个mod实例"模型**：`RandomAcreage`/`GetAcreageMin`/`GetAcreageMax`/`GetPower`/`Assign`全是不需要实例的static方法，通过`BuildingFactory::RegisterBuilding`注册；楼层/房间等实例级数据都在非static的`Layout()`里设。
- **`direction`兜底**：`direction`对显式占位是真实方向，权重CDF/`FillRemainder`落地时是`-1`。住宅/商店/工厂/Mohar的`Layout()`先从`boundaryRoads`里随机挑一个有真实边界路的方向回写(行人/车辆导航的"outside"端点要靠它找到该连的路，见`building_mod.h`的`Layout()`注释)；之后按`quad`尺寸/长宽比算一个偏好方向，**只有偏好方向在`boundaryRoads`里确实有非空条目时才采用**，否则保留兜底方向。
- **`GetPower(AREA_TYPE)`限定分区类型**：住宅/商店/工厂分别只在住宅/商业/工业三档分区返回`1.f`，否则会在别的分区和其它类型一起参与`FillRemainder`的权重CDF竞争而混入。没有对应建筑类型的分区(目前办公区，除中心lot被火车站/机场/园区占用外)保持空地，属预期。分区由路网生成，见`roadnet_basic.md`。
- **火车站/机场/公园/Mohar的`Layout`依赖`RoadnetMod`之外的约定**：`stationMod`字符串指向`traffic/station_basic`里的StationMod。

## ResidenceBuilding

- `GetPower`：`AREA_RESIDENTIAL_HIGH/MIDDLE/LOW`返回`1.f`。`RandomAcreage()=2000*(1+rand*2)^2`，`GetAcreageMin()=2000`，`GetAcreageMax()=18000`。`Assign()`空实现。
- `Layout()`：`footprint`居中占80%，`basements=1`；`layers`按`quad.GetAcreage()`分档(`<5000`→3-5，`<10000`→6-9，否则10-14)；层高地上统一0.4、地下室0.35。按`quad`尺寸分档随机选一套模板组合(`layout` 0-3)和房间槽位密度`size`：`layout==1/3`带电梯井道模板(`preset_lobby_wing_*`/`preset_nshape_double_*`)，配合`AssignElevatorCabin`产出`Elevator`几何(脚本`elevator_basic`)。`layout==3`的第0个槽位是`room_empty`占位，不铺真实房间。
- 大门`doorSpecs["entrance"]`：随机左右滑动单开门(`Cube`资产占位)，只有corridor门洞带`entrance`标签才用到，见`Core/map/door.h`的`ResolveDoorSpec`。
- 挂示例脚本`entity_event`。

## ShopBuilding

- `GetPower`：商业三档分区。`RandomAcreage()=4000*(1+rand)^2`，`GetAcreageMin()=4000`，`GetAcreageMax()=16000`。
- `Layout()`：`footprint`居中占0.6(某边`<6`时收到`(size-2.4)/size`)，`layers`固定2，每层`FloorAssetSpec.wallMaterial`为`White`，层高统一0.4。复用`preset_lobby_linear_*`/`preset_circle_double_*`模板：
  - `quad.GetAcreage() < 6000`：无地下室。1楼`preset_lobby_linear_fg+`(`ShopRoom`+`WarehouseRoom`过道)→2楼`preset_lobby_linear_fu-`(纯`WarehouseRoom`)。
  - 否则`basements=1`，地下室随机选`preset_circle_double_pg+`(`ParkingRoom`)或`preset_circle_double_bg+`(5间`WarehouseRoom`)；1楼`preset_circle_double_fg+-`(2间`ShopRoom`+4间`WarehouseRoom`)，2楼`preset_circle_double_fu+-`(5间`WarehouseRoom`)。
- 大门有名字"商店大门"，但脚本挂在`ShopRoom`的房间门上，原因见`room_basic.md`；`doorSpecs["entrance"]`上也设了同名脚本规格。

## FactoryBuilding

- `GetPower`：工业三档分区。`RandomAcreage()=2000*(1+rand)^2`，`GetAcreageMin()=4000`，`GetAcreageMax()=16000`——公式基数和Min不一致(随机值可能小于Min)是既有数值，不"修正"。
- `Layout()`：`footprint`固定0.6x0.6，`layers`固定1，层高统一0.6。偏好方向按`quad`长宽比选`FACE_NORTH+rand(2)`或`FACE_WEST+rand(2)`，对应边长`>4`时才`basements=1`(是否有地下室由`quad`尺寸判定，和地上层数无关)。有地下室：`preset_single_room_pg+`(`ParkingRoom`，和`ShopBuilding`共用)→1楼`preset_single_room_fg-`；否则只有1楼`preset_single_room_fg`。1楼放1个工坊房间，`FarmRoom`/`RanchRoom`/`FoodFactoryRoom`等概率随机选一。

## TrainStationBuilding / AirportBuilding

- 全图各唯一一个，`GetPower`恒0不参与权重CDF，`RandomAcreage`/`Min`/`Max`固定2000，靠`Assign`显式占位。分别承载`StationMod`的`station_train`/`station_air`(`stationMod`字段)。
- **`Assign`**：只看井字中心lot(`AREA_OFFICIAL_HIGH`)，依次尝试4个方向用`Lot::FindAdaptivePlacement`按当前实际剩余空间自适应找位置，不挑具体方向，和园区(`ResidenceZone`)共存。火车站要沿路`7`x纵深`4`(地图单位)，余量全挪到`marginEnd`贴边起点角；机场优先`12`x`8`，放不下退回`7`x`4`，余量全挪到`marginStart`贴另一端的角，这样两者即使同在一条边上也占两个对角。`request.lot`是命中的自由子块而不是`lot`本身，`adaptiveParent`仅在二者不同时设，供Core把子块残余空间搬回，见`FindAdaptivePlacement`声明。
- **`TrainStationBuilding::Layout`**：`footprint`占满整个Building区域，`basements=1`+`layers=1`，层高0.6；地下1层站台层`preset_train_station_bg+`(楼梯通上来，`Stair_Line`资产)、地面1层大厅`preset_train_station_fg-`放2间`TrainStationRoom`，两层各2台电梯。`TrainStation::Layout`的轨道/接口(`depthRatio` 0.55/0.65)因此落在楼体范围内。
- **`AirportBuilding::Layout`**：无地下室、1层，层高0.6，模板`preset_single_room_fg`+1间`AirportRoom`。`footprint`用`NearRoadFootprint(direction, 0.18, 0.35, 0.6)`把楼体压到靠路一侧，深处留给`AirStation::Layout`的跑道(`depthRatio` 0.65/0.85)，避免公共交通连线穿过楼体实体。

## ParkBuilding

`building_park`：`Layout()`里`basements=0`/`layers=0`(不放楼体，UE层跳过0层建筑)；`LayoutTrees()`里用`AddTreeArea`在整块`quad`里随机种树(建筑植被参考系是整块`quad`，两个对角`(0,0)-(1,1)`就是整块地)，密度每`PARK_ELEMENTS_PER_TREE`(2)个element²一棵。不做显式占位，走权重CDF：`AREA_GREEN`权重1，住宅三档0.25(当前路网不产出`AREA_GREEN`的lot，给住宅区一点权重方便测试)；`RandomAcreage`在3000~8000之间。没有room/component，`basic.cpp`不需要依赖表条目；`config.json`的`building_mods`里要有`"building_park"`才启用。

## MoharBuilding

`building_mohar`：用`AddDecoration`放`3rdParty/Mohar`成品建筑模型，同时和框架自带的楼层/房间系统融合。
- 外观由模型提供，3层、层高`MOHAR_FLOOR1/2/3_HEIGHT`(5.5m/4m/4m)，每层的`FloorAssetSpec::buildShell`由`MOHAR_FLOOR1/2/3_BUILD_SHELL`控制，**都是false**(框架不画墙壁/地板/天花板，房间和Room碰撞盒照常生成)；核对某层范围时把那一层临时改成true。
- 一楼`3rdparty_mohar_fg1.layout`拟合模型里的房间范围，二楼用`3rdparty_mohar_fu2.layout`，三楼用`3rdparty_mohar_fu3.layout`，三层模板各自独立；每层的single槽位实例化成`room_residence`，各层挂在同一个`component_residence`下。
- 楼体`footprint`用朝北时(大门在北墙，+X东、+Y南)相对模型中心的四条边`MOHAR_BODY_WEST/EAST/NORTH/SOUTH`定义(地图单位)，其余朝向按`InversePoint`同一套旋转换算，朝东/西时宽进深互换；模型位置反向补偿，留在地块中心不动。模型默认摆位(缩放`xy 0.06`/`z 0.04`、`MOHAR_YAW=-90`让模型大门和模板朝向一致、离地0)写成文件里的`MOHAR_*`宏，方便按实际效果调。
- 走权重CDF：住宅三档权重1，`RandomAcreage`在3000~8000之间；`config.json`的`building_mods`里要有`"building_mohar"`。

## 依赖关系

- 依赖：`Source/Dependence/map/building_mod.h`(`BuildingMod`基类)、`common/utility.h`(`GetRandom`)；用字符串引用`room_basic`/`component_basic`里各类型的id(Basic层文件间不互相`#include`)。
- 被谁依赖：`Source/Basic/basic.cpp`(`RegisterModBuildings`注册七个类型，依赖表声明所需的component/room/script)；`zone_basic.cpp`(`ResidenceZone::Layout()`内部建筑引用`building_residence`)；`traffic/station_basic`(`station_train`/`station_air`依赖对应Building)。

## 待办/后续阶段

- `MOHAR_*`的footprint、层高、模板、缩放等数值是按实测调的默认值，还要按效果继续调。
- 二楼模板`fu2`目前和一楼`fg1`内容一样，等二楼房间范围对上模型再调。
