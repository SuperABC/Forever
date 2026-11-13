# building_residence_high.h / building_residence_high.cpp

## 职责

`ResidenceHighBuilding`(`building_residence_high`)：住宅(高密度)：只在住宅高密度分区(AREA_RESIDENCE_HIGH)上有权重。

## 关键设计

- **"一个本体独占一个mod实例"模型**：`RandomAcreage`/`GetAcreageMin`/`GetAcreageMax`/`GetPower`/`Assign`全是不需要实例的static方法，通过`BuildingFactory::RegisterBuilding`注册；楼层/房间等实例级数据都在非static的`Layout()`里设。
- **`direction`兜底**：`direction`对显式占位是真实方向，权重CDF/`FillRemainder`落地时是`-1`。住宅/商店/工厂/Mohar的`Layout()`先从`boundaryRoads`里随机挑一个有真实边界路的方向回写(行人/车辆导航的"outside"端点要靠它找到该连的路，见`building_mod.h`的`Layout()`注释)；之后按`quad`尺寸/长宽比算一个偏好方向，**只有偏好方向在`boundaryRoads`里确实有非空条目时才采用**，否则保留兜底方向。
- **`GetPower(AREA_TYPE)`限定分区类型**：住宅/商店/工厂分别只在住宅/商业/工业三档分区返回`1.f`，否则会在别的分区和其它类型一起参与`FillRemainder`的权重CDF竞争而混入。没有对应建筑类型的分区(目前办公区，除中心lot被火车站/机场/园区占用外)保持空地，属预期。分区由路网生成，见`roadnet_jing.md`。
- **火车站/机场/公园/Mohar的`Layout`依赖`RoadnetMod`之外的约定**：`stationMod`字符串指向`traffic/station_*`里的StationMod。

## 三档密度与办公楼

- **住宅/商店/工厂/办公楼各拆成高/中/低三个注册的mod**，共12个，类名`ResidenceHighBuilding`等：`building_{residence,shop,factory,office}_{high,middle,low}`。**每个类完全独立实现，没有共用基类**：各自有自己的`Layout()`、`Assign`、`RandomAcreage`/`GetAcreageMin`/`GetAcreageMax`、`GetPower`和命名计数器；目前同一类型的三档以及办公楼与住宅，逻辑都是同一份的拷贝，以后各自改，互不影响。
- `GetPower`只在对应类型和密度的分区返回`1.f`：`AREA_{RESIDENCE,COMMERCE,INDUSTRY,OFFICE}_{HIGH,MIDDLE,LOW}`。
- 下面`ResidenceBuilding`/`CommerceBuilding`/`IndustryBuilding`几节描述的是对应类型**当前（三档共同）的逻辑**，指的是三个档次各自那份拷贝。

## ResidenceBuilding

- `GetPower`：由三档子类各自决定(见上)。`RandomAcreage()=2000*(1+rand*2)^2`，`GetAcreageMin()=2000`，`GetAcreageMax()=18000`。`Assign()`空实现。
- 住宅高密度的**`Assign`**：在井字中心lot(`AREA_TEST`)上依次试4个方向，用`FindAdaptivePlacement`找12x8(沿路x纵深)的自由子块，贴边起点角占位(余量全给`marginEnd`)，保证开局有住宅和可控市民；与火车站/机场共存(它们先占的空间自动避开)。
- 住宅高密度(`ResidenceHighBuilding`)的面积：`RandomAcreage()=4000+12000*r²`(r 为 0~1 均匀随机，4000~16000 按平方随机、偏向小面积)，`GetAcreageMin()=4000`，`GetAcreageMax()=16000`。
- `ResidenceHighBuilding::Layout()`：四种布局**按 quad 面积(`quad.GetAcreage()`)挑选**：4000~8000 在 side/nshape 里等概率，8000~12000 四种等概率，12000~16000 在 twin/wing 里等概率；`Layout()` 先做四种共用的部分(entity_event、direction 兜底、偏好方向=长边正对的路(仅对应边界路存在时采用)、登记 roof/brick/concrete/clay/pavement/specular/stone 标签材质、`gapMaterial`=Pavement)，再按面积定出的布局调私有的 `LayoutWing/Side/Twin/Nshape`。层数/进深/模板各自为：**wing**＝地下1层 `pg+`(停车库)、地上12~17层(顶上一层只有屋面 `fr`，次顶 `fr-`)，进深≤4，2 台电梯到 `fr-`；**side**＝无地下室、地上5~9层(1楼 `fg+`、顶层 `fu-`、中间 `fu+-`)，进深≤3，每层8个 single、4 台电梯，直跑楼梯 `Stair_Line`；**twin**＝地下1层 `pg+`、地上12~17层(1楼 `fg+-`、2楼 `fl+-`、3楼起奇数楼 `fo+-`/偶数楼 `fe+-`)，进深≤4，row 面积200，2 台电梯；**nshape**＝地下1层 `bg+`、地上5~9层(1楼 `fg+-`、其余 `fu+-`)，row 面积300，2 台电梯，`footprint` 朝向那一边贴 quad 边占满、其余三边各留 0.2 空隙。wing/side/twin 的 `footprint` 统一为朝向轴进深封顶(地块没这么深就占满)、居中，开间两侧各留 0.2。b=地下正常布局，p=地下停车场。
- 大门`doorSpecs["entrance"]`：随机左右滑动单开门(`Cube`资产占位)，只有corridor门洞带`entrance`标签才用到，见`Core/map/door.h`的`ResolveDoorSpec`。
- 挂示例脚本`entity_event`。

## 依赖关系

- 依赖：`Source/Dependence/map/building_mod.h`(`BuildingMod`基类)、`common/utility.h`(`GetRandom`)；用字符串引用`room_*`/`component_*`里各类型的id(Basic层文件间不互相`#include`)。
- 被谁依赖：`Source/Basic/basic.cpp`(`RegisterModBuildings`注册七个类型，依赖表声明所需的component/room/script)；`zone_*.cpp`(`Residence{High,Middle,Low}Zone::Layout()`内部建筑分别引用`building_residence_{high,middle,low}`)；`traffic/station_*`(火车站楼/机场楼的`stationMod`指向`station_train`/`station_air`，依赖表里是站点依赖楼，楼不依赖站点)。

## 待办/后续阶段

- `MOHAR_*`的footprint、层高、模板、缩放等数值是按实测调的默认值，还要按效果继续调。
- 二楼模板`fu2`目前和一楼`fg1`内容一样，等二楼房间范围对上模型再调。
