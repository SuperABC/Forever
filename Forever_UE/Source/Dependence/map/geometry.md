# geometry.h / geometry.cpp

原样移植自旧工程`E:\Projects\Forever_UE\Source\Dependence\map\geometry.h/.cpp`（无windows/UE
类型依赖，纯数学），但阶段4-1 Zone/Building落地时（迁移`Lot`空闲区域分配+小路自动生成）做了
不小的改动——`Quad::DivideSpace`/`Quad::SplitInto`那套老工程原样迁移的"记录导航图失效/新增
连接"机制被**彻底删除**，`Lot`新增了一整套自己的空闲区域裁剪/填充方法。改动理由和用户逐条
确认的设计决策见`Source/Core/map/map.md`"InitZones/InitBuildings"一节，这里只记录geometry层
本身的职责和实现细节。

## 职责

提供导航图节点/连接、`Lot`空闲区域裁剪/填充两组能力：

- **`Node`/`Connection`/`Intersection`/`Road`**：导航图的点和边。`Connection`支持贝塞尔曲线
  控制点（`AddControls`），`GetPoint(f)`/`GetTangent(f, ...)`按**弧长比例**（不是Bezier多项式
  参数）取点和切线，即`f=0.5`对应曲线物理中点，供道路/人行道渲染与寻路采样使用。
- **`Quad`/`Lot`**：`Quad`是纯矩形（中心+尺寸+面积），`Lot`在`Quad`基础上加了旋转角度、地块
  类型、边界`Road`映射、地址编号——以及这次新增的**自由子地块池**（`freeLots`）和围绕它的
  裁剪/填充方法，见下"关键设计"。`SetPosition(n1, n2, n3[, n4], margin)`可以直接用连续几个
  角点+内缩边距反推出矩形的中心/尺寸/旋转，这是旧工程从"三个或四个已知角点"生成建筑/房间
  轮廓的标准做法，这次没有改动。三点/四点构造函数（第十三轮迁移）都新增了一个可选的
  `boundary`参数（`const std::unordered_map<int, Road*>&`，默认空），构造时直接按
  `FACE_DIRECTION`登记边界`Road`——等价于构造完再逐个调`SetBoundaryRoad`，只是不用在调用方
  那边另开一个`pair`/`map`和`Lot`并排存（`RoadnetMod::lots`就是这么改成`vector<Lot>`的，
  见`roadnet_mod.md`）。指针有效性仍然由调用方保证，语义和`SetBoundaryRoad`完全一致。

## 关键设计

- **`Connection`按值语义管理其端点/控制点**——构造函数、拷贝构造、赋值运算符里都会
  `new Node(...)`拷贝一份，析构时对应`delete`，不共享指针；这是因为`Node`本身很轻量（几个
  float+id），按值复制比引入引用计数更简单，唯一的例外是`Node::count`静态自增id计数器，拷贝
  构造/赋值时会同步推高`count`（保证之后新建的`Node`不会撞见已存在的旧id）。
- **弧长参数化用惰性缓存**——`Connection::GetPoint`/`GetTangent`如果有控制点（真正的Bezier
  曲线，不是直线），会在**首次调用时**采样128个点建立弧长查找表（`arcLengthCache`），后续
  调用直接二分查表，`AddControls`会清空缓存强制重新采样。
- **`Quad::DivideSpace`/`Quad::SplitInto`（含内部`Space`类、`RecordBoundary`、`QuadBoundary`
  结构体/`Invalidate`）已删除**——这套老工程的"记录四角四边失效/新增连接"机制是给导航图记账
  用的，Zone/Building这次的空闲区域分配完全不需要它：地块之间的连通性直接靠"分割线本身就是
  一条真正的`Road`"来体现（见下`SplitWithPath`），不需要额外维护一份`Connection`失效表。
  确认过删除前没有除`geometry.cpp`自己以外的调用点。
