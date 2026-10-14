# roadnet_basic.h / roadnet_basic.cpp

## 职责

`JingRoadnet`("井字路网")是Roadnet域内置的默认路网生成器：4个角`Intersection`(NW/NE/SE/SW)
+8条链向外延伸到地图边界的`extern`+环路("中山×路")/放射路("城××路")命名规则+5个默认lot
(4角+1中心)+沿每条放射臂继续细分lot。

## 关键设计

- **非中心lot的分区类型按1:1:1随机分配**：井字中心lot固定`AREA_OFFICIAL_HIGH`(高密度办公
  区)；其余所有lot(4个紧邻中心的臂lot+4条放射臂细分出来的外圈lot，每次细分独立随机)调用
  文件内匿名namespace的`RandomHighDensityArea()`，从`{AREA_RESIDENTIAL_HIGH,
  AREA_COMMERCIAL_HIGH, AREA_INDUSTRIAL_HIGH}`等权重随机选一个(每个lot独立掷骰子，不保证
  整张地图恰好1:1:1均分)。这是支撑`building_basic.md`里`ShopBuilding`/`FactoryBuilding`
  (靠`GetPower(AREA_TYPE)`按分区类型竞争lot)的必要前提——分区类型如果单一固定，地图上只会
  长出一种建筑。
- **隧道**：道路默认高度固定0，但`extendChain`延伸链条时如果遇到`mountain`地形(或前后
  `TUNNEL_LOOKAHEAD_DISTANCE=5`个单位内探测到`mountain`，提前/延后过渡避免隧道口卡在山体
  正中间)，对应`Node`的`z`改成`TUNNEL_HEIGHT=-1.f`(相对平坦Z=0基准往下10m)，其余节点仍是
  `0.f`。隧道口斜坡段额外调`AddHatch(&roads.back(), 0.f, 1.f, roads.back().GetTotalWidth())`
  挖开地形，否则隧道段会被山体实心地形完全挡住看不见；消费方
  `ForeverTerrainFrameworkComponent::LookupTerrain`的判断条件要放宽到"`construction`或者
  这个格子有hatch"，见该文件`.md`。挖洞宽度直接取`roads.back().GetTotalWidth()`，跟着这条
  路实际配好的车道宽度走，不用固定常量——车道配置不对称/加宽时固定宽度的洞挖不穿全部车道。
- **`addRoad`的地面↔隧道过渡拆成三段Connection**：①`groundNode→flatNode`(长
  `TUNNEL_FLAT_APPROACH_LENGTH=1.5`，两端Z相同，`addControls`在两端Z相同时产出的曲线严格
  保持水平)；②`flatNode→splitNode`(真正的S形下坡)；③`splitNode→隧道`(两端同高，不需要
  控制点)。水平引道①的作用是保证`RoadJunction::Build`按`setback`裁掉的那一截必然落在水平
  段以内，不会裁到下坡曲线已经降高的位置，否则路口mesh和裁剪边界处曲线高度会不一致。
  `TUNNEL_FLAT_APPROACH_LENGTH`只需大于`setback`即可，不需要精确相等。两段hatch(引道段+
  下坡段)首尾相接，合起来覆盖范围和未拆分前一致。
  - **`flatNode`/`splitNode`不登记进`intersections`**——它们是几何过渡点，不是真正的路口，
    `RoadJunction::Build`按`setback`裁剪+摆水平路口平面的逻辑不适用于这两个点(尤其
    `splitNode`正好卡在下坡曲线中间)。为了不让导航贯通线在这两个点断开，
    `Map::InitRoadnet()`的`resolveAnchor`专门加了一个"既不是`RoadJunction`也不是`extern`"
    的兜底分支：按端点切线的右手垂线方向+真实车道宽度现算每条车道/人行道各自的锚点(和
    `RoadJunction::Build`同一套横向偏移换算，只是不产生`setback`/路口平面)，按
    (端点id,车行/行人,side,laneIndex)缓存，前后两段`Road`在这个端点各自解析出同一个
    `Node*`，贯通线因此天然接上。详见`Source/Core/map/map.md`"导航图"一节。
- **`addControls`按端点各自的`Z`取控制点高度**(`c1.z=n1.GetZ()`、`c2.z=n2.GetZ()`)，两端Z
  相同时效果和固定`0.f`一样，两端Z不同时(隧道过渡段)能画出平滑升降的曲线。
