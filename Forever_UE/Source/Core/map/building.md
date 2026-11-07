# building.h / building.cpp

## 职责

`Building`：持有一个具体`BuildingMod`实例，代表一栋已经落地的building（继承`Quad`表示自己
占据的矩形）。楼体footprint（基本形状）+楼层高度是`Building`自己的字段（`bodyOffsetX/Y`/
`bodySizeX/Y`/`basements`/`layers`/`floorHeights`）；每层楼内部具体长什么样（走廊/房间隔墙/
门窗/楼梯/电梯/坡道/导航）由mod在`Layout()`里通过`AssignFloor`/`AssignRoom`/`ArrangeRow`
声明，`Building::Layout()`解析成真正的`Floor`/`Room`/`Component`数据。

和`Zone`同一个模式：`Building`独占持有一个mod实例的生命周期，构造时创建、析构时
`factory->DestroyBuilding(mod)`；`rooms`/`components`两个数组在`~Building()`里跟着`delete`
（`Room`/`Component`本身是Core分配/Core释放，只有它们各自持有的`RoomMod*`/`ComponentMod*`
才走跨DLL的factory Create/Destroy，见"跨DLL安全"一节）。

## 关键设计

- **自定义模型(`DecorationInstance`/`GetDecorations()`，结构体定义在feature_spec.h，和园区共用)**——mod在`Layout()`里用
  `BuildingMod::AddDecoration(DecorationSpec)`指定放置的StaticMesh，`Building::Layout()`在
  `mod->Layout()`、算完body尺寸之后按**最终**`direction`换算成世界摆位：
  `BuildingLayoutLibrary::InversePoint(spec.position, face)`(和.layout模板同一个函数，face=-1按
  `FACE_NORTH`原样处理)得到楼体局部点，`LocalToWorld`转世界坐标；yaw=`GetRotation()`+朝向偏移
  (face0=270°/face1=90°/face2=0/face3=180°，和InversePoint的点映射是同一个旋转)+`spec.yaw`。
  所以模型和楼层布局一样跟着建筑朝向转。UE层只在近处LOD生成，见`BuildingElement.md`。
- **允许0层建筑**——`Layout()`里`layers = max(0, mod->layers)`。
  `basements`和`layers`都为0时整栋没有任何`Floor`，`GetFloor(level)`恒返回nullptr，导航/门/
  hatch转发这些按`floors`或`mod->floors`遍历的循环自然什么都不做。给`ParkBuilding`这类只有植被、
  不放楼体的"建筑"用(植被走`BuildingMod::AddTreeArea`，见`vegetation.md`)；UE层
  `ForeverBuildingFrameworkComponent`直接跳过这种building，不生成`ABuildingElement`。
- **模板数据模型（`RectParams`/`PointParams`/`WallHole`，定义在`general_geometry.h`）**：`.layout`
  模板文件里的矩形/点位置都是"ratio+offset相对某个尺寸"的描述——`worldCoord = ratio*W_or_H +
  offset`，同一份模板换到不同实际楼层尺寸时套用同一个公式即可复用。`RectParams`(8个float，
  两个对角点各自的x/y两套ratio+offset)描述一个矩形，`PointParams`(4个float)描述一个点，
  `WallHole`(`unordered_map<int,vector<RectParams>>`，key是`FACE_DIRECTION`)描述一面墙上的
  门/窗开口列表。
- **`Stair`/`Elevator`/`Ramp`/`Ceiling`/`Ground`/`Corridor`/`Hatch`/`Single`/`Row`九个元素类
  +`Floor`+`BuildingLayoutLibrary`**：九个元素类都是`Quad`的子类，构造时只存一份`RectParams`，
  调`InstanciateQuad(width,height)`才按楼层实际宽高换算出绝对`Quad`(局部坐标，原点在楼体
  左下角，未旋转)。`Floor`是一层楼实例化完的完整数据(9个vector)。`BuildingLayoutLibrary`
  是全局共享的模板仓库——`Map::InitBuildings()`加载一次(`ReadTemplates(Config::GetLayouts())`)，
  传给每个`Building::Layout()`查询。这个类故意不叫`Layout`：这个名字会和
  `ZoneMod::Layout()`/`BuildingMod::Layout()`/`Zone::Layout()`/`Building::Layout()`这一整套
  "落地后摆布局"虚方法命名冲突，容易看错成同一个概念。
