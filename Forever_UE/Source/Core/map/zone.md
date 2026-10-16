# zone.h / zone.cpp（`building.h`/`.cpp`同构，见文末）

## 职责

`Zone`是"已经落地的一个具体Zone实例"的Core包装类，模式照抄`terrain.h`：持有一个`ZoneMod*`
（由`ZoneFactory`创建/销毁），转发`GetType`/`GetName`。和`Terrain`不同的是**继承`Quad`**——
`Zone`占据的矩形直接就是它自己（`SetPosition`/`GetPosX`等都是`Quad`现成的方法），不额外
持有一个`footprint`字段。

`Zone`不实现"Zone内部再持有一批`Building`"的递归布局——只是一个类型+矩形的占位对象，落地时
`Map::InitZones()`会调用继承来的`SetPosition`把它摆到`Lot::RequestPlacement`裁剪出来的位置
上，然后就结束了，见`Source/Core/map/map.md`"InitZones"一节。

## 关键设计

- **`Zone`从构造到析构只持有一个`ZoneMod`实例**：`ZoneMod::Assign(lots, emit, context)`是
  不需要任何实例的static方法——`Map::InitZones()`先对这个类型调一次`zoneFactory.Assign(id,
  排好序的GetLots(), &EmitPlacementRequest, &requests)`，一次性扫完全地图的lot拿到这个类型
  想要的全部`LotPlacementRequest`（不存在任何`ZoneMod`实例），只有对应的`lot->
  RequestPlacement(...)`也真的成功了，才`zoneFactory.CreateZone(id)`创建**唯一一次**、真正
  要被长期持有的实例，直接交给新建的`Zone`持有（`Zone`从此独占它，不会再有别的`Zone`共用
  同一个指针），析构时`~Zone()`可以放心`factory->DestroyZone(mod)`——"要不要这块lot"这个
  问题完全不需要构造实例来回答。`walls`/`gates`/`internalBuildings`等数据的填充拆成两步：
  `Assign`（只决定"要不要、往哪摆"）+`Layout(int direction, const Quad& quad, const
  unordered_map<int,Road*>& boundaryRoads)`（只管"摆下去之后长什么样"，在`Zone`真正构造
  出来、`SetPosition`/`SetBoundaryRoad`都设好之后调用一次），`direction`是`Assign`选中的
  真实方向。
- **`parentLot`只是一个方便查询的反向引用**，`Zone`不负责这个指针的生命周期（`Lot`本身也
  不知道有哪些`Zone`落在自己身上——单向引用已经够用，`Map`直接拿着`vector<Zone*>`用）。
- **`GetRotation()`直接转发`parentLot->GetRotation()`，`Zone`/`Building`自己不存这个
  字段**——既然`freeLots`池里所有子块都继承同一个顶层`Lot`的`rotation`（`Lot::
  SplitWithPath`产出的两段都用同一个`this->rotation`构造），而`Zone`/`Building`本来就已经
  通过`parentLot`拿着这个顶层`Lot*`（见下），再自己存一份`rotation`纯粹是冗余拷贝——
  `SetParentLot(lot)`之后`GetRotation()`直接转发`parentLot->GetRotation()`就是同一个值，
  不用额外的`SetRotation`调用。唯一的约束是`GetRotation()`必须在`SetParentLot`之后调用才
  有意义，构造完/`SetParentLot`之前调用返回0（`parentLot`还是空指针）。

- **`boundaryRoads`（四周边界Road）**：`unordered_map<int, Road*>`，下标按`FACE_DIRECTION`，
  和`Lot::boundaryRoads`语义完全一致——非持有指针，不管理生命周期。`Map::InitZones()`落地
  成功后从`Lot::RequestPlacement`的`outBoundaryRoads`输出参数里拷贝过来（这个参数是为了
  解决"`RequestPlacement`内部真正裁剪出来的`Lot`在返回前就被`delete`了，边界Road信息本来
  会跟着丢失"这个问题而加的，见`general_geometry.md`）。
