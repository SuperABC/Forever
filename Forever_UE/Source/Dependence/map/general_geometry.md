# general_geometry.h / general_geometry.cpp

纯数学，无windows/UE类型依赖。

## 职责

提供导航图节点/连接、`Lot`空闲区域裁剪/填充两组能力：

- **`Node`/`Connection`/`Intersection`/`Road`**：导航图的点和边。`Connection`支持贝塞尔曲线
  控制点（`AddControls`），`GetPoint(f)`/`GetTangent(f, ...)`按**弧长比例**（不是Bezier多项式
  参数）取点和切线，即`f=0.5`对应曲线物理中点，供道路/人行道渲染与寻路采样使用。
- **`Quad`/`Lot`**：`Quad`是纯矩形（中心+尺寸+面积），`Lot`在`Quad`基础上加了旋转角度、地块
  类型、边界`Road`映射、地址编号、自由子地块池（`freeLots`）和围绕它的裁剪/填充方法，见下
  "关键设计"。`SetPosition(n1, n2, n3[, n4], margin)`可以直接用连续几个角点+内缩边距反推出
  矩形的中心/尺寸/旋转，是从"三个或四个已知角点"生成建筑/房间轮廓的标准做法。中心偏移=(两侧边距差)/2，按原始边长归一化(不能除缩小后的尺寸，否则两侧边距不等时偏移被放大，lot边缘离路差一小段)。三点/四点构造
  函数都有一个可选的`boundary`参数（`const std::unordered_map<int, Road*>&`，默认空），
  构造时直接按`FACE_DIRECTION`登记边界`Road`——等价于构造完再逐个调`SetBoundaryRoad`，只是
  不用在调用方那边另开一个`pair`/`map`和`Lot`并排存（`RoadnetMod::lots`就是这么用成
  `vector<Lot>`的，见`roadnet_mod.md`）。指针有效性由调用方保证，语义和`SetBoundaryRoad`
  完全一致。

## 关键设计

- **`Connection`按值语义管理其端点/控制点**——构造函数、拷贝构造、赋值运算符里都会
  `new Node(...)`拷贝一份，析构时对应`delete`，不共享指针；这是因为`Node`本身很轻量（几个
  float+id），按值复制比引入引用计数更简单，唯一的例外是`Node::count`静态自增id计数器，拷贝
  构造/赋值时会同步推高`count`（保证之后新建的`Node`不会撞见已存在的旧id）。
- **弧长参数化用惰性缓存**——`Connection::GetPoint`/`GetTangent`如果有控制点（真正的Bezier
  曲线，不是直线），会在**首次调用时**采样128个点建立弧长查找表（`arcLengthCache`），后续
  调用直接二分查表，`AddControls`会清空缓存强制重新采样。
- **不提供"记录导航图失效/新增连接"的通用机制**——地块之间的连通性直接靠"分割线本身就是
  一条真正的`Road`"来体现（见下`SplitWithPath`），不需要额外维护一份连接失效表。
- **`Lot`的自由子地块池只有两层**：一个顶层`Lot`（由`Roadnet`持有，构造后不会再被按值复制）
  持有若干`freeLots`（也是`Lot`类型，复用同一套矩形/边界Road能力），子地块不会再有自己的
  子地块——所有裁剪/填充操作都直接在顶层`Lot`的`freeLots`数组上原地替换元素，不递归下钻。
  因此`Lot`不需要处理拷贝语义：现有代码里所有"按值复制`Lot`"的地方（`RoadnetMod::lots`收集、
  `Roadnet::DistributeRoadnet`深拷贝）都发生在`freeLots`填充之前，只需要给`Lot`补一个会
  释放`freeLots`的析构函数，不需要自定义拷贝构造/赋值。
