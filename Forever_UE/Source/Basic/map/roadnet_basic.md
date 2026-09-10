# roadnet_basic.h / roadnet_basic.cpp

## 职责

`JingRoadnet`（"井字路网"）是Roadnet域内置的默认路网生成器，取代阶段3占位`RoadnetBasic`。
照抄老工程`E:\Projects\Forever_UE\Source\Basic\map\roadnet_basic.cpp`的算法迁移：4个角
`Intersection`（NW/NE/SE/SW）+ 8条链向外延伸到地图边界的`extern` + 环路（"中山×路"）/放射路
（"城××路"）命名规则 + 5个默认lot（4角+1中心）+ 沿每条放射臂继续细分lot。

## 关键设计

- **道路高度固定0，不做隧道判断**——老工程`extendChain`会检测山体地形/邻近山体并把对应
  `Node`标记成隧道高度（`TUNNEL_HEIGHT=-1.f`），`addRoad`再据此判断是否要拆成斜坡+平路两段、
  开隧道口hatch；这次范围裁剪（用户明确要求"先不管隧道和地形高度，道路高度都先全都设置为
  0"），`extendChain`/`addRoad`都不再做这些判断，`sampleHeight`/`hasMountainNearby`两个老工程
  辅助lambda整个不迁移，所有`Node`/`Intersection`构造`z`参数直接传`0.f`。
- **`addRoad`只保留老工程"两端都是隧道或都不是隧道"这一个分支**——也就是老工程本来就有的、
  给每条路两端各加一个控制点（1/3、2/3处）保证节点处切线水平、避免相邻路段在共享node上切线
  塌缩的S形曲线写法。这是要求9"曲线道路"支撑机制的一部分，照抄，不做改动。
- **统一车道配置**：`configureLanes`给每条`Road`加车行道每方向1条（宽0.5）+人行道每侧1条
  （宽0.5，紧贴车行道外侧），不设停车道，不区分环路/放射路——这几个数值是用户给定的确定值
  （对照实际用到的`default_1_1`网格资产比例：单侧车行+人行总宽=0.5+0.5=1.0），不是拍脑袋
  的估计值，PIE视觉效果看起来车道窄也不要自作主张调大。
- **`ROAD_MARGIN=1.0f`**：`Lot`构造用的margin参数（`Lot(n1,n2,n3[,n4],margin)`），对应道路
  中轴线到人行道外边缘的总宽度（0.5车行+0.5人行=1.0）——lot矩形边界卡在人行道外边缘，不压进
  道路横断面。margin数组里`0.0f`的位置对照老工程保留（表示那条边不是真正临街的道路，而是相邻
  lot之间的内部分界线，不需要让出道路宽度）。
- **去掉了挡住后续逻辑的`return;`**（老工程`roadnet_basic.cpp:327`附近）——老工程这段"沿每条
  放射臂继续按`isBuildable`（plain/construction地形）细分lot"的代码写了但从没跑过，这次去掉
  `return`让它真正生效，除了去掉隧道判断外，逻辑原样迁移。每个新产出的lot同样带上边界`Road`
  映射（`FACE_DIRECTION`0-3索引），和角落/中心lot格式一致。
- **`lots`不带边界`Intersection`映射**——对照新接口`RoadnetMod::lots`（`vector<pair<Lot,
  unordered_map<int,Road>>>`，见`roadnet_mod.h`），老工程每个lot还会额外记一份
  `unordered_map<int,Intersection>`，这次不需要（导航图直接挂在`Road`/`Intersection`上，不
  通过lot中转，详见`Source/Core/map/roadnet.md`），构造时相应去掉了这部分。
- **`mesh`/`unit`指向`default_1_1.uasset`**（`meshPath="/Game/Asset/Meshes/
  default_1_1.default_1_1"`，`meshUnit=0.5f`）——老工程本来就是这么配的，这次沿用同一个值；
  这个mesh资产本身画的就是"双向各一车道+两侧人行道"的组合，和上面`configureLanes`定的车道
  宽度是同一份视觉效果的两种表现（数值给结构计算用，mesh资产给视觉用），不用另外配材质。

## 依赖关系

- 依赖：`map/roadnet_mod.h`（`RoadnetMod`基类）。
- 被谁依赖：`Source/Basic/basic.cpp`（`GetModRoadnets`/`RegisterModRoadnets`/
  `FinishModRoadnets`导出符号，注册`JingRoadnet`）。

## 待办/后续阶段

- 阶段4：如果以后要支持地形高度跟随/隧道，`extendChain`/`addRoad`需要重新加回对应逻辑，
  照抄老工程即可（这次只是裁剪范围，不是判断"不需要"永久去掉）。
