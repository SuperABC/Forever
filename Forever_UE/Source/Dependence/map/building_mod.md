# building_mod.h / building_mod.cpp

## 职责

`BuildingMod`：具体建筑类型的mod基类。Building有两种生成方式(显式占位+权重CDF随机填充，见`Source/Core/map/map.md`"InitBuildings"一节)，落地之后通过`Layout(...)`一次性声明楼体footprint/楼层/LOD材质，以及每层楼内部的布局(`AssignFloor`/`AssignRoom`/`ArrangeRow`)，由`Source/Core/map/building.h`的`Building::Layout()`解析成真正的`Floor`/`Room`/`Component`数据。

## 关键设计

- **一个本体独占一个mod实例**：和`Zone`一样，`RandomAcreage`/`GetAcreageMin`/`GetAcreageMax`/`GetPower(AREA_TYPE)`/`Assign(lots, emit, context)`是不需要实例的`static`方法，每个具体子类必须实现(基类不声明，不是override)，通过`BuildingFactory::RegisterBuilding`以裸函数指针注册，只有真正落地成功时才`CreateBuilding`一次。`Assign`一次扫完全地图的lot，想显式占位哪块就调一次`emit(context, request)`，`request.lot`必须设成对应lot；不需要显式占位给空实现。`GetPower`返回该类型在某种分区lot上的权重，0=不考虑。放置请求(`LotPlacementRequest`/`PlacementEmitFunc`)机制及"mod不能直接调Core对象非虚成员"的跨DLL原因见`zone_mod.md`。
- **`GetName()`**：遵循CONVENTIONS §5的`static int count`模式，每个实例独占一个Building，名字在整个进程生命周期内唯一。
- **`Layout(int& direction, quad, boundaryRoads)`**：真实例的`SetPosition`/`SetBoundaryRoad`都设好后由`Building::Layout()`调用一次(构造函数此时不知道落地上下文，所以这些值不能在构造函数里定)。`direction`是引用参数：显式占位落地时传入`Assign`选中的方向；权重CDF/`FillRemainder`落地时传入-1。此时若mod关心方向，应从`boundaryRoads`里挑一个非空的key随机选一个方向回写，让building最终有一个确定的、有真实边界路的方向——building内部行人导航的`"outside"`端点要靠这个方向找到该连去哪条路(见`Source/Core/map/building.md`)；一条边界路都没有就保持-1。不关心方向的mod不动这个参数即可。`quad`/`boundaryRoads`是纯只读输入，无跨DLL容器所有权问题。
- **配置字段**：`footprint`(`BuildingFootprintSpec`，全是比例，故意不复用`Quad`以免语义混淆)、`basements`/`layers`、`floorHeights`(长度应为`basements+layers`，长度不对由`Building::Layout()`补默认值)、`lodMaterial`(留空用默认灰色)、`stateOwnedChance`(被政府拥有的概率，`Map::Checkin`读取)、`stationMod`(承载的StationMod id，空=非站点建筑，`Traffic::InitStations()`据此创建Station，见`traffic/station_mod.md`)、`scriptModName`/`milestoneNames`(建筑自己的Script，`milestoneNames`非空才创建，进出事件只发给它自己的Script，见`Source/Core/map/map.md`"实体脚本")。
- **`FloorAssetSpec`/`FloorLayoutSpec`**：一层楼的资产(墙/地/顶材质、楼梯/坡道网格)，留空由Forever层的组件级默认值兜底。不区分外墙/内墙材质。`.layout`模板本身不携带任何材质/网格信息，楼梯/坡道的3D网格必须由mod在`AssignFloor`里显式指定。`buildShell=false`表示这一层不由框架构造近处墙/地板/天花板(外观已由`AddDecoration`的成品模型提供时用)；只影响渲染，房间/导航/门/楼梯网格/Room碰撞盒与远处LOD方块照常，见`Source/Forever/Element/BuildingElement.md`。
- **`AssignFloor`/`AssignFloors`(x2)/`AssignRoom`/`ArrangeRow`**：把调用记录进`floors`(key=人类直觉层号，0=1楼，负数=地下室，同一层重复调用覆盖)、`singles`/`rows`(key=`(component名字,id)`，`BuildingComponentKeyHash`；同一key下的Room归到同一个Component，哪怕分布在不同楼层)。`AssignFloors`向量版按最深地下室到最高层排列，多出的条目忽略，不够的层不赋值。容器增长全发生在mod自己编译的代码里，`Building::Layout()`只读。
- **`ElevatorCabinSpec`/`AssignElevatorCabin`**：`shaftIndex`是每层`.layout`模板`elevators`数组里的下标，**不从几何位置反推**，要求mod保证模板里elevator条目顺序跨楼层一致。同一口井可按楼层分段声明多台(范围不能重叠，重叠的后一条被跳过)。`scriptModName`/`milestoneNames`指定电梯自己的Script。`Building::Layout()`把`cabins`快照下来创建电梯实体`Cabin`，见`Source/Core/map/cabin.md`。
- **`SetSurfaceMaterial(tag, materialPath)`/`surfaceMaterials`**：地板/天花板标签->材质实例软路径。`.layout`的`ceilings`/`grounds`条目可写成`{"rect":[8个数], "tag":"roof"}`(旧的纯数组写法仍可用，无标签)；`Building::ReadFloor`实例化时按标签查这张表，把路径抄到每块`Ceiling`/`Ground`上(随存档保存)，Forever层渲染时用它，没有标签、标签没登记、或材质加载失败都退回楼层`FloorAssetSpec`指定的材质/框架默认的白色。在`Layout()`里调用。
- **`doorSpecs`**：tag->`DoorSpec`，负责非single/row元素(目前只有corridor)上的门；layout里门洞带的tag查不到、或查到但mesh为空，就只是个洞(见`feature_spec.md`/`Source/Core/map/door.md`)。
- **树**：`LayoutTrees(quad, boundaryRoads)`(默认空)在整张地图生成完后由`Map::InitVegetation`对每栋building调用一次，mod里调`AddTreeSingle/AddTreeLine/AddTreeArea`只记请求进`trees`(`TreeRequests`，见`feature_spec.md`)。参考系是整块建筑quad(不是楼体footprint)，按朝北写，Core按建筑朝向换算。
- **`AddDecoration`/`decorations`**：放自定义StaticMesh(如成品建筑模型)，位置/yaw按face=2定义，Core按最终朝向换算；`meshPath`为空直接忽略；只在近处LOD显示。`Building::Layout()`在`mod->Layout()`之后读取。
- **存档钩子**：`SaveState`/`LoadState`/`GetStateVersion`/`GetInstanceCounter`/`SetInstanceCounter`，默认什么都不存；公共声明字段由Core直接快照，只有mod私有数据走钩子，见`Source/Dependence/common/mod_state.md`。

## 依赖关系

- 依赖：`map/feature_spec.h`(`DoorSpec`/`DecorationSpec`/`TreeRequests`)、`map/general_geometry.h`(`Quad`/`Road`/`AREA_TYPE`/`PlacementEmitFunc`)、`common/mod_state.h`。
- 被谁依赖：`building_factory.h`、`Source/Core/map/building.h`/`.cpp`(`Building::Layout()`读取`floors`/`singles`/`rows`/`cabins`/`doorSpecs`/`decorations`/`stationMod`)、各具体`XxxBuilding`子类(`Source/Basic/map/`)与外部Mod。

## 待办/后续阶段

- 无。
