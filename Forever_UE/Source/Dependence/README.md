# Dependence 阶段3骨架总览

## 职责

阶段3(`REFACTOR_PLAN.md`)把旧工程"每个domain按concept各一套`<Concept>Mod`+`<Concept>Factory`"的
Mod可扩展骨架,一次性铺到全部8个domain、21个concept。这份文档统一说明这21×2份近乎相同的
空实现文件,不逐份配`.md`——按`Forever_UE/Source/Forever/Framework/ForeverFrameworkComponent.md`
已经立下的先例:一批没有独特逻辑的空实现共用一份文档,避免42份重复样板文字。

## 21个concept对照表

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
| society | Calendar | `society/calendar_mod.h` → `CalendarMod` | `society/calendar_factory.h/.cpp` → `CalendarFactory` | `GetModCalendars` |
| society | Organization | `society/organization_mod.h` → `OrganizationMod` | `society/organization_factory.h/.cpp` → `OrganizationFactory` | `GetModOrganizations` |
| story | Script | `story/script_mod.h` → `ScriptMod` | `story/script_factory.h/.cpp` → `ScriptFactory` | `GetModScripts` |
| industry | Product | `industry/product_mod.h` → `ProductMod` | `industry/product_factory.h/.cpp` → `ProductFactory` | `GetModProducts` |
| industry | Storage | `industry/storage_mod.h` → `StorageMod` | `industry/storage_factory.h/.cpp` → `StorageFactory` | `GetModStorages` |
| industry | Manufacture | `industry/manufacture_mod.h` → `ManufactureMod` | `industry/manufacture_factory.h/.cpp` → `ManufactureFactory` | `GetModManufactures` |
| traffic | Route | `traffic/route_mod.h` → `RouteMod` | `traffic/route_factory.h/.cpp` → `RouteFactory` | `GetModRoutes` |
| traffic | Station | `traffic/station_mod.h` → `StationMod` | `traffic/station_factory.h/.cpp` → `StationFactory` | `GetModStations` |
| traffic | Vehicle | `traffic/vehicle_mod.h` → `VehicleMod` | `traffic/vehicle_factory.h/.cpp` → `VehicleFactory` | `GetModVehicles` |

`common/`不属于以上任何concept,只放`error.h/.cpp`(异常类)、`json.h/.cpp`(JSON解析器,均为
原样移植的支持代码,见下方说明)。

## 关键设计

- **每个`<Concept>Mod`只声明`GetType()`/`GetName()`两个纯虚接口,外加一个非纯虚的
  `ApplyArgs(const std::string&)`(默认空实现)**——业务接口(如`BuildingMod`的
  `RandomAcreage`/`LayoutBuilding`等)要等阶段4迁移对应系统、读到旧工程实际代码后才补,提前
  设计容易和真实需求对不上;`ApplyArgs`是阶段3新增的参数传递钩子,见下面单独一条说明。
- **`GetId()`不放进`<Concept>Mod`基类**——旧代码`building_mod.h`里有个从未实现的
  `static const char* GetId();`声明(static不能是虚函数,写在基类没有意义,是旧代码的死代码)。
  新工程里`GetId()`只是具体mod子类自己的静态方法约定(如`PengzhanBuilding::GetId()`),不通过
  基类指针调用,也不是DLL导出符号本身。
- **`<Concept>Factory`和`<Concept>Mod`同放在Dependence,不在Core**——这是实现期修正,原因见
  `REFACTOR_PLAN.md`同目录的阶段3计划文档"实现期修正"一节:Mod DLL只链接`Dependence.lib`,
  若`Factory`的方法实现只编译进`Core.lib`,Mod就无法调用`factory->RegisterXxx(...)`这个非虚
  方法。把Factory放回Dependence,让Mod和主程序各自静态链接一份`Dependence.lib`即可解决,这也是
  旧工程`building_mod.h`把两者放在同一文件的真正原因。
- **`Factory`的销毁必须走注册时mod提供的deleter**——`Destroy<Concept>`通过`liveInstances`
  (实例指针→注册id的映射)反查该实例是哪个mod注册的,再调用对应的deleter释放,不能
  Factory自己`delete`一个由mod DLL `new`出来的对象,这是跨模块内存安全的具体落地
  (`REFACTOR_PLAN.md`的"跨模块new/delete安全"约定)。
- **`<Concept>Factory`所有公开方法都是`virtual`(含析构函数),即使目前没有任何派生类**——
  这是运行时验证阶段3链路时实际踩到的坑:`RegisterX`等方法是非虚函数时,mod
  DLL(如`Wxdj.dll`)里调用`factory->RegisterScript(...)`,编译器会静态绑定到**mod自己
  静态链接的那份`Dependence.lib`副本**的`RegisterScript`机器码,而不是宿主(UE Editor进程)
  的副本——即使`factory`指针指向的对象是宿主`new`出来的。这个mod侧的`RegisterScript`
  在内部往`registries`(`unordered_map`)插入新节点时,分配走的是mod DLL自己链接的CRT
  分配器;而宿主之后`delete factory`时,`~Factory()`析构`registries`用的是UE覆盖过的全局
  分配器——两边分配器不一致,直接导致堆损坏崩溃(实测复现:`ScriptFactory`析构时崩溃在
  `std::list::~list()`,即`unordered_map`内部节点链表的析构)。把这些方法标记`virtual`后,
  跨DLL调用会走对象自带的vtable指针,而vtable是宿主构造对象时装配的,永远指向宿主自己编译
  的那份实现——分配和释放因此总是发生在同一侧,问题消失。这是`REFACTOR_PLAN.md`"跨模块
  new/delete安全"约定在Factory这一层的具体体现,比"Mod实例创建/销毁走deleter"这条更隐蔽,
  阶段4新增Factory方法时也要延续这个做法(公开方法一律`virtual`)。
