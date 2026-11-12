# zone.h / zone.cpp

## 职责

`Zone`是"已经落地的一个具体Zone实例"的Core包装类，模式照抄`terrain.h`：持有一个`ZoneMod*`
（由`ZoneFactory`创建/销毁），转发`GetType`/`GetName`。和`Terrain`不同的是**继承`Quad`**——
`Zone`占据的矩形直接就是它自己（`SetPosition`/`GetPosX`等都是`Quad`现成的方法），不额外
持有一个`footprint`字段。

`Zone`落地时`Map::InitZones()`调用继承来的`SetPosition`把它摆到`Lot::RequestPlacement`裁剪出来的位置上；
园区内部的道路/建筑由`ZoneMod`显式声明（见下），不是自动填充剩余空间，见`Source/Core/map/map.md`"InitZones"一节。

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
- **围墙/大门/内部道路/内部建筑（Zone内部布局）**：`GetWalls()`/`GetGates()`返回`Layout()`时从
  `mod->walls`/`mod->gates`拷贝的快照，不直接转发mod字段——读档时mod是重新创建的空实例、`Layout`不会
  再跑（见`map_save.md`）。`internalRoads`(`vector<Road*>`，真正实例化
  出来的`Road`)/`internalBuildings`(`vector<Building*>`，真正实例化出来的`Building`)这两个
  **是**`Zone`自己的字段——它们和`mod->internalRoads`(`vector<ZoneInternalRoadSpec>`)/
  `mod->internalBuildings`(`vector<ZoneInternalBuildingSpec>`)类型完全不同（前者是Core
  实例化出来的真实对象，后者是mod提供的原始描述），不能相互替代。生命周期：`internalRoads`
  是专门为这个Zone新建的`Road`（`~Zone()`里`delete`），`internalBuildings`只登记指针不
  持有生命周期（所有权在`Map::buildings`，和其余顶层`Building`一样由`~Map()`统一释放）。
  围墙/大门的局部坐标语义（参考边+margin+depth，原点在Zone矩形中心）不需要额外坐标转换
  字段——渲染时Forever层直接拿`Zone`自己的`GetPosX/PosY/SizeX/SizeY/GetRotation()`+这份
  数据现算世界坐标。
- **`gapMaterialPath`**：`Layout`时从`mod->gapMaterial`拷贝的园区地面材质快照（`GetGapMaterialPath()`），进存档（紧跟gates之后）。
- **`Layout(int direction)`**：转发`mod->Layout(direction, *this, GetBoundaryRoads())`，并把`walls`/`gates`/
  脚本字段(`scriptModName`/`milestoneNames`)快照到`Zone`自己身上、记下`direction`、换算`decorations`——读档时mod是
  重新创建的空实例、`Layout`不会再跑，这些快照由`MapSaveCodec`直接还原（`map_save.md`）。`BuildDoors`遍历快照`gates`；读档后门由存档里的`Door`记录重建，不再调用`BuildDoors`
  （它判断车辆门时读的`mod->vehicleEntries/vehicleExits`没有快照，所以只能在新开局`Layout`之后调用）。
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

`ZoneGateSpec`带`DoorSpec door`字段（见`Dependence/map/zone_mod.h`），`mesh`为空=只是
围墙让出的空当，不放门。`Zone`持有`doorEntities`（析构时释放）+`BuildDoors(Map&)`：遍历快照`gates`，
`gate.door.mesh`非空就按和`ForeverZoneFrameworkComponent::BuildWallSegment`同一套
"参考边+margin+depth+depthInward"公式算出世界坐标(Core端复刻，算地图单位不是UE单位)，
调`map.CreateDoor(DOOR_KIND_ZONE, ...)`。`IsVehicleGate`按这段范围是否和
`mod->vehicleEntries`/`vehicleExits`里任意一个出入口点重叠判定(按局部坐标离哪条边最近
归属，和`Map::ConnectZoneAccessPoint`同一个思路)。由`Map::InitZones()`在
`zone->Layout(direction)`之后调用一次，详见`door.md`。

### 朝向、自定义模型和树的坐标换算

`Layout(direction)`另外记下`direction`(大门方向，`GetDirection()`)。`NorthPointToWorld(PointParams)`
把"朝北"定义的园区quad坐标(原点西北角)按`direction`用`BuildingLayoutLibrary::InversePoint`旋转，
转成中心原点后按`GetRotation()`旋转加上中心——和`Map::ZoneLocalToWorld`同一套旋转写法；
`NorthYawToWorld`叠加`FaceYawDegrees(direction)`和`GetRotation()`。`Layout()`之后把`mod->decorations`
换算成`GetDecorations()`(`DecorationInstance`，和建筑共用)；`Map::InitVegetation`用同一个函数把
`mod->trees`里的树请求换成世界坐标(拆成`NorthPointToLocal`+`LocalToWorld`两步，种一片树要在局部坐标里取矩形)。direction为-1时按朝北原样处理。

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
  读`Zone::GetWalls()`/`GetGates()`铺围墙ISM）。

## 待办/后续阶段

- Zone内部再对自己剩余的矩形跑一次Building分配流程（推迟，等设计好细节再补）——现有的
  `internalBuildings`是`ZoneMod`显式指定的固定建筑列表，不是自动填充剩余空间，两者不是
  一回事。