- **`Lot::SplitWithPath(splitAlongX, splitCoordinate, spec)`是唯一的几何裁剪原语**：把当前
  矩形沿局部坐标系（原点在WEST-NORTH角：局部x=0是WEST、x=sizeX是EAST，局部y=0是NORTH、
  y=sizeY是SOUTH——这是`GetPosition`/`GetVertex`已有的约定）某个坐标切成"近端(lower)/1单位
  宽小路/远端(upper)"三段，只返回lower/upper两个新`Lot`（小路本身不是可用地块，只作为
  `Road*`单独返回，不会自己保留一份——由调用方决定归属，见下）。新建的小路`Road`构造完
  立刻`SetPathRoad(true)`标记自己——是不是小路这个分类信息记在`Road`自己身上，任何持有
  这个`Road*`的调用方直接问它自己就够了，不需要另外查一份外部列表。
  **如果this在切割线两个端面方向都没有边界Road（原边界，或更早一刀生成、如今仍是某自由子块
  边界的小路Road），直接拒绝这次切割**，返回`{nullptr,nullptr,nullptr}`——不产生两端都不
  连接任何路网的孤岛小路。`RequestPlacement`（显式矩形占位，通过最多3次`SplitWithPath`调用
  实现，**固定顺序：先沿路方向(frontage轴)裁marginStart端、marginEnd端，最后裁深度方向**——沿路切割线两端落在请求方向的路和它对面的边上，请求方向必有路，所以一定连通；沿路切过之后新小路又成了深度那一刀的端点路。居中放是两刀沿路，贴角只有一刀；**每一侧切完要给剩下的地留出一条小路加一块不小于`MIN_LOT_EXTENT`(2)的地，该侧空隙不到 小路宽度+2(默认3个单位)就不切，目标直接拉伸到那条边**，所以`marginStart/marginEnd/depth`给的沿路宽度和进深是最小值，实际占地偶尔会更大，调用方的`Layout()`必须按`Quad`的实际尺寸排布；自由地沿路宽度放不下请求的宽度则拒绝；**深度那一刀：背后(远离请求方向的一侧)本来就接着对侧的路时(放射臂)，目标和剩余那块各有各的路，用不带小路的`Lot::SplitPlain`直接一分为二，不加深度小路；背后没路才用`SplitWithPath`给剩余地连路，连不通(两个端点边都没路被拒绝)则按请求的深度普通一分为二、剩下的地块丢弃。这一刀的任何结果都不算请求失败**，园区只要请求方向有路就能通达）和`FillRemainder`（采样填满+合并+递归二分，见下
  "FillRemainder"一节）都建立在这个原语之上，具体分配流程/可达性规则见map.md。这两个函数都是
  RoadnetMod初始化的那个顶层`Lot`自己的成员函数（在`freeLots`池里的某个子块上调用
  `SplitWithPath`，`this`永远是顶层`Lot`本身），每切出一条小路就直接`push_back`一条
  `PathRoadLink`进`this->pathRoadLinks`——**小路的归属和生命周期都落在这个顶层`Lot`身上**，
  不需要`Map`另开一份列表重复持有，析构顶层`Lot`时`~Lot()`一并`delete`每条link的`.road`。
  `GetPathRoadLinks()`只读枚举这个列表；`GetPathRoads()`是从它按值筛出`.road`的便捷视图，
  `Map::GetPathRoads()`遍历所有顶层`Lot`把各自的这个视图汇总返回，供Forever层渲染小路用。
- **`PathRoadLink`携带两端各自连到哪条Road、哪个弧长比例t（`endRoad1/endT1`对应切割线
  NORTH/WEST那一端，`endRoad2/endT2`对应SOUTH/EAST那一端，和`pathRoad`自己的Start/End
  一一对应），是Core层`Map::ConnectPathRoad`把小路接入`vehicleNavGraph`/`pedestrianNavGraph`
  唯一需要的定位信息**。`endT`按Start->End直线投影近似算（`ProjectT`），和`RoadOpening.t`
  用的是同一次计算结果，不重复算两遍。`ConnectPathRoad`具体怎么用这两组信息断线/建锚点，见
  `Source/Core/map/map.md`"ConnectPathRoad"一节——这部分逻辑完全在Core层，`general_geometry.cpp`
  自己不触碰`vehicleNavGraph`/`throughLines`等Map内部结构，只负责把定位信息透传出去。