- **围墙/大门/内部道路/内部建筑（Zone内部布局）**：`GetWalls()`/`GetGates()`直接返回
  `mod->walls`/`mod->gates`——`mod`是这个`Zone`独占持有的实例，数据不会失效，不需要再在
  `Zone`自己身上拷贝一份`walls`/`gates`字段。`internalRoads`(`vector<Road*>`，真正实例化
  出来的`Road`)/`internalBuildings`(`vector<Building*>`，真正实例化出来的`Building`)这两个
  **是**`Zone`自己的字段——它们和`mod->internalRoads`(`vector<ZoneInternalRoadSpec>`)/
  `mod->internalBuildings`(`vector<ZoneInternalBuildingSpec>`)类型完全不同（前者是Core
  实例化出来的真实对象，后者是mod提供的原始描述），不能相互替代。生命周期：`internalRoads`
  是专门为这个Zone新建的`Road`（`~Zone()`里`delete`），`internalBuildings`只登记指针不
  持有生命周期（所有权在`Map::buildings`，和其余顶层`Building`一样由`~Map()`统一释放）。
  围墙/大门的局部坐标语义（参考边+margin+depth，原点在Zone矩形中心）不需要额外坐标转换
  字段——渲染时Forever层直接拿`Zone`自己的`GetPosX/PosY/SizeX/SizeY/GetRotation()`+这份
  数据现算世界坐标。
- **`Layout(int direction)`（Core编译的转发方法）**：`mod->Layout(direction, *this,
  GetBoundaryRoads())`——`Zone`这一层没有需要额外解析缓存的数据(`GetWalls()`/`GetGates()`
  本来就直接转发`mod->walls`/`mod->gates`)，这个方法纯粹是让`Map::InitZones()`不用自己摸
  `mod`指针，和`Building::Layout()`保持同样的调用形态（`Building`那边因为要解析楼体/楼层/
  LOD数据，`Layout()`内部除了转发还要做更多事，见下）。
- **寻址唯一性**：`GetName()`直接转发`mod->GetName()`，唯一性由mod自己在**构造函数**里维护
  static计数器保证，`Zone`本身不做任何消歧——`Map::AddZone`发现重名只是拒绝加入+`debugf`
  警告，作为mod实现有误时的兜底，不是保证唯一性的主要手段。`Map::GetZone(name)`按这个唯一
  name做扁平查找；`Map::LocateZone(address)`则是按"`<road> <index> <zoneName>`"分层地址
  查找（`road`/`index`对应这个Zone所在顶层Lot的`(road,index)`地址，见`Roadnet::LocateLot`），
  两套查找方式详见`map.md`"寻址"一节。
- **`GetMod()`暴露这个Zone持有的mod实例，供`Map::InitBuildings()`直接读
  `zone->GetMod()->internalBuildings`实例化Building**——`Map::InitZones()`阶段
  `buildingFactory`还没注册mod，不能在那时候`new Building(&buildingFactory, spec.type)`
  （`CreateBuilding`会返回`nullptr`导致`Building`构造函数抛异常崩溃）；既然`Zone`本来就
  独占持有着那个mod实例（见上一条），直接在`InitBuildings()`里问`zone->GetMod()->
  internalBuildings`要，不需要额外的拷贝/暂存/清空接口。
- **`owner`/`GetStated()`（房产归属）**：`Citizen* owner = nullptr`+`bool stated = false`，
  和`Room`/`Building`同一套"owner非空=私有，stated=true=公有，二者互斥"语义（见
  `room.md`）。`Map::Checkin()`给每个zone随机roll一次"整个zone统一归属"（公有/私有/都
  不是），如果不是"整个zone统一"就下探到各个`internalBuilding`各自独立决定；zone一旦统一
  归属，会把这个owner/stated**级联写到它所有的`internalBuildings`以及这些building的所有
  `Room`**——一个园区属于某人或公有时，这个园区里的所有房间/建筑都属于这个人或公有，详见
  `Source/Core/populace/populace.md`"房产归属"一节。

### 园区大门

