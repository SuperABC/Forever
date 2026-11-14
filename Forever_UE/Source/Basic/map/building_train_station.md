# building_train_station.h / building_train_station.cpp

## 职责

`TrainStationBuilding`(`building_train_station`)：火车站，承载StationMod"station_train"。公交站不走Building/Lot路径，见 station_*.h的BusStation::AssignRoads。

## 关键设计

- **"一个本体独占一个mod实例"模型**：`RandomAcreage`/`GetAcreageMin`/`GetAcreageMax`/`GetPower`/`Assign`全是不需要实例的static方法，通过`BuildingFactory::RegisterBuilding`注册；楼层/房间等实例级数据都在非static的`Layout()`里设。
- **`direction`兜底**：`direction`对显式占位是真实方向，权重CDF/`FillRemainder`落地时是`-1`。住宅/商店/工厂/Mohar的`Layout()`先从`boundaryRoads`里随机挑一个有真实边界路的方向回写(行人/车辆导航的"outside"端点要靠它找到该连的路，见`building_mod.h`的`Layout()`注释)；之后按`quad`尺寸/长宽比算一个偏好方向，**只有偏好方向在`boundaryRoads`里确实有非空条目时才采用**，否则保留兜底方向。
- **`GetPower(AREA_TYPE)`限定分区类型**：住宅/商店/工厂分别只在住宅/商业/工业三档分区返回`1.f`，否则会在别的分区和其它类型一起参与`FillRemainder`的权重CDF竞争而混入。没有对应建筑类型的分区(目前办公区，除中心lot被火车站/机场/园区占用外)保持空地，属预期。分区由路网生成，见`roadnet_jing.md`。
- **火车站/机场/公园/Mohar的`Layout`依赖`RoadnetMod`之外的约定**：`stationMod`字符串指向`traffic/station_*`里的StationMod。

## 三档密度与办公楼

- `GetPower`只在对应类型和密度的分区返回`1.f`：`AREA_{RESIDENCE,COMMERCE,INDUSTRY,OFFICE}_{HIGH,MIDDLE,LOW}`。

## TrainStationBuilding / AirportBuilding

- 机场/火车站都不再全图唯一(见各自`Assign`)，`GetPower`恒0不参与权重CDF，`RandomAcreage`/`Min`/`Max`固定2000，靠`Assign`显式占位。分别承载`StationMod`的`station_train`/`station_air`(`stationMod`字段)。
- **`Assign`(火车站：1座中央 + 东南西北各1座)**：一次扫完全图lot。**中央车站**：随机一块能放的`AREA_COMMERCE_HIGH`地块(**放不了就退化**：在随机一块能放的`AREA_COMMERCE_MIDDLE/LOW`地块放一座车站，朝向不限，不再放外围四座；中/低密度地块也没有则一座都不放)，朝向不限。**外围车站**：在`AREA_COMMERCE_MIDDLE/LOW`里为东、南、西、北各挑一块(随机)，要求：1) 地块中心到中央地块中心距离>=200；2) 方向判定是半个地图：连线(中央->地块)和该方向轴线夹角<=45度(x比中央大=东，y比中央大=南)；3) 选边：东/西站在这块地能放的边里取**沿路方向(世界坐标，按地块旋转换算)更贴近东西走向**的那条，南/北站取**更贴近南北走向**的(不再按和连线的夹角约束)；4) **连线和中央车站走向(中央车站沿路方向，世界坐标)的夹角<=45度**，走向是无向轴，取靠近这座站的那一端：中央走向西南-东北时，西站要落在它西南端前方、夹角<=45度，避免轨道从中央出来拐去外围站时出现钝角。某个方向没有满足的地块就不放。占位规则同机场的一半：沿路边长L<15或进深D<5拒绝；L<20沿路占满，否则取沿路末端一角、沿路15；D<=10占满，否则取7.5；沿路边长取的是车站朝向对应的那条边，不再按长边选。取角贴末端(机场贴起端)，放不下`RequestPlacement`自然失败被跳过。线路怎么连见`route_train.md`。
- **`TrainStationBuilding::Layout`**：`footprint`占满整个Building区域，`basements=0`+`layers=3`，层高1楼0.75、2楼0.75、3楼1.0；1楼大厅`preset_station_train_fg+`(楼梯通上去，`Stair_Line`资产)放4间`room_empty`、2楼`preset_station_train_fl+-`、3楼`preset_station_train_fu-`，三层各4台电梯。`TrainStation::Layout`的轨道/接口(`depthRatio` 0.45/0.55，z=1.5)因此落在楼体范围内。
- **`AirportBuilding::Layout`**：无地下室、1层，层高0.6，模板`preset_single_room_fg`+1间`AirportRoom`。`footprint`用`NearRoadFootprint(direction, 0.18, 0.35, 0.6)`把楼体压到靠路一侧，深处留给`AirStation::Layout`的跑道(`depthRatio` 0.65/0.85)，避免公共交通连线穿过楼体实体。

## 依赖关系

- 依赖：`Source/Dependence/map/building_mod.h`(`BuildingMod`基类)、`common/utility.h`(`GetRandom`)；用字符串引用`room_*`/`component_*`里各类型的id(Basic层文件间不互相`#include`)。
- 被谁依赖：`Source/Basic/basic.cpp`(`RegisterModBuildings`注册七个类型，依赖表声明所需的component/room/script)；`zone_*.cpp`(`Residence{High,Middle,Low}Zone::Layout()`内部建筑分别引用`building_residence_{high,middle,low}`)；`traffic/station_*`(火车站楼/机场楼的`stationMod`指向`station_train`/`station_air`，依赖表里是站点依赖楼，楼不依赖站点)。

## 待办/后续阶段

- `MOHAR_*`的footprint、层高、模板、缩放等数值是按实测调的默认值，还要按效果继续调。
- 二楼模板`fu2`目前和一楼`fg1`内容一样，等二楼房间范围对上模型再调。
