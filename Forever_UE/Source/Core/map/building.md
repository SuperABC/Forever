# building.h / building.cpp

## 职责

`Building`：持有一个具体`BuildingMod`实例，代表一栋已经落地的building（继承`Quad`表示自己
占据的矩形）。楼体footprint（基本形状）+楼层高度是`Building`自己的字段（`bodyOffsetX/Y`/
`bodySizeX/Y`/`basements`/`layers`/`floorHeights`）；每层楼内部具体长什么样（走廊/房间隔墙/
门窗/楼梯/电梯/坡道/导航）由mod在`Layout()`里通过`AssignFloor`/`AssignRoom`/`ArrangeRow`
声明，`Building::Layout()`解析成真正的`Floor`/`Room`/`Component`数据，是本文件迁移自老工程
`E:\Projects\Forever_UE\Source\Core\map\building.cpp`的最大一块。

和`Zone`同一个模式：`Building`独占持有一个mod实例的生命周期，构造时创建、析构时
`factory->DestroyBuilding(mod)`；这次新增`rooms`/`components`两个数组，`~Building()`
也要跟着`delete`（`Room`/`Component`本身是Core分配/Core释放，只有它们各自持有的
`RoomMod*`/`ComponentMod*`才走跨DLL的factory Create/Destroy，见"跨DLL安全"一节）。

## 关键设计

- **模板数据模型（`RectParams`/`PointParams`/`WallHole`，定义在`geometry.h`）照抄老工程，
  纯数学、和UE无关**：`.layout`模板文件里的矩形/点位置都是"ratio+offset相对某个尺寸"的
  描述——`worldCoord = ratio*W_or_H + offset`，同一份模板换到不同实际楼层尺寸时套用同一个
  公式即可复用。`RectParams`(8个float，两个对角点各自的x/y两套ratio+offset)描述一个矩形，
  `PointParams`(4个float)描述一个点，`WallHole`(`unordered_map<int,vector<RectParams>>`，
  key是`FACE_DIRECTION`)描述一面墙上的门/窗开口列表。
- **`Stair`/`Elevator`/`Ramp`/`Ceiling`/`Ground`/`Corridor`/`Hatch`/`Single`/`Row`九个元素类
  +`Floor`+`BuildingLayoutLibrary`**：九个元素类都是`Quad`的子类，构造时只存一份`RectParams`，
  调`InstanciateQuad(width,height)`才按楼层实际宽高换算出绝对`Quad`(局部坐标，原点在楼体
  左下角，未旋转)。`Floor`是一层楼实例化完的完整数据(9个vector)。`BuildingLayoutLibrary`
  是全局共享的模板仓库——`Map::InitBuildings()`加载一次(`ReadTemplates(Config::GetLayouts())`)，
  传给每个`Building::Layout()`查询。**故意不叫老工程的类名`Layout`**：这个名字和
  `ZoneMod::Layout()`/`BuildingMod::Layout()`/`Zone::Layout()`/`Building::Layout()`这一整套
  "落地后摆布局"虚方法命名严重冲突，容易看错成同一个概念。
- **`InverseParams`/`InverseDirection`/`InversePoint`/`InverseWall`四个方向转换函数逐字节
  照抄老工程**（`BuildingLayoutLibrary::ReadTemplates`解析`.layout`文件时，每个元素预算好
  4个朝向存起来，运行时按`face`查表是O(1)，不用现算）：`InverseParams`把矩形ratio参数转到
  另一个朝向；`InverseDirection`把`FACE_DIRECTION`本身转到另一个朝向；`InversePoint`同
  `InverseParams`但用于4元素的点；`InverseWall`处理"门/窗在墙上的位置"这种特殊情况——某些
  旋转会翻转沿墙方向，此时要把起终点对调并做`1-ratio`镜像，不能直接套`InverseParams`。
  这4个函数设成`public static`（不是`private`，虽然只有`ReadTemplates`内部用得到）——因为
  `ParseNavigationBlock`是`building.cpp`里的匿名namespace自由函数，不是`BuildingLayoutLibrary`
  的成员，访问不了`private`静态方法。
