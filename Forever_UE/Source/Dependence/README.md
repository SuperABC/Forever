# Dependence 概览

## 职责

`Source/Dependence`是和UE完全无关的纯C++静态库，为每个domain的每个concept提供一套
`<Concept>Mod`(可扩展接口)+`<Concept>Factory`(创建/销毁/枚举)。`common/`放`error.h/.cpp`
(异常类)、`json.h/.cpp`(JSON解析器)这类不属于任何具体concept的共享支持代码。

## concept对照表

| Domain | Concept | Mod接口 | Factory(创建/销毁/枚举) | 加载器探测符号 |
|---|---|---|---|---|
| map | Terrain | `map/terrain_mod.h` → `TerrainMod` | `map/terrain_factory.h/.cpp` → `TerrainFactory` | `GetModTerrains` |
| map | Roadnet | `map/roadnet_mod.h` → `RoadnetMod` | `map/roadnet_factory.h/.cpp` → `RoadnetFactory` | `GetModRoadnets` |
| map | Zone | `map/zone_mod.h` → `ZoneMod` | `map/zone_factory.h/.cpp` → `ZoneFactory` | `GetModZones` |
| map | Building | `map/building_mod.h` → `BuildingMod` | `map/building_factory.h/.cpp` → `BuildingFactory` | `GetModBuildings` |
| map | Component | `map/component_mod.h` → `ComponentMod` | `map/component_factory.h/.cpp` → `ComponentFactory` | `GetModComponents` |
| map | Room | `map/room_mod.h` → `RoomMod` | `map/room_factory.h/.cpp` → `RoomFactory` | `GetModRooms` |
| player | Asset | `player/asset_mod.h` → `AssetMod` | `player/asset_factory.h/.cpp` → `AssetFactory` | `GetModAssets` |
| player | App | `player/app_mod.h` → `AppMod` | `player/app_factory.h/.cpp` → `AppFactory` | `GetModApps` |
| player | Puzzle | `player/puzzle_mod.h` → `PuzzleMod` | `player/puzzle_factory.h/.cpp` → `PuzzleFactory` | `GetModPuzzles` |
| populace | Name | `populace/name_mod.h` → `NameMod` | `populace/name_factory.h/.cpp` → `NameFactory` | `GetModNames` |
| populace | Scheduler | `populace/scheduler_mod.h` → `SchedulerMod` | `populace/scheduler_factory.h/.cpp` → `SchedulerFactory` | `GetModSchedulers` |
| society | Job | `society/job_mod.h` → `JobMod` | `society/job_factory.h/.cpp` → `JobFactory` | `GetModJobs` |
| society | Organization | `society/organization_mod.h` → `OrganizationMod` | `society/organization_factory.h/.cpp` → `OrganizationFactory` | `GetModOrganizations` |
| story | Script | `story/script_mod.h` → `ScriptMod` | `story/script_factory.h/.cpp` → `ScriptFactory` | `GetModScripts` |
| industry | Product | `industry/product_mod.h` → `ProductMod` | `industry/product_factory.h/.cpp` → `ProductFactory` | `GetModProducts` |
| industry | Storage | `industry/storage_mod.h` → `StorageMod` | `industry/storage_factory.h/.cpp` → `StorageFactory` | `GetModStorages` |
| industry | Manufacture | `industry/manufacture_mod.h` → `ManufactureMod` | `industry/manufacture_factory.h/.cpp` → `ManufactureFactory` | `GetModManufactures` |
| traffic | Route | `traffic/route_mod.h` → `RouteMod` | `traffic/route_factory.h/.cpp` → `RouteFactory` | `GetModRoutes` |
| traffic | Station | `traffic/station_mod.h` → `StationMod` | `traffic/station_factory.h/.cpp` → `StationFactory` | `GetModStations` |
| traffic | Vehicle | `traffic/vehicle_mod.h` → `VehicleMod` | `traffic/vehicle_factory.h/.cpp` → `VehicleFactory` | `GetModVehicles` |

