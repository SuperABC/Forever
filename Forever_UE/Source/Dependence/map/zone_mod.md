# zone_mod.h / zone_mod.cpp

## 职责

`ZoneMod`：园区(Zone)类型的mod基类。Zone只有"显式指定矩形"一种生成方式(不参与权重/CDF随机填充)，落地后在`Layout(...)`里声明围墙/大门/出入口/内部道路/内部建筑/脚本/自定义模型。完整的引擎侧编排见`Source/Core/map/map.md`"InitZones/InitBuildings"一节。

## 放置请求机制(Zone和Building共用)

- **`LotPlacementRequest`**(`general_geometry.h`)：`{Lot* lot, int direction, float marginStart, float marginEnd, float depth}`，描述"贴着lot某条边界路、沿路方向留出两端margin、往里伸depth"的一块矩形。
- **`PlacementEmitFunc`**：`void(*)(void* context, const LotPlacementRequest&)`裸函数指针，回调体编译在调用方一侧。
- **`static void Assign(lots, emit, context)`**：子类必须实现(基类不声明)，通过`ZoneFactory::RegisterZone`以裸函数指针注册。引擎一次传入全图lot列表(按`Lot::GetFreeAcreage()`降序)，mod自己遍历、自己决定尺寸(direction/margin/depth)，想要哪块就调一次`emit`，`request.lot`必须设成对应lot。**不是虚方法**：这类查询在真正落地之前就要用，做成虚方法就得先构造一个很可能马上销毁的实例；只有落地成功才`CreateZone`一次。`BuildingMod`多出的static查询见`building_mod.md`。
- **Zone没有权重/CDF方法**，不会被喂进`Lot::FillRemainder`：Zone阶段只占mod主动要的那一块，不把地块填满；Zone先占完地，Building再用权重CDF填剩余空间。
- **mod不能直接调用Core对象的非虚成员去写它的容器**：例如mod dll直接调`Lot::AddCandidate`会让`Lot::candidates`的缓冲由mod dll的分配器分配，而`Lot`由`Forever.dll`析构，UE每个模块都覆写了`operator new/delete`，两边对不上，退出时析构`Lot`会`EXCEPTION_ACCESS_VIOLATION`。正确做法是走emit回调/static查询，由引擎自己调`lot->AddCandidate`(`Map::InitBuildings()`查到`BuildingFactory::GetPower`后自己加)。

## 关键设计

- **一个本体独占一个mod实例**：`Map::InitZones()`对每个落地成功的Zone单独`CreateZone(id)`，再调一次`mod->Layout(direction, quad, boundaryRoads)`，所以布局可以按这次落地的具体情况决定，不是整个mod类型统一一份配置。`direction`是`Assign`选中的方向；`quad`/`boundaryRoads`是纯只读输入(Zone本身是`Quad`子类)。默认空实现。
- **布局输出全是值类型**(`string`/`vector`/`unordered_map<int,int>`)，符合跨DLL安全规则：`walls`/`gates`由`Zone::GetWalls()`/`GetGates()`直接转发；`vehicleEntries`/`vehicleExits`/`pedestrianAccess`/`internalRoads`由`Map::InitZones()`读回实例化；`internalBuildings`推迟到`Map::InitBuildings()`读(此时`buildingFactory`才注册好mod)。具体消费见`Source/Core/map/zone.md`和`map.md`"Zone内部布局"。
  - `ZoneWallSpec{direction,marginStart,marginEnd,depth,depthInward,mesh,unit}`：沿`direction`参考边铺设，`depthInward`控制进深往矩形内/外量(哪个正确由围墙资产朝向决定，默认`true`)。`mesh`/`unit`与`Road::GetMesh()`/`GetUnit()`同语义，沿边重复铺设，规则同`ForeverRoadnetFrameworkComponent::BuildRoadInstances`的`tileRange`(缩放后的unit落在`[0.8,1.2]*unit`)，渲染在`ForeverZoneFrameworkComponent`，Core只透传。
  - `ZoneGateSpec`：位置部分与墙同构(围墙据此让出这段空当)，加`door`(`DoorSpec`)；`door.mesh`为空=只是空当不放门，同建筑门/房间门的约定，见`feature_spec.md`。
  - `ZoneAccessPoint{x,y,width}`：局部坐标，**原点在Zone矩形中心**(与`Lot`局部坐标原点在WEST-NORTH角不同；`ZoneInternalRoadSpec`/`ZoneInternalBuildingSpec`同此约定)。`Map::ConnectZoneAccessPoint`在`zone->GetBoundaryRoads()`里找最近的真实道路，按`ResolvePathEndAnchors`同一套点积判断近侧选车道，`BreakThroughLine`断出node，与zone侧孤立锚点建一条`Connection`(车行按`isEntry`单向，行人双向)。车行入口/出口是两个独立数组，行人只有一个数组。
  - `ZoneInternalRoadSpec{x1,y1,x2,y2,isVehicle,width}`：两端点+单一车道，**只能是"一条车辆单行道"或"一条人行道"二选一**(与入口/出口/行人分开指定的模型一致)。要双向车行用两条反向spec；行人+车行都通用两条各给一个类型，端点坐标相同会在`anchorCache`(`Map::InitZones()`局部变量，出入口和内部道路共用)里按坐标+类别合并。`Map::ConnectZoneInternalRoad`实例化成`mesh=""`/`unit=0.f`的`Road`(只连导航图，不参与`BuildRoadInstances`)，固定用side0，不经`RoadJunction`，端点位置用`ComputeLaneAnchorPosition`算。
  - `ZoneInternalBuildingSpec{type,x,y,sizeX,sizeY,relativeRotation,direction,roadIndices}`：`type`供Core构造Building；`relativeRotation`是相对zone自身rotation的附加旋转；`direction`(默认`FACE_WEST`)传给该内部建筑的`BuildingMod::Layout()`(内部建筑没有`Assign`挑的方向，由mod声明)；`roadIndices`是`FACE_DIRECTION`->`internalRoads`下标(mod执行阶段道路还没实例化，Core实例化后再解析成`Road*`)。`Map::PlaceZoneInternalBuilding`同时设`building->SetParentZone(zone)`和`SetParentLot(zone->GetParentLot(), relativeRotation)`，两者同时持有，见`Source/Core/map/building.md`。
