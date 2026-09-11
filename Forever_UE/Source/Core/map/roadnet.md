# roadnet.h / roadnet.cpp

## 职责

`Roadnet`是`RoadnetMod`的包装类，模式和`terrain.h`的`Terrain`一致：持有`RoadnetMod*`+
`RoadnetFactory*`，构造时`CreateRoadnet`，析构走`factory->DestroyRoadnet`。比`Terrain`复杂的
地方是`DistributeRoadnet`调用后要把mod产出的`Node`/`Intersection`/`Road`/`Lot`**深拷贝**成自己
堆上持有的指针——mod实例本身在`Map::InitRoadnet()`跑完后就会被销毁，其内部`vector<Node>`等值
语义容器跟着析构，`Roadnet`必须有自己独立的一份数据才能长期存活。

`RoadJunction`是路口数据结构：以一个`Intersection`为中心，收集所有以它为端点的`Road`，生成
车行/行人导航锚点+路缘角点，并能构建路口内部的连接线。这是这次会话新设计的部分（老工程完全没有
路口级别的数据结构或mesh，见下方"和老工程的关系"）。

## 关键设计

- **一次只应该有一个路网布局方案生效**——`RoadnetFactory::SetConfig`/`GetRoadnet()`单选，
  详见`roadnet_factory.md`。`Roadnet`本身不关心这个，只是按调用方传入的id构造。
- **`lots`深拷贝时连同边界`Road`一起拷贝，但每个边界`Road`都是独立的新对象**——不是从
  `Roadnet::roads`（主数组）里找同一个对象复用指针。原因：老工程`JingRoadnet`构造lot边界时，
  有些确实复用了主`roads`数组里已经`push_back`过的同一个`Road`值（比如角落大lot直接引用
  `roads[0]`），但也有一些是**临时构造的、代表某条命名道路一小段的独立`Road`对象**（比如
  `Road("城西北路", intersections[horizontalNode1w[0].second], intersections[0], ...)`，只是
  这条命名路的一个子段，不是`roads`数组里的任何一个entry）。`Road`没有自己的唯一id（不像
  `Node`/`Intersection`有`GetId()`），没法可靠地按值反查回主数组的哪个entry，所以`Roadnet`的
  深拷贝就不试图去做这个"指针复用"，每个lot的边界`Road`都独立`new`一份。这对下游没有影响——
  边界`Road`唯一的用途是地址编号（`GetName()`）和未来Zone/Building消费（大概率也只需要
  `GetName()`/端点坐标定朝向），车行/行人导航图和mesh生成永远只读`Roadnet::roads`（主数组），
  不读lot的边界`Road`。
- **边界`Road`指针直接存在`Lot`自己身上，不再是`Roadnet::lots`旁边单独挂一份
  `unordered_map`（第八轮迁移）**——`Dependence/map/geometry.h`的`Lot`新增
  `SetBoundaryRoad(dir, Road*)`/`GetBoundaryRoad(dir)`/`GetBoundaryRoads()`（`Lot`不持有
  这些指针的生命周期，仍然由`Roadnet`统一`new`/析构时统一`delete`，只是"查询入口"从"先查
  `Roadnet::GetLots()`那个pair的第二个元素"变成"直接问`lot`自己"）。原来`Roadnet::lots`/
  `Map::GetLots()`是`vector<pair<Lot*, unordered_map<int,Road*>>>`，现在简化成
  `vector<Lot*>`——这是"为了后续concept"的改动：Zone/Building迁移后拿到一个`Lot*`就能
  直接查它周围临街的路，不需要额外维护/传递一份"lot到边界路映射"的旁路数据结构。
  `RoadnetMod`（mod层，`RoadnetMod::lots`）不受影响，仍然是`vector<pair<Lot,
  unordered_map<int,Road>>>`——mod实例本身在`DistributeRoadnet`跑完后就销毁，`Lot`的
  `boundaryRoads`指针如果在mod层就指向mod自己管理的`Road`，会在mod销毁后失效；边界映射
  只在`Roadnet::DistributeRoadnet`深拷贝时才真正"搬进"新`Lot*`自己身上，深拷贝之前
  （mod层）仍然是旧的pair写法，两层各自独立管理生命周期，不需要保持结构一致。
- **`AllocateAddress()`直接用`lot->GetBoundaryRoad(dir)`（0-3=`FACE_DIRECTION`）**，不是
  像最初考虑过的"按几何邻近反推"方案（这个方案讨论过又被推翻，见对话记录：用户明确要求lot
  要保留边界road信息，因为Roadnet阶段划出的地就是给以后Zone/Building用的）。每条非空的
  边界路都给这个lot调一次`lot->AddAddress(road->GetName(), index)`，`index`是该路名下当前
  已分配lot的序号（从0递增，一个lot可能临街多条路，各自有一个编号）。
