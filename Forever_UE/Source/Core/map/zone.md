# zone.h / zone.cpp（`building.h`/`.cpp`同构，见文末）

## 职责

`Zone`是"已经落地的一个具体Zone实例"的Core包装类，模式照抄`terrain.h`：持有一个`ZoneMod*`
（由`ZoneFactory`创建/销毁），转发`GetType`/`GetName`。和`Terrain`不同的是**继承`Quad`**——
`Zone`占据的矩形直接就是它自己（`SetPosition`/`GetPosX`等都是`Quad`现成的方法），不额外
持有一个`footprint`字段。

`Zone`这次**不实现**老工程"Zone内部再持有一批`Building`"的递归布局——只是一个
类型+矩形的占位对象，落地时`Map::InitZones()`会调用继承来的`SetPosition`把它摆到
`Lot::RequestPlacement`裁剪出来的位置上，然后就结束了，见`Source/Core/map/map.md`
"InitZones"一节。

## 关键设计

- **`Zone`从构造到析构只持有一个`ZoneMod`实例，仿照老工程的做法（第十六轮迁移，取代了
  早前"扫描用实例先跑一遍全部lot、每条成功结果再另开一个landing实例"的两段式设计）**：
  `Map::InitZones()`对每个(mod类型,lot)组合单独`zoneFactory.CreateZone(id)`一个新实例，
  直接对这一个lot调用`mod->Distribute({lot})`——成功就把这个mod实例原样交给新建的`Zone`
  持有（`Zone`从此独占它，不会再有别的`Zone`共用同一个指针），失败就地
  `zoneFactory.DestroyZone(mod)`。这样mod实例的所有数据（`walls`/`gates`/
  `internalBuildings`等）从始至终只属于一个`Zone`，不需要再拷贝一份到`Zone`自己身上——
  早前的两段式设计里，`walls`/`gates`等字段是从"扫描实例"拷到"落地实例"的，如果一次
  `Distribute()`产出了不止一个`explicitPlacements`，所有落地的`Zone`会读到同一份"最后
  一次写入"的配置，是当时没有暴露出来的潜在bug，这次的单实例设计顺带修掉了它。
- **`parentLot`只是一个方便查询的反向引用**，`Zone`不负责这个指针的生命周期（`Lot`本身也
  不知道有哪些`Zone`落在自己身上——这次没有像老工程`Block::AddZone`那样维护双向映射，单向
  引用已经够用，`Map`直接拿着`vector<Zone*>`用）。
- **`GetRotation()`直接转发`parentLot->GetRotation()`，`Zone`/`Building`自己不存这个字段**
  （第十三轮迁移，简化了一版早前的实现）——`Quad`本身没有旋转，最初照抄`Lot`"在`Quad`基础上
  自己加一个旋转角度"的做法给`Zone`/`Building`也单独存了一份`rotation`（PIE验证发现斜向道路
  旁边裁出来的Zone/Building如果不带旋转，扁cube会显示成轴对齐、和实际地块朝向对不上，需要
  旋转数据本身没有错），但既然`freeLots`池里所有子块都继承同一个顶层`Lot`的`rotation`
  （`Lot::SplitWithPath`产出的两段都用同一个`this->rotation`构造），而`Zone`/`Building`本来
  就已经通过`parentLot`拿着这个顶层`Lot*`（见下），再自己存一份`rotation`纯粹是冗余拷贝——
  `SetParentLot(lot)`之后`GetRotation()`直接转发`parentLot->GetRotation()`就是同一个值，不用
  额外的`SetRotation`调用，`Map::InitZones()`/`InitBuildings()`落地时也就不需要再单独调一次
  `SetRotation`了。唯一的约束是`GetRotation()`必须在`SetParentLot`之后调用才有意义，构造完/
  `SetParentLot`之前调用返回0（`parentLot`还是空指针）。

- **`boundaryRoads`（四周边界Road），第十五轮迁移新增**：`unordered_map<int, Road*>`，下标按
  `FACE_DIRECTION`，和`Lot::boundaryRoads`语义完全一致——非持有指针，不管理生命周期。
  `Map::InitZones()`落地成功后从`Lot::RequestPlacement`新增的`outBoundaryRoads`输出参数里
  拷贝过来（这个参数是为了解决"`RequestPlacement`内部真正裁剪出来的`Lot`在返回前就被
  `delete`了，边界Road信息本来会跟着丢失"这个问题而加的，见`geometry.md`）。
- **围墙/大门/内部道路/内部建筑（Zone内部布局），第十五轮迁移新增，第十六轮迁移改成直接转发
  持有的mod**：`GetWalls()`/`GetGates()`直接返回`mod->walls`/`mod->gates`——`mod`是这个
  `Zone`独占持有的实例，数据不会失效，不需要再在`Zone`自己身上拷贝一份`walls`/`gates`字段。
  `internalRoads`(`vector<Road*>`，真正实例化出来的`Road`)/`internalBuildings`
  (`vector<Building*>`，真正实例化出来的`Building`)这两个**是**`Zone`自己的字段——它们和
  `mod->internalRoads`(`vector<ZoneInternalRoadSpec>`)/`mod->internalBuildings`
  (`vector<ZoneInternalBuildingSpec>`)类型完全不同（前者是Core实例化出来的真实对象，后者是
  mod提供的原始描述），不能相互替代。生命周期：`internalRoads`是专门为这个Zone新建的`Road`
  （`~Zone()`里`delete`），`internalBuildings`只登记指针不持有生命周期（所有权在
  `Map::buildings`，和其余顶层`Building`一样由`~Map()`统一释放）。围墙/大门的局部坐标语义
  （参考边+margin+depth，原点在Zone矩形中心）不需要额外坐标转换字段——渲染时Forever层直接拿
  `Zone`自己的`GetPosX/PosY/SizeX/SizeY/GetRotation()`+这份数据现算世界坐标。
