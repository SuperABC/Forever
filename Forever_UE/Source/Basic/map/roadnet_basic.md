# roadnet_basic.h / roadnet_basic.cpp

## 职责

`JingRoadnet`（"井字路网"）是Roadnet域内置的默认路网生成器，取代阶段3占位`RoadnetBasic`。
照抄老工程`E:\Projects\Forever_UE\Source\Basic\map\roadnet_basic.cpp`的算法迁移：4个角
`Intersection`（NW/NE/SE/SW）+ 8条链向外延伸到地图边界的`extern` + 环路（"中山×路"）/放射路
（"城××路"）命名规则 + 5个默认lot（4角+1中心）+ 沿每条放射臂继续细分lot。

## 关键设计

- **隧道（第二轮迁移，恢复老工程逻辑）**：道路默认高度固定0（第一轮迁移的范围裁剪结论仍然
  成立——不恢复老工程"全程按真实地形/水面高度起伏"的地形高度跟随，`sampleHeight`这个老工程
  辅助lambda不迁移），但`extendChain`延伸链条时如果遇到`mountain`地形（或前后
  `TUNNEL_LOOKAHEAD_DISTANCE=5`个单位内探测到`mountain`，提前/延后过渡避免隧道口卡在山体
  正中间），对应`Node`的`z`改成`TUNNEL_HEIGHT=-1.f`（地图单位，相对平坦Z=0基准往下10m），
  其余节点仍然是`0.f`。
- **隧道口地形hatch已恢复（第三轮迁移）**：`addRoad`的斜坡段额外调
  `AddHatch(&roads.back(), 0.f, 1.f, TUNNEL_HATCH_WIDTH)`，覆盖斜坡段的地形挖洞。
  第二轮迁移时曾判断"这个hatch即使搬过来也不会有可见效果"而跳过——那个判断只在
  `ForeverTerrainFrameworkComponent::LookupTerrain`的挖洞逻辑锁死在`"construction"`地形时
  成立；PIE验证发现隧道段确实被山体实心地形完全挡住看不见后，改成把那个消费方的判断条件
  放宽到"`construction`或者这个格子有hatch"，隧道口的hatch才真正有了用武之地，详见
  `Source/Forever/Framework/ForeverTerrainFrameworkComponent.md`。
- **`addRoad`的地面↔隧道分支现在拆成三段Connection，不是两段（第四轮迁移，修复路口高度
  连续性问题）**：①`groundNode→flatNode`（长`TUNNEL_FLAT_APPROACH_LENGTH=1.5`，两端Z相同，
  addControls产出的曲线严格保持水平）；②`flatNode→splitNode`（真正的S形下坡，addControls
  给出两端切线水平的曲线）；③`splitNode→隧道`（两端同高，不需要控制点）。原来只有②③两段，
  S形下坡从`groundNode`（真实路口所在的`Intersection`）本身就开始——PIE验证发现两个问题都是
  同一个根因：`RoadJunction::Build`按`setback`（默认车道配置=1.0地图单位）把路口这一端的曲线
  裁掉一截，裁剪点`sampleT`对应的弧长比例在只有②③两段时会落在下坡曲线已经下降了一部分高度的
  位置（哪怕只裁到很靠前的比例，三次贝塞尔的曲率在起点附近就已经非零），于是：①路口mesh
  被强制铺成一个平面（用`Intersection`自身高度，见
  `Source/Forever/Framework/ForeverRoadnetFrameworkComponent.md`"路口高度"一节）时，各条
  curb的真实高度已经不一致，路口整体不再共面；②即使路口mesh保持平面，可见路面（裁剪边界以外
  的部分）开始的地方也已经比路口平面低了一截，看起来像是"S形下坡在路口范围内就已经开始"。
  修复思路：在真正的下坡曲线①之前，插入一段**两端Z完全相同**的水平引道①——因为addControls
  在两端Z相同时产出的整条曲线严格保持水平（不是"近似水平"，是数学上处处相等），不管
  `RoadJunction::Build`实际按setback裁掉多长（哪怕裁剪比例clamp到上限0.45），裁掉的那一截
  必然完全落在这段水平引道以内，路口mesh（强制用`Intersection`高度铺平）和裁剪边界处曲线的
  真实高度因此永远一致，S形下坡只会在引道结束之后（视觉上就是路口边缘以外）才真正开始下降。
  `TUNNEL_FLAT_APPROACH_LENGTH`不需要精确等于`setback`，比它大留出余量即可（引道本身处处
  水平，不存在"裁多了露馅"的风险）。`flatNode`/`splitNode`都是普通`Node`（不登记进
  `intersections`），不会产生额外的`RoadJunction`，纯几何过渡点。原来只在斜坡段开的hatch
  现在拆成两个（引道段+下坡段各一个，首尾相接），合起来覆盖范围和原来"整段一次性开洞"完全
  一致，不会因为拆分出引道而漏挖——引道段虽然处处水平，但探测半径更大的
  `hasMountainNearby`仍可能已经把这段地形判成需要下坡的`mountain`，所以照样要挖。