各concept独立`.md`文档见各自domain子目录(如`map/terrain_mod.md`)；本文件只记录跨concept
共通的设计约定。

## 关键设计

- **每个`<Concept>Mod`只声明`GetType()`/`GetName()`两个纯虚接口**，业务接口因concept而异，
  分别在各自的`<concept>_mod.h`/对应`.md`里定义。参数不经过Mod基类的任何钩子方法——
  `Create<Concept>(id)`创建实例那一刻直接把参数字符串传给注册的creator函数，mod自己决定
  怎么用(传给具体子类的构造函数最常见)，见下面"Mod参数传递"一条。
- **`GetId()`不放进`<Concept>Mod`基类**，只是具体mod子类自己的静态方法约定(如
  `PengzhanBuilding::GetId()`)，不通过基类指针调用，也不是DLL导出符号本身。
- **`<Concept>Factory`和`<Concept>Mod`同放在Dependence，不在Core**——Mod DLL只链接
  `Dependence.lib`，若`Factory`的方法实现只编译进`Core.lib`，Mod就无法调用
  `factory->RegisterXxx(...)`这个非虚方法。
- **`Factory`的销毁必须走注册时mod提供的deleter**——`Destroy<Concept>`通过`liveInstances`
  (实例指针→注册id的映射)反查该实例是哪个mod注册的，再调用对应的deleter释放，不能
  Factory自己`delete`一个由mod DLL `new`出来的对象，这是跨模块内存安全的具体落地(见
  `CONVENTIONS.md`"跨模块new/delete安全"一节)。
- **`<Concept>Factory`所有公开方法都是`virtual`(含析构函数)，即使目前没有任何派生类**——
  非虚方法会在调用方(Mod DLL)里被静态绑定到调用方自己链接的那份`Dependence.lib`机器码，
  即使对象是宿主`new`出来的；这份被静态绑定的实现如果在内部做内存分配(往`unordered_map`
  插入节点等)，用的是调用方自己的CRT分配器，和对象真正的生命周期管理方(宿主)对不上，
  导致堆损坏崩溃。标`virtual`后调用走对象自带的vtable(宿主构造时装配，永远指向宿主自己
  编译的实现)，分配和释放因此总在同一侧。新增Factory方法时延续这个做法。
- **`BuildingFactory`/`ZoneFactory::RegisterBuilding`/`RegisterZone`除了`creator`/
  `deleter`，还额外注册几个static查询函数指针**(`BuildingFactory`：`RandomAcreageFunc`/
  `AcreageBoundFunc`(x2)/`PowerFunc`/`AssignFunc`；`ZoneFactory`只多`AssignFunc`)——
  `RandomAcreage`/`GetAcreageMin`/`GetAcreageMax`/`GetPower`/`Assign`这几个方法本身是
  无捕获的static普通函数，天然能隐式转换成裸函数指针，不需要实例，适用"调用哪个模块编译
  的代码就用哪个模块的内存管理"这条规则；`AssignFunc`
  (`void(*)(const std::vector<Lot*>&, PlacementEmitFunc, void*)`)内部通过
  `PlacementEmitFunc`(同样是裸函数指针)回调把结果交回调用方，回调函数体编译在调用方那
  一侧，不会出现"mod分配、宿主释放"的跨DLL问题。`Factory::RandomAcreage(id)`/
  `GetAcreageMin(id)`/`GetAcreageMax(id)`/`GetPower(id,area)`/`Assign(id,lots,emit,
  context)`是对应的转发方法，`id`未注册时分别返回`0.f`/直接不调用`emit`，详见
  `Source/Dependence/map/zone_mod.md`。