- **`Lot`的自由子地块池只有两层**：一个顶层`Lot`（由`Roadnet`持有，构造后不会再被按值复制）
  持有若干`freeLots`（也是`Lot`类型，复用同一套矩形/边界Road能力），子地块不会再有自己的
  子地块——所有裁剪/填充操作都直接在顶层`Lot`的`freeLots`数组上原地替换元素，不递归下钻。
  因此`Lot`不需要处理拷贝语义：现有代码里所有"按值复制`Lot`"的地方（`RoadnetMod::lots`收集、
  `Roadnet::DistributeRoadnet`深拷贝）都发生在`freeLots`填充之前，只需要给`Lot`补一个会
  释放`freeLots`的析构函数，不需要自定义拷贝构造/赋值。
- **`Lot::SplitWithPath(splitAlongX, splitCoordinate, spec)`是唯一的几何裁剪原语**：把当前
  矩形沿局部坐标系（原点在WEST-NORTH角：局部x=0是WEST、x=sizeX是EAST，局部y=0是NORTH、
  y=sizeY是SOUTH——这是`GetPosition`/`GetVertex`已有的约定，不是这次新发明的）某个坐标切成
  "近端(lower)/1单位宽小路/远端(upper)"三段，只返回lower/upper两个新`Lot`（小路本身不是可用
  地块，只作为`Road*`单独返回，不会自己保留一份——由调用方决定归属，见下）。新建的小路`Road`
  构造完立刻`SetPathRoad(true)`标记自己——是不是小路这个分类信息记在`Road`自己身上，任何
  持有这个`Road*`的调用方直接问它自己就够了，不需要另外查一份外部列表。
  **如果this在切割线两个端面方向都没有边界Road（原边界，或更早一刀生成、如今仍是某自由子块
  边界的小路Road），直接拒绝这次切割**，返回`{nullptr,nullptr,nullptr}`——不产生两端都不
  连接任何路网的孤岛小路。`RequestPlacement`（显式矩形占位，通过最多3次`SplitWithPath`调用
  实现：深度方向1刀+frontage方向最多2刀）和`FillRemainder`（采样填满+合并+递归二分，见下
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
  唯一需要的定位信息**（第十二轮迁移，小路正式接导航图——第十轮迁移曾经做过一版类似设计，
  后来因为设计问题被完整撤销，这次是重新设计后的实现，不是简单地"改回去"）。`endT`按
  Start->End直线投影近似算（`ProjectT`），和`RoadOpening.t`用的是同一次计算结果，不重复
  算两遍。`ConnectPathRoad`具体怎么用这两组信息断线/建锚点，见`Source/Core/map/map.md`
  "ConnectPathRoad"一节——这部分逻辑完全在Core层，`geometry.cpp`自己不触碰
  `vehicleNavGraph`/`throughLines`等Map内部结构，只负责把定位信息透传出去。
