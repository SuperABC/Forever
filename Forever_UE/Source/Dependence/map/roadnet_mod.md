# roadnet_mod.h / roadnet_mod.cpp

## 职责

`RoadnetMod`定义路网布局方案Mod必须实现的业务接口：产出`Node`/`Intersection`/`Road`/`Lot`，以及隧道口挖洞、路面贴地、行道树、路灯请求。车道级导航图、路口mesh、路中间加Node分裂车道的设计见`Source/Core/map/roadnet.md`。

## 关键设计

- **一次只有一个路网布局方案生效**，不是Terrain那种`GetPriority()`多mod叠加分发——`RoadnetFactory::SetConfig`/`GetRoadnet()`做单选，见`roadnet_factory.md`。
- **`DistributeRoadnet`有`getTerrain`/`getWater`/`getHeight`三个只读回调，没有`setTerrain`/`setHeight`**：道路高度由mod自己决定。默认可固定Z=0(隧道段用固定`TUNNEL_HEIGHT`常量下探，见`JingRoadnet`)，也可用`getHeight(x,y)`(地图element单位的连续坐标，含细化高度)采样山表面高度作为路面Z，再用`AddTerrainFit`把地形贴到路面上。`JingRoadnet`靠`getTerrain`探测`mountain`/`ocean`判断隧道/抬升，`getWater`目前未被用到。
- **`nodeStaticCount`**：mod实现必须在`DistributeRoadnet`开头第一步调`Node::SetCount(nodeStaticCount)`(跨DLL的`Node` id计数器同步)，构造的所有`Node`/`Intersection`/`Road`都必须通过`externs`/`intersections`/`roads`/`lots`四个成员返回，不能留下"野"实例，否则宿主和mod的id计数器不同步，后续深拷贝会拿到重复id。
- **`lots`是`vector<Lot>`，边界`Road`映射记在每个`Lot`自己身上**(`Lot::SetBoundaryRoad`/`GetBoundaryRoads`，键是`FACE_DIRECTION`0-3，缺失=那一侧没有路；`Lot`构造函数的可选`boundary`参数等价于构造后逐个`SetBoundaryRoad`)。不存边界`Intersection`映射：车行/行人导航图直接挂在`Road`/`Intersection`上(`Source/Core/map/roadnet.h`的`RoadJunction`)，不经lot中转。
- **边界`Road*`必须指向`this->roads`里的同一个元素，不能是独立拷贝**：Zone/Building裁剪Lot空间时(`Lot::SplitWithPath`)会通过这个指针给"大路"加`RoadOpening`，独立拷贝会让开口加不到真正被渲染的那条路上(PIE里看不到开口)。实现方要保证指针有效：取地址前必须把所有往`roads`增长的调用做完，`vector`扩容会让之前取的地址失效，做法见`roadnet_basic.md`的`makeBoundaryRoad`。
- **车道/开口数据挂在`Road`自己身上**(`vehicleLanes`/`parkingLanes`/`pedestrianLanes`/`openings`，见`general_geometry.h`)，`RoadnetMod`只负责产出`Road`实例。
- **挖洞/贴地**：`AddHatch`/`AddTunnelHatch`/`AddTerrainFit`三个非虚工具方法只是转调`general_geometry.h`的`BuildHatch`/`BuildTerrainFit`(`RouteMod`火车铺轨共用同一份实现)。
  - `AddHatch(connection, t1, t2, width)`：取`connection`在`[t1,t2]`两端的点，中点定位、连线方向定朝向、弧长定长度、`width`定宽度，包成`TerrainHatch`追加进`hatches`。消费方`ForeverTerrainFrameworkComponent::LookupTerrain`挖洞判断是"`construction`地形或这个格子有hatch"，隧道口靠这条规则在山体上挖出缺口，否则隧道段被实心地形挡住，见`Source/Forever/Framework/ForeverTerrainFrameworkComponent.md`。
  - `AddTunnelHatch`：隧道口专用，`TerrainHatch`带`wall=true`、`floorStart/floorEnd`=`connection`在`t1/t2`处的路面高度，UE层据此在洞的两条长边生成竖直侧壁；其它挖洞一律用`AddHatch`。
  - `AddTerrainFit(connection, t1, t2, width, falloff)`：沿`[t1,t2]`按弧长约0.5单位一个点采样成路面中轴折线(x,y,z，z=Node Z即路面高度)，连同`width`(路面整宽)和`falloff`(边缘向外渐变回原地形的距离)包成`TerrainFitRequest`追加进`terrainFits`。存折线而不是`Connection*`，因为mod的`roads`会扩容、指针失效。宿主`Map::InitRoadnet`把请求交给`Map::ApplyTerrainFits`，路面比地形高就填、低就切，见`Source/Core/map/map.md`"地形贴合"。
- **`pathRoadMaterial`**：Zone/Building裁剪`Lot`自由空间时每次真正的切分都会自动生成一条1单位宽的小路(`Lot::SplitWithPath`)。小路不走"按左右车道数选`default_x_x_x`资产"的管线(0.3车行/0.2人行这种非整车道宽度套不进去)，Forever层直接画贴材质的扁cube，材质就是这个字段；留空退化用`DefaultRoad`(`JingRoadnet`不设置也是合法状态)。`Roadnet`在`DistributeRoadnet`时原样拷贝一份。
- **行道树**：`LayoutTrees(hostRoads)`(默认空)+非虚`AddTreeSingle/AddTreeLine/AddTreeArea`+`trees`。`Map::InitVegetation`在路网/园区/建筑/房间全部生成完之后调一次，`hostRoads`是Core持有的路网深拷贝(已带全部开口`Road::GetOpenings`，可避开出入口，不是mod自己`roads`那份旧拷贝)，只读，不要调会修改它的方法。参考系是整张地图(不旋转)，世界坐标写成`{0,x,0,y}`。宿主`Roadnet::LayoutTrees`按值拷贝走`trees`，再由Core的`Vegetation`展开，见`feature_spec.md`。
- **路灯**：`LayoutLamps(hostRoads)`+`AddLampSingle/AddLampLine/AddLampArea`+`lamps`，完全同行道树；`Map::InitLamps`在`InitVegetation`之后调一次，宿主`Roadnet::LayoutLamps`拷贝走交给Core的`Lamps`展开。
- 带存档钩子(`SaveState`/`LoadState`/`GetStateVersion`/`GetInstanceCounter`/`SetInstanceCounter`)，默认不存任何东西，见`common/mod_state.md`。

## 依赖关系

- 依赖：`map/general_geometry.h`(`Node`/`Intersection`/`Road`/`Lot`/`TerrainHatch`/`TerrainFitRequest`)、`map/feature_spec.h`、`common/mod_state.h`。
- 被谁依赖：`Source/Core/map/roadnet.h`(`Roadnet`包装类持有`RoadnetMod*`)、`Source/Basic/map/roadnet_basic.h`(`JingRoadnet`)、`Forever_Mod/Empty`的`EmptyRoadnet`、`Source/Core/map/zone.md`/`building.md`(读取lot的边界`Road`映射)。

## 待办/后续阶段

- 无。