- **`BuildingMod::Layout`签名改成`int& direction`引用参数**：`direction`对显式占位落地是
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
  默认值——这次**不做外墙/内墙材质区分**，统一一份`wallMaterial`，以后要分再加字段)；
  `AssignRoom`/`ArrangeRow`把某层某个槽位(`Single`/`Row`)实例化成真正的`Room`，按
  `(component名字,id)`分组挂到对应`Component`上。`Building::Layout()`按这些记录：①填
  `floors`(每层调`ReadFloor`)；②按`(component,id)`去重创建`Component`，逐个槽位调
  `AssignRoom`/`ArrangeRow`创建`Room`；③给每个`Room`创建导航锚点；④调
  `BuildPedestrianNavigation`构建内部导航图。
- **`.layout`模板只描述楼梯/坡道的2D位置+朝向+哪几侧有墙，不携带3D网格信息**（和老工程
  一致——`ABuildingBase::ConstructBuilding`把`Stair.Stair`/`Ramp.Ramp`两个网格路径写死在
  C++渲染代码里，不是per-mod可配置的）；这次要让mod能按楼层自定义楼梯/坡道用什么3D网格，
  所以`FloorAssetSpec::stairMeshPath`/`rampMeshPath`是`AssignFloor`的显式参数——模板名字
  只决定楼梯/坡道摆在楼体的哪个位置、多大、朝哪，不决定拿什么3D资产去画它。
- **`Hatch`不是用来挖building自己楼层的`Ground`/`Ceiling`的**——`Ceiling`/`Ground`都是照
  模板原样绘制，不做任何运行时裁剪(和老工程一致)；`Hatch`真正的用途是喂给`Map::AddHatch`
  去挖**世界地形**的洞，而且只有地下一层(离地表最近的那层basement)的`Hatch`才有意义——只有
  basement会真正凿穿室外地表(比如地下室楼梯/电梯井道贯通到室外，形成一个采光井/下沉式
  入口)，地面以上楼层之间(1楼→2楼之类)本来就完全在地表之上，`Ceiling`/`Ground`两片slab
  之间那条缝是正常的楼层间隙，不需要跟世界地形产生任何关系。这一步在`Map::
  ForwardBuildingHatches(building)`里做（取`building->GetFloor(-1)->GetHatches()`换算成
  世界坐标后调`this->AddHatch(...)`，`InitBuildings()`三段落地循环各自在
  `MergeBuildingNavigation`之后调用一次），不在`Building`内部——和roadnet隧道口完全复用
  同一套挖洞机制。
- **行人导航（`BuildPedestrianNavigation`）**：按各楼层的`pedestrianNavigation`模板解析
  node/line/connection，`upstair`/`downstair`按相邻楼层世界坐标贪心最近匹配，产出这栋
  building自己内部的导航节点/连接。**`"outside"`端点不照抄老工程"连到楼体边界最近两个角"
  的近似**，而是只在这一步实例化出它自己的世界坐标节点、收集进`BuildingNavResult::
  outsideNodes`——真正"接上道路网"这一步挪到`Map::InitBuildings()`（`Map::
  MergeBuildingNavigation`+`Map::FlushPendingBuildingRoadAccess`）：用`building->
  GetBoundaryRoad(building->GetDirection())`找到building朝向的边界`Road`，把`outside`
  节点投影到该`Road`中心线上求弧长比例`t`，判断building在`Road`哪一侧——但不立即调用
  `Map::AddRoadAccessNode`，而是记进一个pending列表，等这次`InitBuildings()`涉及的所有
  building都处理完之后，再按每条Road/每条车道各自的物理顺序（沿road弧长排序）统一断开，
  最后建一条`Connection`把两者接起来。这样做是因为同一条Road上可能有好几栋building各自
  贡献一个`outside`端点，而`Map::InitBuildings()`处理building的顺序和它们在Road上的物理
  位置无关，如果按处理顺序直接断（老实现），物理上靠后的building可能先断、抢占物理上靠前
  building还没轮到的那一段贯通线，导致断点次序错位、行人贯通线可视化上出现交叉/跳跃
  （PIE验证发现的bug，详见`map.md`"InitBuildings"一节）。修复后building内部导航图和
  道路网导航图仍然共享同一个断点，不是"两个图靠坐标凑近似"。`vehicleNavigation`这次只
  解析、不使用(留给以后车辆域真正做"车辆能进建筑内部"这个玩法时再消费)。
