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
  实现：深度方向1刀+frontage方向最多2刀）和`FillRemainder`（权重CDF随机填充，每确定一个
  候选的目标面积后最多用2次`SplitWithPath`切成接近正方形——深度方向1刀+面宽方向1刀，见下
  "已修复"一节）都建立在这个原语之上，具体分配流程/可达性规则见map.md。这两个函数都是
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
- **已修复（PIE验证发现）：`FillRemainder`原来用整条面宽反推进深，面宽远大于
  `sqrt(acreage)`时进深会薄到不满足最小2x2单位，`SplitWithPath`直接失败，这块地就被整块
  丢弃不铺任何Zone/Building——这是"周围明明有路但大片区域没有Zone/Building"这个bug的根因，
  不是可达性判断错了**。修复成分两刀裁：先在深度轴上裁到`min(sqrt(acreage/
  ACREAGE_SCALE_FACTOR), 实际面宽)`算出来的目标进深，再在面宽轴上裁到目标面宽，尽量做出
  接近正方形的footprint；任何一刀因为尺寸或`SplitWithPath`自己的可达性检查裁不动，就跳过
  那一刀，用当前已经裁出来的矩形（可能比理想大小大）直接落地，不会再整块放弃——之前"裁不出
  来就把target整块丢进results"和"裁不出来就把target整块从pool移除、放弃"两条分支合并成了
  同一套"退化成用当前working的实际尺寸"的兜底逻辑。
- **旋转的传递方式**：`RequestPlacement`/`FillRemainder`返回的`Quad`/`FillResult::footprint`
  本身不带旋转（`Quad`没有旋转字段），但`freeLots`池里所有子块都严格继承同一个顶层`Lot`的
  `rotation`（`SplitWithPath`产出的近端/远端两段都用`this->rotation`构造，见实现），所以
  调用方不需要额外传递——`Map::InitZones`/`InitBuildings`直接用`request.lot->GetRotation()`
  （显式占位）或`lot->GetRotation()`（`FillRemainder`）赋给`Zone`/`Building`自己新增的
  `rotation`字段即可，取的都是同一个顶层Lot的值。

## 依赖关系

- 依赖：`common/utility.h`（`OBJECT_HOLDER`标记宏、`GetRandom`——`Lot::FillRemainder`的CDF
  采样用它）、`common/error.h`（`THROW_EXCEPTION`）。
- 被谁依赖：`Source/Core/map/zone.h/.cpp`、`building.h/.cpp`、`map.h/.cpp`（`Map::InitZones`/
  `InitBuildings`直接调用`Lot::RequestPlacement`/`FillRemainder`）；`Source/Dependence/map/
  zone_mod.h`/`building_mod.h`（`LotPlacementRequest`定义在这里，两个mod头文件共用）。

## 待办/后续阶段

- 已完成：小路接入`vehicleNavGraph`/`pedestrianNavGraph`，见`Source/Core/map/map.md`
  "ConnectPathRoad"一节。