`ZoneGateSpec`新增`DoorSpec door`字段（见`Dependence/map/zone_mod.h`），`mesh`为空=只是
围墙让出的空当，不放门。`Zone`新增`doorEntities`+`BuildDoors(Map&)`：遍历`mod->gates`，
`gate.door.mesh`非空就按和`ForeverZoneFrameworkComponent::BuildWallSegment`同一套
"参考边+margin+depth+depthInward"公式算出世界坐标(Core端复刻，算地图单位不是UE单位)，
调`map.CreateDoor(DOOR_KIND_ZONE, ...)`。`IsVehicleGate`按这段范围是否和
`mod->vehicleEntries`/`vehicleExits`里任意一个出入口点重叠判定(按局部坐标离哪条边最近
归属，和`Map::ConnectZoneAccessPoint`同一个思路)。由`Map::InitZones()`在
`zone->Layout(direction)`之后调用一次，详见`door.md`。

## 依赖关系

- 依赖：`map/zone_mod.h`、`map/zone_factory.h`、`map/general_geometry.h`（`Quad`/`Lot`）、
  `map/door.h`（`BuildDoors`调`Map::CreateDoor`）、
  `building.h`（`vector<Building*> internalBuildings`需要完整类型，只存指针可以只前向声明，
  但`.cpp`里`Building*`没有调用任何方法所以`zone.cpp`本身不需要include `building.h`，
  头文件里`class Building;`前向声明即可）。
- 被谁依赖：`Source/Core/map/map.h`（`Map::zones`成员，`InitZones()`产出，以及新增的
  `ZoneLocalToWorld`/`ConnectZoneAccessPoint`/`ConnectZoneInternalRoad`/
  `PlaceZoneInternalBuilding`四个私有方法）、
  `Source/Forever/Framework/ForeverZoneFrameworkComponent.h/.cpp`（`GenerateZones(Map*)`
  读`Zone::GetWalls()`铺围墙ISM——不画扁box占位，围墙本身已经足够表达zone范围）。

## 待办/后续阶段

- Zone内部再对自己剩余的矩形跑一次Building分配流程（推迟，等设计好细节再补）——现有的
  `internalBuildings`是`ZoneMod`显式指定的固定建筑列表，不是自动填充剩余空间，两者不是
  一回事。
- 大门资产/渲染（`ZoneGateSpec`这次只存位置数据，没有mesh字段）。

---

# building.h / building.cpp

和`Zone`结构基本一样（同样继承`Quad`），`Building`从构造到析构只持有**一个**`BuildingMod`
实例：`Building(BuildingFactory* factory, BuildingMod* mod)`构造，`~Building()`里
`factory->DestroyBuilding(mod)`——每个mod实例的生命周期完全绑定它独占的`Building`，这样
"寻址唯一性计数器写在mod构造函数里"这套和`Zone`一致的机制能正确工作（mod实例如果被多个
`Building`共用，构造函数只会跑一次却要服务好几个`Building`，计数器起不到区分作用）。

`BuildingMod`没有`Distribute()`/`explicitPlacements`/`candidateWeights`：
- **显式占位**：和`ZoneMod`同样是static `Assign(lots, emit, context)`，一次调用扫完
  全地图的lot；`Map::InitBuildings()`的处理流程和`InitZones()`完全同构，见`map.md`。
- **`RandomAcreage()`/`GetAcreageMin()`/`GetAcreageMax()`（`FillRemainder`采样面积用）、
  `GetPower(AREA_TYPE)`（按分区类型给lot登记权重）**：子类必须实现的static方法，通过
  `BuildingFactory::RegisterBuilding`的额外函数指针参数注册（和`creator`/`deleter`同样的
  裸函数指针机制——`AssignFunc`内部用`PlacementEmitFunc`回调把结果交回调用方、回调函数体
  编译在调用方那一侧，不返回/不持有任何容器，避免跨DLL分配器问题，`RandomAcreage`等返回值
  是纯`float`，同样没有容器所有权问题）。这些查询完全不需要构造`BuildingMod`实例，只有
  真正落地成功才会`CreateBuilding`一次——`Map::InitBuildings()`不需要`scanners`表。