- **`InverseParams`/`InverseDirection`/`InversePoint`/`InverseWall`四个方向转换函数**
  （`BuildingLayoutLibrary::ReadTemplates`解析`.layout`文件时，每个元素预算好4个朝向存起来，
  运行时按`face`查表是O(1)，不用现算）：`InverseParams`把矩形ratio参数转到另一个朝向；
  `InverseDirection`把`FACE_DIRECTION`本身转到另一个朝向；`InversePoint`同`InverseParams`但
  用于4元素的点；`InverseWall`处理"门/窗在墙上的位置"这种特殊情况——某些旋转会翻转沿墙方向，
  此时要把起终点对调并做`1-ratio`镜像，不能直接套`InverseParams`。这4个函数设成`public
  static`（不是`private`）——因为`ParseNavigationBlock`是`building.cpp`里的匿名namespace自由
  函数，不是`BuildingLayoutLibrary`的成员，访问不了`private`静态方法。
- **`BuildingMod::Layout`签名是`int& direction`引用参数**：`direction`对显式占位落地是
  `Assign`选中的真实方向；对权重CDF/`FillRemainder`落地传进来是`-1`(没有方向概念)。如果这个
  mod的楼层内部布局需要按方向摆(`AssignFloor`的`face`参数)，收到`-1`时应该从`boundaryRoads`
  里挑一个非空的key、`GetRandom(...)`随机选一个回写进`direction`，让这栋building最终有一个
  确定的、有真实边界路的方向——building内部行人导航的`"outside"`端点要靠这个方向找到
  `GetBoundaryRoad(direction)`该连去哪条路(见下面"行人导航"一节)，没有这一步`outside`端点
  就永远连不上路网。不需要按方向摆的mod(比如`footprint`/楼层数据是固定值的测试类型)不用管
  这个参数，保持传入值不变没有副作用。`Building::Layout()`调用`mod->Layout(...)`之后把
  (可能被mod改写过的)最终值存进`this->direction`，`GetDirection()`返回这个值。
- **`AssignFloor`/`AssignFloors`/`AssignRoom`/`ArrangeRow`四个API**（声明在`building_mod.h`，
  记录进`BuildingMod::floors`/`singles`/`rows`三个容器，纯值类型增长，mod自己编译的代码里
  发生，不涉及跨DLL问题）：`AssignFloor(level, templateName, face, assets)`声明第`level`层
  (0-based，0=1楼，负数=地下室)用哪个`.layout`模板+朝向+资产(`FloorAssetSpec`：墙体材质+
  地板材质+天花板材质+楼梯网格+坡道网格，留空用`ForeverBuildingFrameworkComponent`的组件级
  默认值——不区分外墙/内墙材质，统一一份`wallMaterial`)；
  `AssignRoom`/`ArrangeRow`把某层某个槽位(`Single`/`Row`)实例化成真正的`Room`，按
  `(component名字,id)`分组挂到对应`Component`上。`Building::Layout()`按这些记录：①填
  `floors`(每层调`ReadFloor`)；②按`(component,id)`去重创建`Component`，逐个槽位调
  `AssignRoom`/`ArrangeRow`创建`Room`；③给每个`Room`创建导航锚点；④调
  `BuildPedestrianNavigation`构建内部导航图。
- **模板名必须存在**：`Building::Layout()`对`mod->floors`里每个非空的`templateName`先用`BuildingLayoutLibrary::HasTemplate`校验，找不到直接抛`InvalidArgumentException`(开局即报错)；读档前`Config::FindMissingLayouts`对存档manifest里的模板名做同样的检查，两边口径一致，不会出现能开局、存了却读不回来的情况。空模板名表示这层故意不用模板，不校验。
- **`.layout`模板只描述楼梯/坡道的2D位置+朝向+哪几侧有墙，不携带3D网格信息**——
  楼梯/坡道的实际3D网格由`FloorAssetSpec::stairMeshPath`/`rampMeshPath`作为`AssignFloor`
  的显式参数指定，模板名字只决定楼梯/坡道摆在楼体的哪个位置、多大、朝哪，不决定拿什么3D
  资产去画它。
- **电梯轿厢（`ElevatorCabinSpec`/`BuildingMod::AssignElevatorCabin`）**：
  `ElevatorCabinSpec{shaftIndex, minFloor, maxFloor, cabinMeshPath, scriptModName, milestoneNames}`，mod显式声明"第
  `shaftIndex`口井道(对应每层模板`elevators`数组里第几个，0-based)有一台轿厢，在
  `minFloor`~`maxFloor`之间，用这个Script"，是mod显式配置，不是从`Elevator`几何反推出来的。同一口井可以声明多台
  (楼层范围不能重叠)。`Layout()`把spec拷进快照`cabins`(`GetCabins()`)，`BuildCabins(Map&)`按它逐条创建Core的
  `Cabin`(电梯实体：Script、交互选项、所在楼层、行程状态)，登记进`cabinEntities`(`GetCabinEntities()`，析构时释放)，
  详见`cabin.md`。
