# building_commerce_high.h / building_commerce_high.cpp

## 职责

`CommerceHighBuilding`(`building_commerce_high`)：商店(高密度)：只在商店高密度分区(AREA_COMMERCE_HIGH)上有权重。

## 关键设计

- **"一个本体独占一个mod实例"模型**：`RandomAcreage`/`GetAcreageMin`/`GetAcreageMax`/`GetPower`/`Assign`全是不需要实例的static方法，通过`BuildingFactory::RegisterBuilding`注册；楼层/房间等实例级数据都在非static的`Layout()`里设。
- **`direction`兜底**：`direction`对显式占位是真实方向，权重CDF/`FillRemainder`落地时是`-1`。住宅/商店/工厂/Mohar的`Layout()`先从`boundaryRoads`里随机挑一个有真实边界路的方向回写(行人/车辆导航的"outside"端点要靠它找到该连的路，见`building_mod.h`的`Layout()`注释)；之后按`quad`尺寸/长宽比算一个偏好方向，**只有偏好方向在`boundaryRoads`里确实有非空条目时才采用**，否则保留兜底方向。
- **`GetPower(AREA_TYPE)`限定分区类型**：住宅/商店/工厂分别只在住宅/商业/工业三档分区返回`1.f`，否则会在别的分区和其它类型一起参与`FillRemainder`的权重CDF竞争而混入。没有对应建筑类型的分区(目前办公区，除中心lot被火车站/机场/园区占用外)保持空地，属预期。分区由路网生成，见`roadnet_jing.md`。
- **火车站/机场/公园/Mohar的`Layout`依赖`RoadnetMod`之外的约定**：`stationMod`字符串指向`traffic/station_*`里的StationMod。

## 三档密度与办公楼

- `GetPower`只在对应类型和密度的分区返回`1.f`：`AREA_{RESIDENCE,COMMERCE,INDUSTRY,OFFICE}_{HIGH,MIDDLE,LOW}`。
- **商店(高密度 `CommerceHighBuilding`)**：权重分区只有 `AREA_COMMERCE_HIGH`(已不在 `AREA_TEST`)。面积 `RandomAcreage()=8000+8000*rand`(8000~16000线性随机)，min 8000、max 16000。`Layout()` 只用 `preset_common_tshape_*`：无地下室、地上 3~5 层随机(1楼 `fg+`、其余 `fu+-`)，每层一个 component(`componentId`=层号)，含 1 个 single(`room_empty` 占位)+6 个 row(row 0~3：1楼 `room_restaurant`、2楼及以上 `room_shop`；row 4~5：`room_warehouse`；row 面积120)，2 台 `elevator_basic` 贯穿全楼；footprint 占满地块、四周各留固定 0.2 的空隙，朝向偏好长边正对的路，登记 `clay`→Clay、`concrete`→Concrete，`gapMaterial`=Pavement。中/低密度商店仍是旧布局。
- 下面`ResidenceBuilding`/`CommerceBuilding`/`IndustryBuilding`几节描述的是对应类型**当前（三档共同）的逻辑**，指的是三个档次各自那份拷贝。

## CommerceBuilding

- `GetPower`：由三档子类各自决定。`RandomAcreage()=4000*(1+rand)^2`，`GetAcreageMin()=4000`，`GetAcreageMax()=16000`。
- `Layout()`：`footprint`居中占0.6(某边`<6`时收到`(size-2.4)/size`)，`layers`固定2，每层`FloorAssetSpec.wallMaterial`为`White`，层高统一0.4。复用`preset_lobby_linear_*`/`preset_circle_double_*`模板：
  - `quad.GetAcreage() < 6000`：无地下室。1楼`preset_lobby_linear_fg+`(`ShopRoom`+`WarehouseRoom`过道)→2楼`preset_lobby_linear_fu-`(纯`WarehouseRoom`)。
  - 否则`basements=1`，地下室随机选`preset_circle_double_pg+`(`ParkingRoom`)或`preset_circle_double_bg+`(5间`WarehouseRoom`)；1楼`preset_circle_double_fg+-`(2间`ShopRoom`+4间`WarehouseRoom`)，2楼`preset_circle_double_fu+-`(5间`WarehouseRoom`)。
- 大门有名字"商店大门"，但脚本挂在`ShopRoom`的房间门上，原因见`room_*.md`；`doorSpecs["entrance"]`上也设了同名脚本规格。

## 依赖关系

- 依赖：`Source/Dependence/map/building_mod.h`(`BuildingMod`基类)、`common/utility.h`(`GetRandom`)；用字符串引用`room_*`/`component_*`里各类型的id(Basic层文件间不互相`#include`)。
- 被谁依赖：`Source/Basic/basic.cpp`(`RegisterModBuildings`注册七个类型，依赖表声明所需的component/room/script)；`zone_*.cpp`(`Residence{High,Middle,Low}Zone::Layout()`内部建筑分别引用`building_residence_{high,middle,low}`)；`traffic/station_*`(火车站楼/机场楼的`stationMod`指向`station_train`/`station_air`，依赖表里是站点依赖楼，楼不依赖站点)。

## 待办/后续阶段

- `MOHAR_*`的footprint、层高、模板、缩放等数值是按实测调的默认值，还要按效果继续调。
- 二楼模板`fu2`目前和一楼`fg1`内容一样，等二楼房间范围对上模型再调。