- **车道配置按左右两侧车道数参数化**：`configureLanesEx(road, leftV, leftP, leftPd, rightV,
  rightP, rightPd)`按传入的六个数量分别调`AddVehicleLane`/`AddParkingLane`/
  `AddPedestrianLane`(每条车道宽度固定`LANE_WIDTH=0.5`)。`addRoad`默认`(1,0,1,1,0,1)`
  (两侧对称、车行道每方向1条、不设停车道、人行道每侧1条)，绝大多数Road(所有放射臂)用这
  一套；只有井字最中间的四条"中山×路"显式传了不对称/单行的配置，用于验证车道居中
  (`roadnet.md`"车道居中"一节)、单行道开口(`map.md`"单行道开口"一节)：
  ```cpp
  addRoad("中山西路", intersections[0], intersections[3], 1, 0, 1, 2, 0, 1); // 左1右2，双向不对称
  addRoad("中山东路", intersections[1], intersections[2], 2, 0, 1, 1, 0, 1); // 左2右1，镜像
  addRoad("中山北路", intersections[0], intersections[1], 0, 0, 1, 2, 0, 1); // 左0右2，单行道
  addRoad("中山南路", intersections[3], intersections[2], 2, 0, 1, 0, 0, 1); // 左2右0，单行道(镜像)
  ```
  left/right对应`Road`的side0/side1(`perp0`定义下的右手边/左手边，和整个代码库"side0=
  右手边"的既有约定一致)。
- **`Lot`的margin从临街`Road`自己的车道宽度直接算**：`roadMargin(road)`返回
  `road.GetTotalWidth() * 0.5f`，不需要关心lot落在road哪一侧("车道居中"改造后，连线是
  整条车道横断面的几何中心，两侧最外缘到连线的距离永远相等)。边界Road先落地成局部指针
  (`Road* boundary2 = makeBoundaryRoad(...)`)算完margin后再传进`unordered_map`。margin
  数组里`0.0f`的位置表示那条边不是真正临街的道路，而是相邻lot之间的内部分界线，不需要让出
  道路宽度。
- **`makeBoundaryRoad`在`roads`里按名字+两端`Node`id查找已存在的真实`Road&`，返回其地址**，
  不新构造独立对象(独立对象即使配置一样也是另一个C++对象，改它不影响`roads`里的真实路段，
  会导致"小路接到大路上加开口画不出来")。调用时必须保证`roads`不会再增长，否则取到的地址
  会被之后的`vector`扩容废掉。
  - **找不到匹配时返回`nullptr`，调用方必须直接放弃这块lot**(不铺Zone/Building)——不要为
    了保住一小块贴着隧道口的地皮而构造一个假边界，隧道里也不应该摆建筑。象限lot用
    `if (boundary&&boundary)`包住`lots.emplace_back`；细分循环里用`continue`跳过。
- **道路3D资产命名规则按车道数编码**：`meshPathFor`拼出
  `default_左车行_左停车_左人行_右车行_右停车_右人行`(六个数字，和`configureLanesEx`的
  六个参数一一对应)，比如默认对称配置对应`default_1_0_1_1_0_1`，"中山北路"(左0右2单行)
  对应`default_0_0_1_2_0_1`，对应资产已经在`Content/Asset/Meshes/`按这套命名放好。
  `meshUnit`固定`0.5f`不随配置变化(假设所有变体资产按同一个物理长度制作，是美术资产层面
  的约定)。
- **`lots`的`emplace_back`里margin的`{...}`必须显式写成`vector<float>{...}`**，不能用裸
  花括号列表——`emplace_back`是模板参数转发，裸花括号在这种上下文推导不出类型，编译不过。
- **放lot之前必须整块扫描地形，不能只抽样四个角**：`isLotFullyConstruction(nw,ne,se,sw)`
  先按四个角算出lot的坐标范围(可能是旋转过的，因为整套井字路网带一个随机`theta`旋转角)，
  对范围内每个element用`isPointInQuad`(凸四边形叉积法，四条边同号即在内部)判断是不是真的
  落在这个lot内部，是的话就要求`getTerrain(x,y)=="construction"`，只要有一个不是就整块
  lot放弃(`break`/跳过，不`emplace_back`)。严格只认`"construction"`，不认`"plain"`——
  `"construction"`是`Map::InitTerrains`"周围3x3全是plain/construction才晋升"规则筛出来的
  (见`Source/Core/map/map.md`)，plain但没被晋升的那一圈格子，本质就是贴着mountain/river/
  ocean这类不规则地形的边缘地带；之前的做法(`isBuildable`)只抽样四个角、而且plain和
  construction都放过，会漏判——四个角都在晋升区内，lot中间却可能横穿一条贴着山体边缘的
  窄长plain地带，那种地方实际地形不规则，不适合铺建筑用地。所有9处`lots.emplace_back`
  调用都经过这个检查：井字中心+4个象限lot是直接`if`包住(原来没有任何地形检查)，4条放射臂
  的细分循环是检查不通过就`break`(停止沿这条臂继续往外细分，和原来`isBuildable`的失败
  语义一致)。

## 依赖关系

- 依赖：`map/roadnet_mod.h`(`RoadnetMod`基类)。
- 被谁依赖：`Source/Basic/basic.cpp`(`GetModRoadnets`/`RegisterModRoadnets`/
  `FinishModRoadnets`导出符号，注册`JingRoadnet`)。

## 待办/后续阶段

- 如果以后要支持"全程按真实地形/水面高度起伏"(不只是隧道这一种情况)，`extendChain`需要把
  非隧道分支的`0.f`换成真实的`sampleHeight(x,y)`(含水面+1的架桥逻辑)。
