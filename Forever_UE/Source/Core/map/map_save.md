# map_save.h / map_save.cpp

## 职责

地图域的存档读写，类名 `MapSaveCodec`。范围包括：
- 地形格子（含细高度）；
- 路网、Lot 树、路口；
- 导航锚点、车行图和行人图、贯通线；
- Zone、Building、Floor、Component、Room；
- 门；
- 植被。

整体方案见仓库根目录的 `SAVE_PLAN.md`。存档的 P2（地图）和 P3（建筑）合并在这里实现，原因见下面"为什么地图和建筑一起恢复"一节。

## 文件格式

地图拆成 4 个二进制段，都用 `ByteWriter`/`ByteReader` 读写，小端序。各段的版本号在 `common/save.cpp` 的 `GetSaveSectionVersions` 里登记。

| 段 | 内容 |
|---|---|
| `world.map.terrain` | 宽高；地形类型名调色板；每个格子的地形下标、高度、水面、hatches、细高度（可选 17×17）；`terrainTextures` |
| `world.map.roadnet` | 宿主的 `Node::count`；Roadnet（mod 注册 id、externs、intersections、hatches）；Road 表；Lot 表；`addressesByRoad`；路口；导航锚点；导航边表；车行图和行人图；贯通线 |
| `world.map.structures` | 门计数器；Zone；Building（含 Floor、Component、Room）；门；门的持有者列表 |
| `world.map.vegetation` | mesh 路径表、全部 `TreeInstance`、`nextId` |

所有 `unordered_map` 都按 key 排序后写出，Zone 和 Building 按名字排序。这样同一份数据每次写出的字节完全一样。

## 关键设计

- **存结果，不存种子**。世界生成里有 `random_device`，还依赖 `unordered_map` 的遍历顺序，无法复现。所以这里把各类对象的字段原样写出。读档时按存档重建对象，**不调用任何 mod 的 `Layout`/`Distribute`/`LayoutTrees`**。
- **直接读写私有字段**。相关类都声明了 `friend class MapSaveCodec;`，包括：
  - `Node`、`Connection`、`Road`、`Lot`；
  - `Roadnet`、`Map`、`Zone`、`Building`；
  - `Floor` 以及 `Stair`、`Elevator`、`Ramp`、`Ceiling`、`Ground`、`Hatch`、`Corridor`、`Single`、`Row`；
  - `Room`、`Component`、`Door`、`Vegetation`。

  友元声明不改变类的布局。mod 基类没有改动。
- **指针换成编号**：
  - Road 和 Lot 按存档时建的表的下标引用。表里还记着每个对象的持有者（见下）。
  - 导航边（`Connection*`）按边表下标引用。行人图里同一条边被两个方向各引用一次，表里只存一份。
  - 导航锚点按 `Node::GetId()` 引用。锚点表由 `Map::navAnchorNodes` 和路网 externs 组成，和 `Map::FindPedestrianPath` 反查指针用的是同一个约定。
  - Zone 和 Building 按写出顺序的下标引用；Room 用 (building 下标, 房间下标) 引用；门按 `Map::allDoors` 的下标引用。
- **持有关系照原样重建**。读档后由谁 `delete` 每个对象，和新开局完全一致：

  | 对象 | 持有者 |
  |---|---|
  | `Roadnet::roads`、只被顶层 Lot 边界引用的独立 Road 拷贝 | `Roadnet` 析构时去重释放 |
  | 小路 | 持有那条 `PathRoadLink` 的 Lot |
  | 园区内部道路 | Zone |
  | 子块 | 父 Lot 的 `freeLots`（Lot 表按前序遍历写出） |
  | 门 | 各自所属的 Zone/Building/Room 的 `doorEntities` |

  有一个例外：自适应占位用过的"游离"子块。新开局时它没有持有者，一直泄漏；读档后改由 `Map::orphanLots` 持有。