- **给mod传参数走`Factory::SetModArgs`+`Mod::ApplyArgs`,不经过DLL导出函数的参数**——这是
  按用户明确要求、仿照旧工程`config.json`格式实现的:`config.json`里每个concept一个
  `"<concept>_mods"`数组,元素是`"id"`或`"id 参数..."`(命令行式写法,和旧工程
  `"test ---name value"`一致)。流程是:`ForeverModSubsystem`调用
  `Config::GetConceptMods("<concept>_mods")`解析出`(id, 参数)`列表,转成map后调用
  `factory.SetModArgs(...)`,原样存进`Factory::configuredArgs`(整个Factory生命周期内不
  再变化);之后不管mod什么时候调用`factory->Register<Concept>(id, creator, deleter)`,
  `RegisterX`都只登记creator/deleter,**不**把参数复制进`Entry`——`Entry`没有`args`字段,
  避免同一份参数在`configuredArgs`和`registries`里存两份。真正用到参数是在
  `Factory::Create<Concept>(id)`创建出实例**之后**,直接按`id`查`configuredArgs`、调用
  `instance->ApplyArgs(...)`。这几步都发生在Factory内部(宿主编译的代码,前提是
  `RegisterX`/`CreateX`都是`virtual`,见上一条),mod自己完全不需要关心参数从哪来、什么
  时候被谁调用,只需要重写`ApplyArgs`接收即可,示例见`Forever_Mod/Empty/`。
  **这条机制曾经有一版是按`dll_paths`根目录配一份共用参数、经`RegisterMod<Concept>(factory,
  args)`导出函数的参数传给mod**——后来发现这和"每个mod id有自己的参数"这个真实需求对不上
  (`ModLoader`按dll路径工作,不知道一次调用会注册哪些id),已废弃,不要照着抄。

## 依赖关系

- 依赖:无(纯C++,不依赖UE、不依赖Core内容)。
- 被谁依赖:`Source/Basic/<domain>/<concept>_basic.h`(继承`<Concept>Mod`提供占位默认实现)、
  `Source/Forever/Mod/ForeverModSubsystem.cpp`(`#include`全部21个`<domain>/<concept>_factory.h`,
  为每个concept构造一个局部Factory、验证注册结果)、`Forever_Mod/`下的各示例Mod(继承
  `<Concept>Mod`、调用`<Concept>Factory::Register<Concept>`)。
- Mod可扩展点:任何未来的Mod DLL,只要`#include`对应`<domain>/<concept>_mod.h`+
  `<domain>/<concept>_factory.h`、链接`Dependence.lib`,就能实现自己的`<Concept>Mod`子类并
  通过`RegisterMod<Concept>`导出函数注册进宿主的Factory实例。

## common/error.h、common/json.h 说明

`error.h/.cpp`(异常类`ExceptionBase`及派生类、`THROW_EXCEPTION`宏)和`json.h/.cpp`
(JsonCpp衍生的宽松JSON解析器,支持`//`/`/* */`注释)均**原样移植自旧工程**
`E:\Projects\Forever_UE\Source\Dependence\common\`,未做修改,不逐函数写文档。
`Core/common/config.cpp`用`json.h`解析`config.json`;`json.cpp`的`AsString`/`AsInt`等
类型转换方法用`error.h`的`THROW_EXCEPTION(JsonFormatException, ...)`在类型不匹配时抛异常。

## 阶段4-0共享基础设施(不属于21个concept)

`common/`、`map/`、`story/`三个目录下,除了以上21个concept的Mod/Factory文件,还各自多了几份
"跨domain共享的原语",在`PHASE4_PLAN.md`里称为"阶段4-0",比Map域还早迁移,因为它们不感知任何
具体domain数据、却被多个domain的Core层代码依赖:

- `common/utility.h/.cpp`:`ValueType`(脚本引擎值类型)、`Time`、`Counter`、`debugf`等,详见
  `common/utility.md`。
- `common/handle.h/.cpp`:`PostHandle`跨模块查询接口,详见`common/handle.md`。
- `map/geometry.h/.cpp`:`Node`/`Connection`/`Quad`/`Lot`等几何与导航图原语,详见
  `map/geometry.md`。
- `story/condition.h/.cpp`、`story/change.h/.cpp`、`story/event.h/.cpp`:通用脚本表达式引擎
  +"动作"/"事件"词汇表,详见各自同名`.md`。这三个文件不是`story`域21个concept之一(`Script`
  才是),是`Script`将来会用到的底层机制,提前迁移。

这几份文件均**原样移植**,唯一的例外是`utility.h`把`debugf`的`LPCSTR`参数改成了`const char*`
并将`<windows.h>`挪进`.cpp`(避免头文件污染,呼应`loader.md`已定的先例),详见
`common/utility.md`。

## 待办/后续阶段

- 阶段4:按系统迁移进度,逐个把对应concept的`<Concept>Mod`从"只有GetType/GetName"升级成
  真正的业务接口(对照旧工程同名`_mod.h`补齐),并从共用文档升级为独立`.md`。
- 阶段4:`<Concept>Factory`目前只有最小注册表(`registries`+`liveInstances`两个map),旧工程
  的`Temp`暂存区+`MergeTemp()`两段式注册(用于隔离"探测阶段"和"正式生效阶段")这次简化掉了,
  如果阶段4发现多个mod并发注册确实需要这层隔离,再按需恢复。