- **路缘角点/导航锚点沿道路方向外移`setback`距离，不是直接贴在Intersection原坐标上**——
  `curbLeft`/`curbRight`和车行/行人锚点的基准点都是"Intersection坐标+沿outward方向外移
  setback"之后的点，不是Intersection原坐标本身。这是PIE验证阶段的修正——最初版本没有这个
  纵向外移，路口多边形对每条路都是"零深度"的点状扇形，和Forever层按同样距离收缩道路tiling的
  结果完全对不上（要么路口太小撑不满收缩出来的空当，要么反过来）。`setback`这个值必须由
  Forever层的道路tiling收缩量直接复用（不能自己另算一套），两者才能严丝合缝，见
  `Source/Forever/Framework/ForeverRoadnetFrameworkComponent.md`。
- **`setback`取的是整个路口所有连接路里、总宽度(`Road::GetTotalWidth()`)最大的那条的**一半**，
  不是任何一条路自己的宽度**（`RoadJunction::Build`先用一个`pending`数组扫一遍这个路口的所有
  approach，边扫边求`globalSetback = max(所有路GetTotalWidth()/2)`，第二遍才用这个全局值给
  每个approach赋`approach.setback`）——这是讨论中发现的问题：如果每条路的`setback`只按自己
  宽度算（比如十字路口，南北向总宽6车道很宽、东西向总宽只有4车道），窄的那条路（东西向）自己
  退得不够深，它的可见路面很快就铺到了路口中心附近；而宽的那条路（南北向）真正需要贯穿
  路口的车道（离中心线2~3个车道宽的那些，见下面"车道居中"一节——居中之后单条路两侧最外缘
  到连线的距离统一都是`GetTotalWidth()/2`），物理上落在窄路已经开始铺路面的范围内——车辆
  笔直穿过路口时，会有一段路径压在"别的路"的可见路面mesh上（车道贴图/走向都不对），哪怕
  两块mesh本身没有空间上的重叠，这在逻辑上也是错的。取整个路口的全局最大值，能保证不管
  哪条路多宽，它的车道在离路口中心`globalSetback`距离以内都还在"路口自己的地盘"上，不会
  被逼着穿过任何一条别的路的可见路面。代价是窄的那条路会因此被迫留出比自己实际需要更深
  的一截路口进深，这是为了车道逻辑正确而接受的空间开销，不是缺陷。
- **车道居中：`Connection`连线现在代表整条车道横断面的几何中心，不再是双向车道的分界线
  （第六轮迁移）**——原来的模型里，`side0`车道从连线开始往一侧堆到`side0Width`、`side1`
  从连线往另一侧堆到`side1Width`，连线本身就是老工程"双向车道分隔双黄线"的位置。这在两侧
  宽度对称时没问题（默认车道配置正是对称的），但单行道（比如`side1`完全没有车道）会导致
  整条路的车道全部堆在连线**一侧**，连线本身反而贴在道路的物理边缘上，路口mesh的形状也会
  因此变得很怪（一侧进深正常、另一侧几乎是贴着连线的零宽度）。修复：把连线重新定义成整条
  车道横断面（`side0`+`side1`所有车道、停车道、人行道加总）的**几何中心**，不管两侧车道
  数/宽度怎么分配，连线始终严格居中——单行3车道时，连线正好在这3条车道的正中间，两侧各有
  1.5车道宽的路面。这只是"以连线为原点，怎么把各条车道摆到左右两侧"这个换算公式变了，
  `Connection`的贝塞尔曲线本身（`Node`/控制点/弧长参数化）完全不受影响，因为要摆的车道
  本身没变，变的只是每条车道中心相对连线的偏移量。
  - `Dependence/map/geometry.h`的`Road`新增`GetSideWidth(int side)`（该side车行+停车+
    人行道宽度总和）和`GetTotalWidth()`（两侧相加）——这是判断"连线该往哪个方向偏、偏多少"
    唯一需要的两个数，取代原来在`roadnet.cpp`/`roadnet_basic.cpp`/
    `ForeverRoadnetFrameworkComponent.cpp`三处分别手写的`SumWidths`/`sumLaneWidths`/
    `SumLanes`局部helper（同一个"三类车道分别求和再相加"的算法被独立发明了三次，这次借这个
    机会收敛成`Road`自己的方法，Dependence层三个模块都能直接用同一份实现）。
  - `RoadJunction::Build`里推导出的换算公式：设`shift = (GetSideWidth(0) -
    GetSideWidth(1)) / 2`，任何按"老的side0/side1分界线为原点、side0方向为正"算出来的有
    符号横向偏移（`curbLeft`/`curbRight`的最外缘偏移、`makeAnchor`里每条车道中心的偏移），
    统一减去这个`shift`，就是"以居中后的连线为原点"的新偏移——这是纯代数换算，不需要重新
    设计整套车道堆叠逻辑（`SumWidths`+`LaneCenterOffset`这套"从内到外累加"的算法完全不变，
    只是最后多减一个`shift`）。可以验证：两侧对称时`shift=0`，和居中之前的行为完全一致
    （默认车道配置属于这种情况，不会有任何视觉回归）；两侧宽度差多大，`shift`就有多大，
    最终结果始终满足`side0最外缘偏移 = side1最外缘偏移(取绝对值) = GetTotalWidth()/2`，
    不管哪一侧车道多、哪一侧车道少。`Map::AddRoadAccessNode`（`map.cpp`）新增车道分裂
    锚点时用的是同一个公式，两处必须保持一致。
  - **`Source/Basic/map/roadnet_basic.cpp`的`roadMargin`因此不再需要判断lot落在road哪
    一侧**——上一轮迁移刚加的`roadSideForPoint`/`lotCenterOf4`（专门用来判断该用`side0`
    还是`side1`的宽度）整个被居中模型淘汰：既然两侧最外缘到连线的距离永远都等于
    `GetTotalWidth()/2`，margin不管lot在哪一侧结果都一样，直接`road.GetTotalWidth()*0.5f`
    即可，不需要再分side。`ForeverRoadnetFrameworkComponent.cpp`的`BuildOpeningMeshes`
    同理简化：它原来就是按"cube以连线为中心对称展开"写的（`cx±perp*halfWidth`），这在
    居中之前其实是隐藏的假设错误（只是默认车道配置对称、从未暴露），居中之后这个假设变成
    了真的成立，代码不用改逻辑，只是把`totalWidth`的来源换成`road->GetTotalWidth()`。