- **`FillRemainder`分三步，只在`general_geometry.cpp`内部用（不进头文件）**：
  1. **采样填满**（`SampleFillElements`）：按`candidates`权重CDF反复抽类型、取
     `randomAcreage`目标面积，累加直到达到当前这个freeLots条目的总面积；只有"抽到但剩余
     空间连这个类型的下限都放不下"才计入尝试次数(上限16，`MAX_ALLOCATION_ATTEMPTS`)，抽到
     能放的类型不计入尝试次数——保证"填满"优先于"次数"。还差一点凑不满时（通常是尝试次数
     耗尽），补一个内部标记`FILL_EMPTY_TYPE`的空地叶子把总面积账目补齐，不会有游离在任何
     叶子之外的面积。
     放完一个元素后若剩余面积小于所有候选类型的最小面积（谁也放不下），先看能否对半分：剩余面积的一半不小于该类型下限就把本次采样减成一半，另一半留给下一次采样（如范围8000-16000、剩20000、抽到15000，改成10000+10000）；分不开才把整块余量并进该元素（可略超它的上限），不留碎空地。
  2. **二分合并**（`BuildFillMergeTree`）：按面积降序排序，反复把数组末尾（最小）两个元素
     合并成一个新内部节点（面积=两者之和），插入排序塞回数组维持降序，直到只剩1或2个顶层
     节点。
  3. **递归安置**（`PlaceFillMergeNode`）：把当前region按左右子树的面积比例递归二分。
     每一刀都是真实的`SplitWithPath`（带宽度的小路），会失败——优先按region较长边选轴，
     切不动就换另一条轴再试一次，两条轴都切不动就说明这个节点没法再细分，把region整个
     让给面积更大的子树（另一支连同它的采样结果一起作废，不产生`FillResult`），这是唯一
     的退化点，只在几何确实分不出两段合法矩形时才触发。
- **旋转的传递方式**：`RequestPlacement`/`FillRemainder`返回的`Quad`/`FillResult::footprint`
  本身不带旋转（`Quad`没有旋转字段），但`freeLots`池里所有子块都严格继承同一个顶层`Lot`的
  `rotation`（`SplitWithPath`产出的近端/远端两段都用`this->rotation`构造），所以调用方
  不需要额外传递——`Zone::GetRotation()`/`Building::GetRotation()`直接转发
  `parentLot->GetRotation()`就是同一个值，不需要在`Zone`/`Building`自己身上再存一份（这两个
  类不自己存`rotation`字段，见`zone.md`）。
- **`RequestPlacement`/`FillResult`要保留边界Road信息**：两者内部真正裁剪出来的`Lot`
  （`working`/`region`）在返回前都会被`delete`，如果只把裸的`Quad`（仅pos+size）透传给
  调用方，"这块地实际靠着哪些边界Road"这份信息就会跟着丢失——`RequestPlacement`有一个可选
  输出参数`std::unordered_map<int, Road*>* outBoundaryRoads`（默认`nullptr`，不传行为
  不变），非空时在`delete working`之前把`working->GetBoundaryRoads()`拷贝进去；
  `FillResult`有`boundaryRoads`字段，`PlaceFillMergeNode`的leaf分支在`delete region`之前
  同样拷一份。这是`Zone`/`Building`记录"自己四周靠着哪些道路"（`boundaryRoads`字段，见
  `zone.md`/`building.md`）的唯一数据来源。

