# building_industry_high.h / building_industry_high.cpp

## 职责

`IndustryHighBuilding`(`building_industry_high`)：工厂(高密度)：只在工厂高密度分区(AREA_INDUSTRY_HIGH)上有权重。

## 关键设计

- **"一个本体独占一个mod实例"模型**：`RandomAcreage`/`GetAcreageMin`/`GetAcreageMax`/`GetPower`/`Assign`全是不需要实例的static方法，通过`BuildingFactory::RegisterBuilding`注册；楼层/房间等实例级数据都在非static的`Layout()`里设。
- **`direction`兜底**：`direction`对显式占位是真实方向，权重CDF/`FillRemainder`落地时是`-1`。住宅/商店/工厂/Mohar的`Layout()`先从`boundaryRoads`里随机挑一个有真实边界路的方向回写(行人/车辆导航的"outside"端点要靠它找到该连的路，见`building_mod.h`的`Layout()`注释)；之后按`quad`尺寸/长宽比算一个偏好方向，**只有偏好方向在`boundaryRoads`里确实有非空条目时才采用**，否则保留兜底方向。
- **`GetPower(AREA_TYPE)`限定分区类型**：住宅/商店/工厂分别只在住宅/商业/工业三档分区返回`1.f`，否则会在别的分区和其它类型一起参与`FillRemainder`的权重CDF竞争而混入。没有对应建筑类型的分区(目前办公区，除中心lot被火车站/机场/园区占用外)保持空地，属预期。分区由路网生成，见`roadnet_jing.md`。
- **火车站/机场/公园/Mohar的`Layout`依赖`RoadnetMod`之外的约定**：`stationMod`字符串指向`traffic/station_*`里的StationMod。

## 三档密度与办公楼

- `GetPower`只在对应类型和密度的分区返回`1.f`：`AREA_{RESIDENCE,COMMERCE,INDUSTRY,OFFICE}_{HIGH,MIDDLE,LOW}`。
- **工厂(高密度 `IndustryHighBuilding`)**：权重分区只有 `AREA_INDUSTRY_HIGH`(已不在 `AREA_TEST`)。面积 `RandomAcreage()=10000+10000*rand`(10000~20000线性随机)，min 10000、max 20000。`Layout()` 只用 `preset_common_single_*`：地下1层 `pg+`(1个 `room_parking`)、地上1层 `fg-`(1个工坊房间，`room_farm`/`room_ranch`/`room_food_factory` 等概率随机)，层高统一0.6，footprint 占地块60%居中，登记 `clay`→Clay 标签材质，`gapMaterial`=Clay。中/低密度工厂仍是旧布局(旧模板名)。
- 下面`ResidenceBuilding`/`CommerceBuilding`/`IndustryBuilding`几节描述的是对应类型**当前（三档共同）的逻辑**，指的是三个档次各自那份拷贝。

## IndustryBuilding

- `GetPower`：工业三档分区。`RandomAcreage()=2000*(1+rand)^2`，`GetAcreageMin()=4000`，`GetAcreageMax()=16000`——公式基数和Min不一致(随机值可能小于Min)是既有数值，不"修正"。
- `Layout()`：`footprint`固定0.6x0.6，`layers`固定1，层高统一0.6。偏好方向按`quad`长宽比选`FACE_NORTH+rand(2)`或`FACE_WEST+rand(2)`，对应边长`>4`时才`basements=1`(是否有地下室由`quad`尺寸判定，和地上层数无关)。有地下室：`preset_single_room_pg+`(`ParkingRoom`，和`CommerceBuilding`共用)→1楼`preset_single_room_fg-`；否则只有1楼`preset_single_room_fg`。1楼放1个工坊房间，`FarmRoom`/`RanchRoom`/`FoodFactoryRoom`等概率随机选一。

## 依赖关系

- 依赖：`Source/Dependence/map/building_mod.h`(`BuildingMod`基类)、`common/utility.h`(`GetRandom`)；用字符串引用`room_*`/`component_*`里各类型的id(Basic层文件间不互相`#include`)。
- 被谁依赖：`Source/Basic/basic.cpp`(`RegisterModBuildings`注册七个类型，依赖表声明所需的component/room/script)；`zone_*.cpp`(`Residence{High,Middle,Low}Zone::Layout()`内部建筑分别引用`building_residence_{high,middle,low}`)；`traffic/station_*`(火车站楼/机场楼的`stationMod`指向`station_train`/`station_air`，依赖表里是站点依赖楼，楼不依赖站点)。

## 待办/后续阶段

- `MOHAR_*`的footprint、层高、模板、缩放等数值是按实测调的默认值，还要按效果继续调。
- 二楼模板`fu2`目前和一楼`fg1`内容一样，等二楼房间范围对上模型再调。
