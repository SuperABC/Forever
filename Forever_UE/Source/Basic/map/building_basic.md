# building_basic.h/.cpp

## 职责

`ResidenceBuilding`/`ShopBuilding`/`FactoryBuilding`是Building域三个默认实现，住宅/商店/
工厂三种建筑类型，合在同一份`building_basic.h/.cpp`里(每个concept统一一份
`<concept>_basic.h/.cpp`，不按业务场景拆文件，和`terrain_basic`/`roadnet_basic`组织方式
一致)。

## ResidenceBuilding

通用占位类型，不绑定具体商业语义，只走权重CDF随机填充路径：

- `GetPower(AREA_TYPE)`：只在`AREA_RESIDENTIAL_HIGH/MIDDLE/LOW`返回`1.f`，其余`0.f`——必须
  限定在住宅分区，否则会和`ShopBuilding`/`FactoryBuilding`的商业/工业分区混入。
- `RandomAcreage()`：`2000.f * powf(1.f + GetRandom(1000)/1000.f*2.f, 2)`；
  `GetAcreageMin()=2000.f`；`GetAcreageMax()=18000.f`。
- `Assign()`：空实现，走权重CDF随机填充路径。
- `Layout()`：按`quad`尺寸分档随机选一套模板组合(`layout` 0-3)+对应房间槽位密度(`size`)，
  `layout==1/3`带电梯井道模板(`preset_lobby_wing_*`/`preset_nshape_double_*`)，配合
  `AssignElevatorCabin`产出`Elevator`几何。`direction<0`时先从`boundaryRoads`随机挑一个
  有真实边界路的方向兜底，再用按`quad`尺寸/朝向算出的偏好方向覆盖，但只有偏好方向在
  `boundaryRoads`里确实有非空条目时才采用，否则保留兜底方向(避免行人/车辆导航的"outside"
  端点查不到路)。楼层数按`quad.GetAcreage()`分档(`<5000`→3-5层，`<10000`→6-9层，否则
  10-14层)，每层高度统一0.4f(地下室0.35f)。

## ShopBuilding

"一个本体独占一个mod实例"模型：

- `GetPower(AREA_TYPE)`：只在`AREA_COMMERCIAL_HIGH/MIDDLE/LOW`返回`1.f`，其余`0.f`。
- `RandomAcreage()`：`4000.f * powf(1.f + GetRandom(1000)/1000.f*1.f, 2)`；
  `GetAcreageMin()=4000.f`；`GetAcreageMax()=16000.f`。
- `Assign()`：空实现，走权重CDF随机填充路径。
- `Layout()`：复用`Resource/Layouts/`下的`preset_lobby_linear_*`/`preset_circle_double_*`
  模板：
  - `quad.GetAcreage() < 6000`：固定2层，`preset_lobby_linear_fg+`(1楼，`ShopRoom`+
    `WarehouseRoom`过道)→`preset_lobby_linear_fu-`(2楼，纯`WarehouseRoom`)。
  - 否则：`basements=1`，地下室随机选`preset_circle_double_pg+`(`ParkingRoom`)或
    `preset_circle_double_bg+`(5间`WarehouseRoom`)；1楼`preset_circle_double_fg+-`(2间
    `ShopRoom`+4间`WarehouseRoom`)，2楼`preset_circle_double_fu+-`(5间`WarehouseRoom`)。
  - `direction<0`兜底逻辑同`ResidenceBuilding`。每层`FloorAssetSpec.wallMaterial=
    "/Game/Asset/Materials/White.White"`，`floorHeights`统一`0.4f`。

## FactoryBuilding

"一个本体独占一个mod实例"模型：

- `GetPower(AREA_TYPE)`：只在`AREA_INDUSTRIAL_HIGH/MIDDLE/LOW`返回`1.f`，其余`0.f`。
- `RandomAcreage()`：`2000.f * powf(1.f + GetRandom(1000)/1000.f*1.f, 2)`；
  `GetAcreageMin()=4000.f`；`GetAcreageMax()=16000.f`(公式基数和min常量不一致是既有数值，
  `RandomAcreage`理论最小值趋近2000，小于`GetAcreageMin`声称的4000，不做"修正")。
