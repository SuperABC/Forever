# room.h / room.cpp

## 职责

`Room`：`Building`内部布局实例化出来的一个具体房间，继承`Quad`表示自己在Building楼体
局部坐标系（以`Building::GetBodySizeX/Y()`为宽高，原点在楼体左下角，未旋转）里占据的矩形。
由`Building::AssignRoom`/`ArrangeRow`按mod声明的槽位创建，不是独立落地的顶层concept。

## 关键设计

- **只保留和"布局/渲染/寻址"直接相关的字段**：`type`/`name`（来自`RoomMod`）、
  `parentBuilding`/`parentComponent`、`layer`（第几层，`Building::GetFloor(level)`的
  `level`）、`direction`（房间朝向，`FACE_DIRECTION`，见下"房间局部坐标系"）、`doors`/`windows`（`WallHole`，每侧的
  开门开窗位置，供`ForeverBuildingFrameworkComponent`渲染墙体开洞用）、`number`（门牌号，
  `"b3-0001"`这种格式，`SetNumber(level,seq)`拼出——basement前缀+4位序号）、
  `navigationNode`（这个房间中心点的导航锚点，供`Building::BuildPedestrianNavigation()`里
  `"single"`/`"row"`类型的导航端点引用）。`Room`本身不持有
  Industry/Populace/Society侧的运行时对象（Storage/Manufacture/Job等由各自的域按`Room`的属性快照创建并反向引用`Room`）。
- **4类占位能力**：`IsResidence()`/`ResidenceCapacity()`（床位数）/`IsWorkspace()`/
  `WorkspaceCapacity()`（工位数）/`IsStorage()`/`StorageType()`（对应StorageMod id，Industry据此生成Storage）/`Rent()`（每日房租，分）/
  `IsManufacture()`/`ManufactureTypes()`（产线描述），读构造时`mod->SetProperty()`之后从`RoomMod`同名字段
  拷贝的快照`properties`（见`Source/Dependence/map/room_mod.h`）——读档时直接按存档恢复这份快照，不依赖重新
  创建的mod再算一遍（见`map_save.md`）。`Source/Basic/map/room_*.cpp`里的各`RoomMod`子类按用途设置这些标志（住宅、工位、仓储、制造等）。
  消费方：`Map::ComputeAccommodationTarget()`/`Map::Checkin()`（居住）、`Organization`（工位）、`Industry`（仓储/制造）、
  `Traffic`（停车位见下"停车位"，不走这4类能力）。
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
- **`GetAddress()`**：`parentBuilding->GetAddress() + " " + number`，和`Map::LocateRoom`的
  解析格式一一对应（"...<buildingName> <number>"）。
- **人/归属和房间的4个独立概念：owner+stated / households / occupants**：
  - `Citizen* GetOwner() const` / `SetOwner(Citizen*)` + `bool GetStated() const` /
    `SetStated(bool)`——房间归属的完整表达，二者互斥：`owner`非空表示私有（归某个具体
    citizen），`stated`为true表示公有（`owner`保持`nullptr`——"公有"本来就不指向任何
    具体的人）。`Map::Checkin()`给每个room解析归属时二者恰好落在其中一种，不会同时/都不
    设，详见`Source/Core/populace/populace.md`"房产归属"一节——**和"谁住在这里"
    （households）完全无关**，一个room的owner可以是从没在这里住过的人（相当于"房东"），
    也可能恰好是自己住在这里的人。
  - `GetHouseholds()` / `AddResident(Citizen*)` / `AddHousehold()`——住在这个房间的户
    （见`populace/household.md`，**直接替代了原来的tenants**）。入住时按夫妻/父母子女关系
    并入已有的户、合并相连的户或新建一户，没有亲属关系的住户分属不同户。私有归属时owner本人
    **有可能**也住在这里，但这不是必然的——"房产归属"和"住处分配"是两个独立、顺序执行的步骤。
  - `GetOccupants()` / `AddOccupant(Citizen*)` / `RemoveOccupant(Citizen*)`——当前物理
    位置在这个房间里的人。和`households`是两回事：一个住户可能人不在家（以后有真正移动
    AI之后，citizen的当前位置可能是别的房间），一个occupant也可能不是这个房间的住户
    （以后有访客/工作场景时会出现）。`Map::Checkin()`分配住处的
    同时就让citizen"住进去"，`households`/`occupants`的初始状态天然重合，但数据结构上必须
    分开存（"人在家但当前不在自己房间里"）。对应
    `Citizen::GetCurrentRoom()`（和`GetRoom()`即"家"是两个概念，见
    `Source/Core/populace/citizen.md`）——citizen当前在哪个房间，就应该出现在那个房间
    的`occupants`列表里，两边由调用方（目前是`Map::Checkin()`）手动同步维护，`Room`/
    `Citizen`自己都不做级联同步。