- **路口高度（`curbZ`）：路缘角点/导航锚点的基准点是沿road实际曲线采样出来的点，不是
  "Intersection原坐标+直线外移"的线性近似**——隧道落地后PIE发现两个问题都是同一个根因：
  ①隧道内的路口mesh完全按平地高度渲染（curb点当时只有X/Y、没有单独的Z，Forever层建mesh时
  统一套用地表高度epsilon，等于无视了`Intersection`的真实深度）；②隧道口路面在路口和可见
  路面衔接处有台阶——原来的线性近似公式`baseX = nodeX + outX*setback`只是在水平面上沿切线
  方向外移，完全没有对应到曲线在这个弧长位置的真实Z（隧道口那一段S形坡道沿途Z连续变化，直线
  近似在有高度变化的路段上会漏掉这段变化）。修复：`RoadJunctionApproach`新增`curbZ`字段，
  `Build()`用`totalLen = road->CalcDistance()`+`tFrac = clamp(setback/totalLen, 0, 0.45)`
  算出`sampleT`（和`BuildRoadInstances`算`trimStart`/`trimEnd`用的是完全相同的公式），再用
  `road->GetPoint(sampleT)`采样出该点的真实X/Y/Z（切线也在同一个`sampleT`重新采样，取代原来
  在端点t=0/1算的切线），基准点变成曲线上的真实点而不是近似——因为两边用的是同一个`sampleT`
  公式，路口边界和可见路面的起点必然采样到曲线上同一个点，高度和位置都严丝合缝。Forever层
  消费见`ForeverRoadnetFrameworkComponent.md`。
- **`RoadJunction`的锚点模型是"全部车道各自独立锚点+显式连接"，不是共享一个图节点**——因为
  不同`Road`的车道宽度不同，车道中心线在路口处相对`Road`标称中心线（也就是`Intersection`的
  精确坐标）是有横向偏移的，几条路在同一个路口不可能都精确交汇于`Intersection`那一个点。
  `RoadJunction::Build`为每条连到该路口的`Road`、在这一端存在的每个物理侧（行人）各生成
  一个锚点`Node`，贴着按车道总宽度算出的路缘偏移位置，而不是复用`Intersection`本身的坐标
  当车道图节点。
