# building_airport.h / building_airport.cpp

## 职责

`AirportBuilding`(`building_airport`)：机场，可有多座，承载StationMod"station_air"。

## 关键设计

- **"一个本体独占一个mod实例"模型**：`RandomAcreage`/`GetAcreageMin`/`GetAcreageMax`/`GetPower`/`Assign`全是不需要实例的static方法，通过`BuildingFactory::RegisterBuilding`注册；楼层/房间等实例级数据都在非static的`Layout()`里设。
- **`direction`兜底**：`direction`对显式占位是真实方向，权重CDF/`FillRemainder`落地时是`-1`。住宅/商店/工厂/Mohar的`Layout()`先从`boundaryRoads`里随机挑一个有真实边界路的方向回写(行人/车辆导航的"outside"端点要靠它找到该连的路，见`building_mod.h`的`Layout()`注释)；之后按`quad`尺寸/长宽比算一个偏好方向，**只有偏好方向在`boundaryRoads`里确实有非空条目时才采用**，否则保留兜底方向。
- **`GetPower(AREA_TYPE)`限定分区类型**：住宅/商店/工厂分别只在住宅/商业/工业三档分区返回`1.f`，否则会在别的分区和其它类型一起参与`FillRemainder`的权重CDF竞争而混入。没有对应建筑类型的分区(目前办公区，除中心lot被火车站/机场/园区占用外)保持空地，属预期。分区由路网生成，见`roadnet_jing.md`。
- **火车站/机场/公园/Mohar的`Layout`依赖`RoadnetMod`之外的约定**：`stationMod`字符串指向`traffic/station_*`里的StationMod。

## 三档密度与办公楼

- `GetPower`只在对应类型和密度的分区返回`1.f`：`AREA_{RESIDENCE,COMMERCE,INDUSTRY,OFFICE}_{HIGH,MIDDLE,LOW}`。

## TrainStationBuilding / AirportBuilding

- 火车站全图唯一一个，机场可有多座；`GetPower`恒0不参与权重CDF，`RandomAcreage`/`Min`/`Max`固定2000，靠`Assign`显式占位。分别承载`StationMod`的`station_train`/`station_air`(`stationMod`字段)。
- **`TrainStationBuilding::Assign`**：只看井字中心lot(`AREA_TEST`)，依次尝试4个方向用`Lot::FindAdaptivePlacement`按当前实际剩余空间自适应找位置，不挑具体方向，和园区共存。要沿路`7`x纵深`4`(地图单位)，余量全挪到`marginEnd`贴边起点角。`request.lot`是命中的自由子块而不是`lot`本身，`adaptiveParent`仅在二者不同时设，供Core把子块残余空间搬回，见`FindAdaptivePlacement`声明。
- **`AirportBuilding::Assign`**(多座，单位均为地图单位=10m)：
  1. `AREA_TEST`中心lot固定放一座(兼容井字路网)：和火车站一样用`FindAdaptivePlacement`，优先沿路`12`x纵深`8`，放不下退回`7`x`4`，余量全挪到`marginStart`贴另一端的角，这样两者同在一条边上也占两个对角。
  2. 遍历`AREA_COMMERCE_LOW`顶层lot(即一个低密度商业区)：取最长边L为沿路方向(sizeX>=sizeY则临北/南侧，否则临西/东侧，取有边界道路的一侧，没有则跳过)，另一边为进深D。L<20跳过；D<10跳过；与已放机场(含`AREA_TEST`那座)中心距离<=800则跳过；D<=20占满D，D>20只占15；L<40沿路占满，L>=40取起始角沿路30(`marginStart=0`、`marginEnd=L-30`)。边界取舍(L=40走取角、D=20占满)是实现时定的。
  3. `request.lot`直接是该商业区，不用`adaptiveParent`；`RequestPlacement`失败Core直接跳过。
  4. 机场数量取决于地图大小：地图边长不足800时只有`AREA_TEST`那一座(或第一个合格商业区)。
- **`TrainStationBuilding::Layout`**：`footprint`占满整个Building区域，`basements=1`+`layers=1`，层高0.6；地下1层站台层`preset_train_station_bg+`(楼梯通上来，`Stair_Line`资产)、地面1层大厅`preset_train_station_fg-`放2间`TrainStationRoom`，两层各2台电梯。`TrainStation::Layout`的轨道/接口(`depthRatio` 0.55/0.65)因此落在楼体范围内。
- **`AirportBuilding::Layout`**：无地下室、1层，层高0.6，模板`preset_common_single_fg`+1间`AirportRoom`。`footprint`用`NearRoadFootprint(direction, 0.18, 0.35, 0.6)`把楼体压到靠路一侧，深处留给`AirStation::Layout`的跑道(`depthRatio` 0.65/0.85)，避免公共交通连线穿过楼体实体。

## 依赖关系

- 依赖：`Source/Dependence/map/building_mod.h`(`BuildingMod`基类)、`common/utility.h`(`GetRandom`)；用字符串引用`room_*`/`component_*`里各类型的id(Basic层文件间不互相`#include`)。
- 被谁依赖：`Source/Basic/basic.cpp`(`RegisterModBuildings`注册七个类型，依赖表声明所需的component/room/script)；`zone_*.cpp`(`Residence{High,Middle,Low}Zone::Layout()`内部建筑分别引用`building_residence_{high,middle,low}`)；`traffic/station_*`(火车站楼/机场楼的`stationMod`指向`station_train`/`station_air`，依赖表里是站点依赖楼，楼不依赖站点)。

## 待办/后续阶段

- `MOHAR_*`的footprint、层高、模板、缩放等数值是按实测调的默认值，还要按效果继续调。
- 二楼模板`fu2`目前和一楼`fg1`内容一样，等二楼房间范围对上模型再调。
