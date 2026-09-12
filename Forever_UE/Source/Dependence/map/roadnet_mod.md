# roadnet_mod.h / roadnet_mod.cpp

## 职责

Roadnet是阶段4-1第二个从共用文档独立出来的concept，`RoadnetMod`定义路网布局方案Mod必须实现的
业务接口。和老工程`E:\Projects\Forever_UE\Source\Dependence\map\roadnet_mod.h`相比是**新设计**，
不是原样迁移——老工程的`Road`只是一条带mesh资产路径的中心线，车道/人行道/停车道完全是Blueprint
侧靠一个mesh资产铺出来的视觉效果，C++层不知道车道数；这次要求车道级导航图、路口mesh、路中间加
Node分裂车道，这些老工程完全没有对应实现，是本次会话跟用户逐条确认后的新设计，详见对话记录和
`Source/Core/map/roadnet.md`。

## 关键设计

- **一次只应该有一个路网布局方案生效**，不是Terrain那种`GetPriority()`多mod叠加分发——
  `RoadnetFactory::SetConfig`/`GetRoadnet()`做单选，详见`roadnet_factory.md`。
- **`DistributeRoadnet`不传`getHeight`回调，也不传`setTerrain`/`setHeight`**——道路默认高度
  仍然是Z=0，不采样/不跟随真实地形高度起伏（用户明确要求的范围裁剪，第二轮迁移加回隧道后
  依然成立，隧道段用的是固定`TUNNEL_HEIGHT`常量，不是采样出来的真实深度）。`getTerrain`/
  `getWater`两个回调保留，`JingRoadnet`的隧道判断（探测`mountain`地形）就是靠`getTerrain`，
  `getWater`目前仍未被用到，留着给以后可能的mod用。
- **`nodeStaticCount`延续`Node`跨DLL计数器传递的写法**——mod实现必须在`DistributeRoadnet`开头
  第一步调`Node::SetCount(nodeStaticCount)`，构造的所有`Node`/`Intersection`/`Road`都必须通过
  `externs`/`intersections`/`roads`/`lots`四个成员返回，不能留下"野"实例（否则宿主侧的id计数器
  和mod侧不同步，后续深拷贝会拿到重复id）。
- **`lots`就是`vector<Lot>`，边界`Road`映射直接记在每个`Lot`自己身上（`Lot::
  SetBoundaryRoad`/`GetBoundaryRoads`），不再额外拿一个`pair`/`map`在旁边和`Lot`并排存**
  （第十三轮迁移简化，之前是`vector<pair<Lot, unordered_map<int, Road*>>>`）——`Lot`本来就有
  这套边界API，没必要在`RoadnetMod`层再维护一份等价的数据结构；`Lot`的三点/四点构造函数都
  新增了一个可选的`boundary`参数（`geometry.h`），实现方可以直接在构造`Lot`的同时把边界
  `Road*`传进去，等价于构造完再逐个调`SetBoundaryRoad`，只是不用另开一个map。不带边界
  `Intersection`映射——Roadnet阶段产出的是"划出一块块地"，后面Zone/Building迁移时会在lot里
  继续摆内容，需要知道这块地临哪条路（建筑朝向、出入口该开在哪条路上），所以边界`Road`信息要
  留着；但lot四角对应哪个`Intersection`这次用不上——车行/行人导航图直接挂在`Road`/
  `Intersection`上（见`Source/Core/map/roadnet.h`的`RoadJunction`），不需要通过lot中转。
  `unordered_map<int, Road*>`的`int`键沿用`geometry.h`的`FACE_DIRECTION`（0-3）。**这里存的
  是`Road*`，而且必须指向`this->roads`里的同一个元素，不能是另外new/构造的独立对象**
  （第十一轮迁移，Zone/Building裁剪Lot空间时给"大路"加开口(`Lot::SplitWithPath`调
  `Road::AddOpening`)才发现的问题：最初这里存的是`Road`值，编译能过，PIE里却看不到开口——
  因为改的是lot边界这份独立拷贝，`roads`里真正会被渲染的那条路根本没变。所以这个字段类型从
  `Road`改成了`Road*`，且要求实现方自己保证指针有效性——取地址前必须确保`this->roads`不会
  再增长，`vector`扩容会让之前取的地址失效，具体做法见`roadnet_basic.md`
  "`makeBoundaryRoad`"一节）。
- **车道/开口数据直接挂在`Road`自己身上**（`vehicleLanes`/`parkingLanes`/`pedestrianLanes`/
  `openings`，见`geometry.h`），不是`RoadnetMod`这一层的字段——`RoadnetMod`只负责产出`Road`
  实例，实例本身已经携带了这些数据。
- **`AddHatch`工具方法+`hatches`输出成员已恢复（第三轮迁移）**——第二轮迁移加回隧道高度/
  几何逻辑时曾判断"这个hatch搬过来也不会有可见效果"而没迁移，后来PIE验证发现隧道段会被
  山体实心地形完全挡住看不见，倒推回来确认这个判断是错的：不是hatch本身没用，是消费方
  （`ForeverTerrainFrameworkComponent::LookupTerrain`）当时把挖洞逻辑锁死在只认
  `"construction"`地形——把这个锁放宽成"`construction`或者这个格子有hatch"之后，隧道口的
  hatch就能正常在山体地形上挖出一个缺口，详见`Source/Forever/Framework/
  ForeverTerrainFrameworkComponent.md`。`AddHatch(connection, t1, t2, width)`签名和实现
  （`roadnet_mod.cpp`）照抄老工程语义：取`connection`在`[t1,t2]`两端的点，中点定位、两点
  连线方向定朝向、弧长定长度、`width`定宽度，包成一个`Quad`追加进`hatches`。

- **`pathRoadMaterial`（第九轮迁移，Zone/Building落地时新增）**：Zone/Building裁剪`Lot`自由
  空间时，每次真正的切分都会自动生成一条1单位宽的小路`Road`（见`Source/Dependence/map/
  geometry.md`的`Lot::SplitWithPath`）。这条小路不走现有"按左右车道数选`default_x_x_x`资产"
  的道路mesh管线（0.3车行/0.2人行两侧这种非整车道宽度套不进那套命名约定），Forever层直接画
  一个贴材质的扁cube，材质路径就是这个字段——留空表示mod没有指定，退化用`RoadPlain`。普通
  `public`字符串成员，和`externs`/`roads`等现有字段风格一致，`JingRoadnet`这次不设置也是
  合法状态。

## 依赖关系

- 依赖：`map/geometry.h`（`Node`/`Intersection`/`Road`/`Lot`）。
- 被谁依赖：`Source/Core/map/roadnet.h`（`Roadnet`包装类持有`RoadnetMod*`，`pathRoadMaterial`
  在`DistributeRoadnet`时原样拷贝一份）、`Source/Basic/map/roadnet_basic.h`（`JingRoadnet`）、
  `Forever_Mod/Empty`的`EmptyRoadnet`demo mod。

## 待办/后续阶段

- 阶段4：Zone/Building已经开始读取lot的边界`Road`映射（`Lot::RequestPlacement`/
  `SplitWithPath`），详见`Source/Core/map/map.md`"InitZones/InitBuildings"一节。
