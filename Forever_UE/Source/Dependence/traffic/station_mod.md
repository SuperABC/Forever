# station_mod.h / station_mod.cpp

## 职责

`StationMod`：一类站点(公交站/火车站/机场...)的"上下车接口在哪、朝哪个方向"声明，供Core
(`Core/traffic/station.h`的`Station`)读取并换算成世界坐标。只描述接口几何，不涉及线路怎么
连——那是`route_mod.h`(`RouteMod`)的职责，两者靠`stationType`这个通用类别词(不是mod名)
耦合，解耦"站点外观"和"线路连接逻辑"。

## 关键设计

- **两种挂载方式二选一**：
  1. 挂在建筑上——`BuildingMod::stationMod`单向声明"这栋楼配哪个站点"，`Traffic::InitStations()`
     据此创建`Station`、调一次`Layout(direction, sizeX, sizeY)`填好`interfaces`。火车站/机场
     用这种方式。
  2. 直接贴着道路摆，不占用任何Lot面积——实现`AssignRoads()`声明想贴哪些路、哪一侧，
     `Traffic::InitRoadsideStations()`据此直接在道路旁创建`Station`，不经过`Layout()`/
     `interfaces`这套建筑相对坐标系。公交站用这种方式。
  两者共用同一个`StationMod`基类/同一套`config.json`"station_mods"注册机制，只是Core侧走的
  创建路径不同(见`station.md`)。`Layout()`不实现默认空实现；`AssignRoads()`是static方法
  (见下)，每个具体子类都必须实现，不需要就给个空实现。
- **`StationInterfaceSpec.position`用建筑占地矩形(`Building`自身的`Quad`，不是楼体footprint)
  的局部坐标声明，原点在占地矩形左下角、未旋转**——和`NavigationNodeTemplate`同一套
  ratio+offset约定(不是`ParkingSpot`那种"相对中心"的约定)，因为轨道/跑道接口经常落在楼体
  外、靠近占地矩形边缘的空地上，用左下角做原点换算更直观。`Building::LocalToWorld`接的是
  "楼体局部坐标"而不是这个"占地矩形局部坐标"，两者之间的换算(抵消`bodyOffset`)在
  `Core/traffic/station.cpp`里做，见`station.md`"接口坐标换算"一节——这个换算的副作用是
  接口的世界坐标**和楼体footprint的偏移(`BuildingFootprintSpec`)完全无关**，楼体想往路边
  挪多远都不会牵动接口位置，这正是这次"让楼体贴路、把深处空间让给跑道/站台"能够不改
  `StationInterfaceSpec`数值就直接生效的原因。
- **`headingDegrees`同时代表到达和出发方向**——这个项目目前没有"进站朝向≠出站朝向"的站点
  类型，`Core/traffic/station.cpp`换算时`arriveDir`/`departDir`恒相等。真正需要两个不同
  朝向的"跑道/轨道两端"场景，靠同一个站点声明**两对**接口(比如`TrainStation`/`AirStation`
  各声明4个接口，见`Basic/traffic/station_basic.md`)解决，不是让单个接口自己有两个朝向。
- **`AssignRoads()`是static方法，和`BuildingMod::Assign`/`ZoneMod`那组"按类型固定、不需要
  任何实例"的函数指针约定完全同一个模式**（2026-10-04从虚方法改过来——这个方法只是跑一次性
  的全图探测，不读/写任何实例状态，没有理由为了调它创建一个`StationMod`实例再销毁）：基类
  不声明，每个具体子类必须实现同名static方法，通过`StationFactory::RegisterStation`的额外
  函数指针参数注册(`station_factory.h`)。`Traffic::InitRoadsideStations()`直接调
  `StationFactory::AssignRoads(id, ...)`转发到注册的函数指针，不创建/销毁任何实例。
  `RoadStationRequest.rightSide`的物理含义("沿Road Start->End方向的右手边")依赖
  `Map::ComputeLaneAnchorPosition`的`perp0`公式，2026-10-04修正过(原来的公式在这个项目
  `+Y=南`的世界坐标下其实是左手边)，见那边声明处的完整排查记录。

## 依赖关系

- 依赖：`Dependence/map/geometry.h`(`PointParams`/`Road`)。
- 被谁依赖：`Core/traffic/traffic.cpp`(`Traffic::InitRoadsideStations()`调
  `StationFactory::AssignRoads(id, ...)`)；`Core/traffic/station.h`(`Station`三个构造函数
  分别对应"挂建筑"/"边缘站点"/"贴路站点"三种场景)；`Dependence/map/building_mod.h`
  (`BuildingMod::stationMod`字符串声明"这栋楼配哪个站点")；`Basic/traffic/station_basic.h`
  的`BusStation`/`TrainStation`/`AirStation`三个具体实现扩展这个基类。

## 待办/后续阶段

- 目前只有"挂建筑"和"贴路"两种挂载方式，没有第三种(比如"贴着zone边界摆")——真需要时再加
  一个新的虚函数，不需要改这次的两个既有虚函数。
