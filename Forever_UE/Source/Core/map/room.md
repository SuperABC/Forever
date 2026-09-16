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
- **4类占位能力（进入populace域时新增）**：`IsResidential()`/`ResidentialCapacity()`
  （床位数）/`IsWorkspace()`/`WorkspaceCapacity()`（工位数）/`IsStorage()`/
  `StorageConfig()`（仓库属性）/`IsManufacture()`/`ManufactureTypes()`（产线描述），全部
  纯转发`RoomMod`同名字段（见`Source/Dependence/map/room_mod.h`）——照抄老工程`RoomMod`
  的字段设计。这次只有`ResidenceRoom`（`Source/Basic/map/room_residence.cpp`，改名自
  `RoomBasic`）真正设了`isResidential=true`+`residentialCapacity=1`，另外3类保持
  `RoomMod`基类默认值（false/空），等Job/Industry域迁移时再由各自的具体`RoomMod`子类真正
  启用，详见`Source/Core/populace/populace.md`。
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
- **人/归属和房间的4个独立概念：owner+stated / tenants / occupants（进入populace域第三/
  四轮迁移新增，用户逐步纠正过两次）**：第一次只加了`tenants`，被用户指出还缺一个"当前
  是否人在这个房间里"的概念；第二次被进一步纠正说其实房间有且只有一个`owner`（当时理解
  成"谁抽到住处谁就是owner"）；第三次用户明确要求migrate老工程的园区/建筑/房间归属分配
  逻辑，指出"归属"和"住处分配"是两个完全独立的关注点（可以理解成"业主"和"租客"的区别），
  且归属除了"某个具体citizen私有"还有"公有"这第三种结果，见下：
  - `Citizen* GetOwner() const` / `SetOwner(Citizen*)` + `bool GetStated() const` /
    `SetStated(bool)`——房间归属的完整表达，二者互斥：`owner`非空表示私有（归某个具体
    citizen），`stated`为true表示公有（`owner`保持`nullptr`——"公有"本来就不指向任何
    具体的人）。`Map::Checkin()`给每个room解析归属时二者恰好落在其中一种，不会同时/都不
    设，详见`Source/Core/populace/populace.md`"房产归属"一节——**和"谁住在这里"
    （tenants）完全无关**，一个room的owner可以是从没在这里住过的人（相当于"房东"），
    也可能恰好是自己住在这里的人。
  - `GetTenants()` / `AddTenant(Citizen*)` / `RemoveTenant(Citizen*)`——住在这个房间的
    人。私有归属时owner本人**有可能**也在`tenants`里（如果他自己抽中了这间当住处），但
    这不是必然的——`Map::Checkin()`的"房产归属"和"住处分配"是两个独立、顺序执行的步骤，
    一个room的owner和它的tenants完全可能是不相干的两拨人。
  - `GetOccupants()` / `AddOccupant(Citizen*)` / `RemoveOccupant(Citizen*)`——当前物理
    位置在这个房间里的人。和`tenants`是两回事：一个tenant可能人不在家（以后有真正移动
    AI之后，citizen的当前位置可能是别的房间），一个occupant也可能不是这个房间的tenant
    （以后有访客/工作场景时会出现）。这次没有真正的移动AI，`Map::Checkin()`分配住处的
    同时就让citizen"住进去"，`tenants`/`occupants`的初始状态天然重合，但数据结构上必须
    分开存，为将来"人在家但当前不在自己房间里"这类场景预留。对应
    `Citizen::GetCurrentRoom()`（和`GetRoom()`即"家"是两个概念，见
    `Source/Core/populace/citizen.md`）——citizen当前在哪个房间，就应该出现在那个房间
    的`occupants`列表里，两边由调用方（目前是`Map::Checkin()`）手动同步维护，`Room`/
    `Citizen`自己都不做级联同步。
  - `RemoveTenant`/`RemoveOccupant`这次没有任何调用方（`Map::Checkin()`只是一次性
    分配，没有"搬家"/"离开房间"的逻辑），照抄老工程`Room::AddTenant/RemoveTenant`同款
    的add/remove配对先加上，等真正的移动/搬家逻辑接入时就有地方用了。

## 依赖关系

- 依赖：`Source/Dependence/map/room_mod.h`（`RoomMod`）、`room_factory.h`、
  `Source/Dependence/map/geometry.h`（`Quad`/`WallHole`/`Node`/`FACE_DIRECTION`）、
  `Source/Core/populace/citizen.h`（`Citizen`，`owner`/`tenants`/`occupants`只存指针，
  前向声明即可，见room.h顶部）。
- 被谁依赖：`Source/Core/map/building.h`/`.cpp`（`Building::AssignRoom`/`ArrangeRow`创建，
  `~Building()`销毁）、`Source/Core/map/component.h`（`Component::AddRoom`，不持有生命周期）、
  `Source/Forever/Framework/ForeverBuildingFrameworkComponent.cpp`（按`GetLayer()==level`
  筛选出这一层的Room，渲染墙体/门窗开洞）、`Source/Core/map/map.cpp`（`Map::Checkin()`写
  `owner`/`tenants`/`occupants`）。

## 待办/后续阶段

- furniture/pivots/storages/manufactures/parkings/vehicles/ownership/tenancy等字段——
  依赖Industry/Populace/Society，等对应领域迁移到了再回来加。
- 4类占位能力里workspace/storage/manufacture这3类目前没有任何生成/消费方（Job/Industry域
  还没迁移），只有residential这一类真正用起来了（`Map::ComputeAccommodationTarget()`/
  `Map::Checkin()`，见`Source/Core/populace/populace.md`）。
