# zone_mod.h / building_mod.h

两个concept的Mod接口在阶段4-1 Zone/Building落地时一起设计（都要向`Lot`要地，接口形状几乎
一样），放在同一份文档里对照说明，避免两份文档大段重复。和老工程`Map::InitContents`比，
这次是**新设计**：老工程权重表是`GetPowers()`静态编译期常量，这次改成mod被引擎调用时动态
往每个`Lot`上push自己的类型+权重（要求8）；老工程只有"权重+随机面积"一种分配方式，这次新增
"直接指定一块贴着某条路的矩形区域"这种显式占位方式（要求1）。详见用户在Plan Mode里确认的
关键设计决策，`Source/Core/map/map.md`"InitZones/InitBuildings"一节有完整的引擎侧编排流程。

## 职责

- **`LotPlacementRequest`**（定义在`geometry.h`，两个mod头文件共用）：`{Lot* lot, int
  direction, float marginStart, float marginEnd, float depth}`，描述"贴着lot某条边界路、
  沿路方向留出marginStart/marginEnd、往里伸depth"的一块矩形。
- **`ZoneMod::Distribute(lots)`/`BuildingMod::Distribute(lots)`**：引擎按当前全图lot列表
  （剩余空闲面积降序，`Lot::GetFreeAcreage()`排序）调用一次。mod在其中往自己的
  `explicitPlacements`（`public`成员，两个mod类都有）push占位请求；`BuildingMod`还可以额外
  往自己的`candidateWeights`（`public`成员，`{Lot*, float}`）push权重登记——**不能直接调用
  `lot->AddCandidate(...)`，原因见下"关键设计"。

## 关键设计

- **Zone这次只有显式占位一种方式，没有权重/CDF方法**——`ZoneMod`不像`BuildingMod`那样有
  `RandomAcreage`/`GetAcreageMin`/`GetAcreageMax`，也不会被喂进`Lot::FillRemainder`。这是
  Plan Mode里用户明确的修正："zone生成阶段不要填满lot"——Zone不应该像老工程`ArrangeBlocks`
  那样把地块"填满"，只应该占据mod自己主动要的那一块。
- **`BuildingMod`两种方式都有**，照抄老工程`Map::InitContents`对Building的处理顺序（Zone先
  占完地，Building再用权重CDF填剩下的空间）：`RandomAcreage()`/`GetAcreageMin()`/
  `GetAcreageMax()`供`Lot::FillRemainder`采样目标面积、判断是否够放。
- **`explicitPlacements`是mod自己维护的`public`成员，不是虚方法的返回值**——引擎调用完
  `Distribute()`之后直接读这个数组，处理完（无论成功失败）就丢弃，不需要mod自己清空；下一次
  再调`Distribute()`(比如另一个注册的类型)是另一个新的mod实例，不会复用旧数组。
- **`LayoutZone()`/`LayoutBuilding()`（老工程用来配置mesh/组件布局的方法）这次不加**——Zone
  内部再摆Building的递归布局、Building的Room/Component布局都明确推迟（详见map.md），加了也
  没有消费方。
- **`Distribute(lots)`这次暂时不按`lot->GetArea()`筛选/区分权重，`ZoneBasic`/`BuildingBasic`
  对所有lot一视同仁**——地块类型本身`RoadnetMod`已经在构造lot时用`Lot::SetArea(AREA_TYPE)`
  标好了（`AREA_TYPE`定义在`geometry.h`，`Source/Basic/map/roadnet_basic.cpp`的
  `JingRoadnet::DistributeRoadnet`给每个lot都设置了），但这两个mod目前都只是验证链路用的
  通用占位类型（一个验证显式占位，一个验证权重CDF），用户明确要求先不区分，等以后设计具体
  建筑/园区类型（对照老工程`ResidentialZone::ZoneAssigner`/各`XxxBuilding::GetPowers()`按
  地块类型区分权重那套）时再按`lot->GetArea()`细化。
- **`BuildingMod`不能直接调用`lot->AddCandidate(...)`，必须push进自己的`candidateWeights`
  再由引擎代为登记（PIE验证发现的退出崩溃，已修复）**：`Lot::AddCandidate`是非虚成员函数，
  `Basic.dll`/`Empty.dll`等mod dll和`Forever.dll`各自独立编译了一份`Dependence.lib`，如果
  `BuildingMod::Distribute()`直接调用它，`Lot::candidates`这个`std::vector`的内部缓冲会被
  mod dll的（普通CRT）分配器分配；但`Lot`对象本身是`Forever.dll`分配、也由`Forever.dll`
  （经`Roadnet::~Roadnet()`）析构的，UE给每个模块都覆写了`operator new`/`delete`
  （`PerModuleInline.inl`，走`FMemory`），`Forever.dll`析构时会用自己的`FMemory::Free`释放
  一块mod dll用普通CRT分配出来的内存，两边分配器对不上，退出时析构`Lot`会直接崩溃
  （`EXCEPTION_ACCESS_VIOLATION`，栈顶在`vector::_Tidy()`/`operator delete`）。修复成
  `BuildingMod`新增一个自己拥有的`candidateWeights`表（`{Lot*, float}`纯数据），
  `Distribute()`只push进这个表，`Map::InitBuildings()`（Forever.dll编译的代码）读到之后
  自己调用`lot->AddCandidate(...)`——分配器和后续析构完全一致。`explicitPlacements`当初就是
  按这个原则设计的（mod只push纯数据、引擎自己处理），这次是`AddCandidate`这一条路径漏改了，
  修完之后两条路径手法完全统一。**任何以后新增的、mod需要"通知引擎某个Core层对象该怎么样"
  的接口，都要走这个"mod自己的输出表+引擎读回去处理"模式，不能让mod直接调用Core层对象的
  非虚成员函数写它自己的容器。**

## 依赖关系

- 依赖：`map/geometry.h`（`Lot`、`LotPlacementRequest`）。
- 被谁依赖：`Source/Core/map/zone.h`/`building.h`（`Zone`/`Building`包装类持有一个mod实例）、
  `Source/Core/map/map.h`（`Map::InitZones`/`InitBuildings`）、
  `Source/Basic/map/zone_basic.h`/`building_basic.h`（`ZoneBasic`/`BuildingBasic`默认内容）、
  `Forever_Mod/Empty`的`EmptyZone`/`EmptyBuilding`demo mod。

## 待办/后续阶段

- 阶段4：Zone内部再对自己的剩余空间跑一次Building分配流程（关键设计决策1明确推迟，等用户
  设计好细节再做）。