- **`assets`（房间地上摆放的资产）**：`Room`独占持有这些`Asset`（析构时`delete`，和`households`/`occupants`
  那两个非owning指针不同），key=`Asset::GetName()`；`RemoveAsset`只摘除不delete，调用方接手所有权。
  "当前房间"是玩家真实物理位置（`Player::currentRoom`），见`Source/Core/player/asset.md`。

### 房间门

`SetDoors`是两参数(`WallHole doors, DoorTags tags = {}`)——`DoorTags`和门洞的layout标签
一一对应，从`Single`/`Row`的`GetDoors()`/`GetDoorTags()`原样转发过来，见`building.md`
"建筑门"一节`Building::AssignRoom`/`ArrangeRow`的调用处。`doorEntities`+
`AddDoorEntity`/`GetDoorEntities`——真正放了门扇的房间门由`Building::BuildDoors()`遍历
这个room的门洞、用`mod->doorSpecs`做`ResolveDoorSpec`、调`map.CreateDoor(DOOR_KIND_ROOM,
...)`创建后登记进这里，`Room`持有生命周期(析构时释放)，不是`Building`持有(尽管是
`Building::BuildDoors`在调用`AddDoorEntity`，所有权仍然转交给了room)。

## 依赖关系

- 依赖：`Source/Dependence/map/room_mod.h`（`RoomMod`）、`room_factory.h`、
  `Source/Dependence/map/general_geometry.h`（`Quad`/`WallHole`/`Node`/`FACE_DIRECTION`）、
  `Source/Core/populace/citizen.h`（`Citizen`，`owner`/`households`/`occupants`只存指针，
  前向声明即可，见room.h顶部）。
- 被谁依赖：`Source/Core/map/building.h`/`.cpp`（`Building::AssignRoom`/`ArrangeRow`创建，
  `~Building()`销毁）、`Source/Core/map/component.h`（`Component::AddRoom`，不持有生命周期）、
  `Source/Forever/Framework/ForeverBuildingFrameworkComponent.cpp`（按`GetLayer()==level`
  筛选出这一层的Room，渲染墙体/门窗开洞）、`Source/Core/map/map.cpp`（`Map::Checkin()`写
  `owner`/`households`/`occupants`）。

## 待办/后续阶段

- 家具在UE里每件一个包围盒碰撞(见`BuildingElement.md`)，Core不感知碰撞；市民寻路不感知家具(靠布局器保证房间中央/门前留空)；分区之间没有隔墙。

## 房间局部坐标系

`direction`是layout里指定的房间"正面"(门/临路一侧)，`ReadTemplates`已经按建筑实例朝向烘焙成**建筑局部(未旋转)坐标系下的朝向**(`InverseDirection(layoutDir, buildingFace)`)，存档里本来就存。世界朝向 = 这个方向 + `Building::GetRotation()`。

房间局部坐标系和建筑/layout一样以角为原点：房间"朝北"时原点在房间(minX,minY)角(建筑局部系原点那种角)，u=沿正面墙、v=朝正面为正，yaw 0度=朝向正面；朝向是W/E/S时整套坐标系做纯旋转(和`BuildingLayoutLibrary::InversePoint`同一套)。所以家具/车位等按"房间朝北"写一次，转向由Core换算。PointParams的"比例x该轴长度+米数偏移"里，比例乘`AxisSizeU()`/`AxisSizeV()`(W/E朝向时u轴长=建筑局部SizeY，v轴长=SizeX)。

