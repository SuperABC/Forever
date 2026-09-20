# building_basic.h/.cpp

## 职责

`ResidenceBuilding`/`ShopBuilding`/`FactoryBuilding`是Building域三个默认内容，合并进
同一份`building_basic.h/.cpp`(不再按residence/shop/plant各开一个文件，和
`terrain_basic.h/.cpp`里`OceanTerrain`/`MountainTerrain`合并的方式一样，见
`Source/Basic/README.md`)。照抄老工程`E:\Projects\Forever_UE\Source\Basic\map\
building_basic.h/.cpp`对应的三个类，为society域后续的job系统（商店/工厂提供工作岗位）
做准备——这次**不**迁移`ShopSalerJob`/`ShopOrganization`/`ExperienceManufacture`/
`ExperienceProduct`，Society/Industry域这几个概念目前还是空骨架（只有`GetType`/
`GetName`），留到`PHASE4_PLAN.md`真正点名迁移时再做。

## ResidenceBuilding

只走权重CDF随机填充路径（不使用显式占位），是三个类型里唯一没有具体业务场景语义
（不是商店/工厂）的通用占位类型：

- `GetPower(AREA_TYPE)`：只在`AREA_RESIDENTIAL_HIGH/MIDDLE/LOW`上返回`1.f`，其余`0.f`——
  引入`ShopBuilding`/`FactoryBuilding`各自只在商业/工业分区有权重之后，`ResidenceBuilding`
  如果对所有分区一视同仁会导致商业区/工业区里混入住宅建筑（PIE验证发现），改成只在住宅
  分区上有权重。
- `RandomAcreage()`：`2000.f * powf(1.f + GetRandom(1000)/1000.f*2.f, 2)`；
  `GetAcreageMin()=2000.f`；`GetAcreageMax()=18000.f`——数值照抄老工程
  `ResidentialBuilding`。
- `Assign()`：空实现，走权重CDF随机填充路径。
- `Layout()`：按`quad`尺寸分档随机选一套模板组合（`layout` 0-3）+对应的房间槽位密度
  （`size`），完整照抄老工程`ResidentialBuilding::LayoutBuilding`的选型逻辑，`layout==1/3`
  自带电梯井道模板数据（`preset_lobby_wing_*`/`preset_nshape_double_*`），配合
  `AssignElevatorCabin`产出真正的`Elevator`几何。`direction<0`（`FillRemainder`落地）时
  先从`boundaryRoads`随机挑一个有真实边界路的方向兜底，再用按`quad`尺寸/朝向算出的
  "偏好方向"覆盖——但只有偏好方向在`boundaryRoads`里确实有非空条目时才采用，否则保留
  兜底方向（不照抄老工程"直接覆盖、不检查"的写法，避免行人/车辆导航"outside"端点查不到
  路）。楼层数按`quad.GetAcreage()`分档（`<5000`→3-5层，`<10000`→6-9层，否则10-14层），
  每层高度统一0.4f（地下室0.35f），不再循环取不同值。

## ShopBuilding

和`ResidenceBuilding`一样是"一个本体独占一个mod实例"模型：

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
  - `direction<0`兜底逻辑同`ResidenceBuilding`，`footprint`占地比例照抄`PlaceConstruction`
    公式（`quad`任一边小于6时按`(边长-2.4)/边长`收缩，否则0.6）。每层
    `FloorAssetSpec.wallMaterial="/Game/Asset/Materials/White.White"`，`floorHeights`
    统一`0.4f`。

## FactoryBuilding

和`ResidenceBuilding`/`ShopBuilding`一样是"一个本体独占一个mod实例"模型：

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
    同一个`ParkingRoom`类，定义在`room_basic.h`）→ 1楼`preset_single_room_fg-`；否则
    只有1楼`preset_single_room_fg`。1楼都放1个`FactoryRoom`。
  - `footprint`固定0.6x0.6（照抄`PlaceConstruction`），`layers`应用户要求显式固定为1
    （地上1层，不依赖`BuildingMod`基类默认值——老工程没有显式设置，靠基类默认值凑出同样
    的1层，这次显式赋值，行为不变），`floorHeights`统一`0.6f`,
    `FloorAssetSpec.wallMaterial="/Game/Asset/Materials/White.White"`。`basements`是否
    有地下室由`quad`尺寸判定，和地上层数无关。

## 文件合并说明

原来这三个类型分别放在`building_residence.h/.cpp`、`building_shop.h/.cpp`、
`building_plant.h/.cpp`三个文件里——后两个之所以起`shop`/`plant`这种和`GetId()`不完全
对应的名字（`FactoryBuilding`文件名是`building_plant`而不是`building_factory`），是为了
避开和`Source/Dependence/map/building_factory.h`（`BuildingFactory`注册表类）在Basic/
Dependence两个include目录下同名冲突。这次合并成一份`building_basic.h/.cpp`之后，
这个历史包袱自然消失——`building_basic.h`本身和Dependence那边任何文件都不同名，不需要
再为了避让`_factory`后缀而给类型本身起一个和`GetId()`不一致的文件名。合并之后Basic/map
下每个concept统一只对应一份`<concept>_basic.h/.cpp`，不再按"这个类型是哪种业务场景
(residence/shop/plant)"拆文件，和`terrain_basic`/`roadnet_basic`保持同一个组织方式。

## 依赖关系

- 依赖：`Source/Dependence/map/building_mod.h`（`BuildingMod`基类）、`room_basic.h`
  （`ResidenceRoom`/`ShopRoom`/`WarehouseRoom`/`ParkingRoom`/`FactoryRoom`，`Layout()`
  按房间类型调`ArrangeRow`/`AssignRoom`）、`common/utility.h`（`GetRandom`）。
- 被谁依赖：`Source/Basic/basic.cpp`（`RegisterModBuildings`注册三个类型）、
  `zone_basic.cpp`（`ResidenceZone::Layout()`内部建筑复用`ResidenceBuilding::GetId()`）。

## 验证结果

一次PIE/`-game`地图生成实测（临时`UE_LOG`统计，验证完已删除）：`AREA_TYPE`分布
`AREA_OFFICIAL_HIGH`(井字中心)恰好1个，`AREA_RESIDENTIAL_HIGH`/`AREA_COMMERCIAL_HIGH`/
`AREA_INDUSTRIAL_HIGH`三者分别14/17/16个（1:1:1随机分配，见`roadnet_basic.md`）；
`Building`类型统计`building_shop`91个、`building_residence`377个、`building_factory`
104个，全部成功生成，无崩溃。