- **车行锚点是逐车道的，不是每个方向一个（第九轮迁移，`RoadJunctionApproach::
  vehicleInbound`/`vehicleOutbound`从`Node*`改成`std::vector<Node*>`）**——起因是PIE验证
  导航图可视化（`bShowNavigationDebug`）时发现：井字最中间几条不对称路里车道数>=2的那一侧，
  可视化只画出一条线，和实际车道数对不上。根因是原来每个方向（不管这一侧有几条车道）只在
  路口生成**一个**代表性锚点（`LaneCenterOffset(lanes,0)`，固定用最内侧车道的位置），这一侧
  所有车道的贯通线因此被迫共用同一对端点——哪怕后来（第九轮迁移前半）已经改成给每条车道建
  各自的`Connection`，这些`Connection`的起止点仍然是同一个共享锚点，几何上完全重合，肉眼
  看起来还是只有一条线。真正的修复要往前一步：`RoadJunction::Build`给`GetVehicleLanes(side)`
  里的**每条车道**都各自调一次`makeAnchor`，得到一个位置精确对应该车道中心的独立锚点，
  `vehicleInbound[i]`/`vehicleOutbound[i]`的下标直接对应车道数组下标。行人锚点
  （`pedestrianSide[2]`）这次没有同步扩展成逐车道——现有场景人行道每侧固定1条，暂时没有
  真正需要验证多车道人行道的数据，扩展方式和车行完全类似（如果以后需要，参照这次的改法）。
- **车行锚点全联通，行人锚点是人行横道+转角连通**（`RoadJunction::BuildConnectors`）：
  - 车行：每条inbound车道各自的锚点连到每条outbound车道各自的锚点（含同一条路的inbound连
    自己的outbound，允许U形连接），单向插入图（体现"这是一条能走的路径"，不代表能反着走）。
    车道级锚点化之后，这是车道对车道的叉乘，不再是approach对approach，连接数量比早期版本
    多——路口车道数较多时图规模会明显增长，这次先按"数据正确优先"处理，性能问题留到Traffic
    域（阶段4-3）真的跑寻路时再评估是否需要精简，和原计划"全联通规模问题" 的态度一致。
  - 行人**不是全联通**：①人行横道——同一条路自己两侧都有人行道锚点时，直接连一条
    `pedestrianSide[0]↔pedestrianSide[1]`，代表穿过这条路本身的人行横道；单侧人行道的路不
    生成人行横道。②转角人行道——按夹角排序后，每一对夹角相邻（环状，含首尾）的路，各自算出
    朝向对方那一侧的人行道锚点（用outward方向+isStart推导，见`roadnet.cpp`注释），两侧都有
    人行道才连，代表沿人行道绕过这个转角、不横穿任何一条路。行人边双向插入图（人行道没有
    方向限制）。
  - 这次先做直线连接，不做转弯半径/圆角——和路口mesh的简化程度一致（见
    `ForeverRoadnetFrameworkComponent.md`）。
- **`Map::AddRoadAccessNode`用到的"贯通线"记录（`Map::ThroughLine`）是`Map`自己的私有实现
  细节，不在`Roadnet`/`RoadJunction`里**——因为它要跨越`InitRoadnet`一次性建图和之后任意时刻
  调用`AddRoadAccessNode`两个阶段，需要`Map`长期持有可变状态（哪条边被拆分过），放在`Roadnet`
  （只在`DistributeRoadnet`时写一次、之后只读）里不合适。

## 和老工程的关系

老工程`Roadnet`（`E:\Projects\Forever_UE\Source\Core\map\roadnet.h`）只是`RoadnetMod`的薄包装
+ lot深拷贝 + `AllocateAddress`/`LocateBlock`，这几部分**原样对照迁移**（深拷贝手法、
`AllocateAddress`按边界路分配序号的语义）。但老工程完全没有：
- 车道级别的任何数据（`Road`只是一条中心线+mesh资产路径）。
- 路口数据结构或路口mesh（`RoadnetBase.cpp`视觉上就是几条路的mesh直接重叠）。
- 车行/行人分离的导航图（老工程导航图在`Map::InitNavigationGraph`，粒度是"每条Road一条边"，
  端点直接是共享的`Intersection`坐标，没有车道级偏移）。

`RoadJunction`、车行/行人双图、`Map::AddRoadAccessNode`都是这次会话跟用户逐条确认后的新设计，
不是照抄老工程代码，具体决策过程见对话记录和`Source/Dependence/map/roadnet_mod.md`。

## 依赖关系

- 依赖：`roadnet_mod.h`、`roadnet_factory.h`、`common/error.h`。
- 被谁依赖：`Source/Core/map/map.h`（`Map::roadnet`/`Map::junctions`成员，`InitRoadnet()`
  编排整个构建流程）、`Source/Forever/Framework/ForeverRoadnetFrameworkComponent.h/.cpp`
  （`GenerateRoadnet(Map*)`读取`Map::GetRoads()`/`GetJunctions()`/`GetLots()`建mesh）。

## 待办/后续阶段

- 阶段4：Zone/Building迁移时会开始真正消费lot的边界`Road`映射；Traffic域（阶段4-3）会开始
  消费`vehicleNavGraph`/`pedestrianNavGraph`做寻路，目前这两张图只负责"建出来、能查询"，不
  涉及任何寻路算法。