- **楼层先占位再填充**：`Layout()`先按层号(`-basements..layers-1`)建出全部空`Floor`，再用`mod->floors`的模板逐层
  `ReadFloor`填充——mod漏配某一层时`GetFloor(level)`返回"没有任何元素"的空`Floor`而不是nullptr，渲染层不用特殊处理。
  `floorHeights`长度和`basements+layers`对不上时整体按`kDefaultFloorHeight`(0.4)补齐。
- **`ArrangeRow`切分房间数**：`num = row面积/acreage`，`num<1`或小数部分>=0.5就进一(四舍五入)，至少1间，沿`row`长边等分；
  `acreage<=0`直接不建。
- **`BuildCabins`的校验**：楼层范围非法(min>max或超出楼层)、或和同一口井里已建的轿厢楼层范围重叠，打warning并跳过该条spec。
- **`Hatch`不是用来挖building自己楼层的`Ground`/`Ceiling`的**——`Ceiling`/`Ground`都是照
  模板原样绘制，不做任何运行时裁剪；`Hatch`真正的用途是喂给`Map::AddHatch`去挖**世界地形**
  的洞，而且只有地下一层(离地表最近的那层basement)的`Hatch`才有意义——只有basement会真正
  凿穿室外地表(比如地下室楼梯/电梯井道贯通到室外，形成一个采光井/下沉式入口)，地面以上
  楼层之间(1楼→2楼之类)本来就完全在地表之上，`Ceiling`/`Ground`两片slab之间那条缝是正常的
  楼层间隙，不需要跟世界地形产生任何关系。这一步在`Map::ForwardBuildingHatches(building)`
  里做（取`building->GetFloor(-1)->GetHatches()`换算成世界坐标后调`this->AddHatch(...)`，
  `InitBuildings()`三段落地循环各自在`MergeBuildingNavigation`之后调用一次），不在`Building`
  内部——和roadnet隧道口完全复用同一套挖洞机制。
- **行人导航（`BuildPedestrianNavigation`）**：按各楼层的`pedestrianNavigation`模板解析
  node/line/connection，`upstair`/`downstair`按相邻楼层世界坐标贪心最近匹配，产出这栋
  building自己内部的导航节点/连接。`"outside"`端点只在这一步实例化出它自己的世界坐标节点、
  收集进`BuildingNavResult::outsideNodes`——真正"接上道路网"这一步挪到`Map::InitBuildings()`
  （`Map::MergeBuildingNavigation`+`Map::FlushPendingBuildingRoadAccess`）：用`building->
  GetBoundaryRoad(building->GetDirection())`找到building朝向的边界`Road`，把`outside`
  节点投影到该`Road`中心线上求弧长比例`t`，判断building在`Road`哪一侧——但不立即调用
  `Map::AddRoadAccessNode`，而是记进一个pending列表，等这次`InitBuildings()`涉及的所有
  building都处理完之后，再按每条Road/每条车道各自的物理顺序（沿road弧长排序）统一断开，
  最后建一条`Connection`把两者接起来。这样做是因为同一条Road上可能有好几栋building各自
  贡献一个`outside`端点，而`Map::InitBuildings()`处理building的顺序和它们在Road上的物理
  位置无关，如果按处理顺序直接断，物理上靠后的building可能先断、抢占物理上靠前building
  还没轮到的那一段贯通线，导致断点次序错位、行人贯通线可视化上出现交叉/跳跃（详见`map.md`
  "InitBuildings"一节）。修复后building内部导航图和道路网导航图共享同一个断点，不是"两个图
  靠坐标凑近似"。
- **车辆导航（`BuildVehicleNavigation`）**：和`BuildPedestrianNavigation`共用同一份算法实现
  `BuildNavigationGraph(library, direction, isVehicle, newNodes, newConnections,
  outsideNodes, registerRoomNodes)`——node/line/connection解析逻辑本来就是纯数据驱动、
  和"行人"这个语义无关，`isVehicle`只决定读`library.GetPedestrianNavigation`还是
  `GetVehicleNavigation`。`BuildVehicleNavigation`写进`BuildingNavResult`的平行字段
  `vehicleNodes`/`vehicleConnections`/`vehicleEntranceNodes`/`vehicleExitNodes`，且必须传
  `registerRoomNodes=false`——`Room::GetNavigationNode()`的登记只能做一次(已经在
  `BuildPedestrianNavigation`那次调用里做过)，车辆build如果重复登记，`Map::navAnchorNodes`
  会出现同一个`Node*`两次，`~Map()`清理时double free。车辆接路网的方向（单向，车道本来就有方向性）由layout里的`entrance`/`exit`端点类型直接决定，见下面"车辆导航的显式入口/出口端点"一节，详见
  `map.md`"InitBuildings"一节`Map::MergeBuildingNavigation`的车辆分支。