- **`Room`自己中心点的导航节点(`Room::SetNavigationNode`)必须登记进
  `BuildingNavResult::nodes`**——这些节点由`Building::Layout()`创建、不在`floorFixedNodes`/
  `line`锚点那条链路里，容易漏登记(照抄老工程`BuildNavigation`结尾"for (auto room : rooms)
  newNodes.push_back(room->GetNavigationNode())"这一步，漏了会导致`Map::navAnchorNodes`
  永远拿不到这些节点——内存泄漏+导航图可视化画不出来)。
- **`Room`/`Component`大幅裁剪**：老工程`Room`还挂着`ownership`/`tenancy`/`workers`/
  `storages`/`manufactures`/`parkings`/`vehicles`/`assets`等字段，全部依赖Industry/
  Populace/Society这些还没迁移的领域，这次不迁移，等对应领域迁移到了再回来加，详见
  `room.h`/`component.h`。**这次只做`Component`(组合，绑定在单个Building内部)，不做
  `Organization`(公司/组织，持有跨building的多个组合)**——`Organization`依赖的
  `Populace`/`Job`还没迁移，留到以后Society阶段。
- **`RoomMod`/`ComponentMod`极简，只有`GetType()`/`GetName()`**：`Room`/`Component`不是
  像`Zone`/`Building`那样要参与"在地图上竞争地块"的顶层concept，永远是`Building`自己在
  `Layout()`里显式创建的，不需要`Assign`/`RandomAcreage`/`GetPower`这套static注册机制。
- **跨DLL安全**：`Building::Layout()`里`new Room(...)`/`new Component(...)`都是Core编译的
  代码(building.cpp)，`~Building()`里`delete room`/`delete component`也是Core编译的代码——
  同一个模块做分配和释放，没有跨DLL问题；只有`Room`/`Component`各自持有的`RoomMod*`/
  `ComponentMod*`原始mod实例才需要走`factory->DestroyRoom/DestroyComponent(mod)`这条
  跨DLL安全的路径(委托给注册时mod自己提供的deleter函数指针，和`Building`/`Zone`同一个模式)。

## 依赖关系

- 依赖：`Source/Dependence/map/building_mod.h`(`BuildingMod`/`FloorAssetSpec`/
  `FloorLayoutSpec`)、`building_factory.h`、`Source/Dependence/map/geometry.h`
  (`RectParams`/`PointParams`/`WallHole`/`NavigationXxxTemplate`/`Quad`/`Node`/`Connection`/
  `Road`)、`Source/Core/map/room.h`/`component.h`、`Source/Dependence/common/json.h`
  (`BuildingLayoutLibrary::ReadTemplates`解析`.layout`文件)。
- 被谁依赖：`Source/Core/map/map.h`/`.cpp`(`Map::InitBuildings()`加载`buildingLayoutLibrary`+
  调用`building->Layout(...)`+合并行人导航+转发地下室`Hatch`)、
  `Source/Forever/Framework/ForeverBuildingFrameworkComponent.h`/`.cpp`(近处LOD楼层几何渲染，
  见该文件md)。

## 待办/后续阶段

- `Organization`(公司/组织)、Society领域的跨building组合撮合——依赖还没迁移的Populace/Job。
- 电梯轿厢(`Cabin`)/`Script`挂钩/`AddElevator` API——这次只做井道墙体几何，不创建任何
  "电梯轿厢"数据对象。
- 车辆导航(`vehicleNavigation`)的实例化/寻路逻辑——这次只有数据格式+编辑器UI，
  `BuildPedestrianNavigation`解析后直接丢弃。
- `Room`的furniture/pivots/storages/manufactures/parkings/vehicles/ownership/tenancy等
  字段——依赖Industry/Populace/Society，等对应领域迁移到了再回来加。
- 门的实际mesh（老工程本来就没有，纯几何缺口，窗户才有`Window.Window`网格）。
- 外墙/内墙材质区分——这次统一用一份`wallMaterial`，用户明确要求以后自己设计怎么分。