- `LocalToBuildingLocal(u,v,ox,oy)`：u/v米数 -> 相对房间中心的建筑局部偏移。NORTH `(u-W/2, v-H/2)`、SOUTH `(W/2-u, H/2-v)`、WEST `(v-W/2, H/2-u)`、EAST `(W/2-v, u-H/2)`(W=SizeX，H=SizeY)；朝向无效按NORTH。
- `FacingYawDegrees()`：局部yaw 0度在建筑局部系里的数学角(逆时针，0度=+X)：W0/E180/N90/S270(等于局部+v轴在建筑局部系里的方向，和`LocalToBuildingLocal`一致；曾经W/E写反，W/E朝向的房间家具前后全反)。
- `LocalToWorld(u,v)` / `LocalYawToWorld(yawDegrees)`：直接换算成世界XY(地图单位)和世界yaw(弧度，叠加建筑旋转)，给家具/asset放置用。
- 消费方：家具(`LayoutFurniture`/`BuildLayoutInfo`，见下"家具")；车位已经是建筑局部坐标，不用房间局部坐标；房间触发盒/墙/门/导航节点/市民落点都是中心+轴对齐，和朝向无关。

## 家具

`Room`持有`furniture`(`std::vector<DecorationInstance>`，世界坐标，和建筑装饰同结构)，`GetFurniture()`只读，UE侧`ABuildingElement::BuildRoomFurniture`据此摆(见`BuildingElement.md`)。

- `LayoutFurniture()`：`Building::AssignRoom`/`ArrangeRow`在设好位置/朝向/门窗/墙样式/门牌号之后调用(创建room时就算，不是读档时)。流程：`BuildLayoutInfo()`构造`RoomLayoutInfo` -> 在`RandomScope(RandomRegistry::GetInstanceStream(mod))`里调`mod->Layout(info)` -> 读`mod->furniture`，每件换算：`x,y=LocalToWorld(u/10,v/10)`(地图单位=10米)、`z=building->GetFloorBaseZ(layer)+spec.z/10`、`yaw=LocalYawToWorld(spec.yaw)`。**只读不清**`mod->furniture`(跨DLL堆)；mod对象是新建的、vector本来为空。
- `BuildLayoutInfo()`：宽/深取房间局部系(`AxisSizeU/V`，乘10转米)，层高取所在层；每个`WallHole`洞按房间朝向用`BuildingOffsetToLocal`(`LocalToBuildingLocal`的逆)换回房间局部(u,v)，再判定落在哪面墙(`ROOM_WALL_*`)、沿墙范围、离地高度(洞口高度是从天花板往下量的，这里换成离地)。门窗方向/位置的换算和墙体开洞(`ComputeDoorFaces`)是同一套。
- **存档**：`map_save.cpp`房间段在`wallStyles`之后写读`furniture`(`WriteDecorations`/`ReadDecorations`)，读档**不重新布局**(否则读档会改摆放)；没升版本号，旧存档不兼容。

## 墙样式（wallStyles）
`Room::GetWallStyle(dir)/SetWallStyle`/`GetWallStyles()`：按墙朝向存四个侧面的标签与材质路径，来源见 building.md"墙的侧面标签"。
存档随 Room 写读（没升版本号）。

## 停车位 `ParkingSlot`

车位属于`Room`(`GetParkingSlots()`，Room持有所有权)，没有车位的房间返回空。几何是**建筑局部坐标**(和`Room::GetPosX/Y`同一套)：
`centerX/centerY`(矩形切分后每个车位的中心)、`halfWidth`(沿垂直开口的轴)、`halfDepth`(沿开口轴)、`direction`(开口朝向，朝通道；
车头朝向=开口反向，`VehicleYawDegrees()`)、`categories`(可停种类car/truck/plane)、`navigationNode`(车位中心的车行导航节点，
`Map`持有生命周期)、`reservedBy`(在途车)、`parkedVehicle`(已停车)。

- **来源**：layout的`parkings`矩形 + `BuildingMod::AssignParking(level, slot, parkingIndex, spacing, categories)`。`Building::BuildParkingSlots()`
  在`Layout`里(建导航图之前)把矩形沿垂直开口的轴按`spacing`切成`floor(边长/spacing)`份(实际宽度>=spacing，0份=没有车位；**`spacing<0`(约定-1)=整个矩形就是一个车位**，不管矩形多宽，机场停机坪用它)，每份生成一个车位挂到
  (level, singleRoomSlot)对应的房间。
- **查询**：`FindFreeSlot(category)`(第一个空闲且允许该种类，没有=-1，寻路据此拒绝)、`HasSlotFor(category)`、
  `FindSlotContaining(x,y,yawDeg)`(玩家下车判定：车头朝里=朝向与开口朝向点积<-0.8、车中心在矩形内、车位没有已停的车)。
- **存档**：房间存车位几何+categories+导航节点id；占用由车辆反推(`traffic_save.md`)。