- **`Room`自己中心点的导航节点(`Room::SetNavigationNode`)必须登记进
  `BuildingNavResult::nodes`**——这些节点由`Building::Layout()`创建、不在`floorFixedNodes`/
  `line`锚点那条链路里，容易漏登记，漏了会导致`Map::navAnchorNodes`永远拿不到这些节点
  （内存泄漏+导航图可视化画不出来）。
- **`Component`只绑定在单个Building内部**，跨building的撮合由`Organization`负责，见`component.md`；`Room`的字段见`room.md`。
- **`GetAddress()`**：有`parentZone`时`parentZone->GetAddress() + " " + GetName()`，否则
  `parentLot->GetAddress() + " " + GetName()`，和`Map::LocateBuilding`的两种解析格式
  （`"<road> <index> <buildingName>"`/`"<road> <index> <zoneName> <buildingName>"`）
  一一对应。`Lot::GetAddress()`/`Zone::GetAddress()`/`Building::GetAddress()`/
  `Room::GetAddress()`/`Map::LocateRoom()`是配套的完整链条，详见`map.md`"寻址"一节。
- **`RoomMod`/`ComponentMod`极简，只有`GetType()`/`GetName()`**：`Room`/`Component`不是
  像`Zone`/`Building`那样要参与"在地图上竞争地块"的顶层concept，永远是`Building`自己在
  `Layout()`里显式创建的，不需要`Assign`/`RandomAcreage`/`GetPower`这套static注册机制。
- **跨DLL安全**：`Building::Layout()`里`new Room(...)`/`new Component(...)`都是Core编译的
  代码(building.cpp)，`~Building()`里`delete room`/`delete component`也是Core编译的代码——
  同一个模块做分配和释放，没有跨DLL问题；只有`Room`/`Component`各自持有的`RoomMod*`/
  `ComponentMod*`原始mod实例才需要走`factory->DestroyRoom/DestroyComponent(mod)`这条
  跨DLL安全的路径(委托给注册时mod自己提供的deleter函数指针，和`Building`/`Zone`同一个模式)。

### 建筑门

`Corridor`/`Single`/`Row`新增`DoorTags`(和`WallHole`同形状的平行数据，下标按
`FACE_DIRECTION`、每个方向的vector和对应`WallHole[dir]`逐元素对齐)，`AddDoor`多了一个
可选的`tag`参数——这是layout里的门洞标签，没有标签的洞恒为空字符串，不可选中成真正的门，
见`Core/map/door.h`"`ResolveDoorSpec`"一节。`Building`新增`doorEntities`+`BuildDoors(Map&)`:
遍历每层`Floor::GetCorridors()`的每个门洞和每个`room`的每个门洞，分别用
`mod->doorSpecs`/`room->GetMod()->doorSpecs`做`ResolveDoorSpec`，需要放门就调
`map.CreateDoor(...)`——corridor的门登记进`Building`自己的`doorEntities`，room的门登记
进对应`Room`自己的`doorEntities`(`Room::AddDoorEntity`)，不是都归`Building`持有。几何
换算照抄`Forever/Element/BuildingElement.cpp::BuildWallsForElement`的`collect`公式
(`x1=p[0]*horizSpan+p[1]`等)，只是这次是Core端复刻(算地图单位，不是UE单位)，详见
`door.md`。由`Map::InitBuildings()`在`building->Layout(...)`的**三个**调用点(显式占位/
园区内部建筑/`FillRemainder`)之后各调一次`building->BuildDoors(*this)`。
门洞的`y1/y2`是"从天花板往下量"的距离（和渲染层挖墙洞的约定一致），所以门底边离地板的高度是
`floorHeight-max(y1,y2)`，不是`min(y1,y2)`（写反过，门会悬在半空）；门的yaw=墙外法线朝向(`normalYaw`)+
`building->GetRotation()`。