- **`FindAdaptivePlacement`必须是纯查询，真正的裁剪/搬运必须在Core侧做**（公共交通车站
  功能）：`Lot`的非虚成员函数（包括`RequestPlacement`/`SplitWithPath`/`GetFreeLots()`的
  lazy-init分支）都定义在`Dependence.lib`里，这个静态库被`Basic.dll`（以及`Forever_Mod/*`
  mod DLL）和`UnrealEditor-Forever.dll`（经由`Core.lib`）分别独立静态链接各一份——同一份
  源码，两份编译产物。`UnrealEditor-Forever.dll`是真正的UE模块，`operator new`/`operator
  delete`被`HAL/PerModuleInline.inl`接到`FMemory`；`Basic.dll`不是UE模块，走普通CRT分配器。
  `freeLots`/`pathRoadLinks`这类`std::vector`的buffer一旦在其中一个模块的分配器下诞生，
  后续任何会导致它增长/收缩的操作（`push_back`触发的reallocate、`erase`、析构）如果恰好
  执行的是另一个模块的那份拷贝，就会用错误的分配器free/realloc这块内存——heap损坏，
  `STATUS_HEAP_CORRUPTION`（`0xc0000374`），现象是PIE一进场景`UnrealEditor.exe`直接整体
  退出，不走UE自己的崩溃报告流程（这类corruption由`ntdll`的`__fastfail`路径上报，绕过SEH，
  不会生成`Saved/Crashes/*`，只能从Windows事件日志的`Application`日志、错误模块
  `ntdll.dll`、异常码`0xc0000374`反查）。修正方式：`Lot::FindAdaptivePlacement`是`const`
  成员函数，只用`PeekFreeLots()`（`freeLotsInitialized`为`false`时直接返回空vector，不
  触发lazy-init；已初始化时按值拷贝一份，拷贝本身的buffer分配在调用方自己的模块里，不和
  `this->freeLots`共享）去读，命中的候选`Lot*`原样通过`outLot`返回，margin/depth相对它
  自己的尺寸算好——不在这个函数里做任何实际裁剪。真正的`RequestPlacement`调用和"把嵌套
  子块自己的残余`freeLots`/新增`pathRoadLinks`搬回真正的顶层Lot"这两步，在
  `Map::InitBuildings()`（`Source/Core/map/map.cpp`，编译进`Core.lib`→
  `UnrealEditor-Forever.dll`，执行时用的是FMemory路由的那份拷贝）里的
  `Map::PromoteAdaptiveRemainder`，通过`LotPlacementRequest::adaptiveParent`字段（非空=
  `request.lot`是`adaptiveParent`的`freeLots`里的一个嵌套子块，需要在`RequestPlacement`
  成功后做这次搬运；为空=`request.lot`就是调用方传进`Assign()`的顶层Lot本身，和非自适应
  路径完全一样，不需要搬运）触发。嵌套子块对象本身不`delete`——它会被`Building::
  SetParentLot`长期引用，生命周期和顶层Lot一样长到地图结束，这和非自适应路径里`parentLot`
  直接指向一个永不删除的顶层Lot是同一个模型。`TakePathRoadLinks()`/`AdoptPathRoadLinks()`
  这对方法只给`Map`用，把嵌套子块的`pathRoadLinks`转移到顶层Lot身上（否则`Map::
  GetPathRoads()`只遍历`GetLots()`里的顶层Lot，看不到挂在嵌套子块身上的记录，渲染缺一段
  小路；而且嵌套子块的析构函数会把这些`Road*`一并删掉，这些指针可能正被刚创建的
  `Building::boundaryRoads`引用着，变成悬空指针）。**教训**：任何会增长/收缩/析构某个
  `Core`层对象的`std::vector`（或任何容器）的调用，必须确保调用方最终编译进的是
  `UnrealEditor-Forever.dll`（直接在`Source/Core`的.cpp里调），不能是mod DLL——哪怕调用的
  函数本身写在`Dependence`这个"纯数据"层、对两边看起来完全一样。给mod暴露的接口只能是读
  （返回值传出、或者按值拷贝进调用方自己的容器），决不能是会直接触达某个Core对象自己持有的
  容器的非`const`方法。

- **`Road`的路口缩进`SetSetback/GetSetback(atStart)`**：这条路在起点端/终点端离路口中心的缩进距离，默认0，拷贝/赋值会带上。由路网mod在所有
  路的车道配好之后统一写入（宽度是后改的），Core的`RoadJunction::Build`和mod的行道树/路灯布局直接读。不进存档（存档里的`approach.setback`
  已经保存了路口实际用的值）。