**`Layout(int direction)`（Core编译，Building自己的落地入口）**：调用方在
`SetPosition`/`SetBoundaryRoad`都设好之后调用一次，内部先调
`mod->Layout(direction, *this, GetBoundaryRoads())`（`this`已经是`Quad`、边界路也已经是
真实数据），再把`mod->footprint`（`BuildingFootprintSpec`，楼体在Building自身`Quad`里的
位置：两个比例float表示楼体中心、两个比例float表示楼体长宽比例，不复用`Quad`类型本身）/
`basements`/`layers`/`floorHeights`/`lodMaterial`解析成`Building`自己的绝对（相对自身
中心，未旋转局部坐标）数值并缓存——`GetBodyOffsetX/Y()`/`GetBodySizeX/Y()`/
`GetBasementCount()`/`GetLayerCount()`/`GetFloorHeights()`/`GetLodMaterialPath()`
这几个getter读的就是这份缓存。`direction`对显式占位(`Assign`产出)落地的`Building`是`Assign`
选中的真实方向；对权重CDF/`FillRemainder`落地、以及目前的园区内部建筑，`direction`分别传
-1、`spec.direction`（`ZoneInternalBuildingSpec`的字段）。`footprint`/`basements`/
`layers`/`floorHeights`/`lodMaterial`这几个字段**不能在mod构造函数里设**——构造函数此时还
不知道任何落地上下文，必须等`Layout()`才有意义，和`ZoneMod`的`walls`/`gates`同理。楼体/
楼层/LOD渲染见`Source/Forever/Framework/ForeverBuildingFrameworkComponent.md`。

`boundaryRoads`（四周边界Road）和`Zone`同样的字段/语义，`Map::InitBuildings()`在显式占位
和`FillRemainder`两条路径上都会填（`FillRemainder`产出的`Lot::FillResult`也有
`boundaryRoads`字段，见`general_geometry.md`）。

**寻址唯一性**：和`Zone`同样，`GetName()`直接转发`mod->GetName()`，唯一性由mod自己在构造
函数里维护static计数器保证，`Map::AddBuilding`发现重名只是拒绝加入+`debugf`警告兜底。
`Map::GetBuilding(name)`扁平查找；`Map::LocateBuilding(address)`按"`<road> <index>
<buildingName>`"（直接落在Lot上、没有`parentZone`的Building）或"`<road> <index>
<zoneName> <buildingName>`"（Zone内部的Building）分层查找，详见`map.md`"寻址"一节。

**`Building`比`Zone`多两个字段，专门给Zone内部建筑用**：
- `Zone* parentZone`：只是纯粹的反向查询登记（"这个building在哪个园区里"），**不参与
  `GetRotation()`计算**——旋转转发走的还是`parentLot`那条链路。和`parentLot`同时设置，
  不是二选一：`Map::PlaceZoneInternalBuilding`把内部建筑的`parentLot`直接设成**它所属
  Zone自己的`parentLot`**（`zone->GetParentLot()`），这样`GetRotation()`的转发基准天然和
  zone一致，不需要为此单独引入一条"转发到`parentZone->GetRotation()`"的路径。
- `float relativeRotation`：叠加在`GetRotation()`结果上的附加偏移，默认0（普通顶层Building
  不受影响，等价于原来纯转发`parentLot->GetRotation()`的行为）。`SetParentLot(Lot*, float
  relativeRotation = 0.f)`用一个默认参数设置它，旧调用点(`SetParentLot(lot)`)不用改。
  `GetRotation()`是`(parentLot ? parentLot->GetRotation() : 0.f) + relativeRotation`。
  对应`ZoneInternalBuildingSpec::relativeRotation`（"以园区旋转为基准继续旋转"，见
  `zone_mod.md`）。

**`owner`/`GetStated()`（房产归属）**：和`Zone`同一套字段/语义（`Citizen* owner =
nullptr`+`bool stated = false`，互斥），`Map::Checkin()`给每个不属于任何zone的独立building
随机roll一次"整个building统一归属"；属于某个zone的building要么被zone级联下来的归属直接
设定（zone统一归属时），要么在zone没有统一归属的情况下由zone那边的循环单独给每个
internalBuilding各roll一次"整个building统一归属"——不会被`Map::buildings`里对同一个
building再重复roll一次，详见`populace.md`"房产归属"一节。building一旦统一归属，会把
owner/stated**级联写到它所有的`Room`**——一个建筑属于某人或公有时，这个建筑里的所有房间
都属于这个人或公有；**如果building内部不同room各自独立归属不同人，`owner`/`stated`保持
默认值（`nullptr`/`false`）**，不需要显式重置，天然由"只在统一归属分支才调用setter"这个
结构保证。
