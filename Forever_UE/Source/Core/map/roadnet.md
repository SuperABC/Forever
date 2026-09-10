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
- **`lots`深拷贝时连同边界`Road`映射一起拷贝，但每个边界`Road`都是独立的新对象**——不是从
  `Roadnet::roads`（主数组）里找同一个对象复用指针。原因：老工程`JingRoadnet`构造lot边界时，
  有些确实复用了主`roads`数组里已经`push_back`过的同一个`Road`值（比如角落大lot直接引用
  `roads[0]`），但也有一些是**临时构造的、代表某条命名道路一小段的独立`Road`对象**（比如
  `Road("城西北路", intersections[horizontalNode1w[0].second], intersections[0], ...)`，只是
  这条命名路的一个子段，不是`roads`数组里的任何一个entry）。`Road`没有自己的唯一id（不像
  `Node`/`Intersection`有`GetId()`），没法可靠地按值反查回主数组的哪个entry，所以`Roadnet`的
  深拷贝就不试图去做这个"指针复用"，每个lot的边界`Road`都独立`new`一份。这对下游没有影响——
  边界`Road`唯一的用途是地址编号（`GetName()`）和未来Zone/Building消费（大概率也只需要
  `GetName()`/端点坐标定朝向），车行/行人导航图和mesh生成永远只读`Roadnet::roads`（主数组），
  不读lot的边界映射。
- **`AllocateAddress()`直接用lot自带的边界`Road`映射**，不是像最初考虑过的"按几何邻近反推"
  方案（这个方案讨论过又被推翻，见对话记录：用户明确要求lot要保留边界road信息，因为Roadnet
  阶段划出的地就是给以后Zone/Building用的）。遍历每个lot的`unordered_map<int,Road*>`
  （0-3=`FACE_DIRECTION`），每条非空的边界路都给这个lot调一次`lot->AddAddress(road->GetName(),
  index)`，`index`是该路名下当前已分配lot的序号（从0递增，一个lot可能临街多条路，各自有一个
  编号）。
- **路缘角点/导航锚点沿道路方向外移`setback`距离，不是直接贴在Intersection原坐标上**——
  `RoadJunctionApproach::setback`取该端两侧车道总宽度里较宽的一侧，`curbLeft`/`curbRight`
  和车行/行人锚点的基准点都是"Intersection坐标+沿outward方向外移setback"之后的点，不是
  Intersection原坐标本身。这是PIE验证阶段的修正——最初版本没有这个纵向外移，路口多边形对
  每条路都是"零深度"的点状扇形，和Forever层按同样距离收缩道路tiling的结果完全对不上（要么
  路口太小撑不满收缩出来的空当，要么反过来）。`setback`这个值必须由Forever层的道路tiling
  收缩量直接复用（不能自己另算一套），两者才能严丝合缝，见
  `Source/Forever/Framework/ForeverRoadnetFrameworkComponent.md`。
- **`RoadJunction`的锚点模型是"全部车道各自独立锚点+显式连接"，不是共享一个图节点**——因为
  不同`Road`的车道宽度不同，车道中心线在路口处相对`Road`标称中心线（也就是`Intersection`的
  精确坐标）是有横向偏移的，几条路在同一个路口不可能都精确交汇于`Intersection`那一个点。
  `RoadJunction::Build`为每条连到该路口的`Road`、在这一端存在的每个方向（车行）/每个物理侧
  （行人）各生成一个锚点`Node`，贴着按车道总宽度算出的路缘偏移位置，而不是复用`Intersection`
  本身的坐标当车道图节点。
- **车行锚点全联通，行人锚点是人行横道+转角连通**（`RoadJunction::BuildConnectors`）：
  - 车行：每个inbound锚点连到每个outbound锚点（含同一条路的inbound连自己的outbound，允许
    U形连接），单向插入图（体现"这是一条能走的路径"，不代表能反着走）。
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