- **mod声明式字段快照**：`Layout()`时把`mod->floors`/`cabins`/`stationMod`拷进`floorSpecs`/`cabins`/`stationMod`，
  另有`scriptModName`/`milestoneNames`(非空才有`Script`，由`Map::BuildBuildingScripts`创建、析构时释放)，
  UE层(`BuildingElement`读楼层材质/buildShell、电梯轿厢)、Traffic(`InitStations`)、存档manifest(布局模板名)都读
  `GetFloorSpec`/`GetFloorSpecs`/`GetCabins`/`GetStationMod`，不直接读mod——读档时mod是重新创建的空实例、`Layout`
  不会再跑，见`map_save.md`。以后新增"运行时要读的mod公共字段"也照这个做法加快照。

## 依赖关系

- 依赖：`Source/Dependence/map/building_mod.h`(`BuildingMod`/`FloorAssetSpec`/
  `FloorLayoutSpec`)、`building_factory.h`、`Source/Dependence/map/general_geometry.h`
  (`RectParams`/`PointParams`/`WallHole`/`NavigationXxxTemplate`/`Quad`/`Node`/`Connection`/
  `Road`)、`Source/Core/map/room.h`/`component.h`、`Source/Dependence/common/json.h`
  (`BuildingLayoutLibrary::ReadTemplates`解析`.layout`文件)。
- 被谁依赖：`Source/Core/map/map.h`/`.cpp`(`Map::InitBuildings()`加载`buildingLayoutLibrary`+
  调用`building->Layout(...)`+合并行人导航+转发地下室`Hatch`+`BuildDoors`)、
  `Source/Core/map/door.h`(`BuildDoors`调`Map::CreateDoor`)、
  `Source/Forever/Framework/ForeverBuildingFrameworkComponent.h`/`.cpp`(近处LOD楼层几何渲染，
  见该文件md)、`Source/Forever/Element/DoorComponent.h`(读`Door`几何/外观生成门组件)。

## 待办/后续阶段

- 电梯：开关门动画、载客的NPC调度还没做；呼叫/选层/行程/Script已经做了，见`cabin.md`。
- 门/窗的实际mesh——门本来就没有；窗户资产本身有问题，已删掉窗户显示逻辑，现在门/窗都只是
  纯几何缺口，等以后有能用的窗户资产再加回来。
- 外墙/内墙材质区分——目前统一用一份`wallMaterial`，以后再按需要区分。

## 房间朝向
`Room::direction`在`AssignRoom`/`ArrangeRow`里取自烘焙过的single/row朝向(`InverseDirection(layoutDir, face)`)，即建筑局部坐标系下的房间正面方向；房间局部坐标系和换算见`room.md`。

## 车辆导航的显式入口/出口端点（entrance / exit）

`vehicleNavigation`里用两个端点类型标记接路网的node，**只在车行导航里有效**（行人导航里出现会被忽略并打一条
debugf警告）。车行导航**不再支持`outside`**（出现会被忽略并打警告；`outside`只用于行人导航）：

- `{ "type": "exit" }`：另一端的node是出口，接路网后连接方向`node -> 路上access node`。
- `{ "type": "entrance" }`：另一端的node是入口，接路网后连接方向`路上access node -> node`。

和`outside`一样不生成Connection，只是标记；一个node可以同时连`entrance`和`exit`（各产生一个接路网的开口，落在
同一个t上时`BreakThroughLine`会复用同一个access node）。解析在`Building::BuildNavigationGraph`，结果存在
`BuildingNavResult::vehicleEntranceNodes/vehicleExitNodes`，`Map::MergeBuildingNavigation`直接按清单入
`pendingBuildingRoadAccess`，不再看度数。
`Source/layout_editor.html`的"层外连接"里车行导航模式多了"车辆入口/车辆出口"两项。

## 车行导航连线的通行方向（forward / backward）

`vehicleNavigation`里每条`connections`和每条`lines`可选带两个布尔字段：`"forward"`（沿begin->end通行）和
`"backward"`（沿end->begin通行），都为true就是双向。缺省值`forward=true, backward=false`，等价于旧layout的
行为（只顺向）。行人导航恒定双向，忽略这两个字段。`NavigationConnectionTemplate/NavigationLineTemplate`在
`Dependence/map/general_geometry.h`，解析在`ParseNavigationBlock`，展开在`BuildNavigationGraph`的
`addConnection`（line投影连接按模板begin/end确定方向）。**上下楼(upstair/downstair)匹配出来的连线车行导航恒定
双向**，entrance/exit/outside这类标记端点不受影响。`layout_editor.html`在车行导航模式的连接/路径面板里有
"顺/逆"两个勾选，画布上用箭头显示方向。