## 依赖关系

- 依赖：`common/utility.h`（`OBJECT_HOLDER`标记宏、`GetRandom`——`Lot::FillRemainder`的CDF
  采样用它）、`common/error.h`（`THROW_EXCEPTION`）。
- 被谁依赖：`Source/Core/map/zone.h/.cpp`、`building.h/.cpp`、`map.h/.cpp`（`Map::InitZones`/
  `InitBuildings`直接调用`Lot::RequestPlacement`/`FillRemainder`）；`Source/Dependence/map/
  zone_mod.h`/`building_mod.h`（`LotPlacementRequest`定义在这里，两个mod头文件共用）。

## ParkingSpot

`ParkingSpot{position, rotationDegrees, categories}`：`categories`(`vector<string>`)是这个车位允许停进来的载具种类(`VehicleMod`静态`GetCategory()`返回的名字)，`Traffic::InitParkedVehicles`在其中随机选一个种类再选车型。存档里紧跟`rotationDegrees`写字符串列表(`map_save.cpp`)。

### 房间局部坐标系

`position`是房间局部坐标系里的PointParams：第一对=u轴(沿正面墙)、第二对=v轴(朝正面为正)，位置=比例x该轴长度+米数偏移，原点在房间角点；坐标系由房间自己的朝向(`Room::GetDirection()`)定义，不是建筑朝向，朝向变化时整套纯旋转，见`Core/map/room.md`。`rotationDegrees`的0度=朝向房间正面，朝房间深处(跑道)是180度。`Traffic::InitParkedVehicles`用`Room::AxisSizeU/V`、`LocalToBuildingLocal`、`FacingYawDegrees`换算成建筑局部偏移和旋转角。

- **`FindAdaptivePlacement`避免细边**：`RequestPlacement`每裁一刀都要求剩余部分≥`MIN_LOT_EXTENT`加一条小路宽，否则整次放置失败并把命中的子块留在半切状态。所以剩余空间(沿路的总余量、纵深的余量)小于`kMinLeftover`(4)时，不留细边，直接并进占地(margin=0 / depth=子块整个纵深)。
- **`FindAdaptivePlacement`的margin/depth相对命中子块`best`自己的尺寸反推**，调用方对`best`调`RequestPlacement`：命中的可能贴着原始边界路，也可能贴着之前切割插入的内部小路(同样是接入导航图的真实Road，贴着它盖房子合法)；换算到顶层Lot坐标系再反推，在子块贴内部小路时会落进已被别的mod占用的区域而匹配失败。
- **`SplitWithPath`的开口标记**：小路端落在"大路"(非小路)上时给那条路加`RoadOpening`(宽度=小路总宽，`t`按直线投影`ProjectT`近似，弯道中间开口不精确)；两条小路互接不标。这只是几何/渲染标记，不接导航图。`forwardSide`按小路所在lot在大路哪一侧填（小路另一端相对大路切线右手垂线`(-ty,tx)`的点积≥0=side0），渲染据此只在这一侧开口。`endT`不论大路小路都要算(`Map::ConnectPathRoad`需要)。

## WallStyle
`struct WallStyle { std::string tags[4]; std::string materials[4]; }`：一面墙四个侧面（下标=FACE_DIRECTION，0西 1东 2北 3南，表示该侧面朝向的方向）的 layout 标签与 Core 查表得到的材质实例软路径；`HasTag()/HasMaterial()` 判断是否有设置。

## Road::SidewalkFreeMesh

`Road::SetSidewalkFreeMesh(side, mesh)`/`GetSidewalkFreeMesh(side)`：去掉side(0/1)侧人行道后的路面资产路径（空=没有）。RoadnetMod建路时配好（该侧有人行道才配），
渲染开口只落在一侧时用它代替整宽平面cube，见`ForeverRoadnetFrameworkComponent.md`"开口按侧处理"。资产原点在它自己横断面的正中心。随Road拷贝/存档。
