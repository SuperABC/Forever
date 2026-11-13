# building_mohar.h / building_mohar.cpp

## 职责

`MoharBuilding`(`building_mohar`)：Mohar：用AddDecoration放一个成品建筑模型(3rdParty/Mohar)，并和楼层/房间系统融合，详见building_*.md。

## 关键设计

- **"一个本体独占一个mod实例"模型**：`RandomAcreage`/`GetAcreageMin`/`GetAcreageMax`/`GetPower`/`Assign`全是不需要实例的static方法，通过`BuildingFactory::RegisterBuilding`注册；楼层/房间等实例级数据都在非static的`Layout()`里设。
- **`direction`兜底**：`direction`对显式占位是真实方向，权重CDF/`FillRemainder`落地时是`-1`。住宅/商店/工厂/Mohar的`Layout()`先从`boundaryRoads`里随机挑一个有真实边界路的方向回写(行人/车辆导航的"outside"端点要靠它找到该连的路，见`building_mod.h`的`Layout()`注释)；之后按`quad`尺寸/长宽比算一个偏好方向，**只有偏好方向在`boundaryRoads`里确实有非空条目时才采用**，否则保留兜底方向。
- **`GetPower(AREA_TYPE)`限定分区类型**：住宅/商店/工厂分别只在住宅/商业/工业三档分区返回`1.f`，否则会在别的分区和其它类型一起参与`FillRemainder`的权重CDF竞争而混入。没有对应建筑类型的分区(目前办公区，除中心lot被火车站/机场/园区占用外)保持空地，属预期。分区由路网生成，见`roadnet_jing.md`。
- **火车站/机场/公园/Mohar的`Layout`依赖`RoadnetMod`之外的约定**：`stationMod`字符串指向`traffic/station_*`里的StationMod。

## 三档密度与办公楼

- `GetPower`只在对应类型和密度的分区返回`1.f`：`AREA_{RESIDENCE,COMMERCE,INDUSTRY,OFFICE}_{HIGH,MIDDLE,LOW}`。

## MoharBuilding

`building_mohar`：用`AddDecoration`放`3rdParty/Mohar`成品建筑模型，同时和框架自带的楼层/房间系统融合。

- 外观由模型提供，3层、层高`MOHAR_FLOOR1/2/3_HEIGHT`(5.5m/4m/4m)，每层的`FloorAssetSpec::buildShell`由`MOHAR_FLOOR1/2/3_BUILD_SHELL`控制，**都是false**(框架不画墙壁/地板/天花板，房间和Room碰撞盒照常生成)；核对某层范围时把那一层临时改成true。

- 一楼`3rdparty_mohar_fg1.layout`拟合模型里的房间范围，二楼用`3rdparty_mohar_fu2.layout`，三楼用`3rdparty_mohar_fu3.layout`，三层模板各自独立；每层的single槽位实例化成`room_residence`，各层挂在同一个`component_residence`下。

- 楼体`footprint`用朝北时(大门在北墙，+X东、+Y南)相对模型中心的四条边`MOHAR_BODY_WEST/EAST/NORTH/SOUTH`定义(地图单位)，其余朝向按`InversePoint`同一套旋转换算，朝东/西时宽进深互换；模型位置反向补偿，留在地块中心不动。模型默认摆位(缩放`xy 0.06`/`z 0.04`、`MOHAR_YAW=-90`让模型大门和模板朝向一致、离地0)写成文件里的`MOHAR_*`宏，方便按实际效果调。

- 走权重CDF：住宅三档权重1，`RandomAcreage`在3000~8000之间；`config.json`的`building_mods`里要有`"building_mohar"`。

## 依赖关系

- 依赖：`Source/Dependence/map/building_mod.h`(`BuildingMod`基类)、`common/utility.h`(`GetRandom`)；用字符串引用`room_*`/`component_*`里各类型的id(Basic层文件间不互相`#include`)。
- 被谁依赖：`Source/Basic/basic.cpp`(`RegisterModBuildings`注册七个类型，依赖表声明所需的component/room/script)；`zone_*.cpp`(`Residence{High,Middle,Low}Zone::Layout()`内部建筑分别引用`building_residence_{high,middle,low}`)；`traffic/station_*`(火车站楼/机场楼的`stationMod`指向`station_train`/`station_air`，依赖表里是站点依赖楼，楼不依赖站点)。

## 待办/后续阶段

- `MOHAR_*`的footprint、层高、模板、缩放等数值是按实测调的默认值，还要按效果继续调。
- 二楼模板`fu2`目前和一楼`fg1`内容一样，等二楼房间范围对上模型再调。
