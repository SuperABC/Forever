# roadnet_mod.h

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
- **`DistributeRoadnet`不传`getHeight`回调，也不传`setTerrain`/`setHeight`**——这次道路一律
  铺在Z=0，不采样/不跟随地形高度（用户明确要求的范围裁剪），`getTerrain`/`getWater`两个回调
  仍然保留（`JingRoadnet`的隧道判断逻辑虽然被去掉了，但地形/水域采样本身是通用能力，留着给
  以后可能的mod用）。
- **`nodeStaticCount`延续`Node`跨DLL计数器传递的写法**——mod实现必须在`DistributeRoadnet`开头
  第一步调`Node::SetCount(nodeStaticCount)`，构造的所有`Node`/`Intersection`/`Road`都必须通过
  `externs`/`intersections`/`roads`/`lots`四个成员返回，不能留下"野"实例（否则宿主侧的id计数器
  和mod侧不同步，后续深拷贝会拿到重复id）。
- **`lots`携带边界`Road`映射，但不带边界`Intersection`映射**——Roadnet阶段产出的是"划出一块块
  地"，后面Zone/Building迁移时会在lot里继续摆内容，需要知道这块地临哪条路（建筑朝向、出入口
  该开在哪条路上），所以边界`Road`信息要留着；但lot四角对应哪个`Intersection`这次用不上——
  车行/行人导航图直接挂在`Road`/`Intersection`上（见`Source/Core/map/roadnet.h`的`RoadJunction`），
  不需要通过lot中转。`unordered_map<int, Road>`的`int`键沿用`geometry.h`的`FACE_DIRECTION`
  （0-3）。
- **车道/开口数据直接挂在`Road`自己身上**（`vehicleLanes`/`parkingLanes`/`pedestrianLanes`/
  `openings`，见`geometry.h`），不是`RoadnetMod`这一层的字段——`RoadnetMod`只负责产出`Road`
  实例，实例本身已经携带了这些数据。
- **没有恢复老工程`RoadnetMod`的`AddHatch`工具方法**——那是给隧道场景的入口/出口开个洞用的，
  这次不做隧道，不需要。

## 依赖关系

- 依赖：`map/geometry.h`（`Node`/`Intersection`/`Road`/`Lot`）。
- 被谁依赖：`Source/Core/map/roadnet.h`（`Roadnet`包装类持有`RoadnetMod*`）、
  `Source/Basic/map/roadnet_basic.h`（`JingRoadnet`）、`Forever_Mod/Empty`的`EmptyRoadnet`
  demo mod。

## 待办/后续阶段

- 阶段4：Zone/Building迁移时会开始真正读取lot的边界`Road`映射（建筑朝向、出入口选路）。