- **`FillRemainder`（第十四轮迁移，改用老工程算法重写）**：老工程`E:\Projects\Forever_UE`
  当年的分配思路是"先采样一串元素直到面积正好填满容器，再自底向上两两合并成二叉树，最后
  自顶向下按累计面积比例递归二分容器本身摆放每个元素"（`Quad::DivideSpace`+`Quad::SplitInto`，
  采样部分在旧`Map::GenerateBuildings`）——比这次之前"贪心挑池子里当前最大的自由块、随机抽
  一个类型、按面积门槛判断放不放得下"的版本更能保证容器被真正填满：旧贪心版本一旦某块自由地
  的面积小于随机抽到类型的`GetAcreageMin()`就把它整块永久丢弃，即便这块地长宽都满足
  `MIN_LOT_EXTENT(2)`——这正是"可达、长宽都>2却还空着"的主因，不是可达性判断错了。
  新实现分三步，只在`geometry.cpp`内部用（不进头文件）：
  1. **采样填满**（`SampleFillElements`，对应旧`Map::GenerateBuildings`的采样循环）：按
     `candidates`权重CDF反复抽类型、取`randomAcreage`目标面积，累加直到达到当前这个freeLots
     条目的总面积；只有"抽到但剩余空间连这个类型的下限都放不下"才计入尝试次数(上限16，沿用
     老工程`MAX_ALLOCATION_ATTEMPTS`)，抽到能放的类型不计入尝试次数——保证"填满"优先于"次数"。
     还差一点凑不满时（通常是尝试次数耗尽），补一个内部标记`FILL_EMPTY_TYPE`的空地叶子把总
     面积账目补齐，不会有游离在任何叶子之外的面积。
  2. **二分合并**（`BuildFillMergeTree`，对应旧`DivideSpace`合并阶段）：按面积降序排序，
     反复把数组末尾（最小）两个元素合并成一个新内部节点（面积=两者之和），插入排序塞回数组
     维持降序，直到只剩1或2个顶层节点。
  3. **递归安置**（`PlaceFillMergeNode`，对应旧`SplitInto`+`DivideSpace`安置阶段）：把当前
     region按左右子树的面积比例递归二分。**和老工程零宽度切割线不同，这里每一刀都是真实的
     `SplitWithPath`（带宽度的小路），会失败**——优先按region较长边选轴，切不动就换另一条轴
     再试一次，两条轴都切不动就说明这个节点没法再细分，把region整个让给面积更大的子树
     （另一支连同它的采样结果一起作废，不产生`FillResult`），这是唯一的退化点，只在几何确实
     分不出两段合法矩形时才触发，不再是"随机抽签运气不好就整块报废"。
- **旋转的传递方式**：`RequestPlacement`/`FillRemainder`返回的`Quad`/`FillResult::footprint`
  本身不带旋转（`Quad`没有旋转字段），但`freeLots`池里所有子块都严格继承同一个顶层`Lot`的
  `rotation`（`SplitWithPath`产出的近端/远端两段都用`this->rotation`构造，见实现），所以
  调用方不需要额外传递——`Zone::GetRotation()`/`Building::GetRotation()`直接转发
  `parentLot->GetRotation()`就是同一个值，不需要在`Zone`/`Building`自己身上再存一份（这两个
  类不自己存`rotation`字段，见`zone.md`）。
- **`RequestPlacement`/`FillResult`同样会丢失边界Road信息，第十五轮迁移补上**：两者内部真正
  裁剪出来的`Lot`（`working`/`region`）在返回前都会被`delete`，之前只把裸的`Quad`（仅
  pos+size）透传给调用方，"这块地实际靠着哪些边界Road"这份信息就跟着丢了——`RequestPlacement`
  新增一个可选输出参数`std::unordered_map<int, Road*>* outBoundaryRoads`（默认`nullptr`，
  不传行为不变），非空时在`delete working`之前把`working->GetBoundaryRoads()`拷贝进去；
  `FillResult`新增`boundaryRoads`字段，`PlaceFillMergeNode`的leaf分支在`delete region`之前
  同样拷一份。这是`Zone`/`Building`记录"自己四周靠着哪些道路"（`boundaryRoads`字段，见
  `zone.md`/`building.md`）的唯一数据来源。

## 依赖关系

- 依赖：`common/utility.h`（`OBJECT_HOLDER`标记宏、`GetRandom`——`Lot::FillRemainder`的CDF
  采样用它）、`common/error.h`（`THROW_EXCEPTION`）。
- 被谁依赖：`Source/Core/map/zone.h/.cpp`、`building.h/.cpp`、`map.h/.cpp`（`Map::InitZones`/
  `InitBuildings`直接调用`Lot::RequestPlacement`/`FillRemainder`）；`Source/Dependence/map/
  zone_mod.h`/`building_mod.h`（`LotPlacementRequest`定义在这里，两个mod头文件共用）。

## 待办/后续阶段

- 已完成：小路接入`vehicleNavGraph`/`pedestrianNavGraph`，见`Source/Core/map/map.md`
  "ConnectPathRoad"一节。
