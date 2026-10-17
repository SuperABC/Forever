# roadnet_mod.h / roadnet_mod.cpp

## 职责

`RoadnetMod`定义路网布局方案Mod必须实现的业务接口。和老工程`E:\Projects\Forever_UE\Source\
Dependence\map\roadnet_mod.h`相比是全新设计——老工程的`Road`只是一条带mesh资产路径的中心线，
车道/人行道/停车道完全是Blueprint侧靠一个mesh资产铺出来的视觉效果，C++层不知道车道数；这个
项目要求车道级导航图、路口mesh、路中间加Node分裂车道，老工程完全没有对应实现，具体设计见
`Source/Core/map/roadnet.md`。

## 关键设计

- **一次只应该有一个路网布局方案生效**，不是Terrain那种`GetPriority()`多mod叠加分发——
  `RoadnetFactory::SetConfig`/`GetRoadnet()`做单选，详见`roadnet_factory.md`。
- **`DistributeRoadnet`不传`getHeight`回调，也不传`setTerrain`/`setHeight`**——道路默认高度
  固定Z=0，不采样/不跟随真实地形高度起伏（有意的范围裁剪，隧道段用的是固定`TUNNEL_HEIGHT`
  常量，不是采样出来的真实深度）。`getTerrain`/`getWater`两个回调保留，`JingRoadnet`的隧道
  判断（探测`mountain`地形）就是靠`getTerrain`，`getWater`目前仍未被用到，留着给以后可能
  的mod用。
- **`nodeStaticCount`延续`Node`跨DLL计数器传递的写法**——mod实现必须在`DistributeRoadnet`
  开头第一步调`Node::SetCount(nodeStaticCount)`，构造的所有`Node`/`Intersection`/`Road`都
  必须通过`externs`/`intersections`/`roads`/`lots`四个成员返回，不能留下"野"实例（否则
  宿主侧的id计数器和mod侧不同步，后续深拷贝会拿到重复id）。
- **`lots`是`vector<Lot>`，边界`Road`映射直接记在每个`Lot`自己身上（`Lot::
  SetBoundaryRoad`/`GetBoundaryRoads`）**，不额外拿一个`pair`/`map`在旁边和`Lot`并排存——
  `Lot`本来就有这套边界API，没必要在`RoadnetMod`层再维护一份等价的数据结构；`Lot`的三点/
  四点构造函数都有一个可选的`boundary`参数（`general_geometry.h`），实现方可以直接在构造`Lot`的
  同时把边界`Road*`传进去，等价于构造完再逐个调`SetBoundaryRoad`，只是不用另开一个map。不带
  边界`Intersection`映射——Roadnet阶段产出的是"划出一块块地"，后面Zone/Building在lot里继续
  摆内容时需要知道这块地临哪条路（建筑朝向、出入口该开在哪条路上），所以边界`Road`信息要
  留着；但lot四角对应哪个`Intersection`用不上——车行/行人导航图直接挂在`Road`/
  `Intersection`上（见`Source/Core/map/roadnet.h`的`RoadJunction`），不需要通过lot中转。
  `unordered_map<int, Road*>`的`int`键沿用`general_geometry.h`的`FACE_DIRECTION`（0-3）。**这里存的
  是`Road*`，而且必须指向`this->roads`里的同一个元素，不能是另外new/构造的独立对象**——
  Zone/Building裁剪Lot空间时给"大路"加开口(`Lot::SplitWithPath`调`Road::AddOpening`)是直接
  改这个指针指向的对象；如果边界这里存的是独立拷贝，改的就只是这份拷贝，`roads`里真正会被
  渲染的那条路根本不受影响，PIE里看不到开口。所以这个字段类型是`Road*`不是`Road`，且要求
  实现方自己保证指针有效性——取地址前必须确保`this->roads`不会再增长，`vector`扩容会让
  之前取的地址失效，具体做法见`roadnet_basic.md`"`makeBoundaryRoad`"一节。
- **车道/开口数据直接挂在`Road`自己身上**（`vehicleLanes`/`parkingLanes`/`pedestrianLanes`/
  `openings`，见`general_geometry.h`），不是`RoadnetMod`这一层的字段——`RoadnetMod`只负责产出`Road`
  实例，实例本身已经携带了这些数据。
- **`AddHatch`工具方法+`hatches`输出成员**：`AddHatch(connection, t1, t2, width)`签名和
  实现（`roadnet_mod.cpp`）：取`connection`在`[t1,t2]`两端的点，中点定位、两点连线方向定
  朝向、弧长定长度、`width`定宽度，包成一个`Quad`追加进`hatches`。消费方
  `ForeverTerrainFrameworkComponent::LookupTerrain`挖洞判断是"`construction`地形或者这个
  格子有hatch"——隧道口的hatch靠这条规则在山体地形上挖出缺口，否则隧道段会被实心地形完全
  挡住看不见，详见`Source/Forever/Framework/ForeverTerrainFrameworkComponent.md`。

- **`pathRoadMaterial`**：Zone/Building裁剪`Lot`自由空间时，每次真正的切分都会自动生成一条
  1单位宽的小路`Road`（见`Source/Dependence/map/general_geometry.md`的`Lot::SplitWithPath`）。这条
  小路不走现有"按左右车道数选`default_x_x_x`资产"的道路mesh管线（0.3车行/0.2人行两侧这种
  非整车道宽度套不进那套命名约定），Forever层直接画一个贴材质的扁cube，材质路径就是这个
  字段——留空表示mod没有指定，退化用`DefaultRoad`。普通`public`字符串成员，和
  `externs`/`roads`等现有字段风格一致，`JingRoadnet`不设置也是合法状态。
- **行道树：虚函数`LayoutTrees(hostRoads)`+非虚工具方法`AddTreeSingle/AddTreeLine/AddTreeArea`+`trees`**——树放在
  整张地图最后一个阶段布局：`Map::InitVegetation`在路网/园区/建筑/房间全部生成完之后调用
  `LayoutTrees`一次(默认空实现)，传入的`hostRoads`是Core持有的那份路网(已经带上全部开口
  `Road::GetOpenings`，可以避开出入口)，不是mod自己`roads`那份旧拷贝，只读访问。mod在里面调
  `AddTree*`只记请求进`trees`(`TreeRequests`，见`feature_spec.md`，参考系是整张地图，世界坐标写成`{0,x,0,y}`)，
  宿主`Roadnet::LayoutTrees`按值拷贝走，再交给Core的`Vegetation`展开(随机旋转/缩放、跳过水面和挖洞格子)。

## 依赖关系

- 依赖：`map/general_geometry.h`（`Node`/`Intersection`/`Road`/`Lot`）。
- 被谁依赖：`Source/Core/map/roadnet.h`（`Roadnet`包装类持有`RoadnetMod*`，`pathRoadMaterial`
  在`DistributeRoadnet`时原样拷贝一份）、`Source/Basic/map/roadnet_basic.h`（`JingRoadnet`）、
  `Forever_Mod/Empty`的`EmptyRoadnet`demo mod、`Source/Core/map/zone.md`/`building.md`
  （Zone/Building读取lot的边界`Road`映射）。