- **`addControls`现在按端点各自的`Z`取控制点高度**（`c1.z=n1.GetZ()`、`c2.z=n2.GetZ()`），
  不再像第一轮迁移那样两个控制点都固定`0.f`——两端Z相同时（目前唯一场景）效果和以前完全
  一样，两端Z不同时（隧道过渡段）能在两个平缓端之间画出平滑升降的曲线，这是要求9"曲线道路"
  支撑机制的一部分，照抄老工程写法。
- **车道配置从"全路网统一一套"改成按左右两侧车道数参数化（第八轮迁移）**：`configureLanesEx
  (road, leftV, leftP, leftPd, rightV, rightP, rightPd)`按传入的六个数量分别调用
  `AddVehicleLane`/`AddParkingLane`/`AddPedestrianLane`（每条车道宽度固定`LANE_WIDTH=0.5`，
  这次验证的重点是数量不对称/单行本身，不是宽度精细调整）。`addRoad`新增六个可选参数，默认
  `(1,0,1,1,0,1)`——两侧对称、车行道每方向1条、不设停车道、人行道每侧1条，和原来的
  `configureLanes`行为完全一致，绝大多数Road（所有"城××路"放射臂）不传这六个参数，用的
  还是这一套；只有井字最中间的四条"中山×路"显式传了不对称/单行的配置，专门用来验证车道
  居中（`Source/Core/map/roadnet.md`"车道居中"一节）、单行道开口（`Source/Core/map/map.md`
  "单行道开口"一节）这些新逻辑——对称默认配置从来没有暴露过这些问题，必须有真正不对称的
  数据才能在PIE里看出来：
  ```cpp
  addRoad("中山西路", intersections[0], intersections[3], 1, 0, 1, 2, 0, 1); // 左1右2，双向不对称
  addRoad("中山东路", intersections[1], intersections[2], 2, 0, 1, 1, 0, 1); // 左2右1，镜像
  addRoad("中山北路", intersections[0], intersections[1], 0, 0, 1, 2, 0, 1); // 左0右2，单行道
  addRoad("中山南路", intersections[3], intersections[2], 2, 0, 1, 0, 0, 1); // 左2右0，单行道(镜像)
  ```
  left/right对应`Road`的side0/side1（`perp0`定义下的右手边/左手边，和整个代码库"side0=
  右手边"的既有约定一致）——**PIE实测发现这个映射最初写反了**（`configureLanesEx`一度把
  left参数加到side1、right参数加到side0），导致不对称资产的贴图左右和实际车道数左右对调
  （比如本该"左2右1"的路，实际生成成了"左1右2"），已改正。