- `Assign()`：空实现，走权重CDF随机填充路径。
- `Layout()`：复用`Resource/Layouts/`下的`preset_single_room_*`模板：
  - 方向按`quad`长宽比选`FACE_NORTH`(`sizeX>sizeY`)或`FACE_WEST`(否则)+`GetRandom(2)`；
    对应边长`>4`时`basements=1`。`direction`引用参数是唯一输出，偏好方向只有
    `boundaryRoads`里确实有对应方向的路时才采用，否则按老规矩随机挑一个有路的方向兜底。
  - `basements>0`：地下室`preset_single_room_pg+`(`ParkingRoom`，和`ShopBuilding`共用同一个
    `ParkingRoom`类，定义在`room_basic.h`)→1楼`preset_single_room_fg-`；否则只有1楼
    `preset_single_room_fg`。1楼都放1个`FactoryRoom`。
  - `footprint`固定0.6x0.6，`layers`显式固定为1(地上1层)，`floorHeights`统一`0.6f`，
    `FloorAssetSpec.wallMaterial="/Game/Asset/Materials/White.White"`。`basements`是否有
    地下室由`quad`尺寸判定，和地上层数无关。
## ParkBuilding

公园(`building_park`)：`Layout()`里`basements=0`/`layers=0`(不放楼体)；`LayoutTrees()`里调用
`AddTreeArea({0,0,0,0}, {1,0,1,0}, max(1, sizeX*sizeY/PARK_ELEMENTS_PER_TREE), ...)`在整块quad里随机种树
(建筑植被参考系是整块quad，这两个对角就是整块地)，
密度暂定每2个element²(约200m²)一棵。不做显式占位，走权重CDF随机填充：`AREA_GREEN`权重1、
住宅三档0.25(现在的井字路网不产出`AREA_GREEN`的lot，给住宅区一点权重方便测试)，
`RandomAcreage`在3000~8000之间。没有room/component，`basic.cpp`里不需要依赖表条目；
`config.json`的`building_mods`里要有`"building_park"`才会启用。UE层跳过0层建筑不渲染楼体。

## MoharBuilding

`building_mohar`：`Layout()`里用`AddDecoration`在楼体中心放`3rdParty/Mohar`的成品建筑模型，测试
"建筑mod指定放置mesh、随朝向旋转"(见`feature_spec.md`"建筑自定义模型"一节)。现在和框架自带的楼层/
房间系统融合：1层，楼体footprint用朝北时(大门在北墙，+X东、+Y南)相对模型中心的四条边`MOHAR_BODY_WEST/EAST/NORTH/SOUTH`定义(地图单位；从目测30m×30m起步、往南挪5m，再按实测收放各边)，其余朝向按`InversePoint`同一套旋转换算，模型位置反向补偿、留在地块中心不动，3层(层高`MOHAR_FLOOR1/2/3_HEIGHT`：5.5m/4m/4m)，一楼用`3rdparty_mohar_fu1.layout`拟合模型里的房间范围，二楼暂时复用同一个
模板(`MOHAR_FLOOR2_TEMPLATE`，对不上再换fu2)，三楼用单独的`3rdparty_mohar_fu3.layout`，每层的single槽位实例化成`room_residence`，各层挂在同一个
`component_residence`下。每层的`FloorAssetSpec::buildShell`由`MOHAR_FLOOR1/2/3_BUILD_SHELL`控制，三层都是false(外观由模型提供，框架
不画墙壁/地板/天花板，房间和Room碰撞盒照常生成)；核对某一层范围时把那一层临时改成true。
direction为-1时和Residence一样从有路的边界里随机挑一个方向回写——模型朝向靠它决定。默认摆位
(楼体中心、初始yaw -90(逆时针90度，让模型大门和模板朝向一致)、离地0、缩放xy 0.06、z 0.04、yaw -90，按朝北定义)写成文件里的`MOHAR_*`宏，方便按实际效果调。走权重
CDF随机填充：住宅三档权重1，`RandomAcreage`在3000~8000之间；没有room/component；`config.json`
的`building_mods`里要有`"building_mohar"`。

## 依赖关系

- 依赖：`Source/Dependence/map/building_mod.h`(`BuildingMod`基类)、`room_basic.h`
  (`ResidenceRoom`/`ShopRoom`/`WarehouseRoom`/`ParkingRoom`/`FactoryRoom`，`Layout()`按
  房间类型调`ArrangeRow`/`AssignRoom`)、`common/utility.h`(`GetRandom`)。
- 被谁依赖：`Source/Basic/basic.cpp`(`RegisterModBuildings`注册三个类型)、
  `zone_basic.cpp`(`ResidenceZone::Layout()`内部建筑复用`ResidenceBuilding::GetId()`)。

## AirportBuilding 占地
机场`Assign`请求的地块(沿路x纵深)优先12x8(地图单位)，放不下退回和火车站一样的7x4(`FindAdaptivePlacement`的minFrontage/minDepth决定占地大小)；楼体`footprint`仍是0.18/0.35/0.6，楼体本身没变。