- **mod 实例重新创建**。每个 Zone/Building/Room/Component 用存档里记的注册 id（不是 `GetType()`）调用 `Factory::Create*`，再调用 `LoadState` 恢复 mod 自己的私有数据。mod 的公共声明式字段（`floors`、`cabins`、`walls`……）在读档后是空的。所以 UE 和 Core 运行时只能读包装类在生成时拷下来的快照：
  - `Building::GetFloorSpec`、`GetCabins`、`GetStationMod`；
  - `Zone::GetWalls`、`GetGates`；
  - `Room` 的属性。

  **以后新增"运行时要读的 mod 公共字段"时，也必须在包装类里拷一份快照，并加进这里的存档。**
- **不从 Core 往 mod 的容器里写数据**。Core 和 mod DLL 的分配器不同（见 memory"Cross-DLL allocator crash"），所以 Core 不能写 mod 自己的 `std::vector` 等容器。这也是用快照而不是"把字段写回 mod"的原因。
- **门**：
  - 读档时用原来的 `Door` 构造函数重建，`flippedSide` 按存档值传入，所以会和新开局一样按 spec 创建 Script。
  - 恢复 `access` 和 `allowedCitizenNames`。
  - `options`（剧情脚本加的交互选项）：存档带脚本状态段（`world.scripts`）时读档会跳过 game_start 广播，这时按存档恢复；没有这一段的旧存档不恢复，因为 game_start 会再加一遍。
  - Script 自身的状态由 `story/script_save` 恢复（键 `door:<门id>`）。
- **`Node::count`**。读档时每构造一个 Node 都会让计数器多加一，所以最后精确设回存档值。新开局之后新建的 Node 会从存档时的编号继续往后编，再存一次档时这个字段也和原存档一致。mod DLL 里各自的那份 `Node::count` 不受影响：读档不会再跑 roadnet mod。
- **布局模板不再需要**。Floor 的全部内容（楼梯、走廊、房间格子……）原样写进存档，读档不重新读 `.layout` 文件。manifest 里的 layouts 列表和读档前的资源检查保留，名字来自 `Building::GetFloorSpecs` 快照。

## 为什么地图和建筑一起恢复

生成 Zone/Building 时会改动路网和地形：
- 在道路上开出入口（`Road::openings`）；
- 切割 Lot 的空闲空间、生成小路；
- 把贯通线断开再接入建筑（`BreakThroughLine`）；
- 往地形上挖洞（hatches）。

存档时这些改动都已经做过了。如果只恢复地图、再在上面重新生成建筑，这些改动会叠加第二次。所以地图域整体按存档恢复。人口、社会、交通、剧情、玩家暂时仍在恢复出来的地图上重新生成：它们只读地图，或者只改不在地图段里的字段，比如 owner 和 tenants。

## 往返一致性

UE 控制台命令 `Forever.SaveRoundTrip`：当前地图写段 → 读回成一张新 Map → 再写段，逐段比较字节。全部一致说明没有漏存的字段。只比较地图段，因为 manifest 里的计数器会因为读档新建 mod 而变化。

## 依赖关系

- 依赖：`map/*`、`common/save_archive`（`ByteWriter`/`ByteReader`/`SaveArchive`）、`common/save`（`JsonStateWriter`/`JsonStateReader`、段版本表）。
- 被谁依赖：
  - `common/save.cpp`（`SaveGame::BuildArchive`）；
  - `Forever/Framework/ForeverFrameworkActor.cpp`（读档时 `EnsureMapGenerated` 改走 `Load`）；
  - `Forever/Mod/ForeverSaveSubsystem.cpp`（`Forever.SaveRoundTrip`）。

## 待办 / 后续

- owner、tenants、occupants、房间里的物品（`Room::assets`）在 P4/P6 存。
- 读档失败时已经建出来的部分对象没有释放（读档失败本来就会退回重新生成世界）。