- **`stateOwnedChance`**(默认0.2)：被政府拥有的概率，`Map::Checkin`读取。**`scriptModName`/`milestoneNames`**：园区自己的Script，非空才创建，进出事件只发给它，见`map.md`"实体脚本"。
- **自定义模型和树**：`AddDecoration(DecorationSpec)`在`Layout()`里调用；园区没有footprint，`position`是相对园区quad(原点西北角，+X东+Y南)的`PointParams`，按"朝北"(大门在北)定义，Core按大门方向用`InversePoint`旋转，yaw叠加`FaceYawDegrees`和园区自身旋转(`Zone::NorthPointToWorld`/`NorthYawToWorld`)，常驻显示(园区没有LOD)。`LayoutTrees(direction, quad, boundaryRoads)`(默认空)由`Map::InitVegetation`调一次，用`AddTreeSingle/Line/Area`提交请求；坐标同样按朝北写，mod只写一份就适配四个朝向(建筑的`AddTreeArea`则不做朝向旋转)。
- 带存档钩子，默认不存任何东西，见`common/mod_state.md`。
- `ResidenceZone`(`Source/Basic`)是用这些字段搭的围墙+大门+内部道路测试场景：大门开口两端内侧各放一个急救包(`ZONE_KIT_*`)；沿四面栅栏内侧`ZONE_TREE_INSET`处每`ZONE_TREE_SPACING`种一棵Mossy_Tree，西/东边含两个角，南/北边各收一个间距避开角，北边(大门边)分两段、大门开口两侧各让出半个间距(与行道树避让开口同规则)。

## 依赖关系

- 依赖：`map/general_geometry.h`、`map/feature_spec.h`、`common/mod_state.h`。
- 被谁依赖：`zone_factory.h`、`Source/Core/map/zone.h`/`map.h`、`Source/Basic/map/zone_basic.*`、`Forever_Mod/Empty`的`EmptyZone`、`Source/Forever/Framework/ForeverZoneFrameworkComponent`(读`Zone::GetWalls()`渲染围墙)。

## 待办/后续阶段

- Zone内部对自己的剩余空间再跑一次Building分配流程(现有`internalBuildings`是mod显式指定的固定列表，不自动填充)。
- 围墙"贴合不满整数unit时"的退化兜底渲染(见`ForeverZoneFrameworkComponent.md`)。
- Building自己的出入口逻辑(`ConnectZoneAccessPoint`只服务Zone)。