- **`Lot`的margin不再是手动指定的固定常量，改成从临街`Road`自己的车道宽度直接算（第五轮
  迁移，第六轮车道居中改造后进一步简化）**：`roadMargin(road)`直接返回
  `road.GetTotalWidth() * 0.5f`——不需要关心lot落在road哪一侧。第五轮迁移时曾经先判断lot
  落在road哪一侧（`roadSideForPoint`+`lotCenterOf4`），只取那一侧的宽度，因为当时的模型下
  `Connection`连线是两侧车道的分界线，两侧宽度不对称时只有margin对应的那一侧宽度是准的；
  第六轮"车道居中"改造（见`Source/Core/map/roadnet.md`"车道居中"一节）把连线重新定义成
  整条车道横断面的几何中心之后，两侧最外缘到连线的距离在数学上永远相等（都等于
  `GetTotalWidth()/2`），margin不管lot在哪一侧结果都一样，`roadSideForPoint`/
  `lotCenterOf4`这两个专门判断"该取哪一侧"的helper因此被删掉，调用点也从
  `roadMargin(road, cx, cy)`简化回`roadMargin(road)`，边界Road仍然要先落地成局部变量
  （`Road boundary2 = makeBoundaryRoad(...)`）算完margin后再传进`unordered_map`，纯写法
  上的需要，不产生额外含义。原来是一个手写的`ROAD_MARGIN=1.0f`常量（对应默认车道配置车行
  0.5+人行0.5），如果以后改了`configureLanes`的车道宽度，这个常量必须手动跟着改；现在
  margin直接查询road自己的车道数据，默认配置下算出来的值和原来的`1.0f`完全一样，换了车道
  配置也会自动算对。margin数组里`0.0f`的位置含义不变（对照老工程保留，表示那条边不是真正
  临街的道路，而是相邻lot之间的内部分界线，不需要让出道路宽度，也没有Road可查）。
- **去掉了挡住后续逻辑的`return;`**（老工程`roadnet_basic.cpp:327`附近）——老工程这段"沿每条
  放射臂继续按`isBuildable`（plain/construction地形）细分lot"的代码写了但从没跑过，这次去掉
  `return`让它真正生效，除了去掉隧道判断外，逻辑原样迁移。每个新产出的lot同样带上边界`Road`
  映射（`FACE_DIRECTION`0-3索引），和角落/中心lot格式一致。
- **`lots`不带边界`Intersection`映射**——对照新接口`RoadnetMod::lots`（`vector<pair<Lot,
  unordered_map<int,Road>>>`，见`roadnet_mod.h`），老工程每个lot还会额外记一份
  `unordered_map<int,Intersection>`，这次不需要（导航图直接挂在`Road`/`Intersection`上，不
  通过lot中转，详见`Source/Core/map/roadnet.md`），构造时相应去掉了这部分。
- **道路3D资产命名规则改成按车道数编码，`meshPathFor`按这个规则拼路径（第八轮迁移）**：
  `default_左车行_左停车_左人行_右车行_右停车_右人行`（六个数字，和`configureLanesEx`的
  六个参数一一对应），比如默认对称配置对应`default_1_0_1_1_0_1`，"中山北路"（左0右2单行）
  对应`default_0_0_1_2_0_1`。老的单一`default_1_1.uasset`资产已经被美术按这套新命名规则
  重新拆分成多份不同车道组合的资产（`Content/Asset/Meshes/`下现在能看到
  `default_1_0_1_1_0_1`/`default_1_0_1_2_0_1`/`default_0_0_1_2_0_1`/`default_2_0_1_0_0_1`/
  `default_2_0_1_1_0_1`几个变体），每个资产的视觉横断面和文件名描述的车道数精确对应，不用
  再另外配材质区分——mesh资产本身画的就是完整的车道+人行道组合。`meshUnit`固定`0.5f`不随
  配置变化（这次假设所有变体资产都按同一个物理长度制作，是艺术资产层面的约定，不是代码
  推导出来的）。

## 依赖关系

- 依赖：`map/roadnet_mod.h`（`RoadnetMod`基类）。
- 被谁依赖：`Source/Basic/basic.cpp`（`GetModRoadnets`/`RegisterModRoadnets`/
  `FinishModRoadnets`导出符号，注册`JingRoadnet`）。

## 待办/后续阶段

- 阶段4：如果以后要支持"全程按真实地形/水面高度起伏"（不只是隧道这一种情况），
  `extendChain`需要把非隧道分支的`0.f`换回老工程的`sampleHeight(x,y)`（含水面+1的架桥逻辑），
  照抄老工程即可，这次仍然是裁剪范围，不是判断"不需要"永久去掉。
