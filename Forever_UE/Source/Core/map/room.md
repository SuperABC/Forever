# room.h / room.cpp

## 职责

`Room`：`Building`内部布局实例化出来的一个具体房间，继承`Quad`表示自己在Building楼体
局部坐标系（以`Building::GetBodySizeX/Y()`为宽高，原点在楼体左下角，未旋转）里占据的矩形。
由`Building::AssignRoom`/`ArrangeRow`按mod声明的槽位创建，不是独立落地的顶层concept。

## 关键设计

- **这次只保留和"布局/渲染/寻址"直接相关的字段**：`type`/`name`（来自`RoomMod`）、
  `parentBuilding`/`parentComponent`、`layer`（第几层，`Building::GetFloor(level)`的
  `level`）、`direction`（朝向，`FACE_DIRECTION`）、`doors`/`windows`（`WallHole`，每侧的
  开门开窗位置，供`ForeverBuildingFrameworkComponent`渲染墙体开洞用）、`number`（门牌号，
  `"b3-0001"`这种格式，`SetNumber(level,seq)`拼出——basement前缀+4位序号，照抄老工程
  `Room::SetNumber`）、`navigationNode`（这个房间中心点的导航锚点，供`Building::
  BuildPedestrianNavigation()`里`"single"`/`"row"`类型的导航端点引用）。**不迁移**老工程
  `Room`身上的`furniture`/`pivots`/`storages`/`manufactures`/`parkings`/`vehicles`/
  `ownership`/`tenancy`/`workers`等字段——那些依赖Industry/Populace/Society这些还没迁移的
  领域，等对应领域迁移到了再回来加。
- **`RoomMod`极简，只有`GetType()`/`GetName()`**：`Room`不是像`Zone`/`Building`那样要参与
  "在地图上竞争地块"的顶层concept，永远是`Building`自己在`Layout()`里通过`AssignRoom`/
  `ArrangeRow`显式创建的，不需要`Assign`/`RandomAcreage`/`GetPower`这套static注册机制。
- **独占持有一个`RoomMod`实例**，和`Zone`/`Building`同一个模式：构造时创建、析构时
  `factory->DestroyRoom(mod)`。但`Room`对象本身（不是它持有的`RoomMod*`）由`Building`
  统一`new`/`delete`（`Building::AssignRoom`/`ArrangeRow`里`new Room(...)`，`~Building()`
  里`delete room`）——`Room`不参与地块竞争，没有独立的"factory查表创建"入口。
- **`navigationNode`不持有生命周期**：和building内部导航图的其它节点一样，创建后交给
  `Map::navAnchorNodes`统一管理（`Building::Layout()`创建完所有`Room`之后统一
  `SetNavigationNode`，再在`BuildPedestrianNavigation()`结尾把它们登记进
  `BuildingNavResult::nodes`，见`building.md`"行人导航"一节）——容易漏登记，漏了会导致
  这个Node永远不会被delete。
- **`GetAddress()`（第N轮迁移补全）**：`parentBuilding->GetAddress() + " " + number`，照抄
  老工程`Room::GetAddress`，和`Map::LocateRoom`的解析格式一一对应（"...<buildingName>
  <number>"）。这是这次"补全Room/Component在map域里的内容"唯一真正缺失的东西——核对过老
  工程`Room`全部字段/方法后发现，除了`GetAddress`之外，纯map域(布局/渲染/寻址)相关的部分
  这次会话之前已经port完了；`pivots`(家具锚点)/`ParkingSpaces`/`Is Residential`等能力
  声明查询都明确跟其它未迁移域绑定，不算"map域缺失"，不补。

## 依赖关系

- 依赖：`Source/Dependence/map/room_mod.h`（`RoomMod`）、`room_factory.h`、
  `Source/Dependence/map/geometry.h`（`Quad`/`WallHole`/`Node`/`FACE_DIRECTION`）。
- 被谁依赖：`Source/Core/map/building.h`/`.cpp`（`Building::AssignRoom`/`ArrangeRow`创建，
  `~Building()`销毁）、`Source/Core/map/component.h`（`Component::AddRoom`，不持有生命周期）、
  `Source/Forever/Framework/ForeverBuildingFrameworkComponent.cpp`（按`GetLayer()==level`
  筛选出这一层的Room，渲染墙体/门窗开洞）。

## 待办/后续阶段

- furniture/pivots/storages/manufactures/parkings/vehicles/ownership/tenancy等字段——
  依赖Industry/Populace/Society，等对应领域迁移到了再回来加。