- **Mod参数传递**：给mod传参数走`Factory::SetModArgs`，`Create<Concept>(id)`创建实例那
  一刻直接把参数字符串传给注册的creator函数，不经过DLL导出函数的参数，也不经过创建后再
  调用的钩子方法。`config.json`里每个concept一个`"<concept>_mods"`数组，元素是`"id"`或
  `"id 参数..."`(命令行式写法)。流程：`ForeverModSubsystem`调用
  `Config::GetConceptMods("<concept>_mods")`解析出`(id, 参数)`列表，转成map后调用
  `factory.SetModArgs(...)`，存进`Factory::configuredArgs`(整个Factory生命周期内不再
  变化)；`CreateFunc`的函数指针类型是`<Concept>Mod*(*)(const std::string&)`，`RegisterX`
  只登记creator/deleter，不复制参数(`Entry`没有`args`字段，避免同一份参数存两份)。
  `Factory::Create<Concept>(id)`创建实例时按`id`查`configuredArgs`，把参数字符串传给
  `it->second.creator(args)`，mod自己的creator决定怎么用(传给具体子类构造函数最常见，
  示例见`Forever_Mod/Empty/`)。**两个被否掉的方案不要重新尝试**：按`dll_paths`根目录配
  一份共用参数经导出函数传给mod(和"每个mod id有自己的参数"的真实需求对不上)；创建实例
  之后再调用`instance->ApplyArgs(args)`钩子(构造函数已经跑完，`const`成员没法再赋值，
  参数到手已经太晚)。

## 依赖关系

- 依赖：无(纯C++，不依赖UE、不依赖Core内容)。
- 被谁依赖：`Source/Basic/<domain>/<concept>_basic.h`(继承`<Concept>Mod`提供默认实现)、
  `Source/Forever/Mod/ForeverModSubsystem.cpp`(`#include`各`<domain>/<concept>_factory.h`，
  为每个concept构造一个局部Factory、验证注册结果)、`Forever_Mod/`下的各示例Mod(继承
  `<Concept>Mod`、调用`<Concept>Factory::Register<Concept>`)。
- Mod可扩展点：任何Mod DLL，只要`#include`对应`<domain>/<concept>_mod.h`+
  `<domain>/<concept>_factory.h`、链接`Dependence.lib`，就能实现自己的`<Concept>Mod`
  子类并通过`RegisterMod<Concept>`导出函数注册进宿主的Factory实例。

## common/error.h、common/json.h 说明

`error.h/.cpp`(异常类`ExceptionBase`及派生类、`THROW_EXCEPTION`宏)和`json.h/.cpp`
(JsonCpp衍生的宽松JSON解析器，支持`//`/`/* */`注释)均为原样移植的支持代码，不逐函数写
文档。`Core/common/config.cpp`用`json.h`解析`config.json`；`json.cpp`的`AsString`/
`AsInt`等类型转换方法用`error.h`的`THROW_EXCEPTION(JsonFormatException, ...)`在类型不
匹配时抛异常。

## common/、map/、story/的跨domain共享原语

`common/`、`map/`、`story/`三个目录下，除了以上concept的Mod/Factory文件，还各自有几份
跨domain共享的原语，不感知任何具体domain数据、却被多个domain的Core层代码依赖：

- `common/utility.h/.cpp`：`ValueType`(脚本引擎值类型)、`Time`、`Counter`、`debugf`等，
  详见`common/utility.md`。
- `common/handle.h/.cpp`：`PostHandle`跨模块查询接口，详见`common/handle.md`。
- `map/general_geometry.h/.cpp`：`Node`/`Connection`/`Quad`/`Lot`等几何与导航图原语，详见
  `map/general_geometry.md`。
- `story/condition.h/.cpp`、`story/change.h/.cpp`、`story/event.h/.cpp`：通用脚本表达式
  引擎+"动作"/"事件"词汇表，详见各自同名`.md`。这三个文件不是某个具体concept本身
  (`Script`才是)，是`Script`会用到的底层机制。

`utility.h`把`debugf`的`LPCSTR`参数改成了`const char*`并将`<windows.h>`挪进`.cpp`，避免
头文件污染(其余均原样移植)。

## 待办/后续阶段

- `<Concept>Factory`目前只有最小注册表(`registries`+`liveInstances`两个map)，没有
  "探测阶段"和"正式生效阶段"两段式注册的隔离机制；如果以后发现多个mod并发注册确实需要
  这层隔离，再按需添加。