- **`GetMod()`暴露这个Zone持有的mod实例，供`Map::InitBuildings()`直接读
  `zone->GetMod()->internalBuildings`实例化Building**——`Map::InitZones()`阶段
  `buildingFactory`还没注册mod，不能在那时候`new Building(&buildingFactory, spec.type)`
  （`CreateBuilding`会返回`nullptr`导致`Building`构造函数抛异常崩溃，PIE验证发现）；一度
  改成让`Zone`额外拷贝一份`pendingInternalBuildingSpecs`暂存到`InitBuildings()`再处理，但
  既然`Zone`现在本来就独占持有着那个mod实例（见上一条），直接在`InitBuildings()`里问
  `zone->GetMod()->internalBuildings`要更直接，不需要再多一层拷贝/暂存/清空的存取接口。

## 依赖关系

- 依赖：`map/zone_mod.h`、`map/zone_factory.h`、`map/geometry.h`（`Quad`/`Lot`）、
  `building.h`（`vector<Building*> internalBuildings`需要完整类型，只存指针可以只前向声明，
  但`.cpp`里`Building*`没有调用任何方法所以`zone.cpp`本身不需要include `building.h`，
  头文件里`class Building;`前向声明即可）。
- 被谁依赖：`Source/Core/map/map.h`（`Map::zones`成员，`InitZones()`产出，以及新增的
  `ZoneLocalToWorld`/`ConnectZoneAccessPoint`/`ConnectZoneInternalRoad`/
  `PlaceZoneInternalBuilding`四个私有方法）、
  `Source/Forever/Framework/ForeverZoneFrameworkComponent.h/.cpp`（`GenerateZones(Map*)`
  读`Zone::GetWalls()`铺围墙ISM——**不再画扁box占位**，围墙本身已经足够表达zone范围）。

## 待办/后续阶段

- Zone内部再对自己剩余的矩形跑一次Building分配流程（用户明确推迟，等设计好细节再补）——
  这次新增的`internalBuildings`是`ZoneMod`显式指定的固定建筑列表，不是自动填充剩余空间，
  两者不是一回事。
- 大门资产/渲染（`ZoneGateSpec`这次只存位置数据，没有mesh字段）。

---

# building.h / building.cpp

和`Zone`结构基本一样（同样继承`Quad`），但**第十六轮迁移后`Building`和`Zone`在"怎么持有
mod"这件事上分道扬镳了**：`Zone`的mod是一个Zone独占一份（见上），`Building`的mod是**按类型
共享**的——同一个类型可能同时走显式占位、`FillRemainder`、Zone内部建筑三条路径，产出好几个
`Building`，全部指向`Map::InitBuildings()`自己的`scanners`表里同一个实例（`unordered_map<
string, BuildingMod*>`，按类型id缓存，活过整个`InitBuildings()`）。`Building`的构造函数
从`Building(BuildingFactory*, const string& buildingId)`（内部自己`CreateBuilding`一个新
实例）简化成`Building(BuildingMod* mod)`（直接接一个外部已经存在、已经跑过`Distribute()`的
实例）——`Building`不再持有`BuildingFactory*`，`~Building()`也不再调用`DestroyBuilding`，
这个mod实例的生命周期完全由`scanners`表管理，`InitBuildings()`函数末尾统一
`buildingFactory.DestroyBuilding(scanner)`一次，是唯一的销毁点（如果`Building`自己也销毁，
同类型的第二个`Building`析构时会对同一个指针重复销毁）。这和`Zone`"一个mod从一开始就只服务
一个即将落地的对象、可以放心独占销毁"的关系不是一回事，不能照抄。落地那一刻要调用
`mod->RandomAcreage()`采样一次面积（`RandomAcreage()`每次调用都是新的随机数，只应该在落地
那一刻调一次，不要在别处重复调），面积最终体现在`SetVertices`/`SetPosition`定出来的矩形
尺寸上，不需要单独存一份。`Map::InitBuildings()`落地流程（含显式占位+权重CDF两条路径）见
`Source/Core/map/map.md`。

`boundaryRoads`（四周边界Road）和`Zone`同样的字段/语义，`Map::InitBuildings()`在显式占位
和`FillRemainder`两条路径上都会填（`FillRemainder`产出的`Lot::FillResult`这次也新增了
`boundaryRoads`字段，见`geometry.md`）。

**`Building`比`Zone`多两个字段，第十五轮迁移新增，专门给Zone内部建筑用**：
- `Zone* parentZone`：只是纯粹的反向查询登记（"这个building在哪个园区里"），**不参与
  `GetRotation()`计算**——旋转转发走的还是`parentLot`那条链路。和`parentLot`同时设置，
  不是二选一：`Map::PlaceZoneInternalBuilding`把内部建筑的`parentLot`直接设成**它所属
  Zone自己的`parentLot`**（`zone->GetParentLot()`），这样`GetRotation()`的转发基准天然和
  zone一致，不需要为此单独引入一条"转发到`parentZone->GetRotation()`"的路径。
- `float relativeRotation`：叠加在`GetRotation()`结果上的附加偏移，默认0（普通顶层Building
  不受影响，等价于原来纯转发`parentLot->GetRotation()`的行为）。`SetParentLot(Lot*, float
  relativeRotation = 0.f)`扩展了一个默认参数来设置它，旧调用点(`SetParentLot(lot)`)不用改。
  `GetRotation()`现在是`(parentLot ? parentLot->GetRotation() : 0.f) + relativeRotation`。
  对应`ZoneInternalBuildingSpec::relativeRotation`（"以园区旋转为基准继续旋转"，见
  `zone_mod.md`）。
