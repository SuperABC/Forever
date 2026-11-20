# BuildingElement.h / .cpp

## 职责

每栋 building 一个专属的 `ABuildingElement`(`AActor`)，由 `UForeverBuildingFrameworkComponent::GenerateBuildings()` 在 `Map::InitBuildings()` 之后为每栋楼 `SpawnActor` 并 `Init(building, framework)`。它自己承担：

- 两级 LOD 状态机(`ELod::Far` 只有外壳：墙体/地板/天花板，开局全城建好常驻 / `Near` 外壳+楼梯/坡道网格+电梯轿厢+自定义模型+门)，按水平距离判定：近处 16 进/32 出(地图单位)与距离触发的近远切换，全部在自己的 `Tick` 里跑。
- 外壳(墙体/地板/天花板 ISM 实例，常驻)、近处专有组件(楼梯坡道网格、装饰模型)、电梯轿厢 mesh，全部 attach 到自己的 `elementRoot`。
- 电梯：按 Core 的 `Cabin` 状态推进行程、摆放轿厢，以及电梯正面/轿厢内的交互触发盒(见 `Framework/ForeverBuildingFrameworkComponent.md`"电梯轿厢"一节与 `Core/map/cabin.md`)。
- building 自己的进入/离开检测碰撞盒(`BuildCollisionBox`)，以及这栋楼所有 Room 各自的进入/离开检测碰撞盒(`BuildRoomCollisionBoxes(level)`)——都随近处 LOD 生成/销毁：进入近处时 `BuildTriggers` 建楼盒、各层 `BuildFloorExtras` 建该层房间盒(只排有楼梯/坡道/房间的层)，离开近处时 `ClearNearSections` 一并销毁；新建后 `UpdateOverlaps()` 补发玩家已在里面的 BeginOverlap。因此靠这些盒子反查位置/房间(`DescribeLocation`、切换操控后判定房间)只在近处有结果。园区盒不在这里，常驻在 `ForeverZoneFrameworkComponent`。

墙体开洞分段算法/坐标转换/材质网格解析/电梯动画公式见 `ForeverBuildingFrameworkComponent.md`；这份文档只记 `ABuildingElement` 本身的设计取舍。

## 关键设计

- **为什么每栋楼一个独立 Actor**：UE 对"同一个 Actor 根组件下挂大量 Static 简单碰撞子组件"会做碰撞体焊接，新增子组件的开销正比于该 Actor 已有组件总数。全地图共用一个 owner 会让所有 building 互相拖累；每栋一个 Actor 把影响范围关进"单栋楼自己的组件数"(完整排查见 `ForeverBuildingFrameworkComponent.md`"性能"一节)。Room 碰撞盒同理，也挂在各自所在楼的 `elementRoot` 上，`UForeverRoomFrameworkComponent` 是空骨架。
- **全地图共用的东西不下放，经 `framework` 弱引用回调**：材质/网格默认值及其按软路径的缓存(`ResolveMaterial`/`ResolveMesh`)、LOD 距离/电梯速度等配置留在 `UForeverBuildingFrameworkComponent`。缓存的意义是"同一软路径只 `LoadObject` 一次"，每个 Element 各一份等于没缓存。用 `TWeakObjectPtr` 而非裸指针(成本几乎为零，更安全)。
- **墙体/地板/天花板 cube：全城合并 ISM**：Element 自己不建 ISM，`AddCubeInstance`/`AddWallInstance` 只把 world transform + 朝向(cos,sin) + 材质(或 `FWallMaterialKey`)经 `framework->QueueShellCube/QueueShellWall` 排给框架组件，由框架按(格子, 材质)合并成全城共享的 `UInstancedStaticMeshComponent`。这些实例是**外壳**：世界生成完后由 `ELodOpType::BuildShell` 一次建完整栋楼所有楼层(`BuildFloorSection(i, shell=true, extras=false)`)，最后 `framework->NotifyShellBuilt()`；全城最后一栋建完时框架一次性提交，之后永远常驻；Near/Far 切换不动外壳。
- **阴影**：外壳 ISM 是全城共享的，阴影不再按楼切换，改由框架按格子距离玩家远近开关(见 `ForeverBuildingFrameworkComponent.md`)。
- **全局 LOD 操作预算 `TryConsumeLodOpBudget(bool isDoorTierOp)`**：每个 Element 自己维护两个 `TQueue<FLodOp>`——`floorTierOpQueue`(墙体/楼梯/电梯井/地板天花板/装饰/远近切换)和 `doorTierOpQueue`(只放 `BuildDoorBatch`)，但执行速度受框架组件的共享计数器 `frameOpBudgetRemaining`(每帧重置为 `maxLodOpsPerTick`)节流，避免大量 building 同帧穿越阈值时集中爆发。**外壳走另一个预算** `TryConsumeShellBudget()`(`maxShellOpsPerTick`，默认每帧 8 栋)：开局全城一千多栋要在冻结期间建完，不能受"每帧 1 条"限制；`Tick` 用 `Peek` 看队首类型选预算。外壳排队期间 `transitionPending=true` 并计入 `NotifyLodTransitionStarted/Finished`，`IsLodSettled()` 因此等全城外壳建完才为真。`Tick` 里**必须先判断自己队列非空再申请预算**(`while (!queue.IsEmpty() && framework->TryConsumeLodOpBudget(...))`)，反过来会让无事可做的 building 白白消耗全局预算。
- **门不在这里**：门(建筑门/房间门/园区大门)已经从建筑里独立出来，由 `ForeverDoorFrameworkComponent` 按玩家距离(跟随市民流式距离)流式生成/销毁，和建筑近/远 LOD 无关，也不再有 `doorTierOpQueue`/`BuildDoorBatch`/`nearDoorsByFloor`。门洞仍由 `BuildWallsForElement` 照常挖出，不管有没有门扇，见 `ForeverDoorFrameworkComponent.md`/`Source/Core/map/door.md`/`DoorComponent.md`。
- **自定义模型(`BuildDecorations`)**：mod 用 `BuildingMod::AddDecoration` 指定的 StaticMesh(`Building::GetDecorations()`，世界摆位 Core 已按朝向换算)远→近时排一条单独的 `ELodOpType::BuildDecorations`，放在所有 `BuildFloorExtras` 之后、`BuildElevatorCabins` 之前，进 `floorTierOpQueue`，没有自定义模型的建筑不排。每个模型一个 Static 的 `UStaticMeshComponent`(`framework->ResolveMesh` 按软路径全局缓存)，Z 用 `BUILDING_HEIGHT_EPSILON`，缩放是资产原尺寸的倍数(不按单位立方体拟合)，碰撞用资产自带的；存在 `nearDecorations`，`ClearNearSections` 一并销毁——只在近处显示。
- **房间家具(`BuildRoomFurniture`)**：`Room::GetFurniture()`(世界摆位，Core 在建 Room 时按 mod 的 `RoomMod::Layout` 算好并存档)按**每层一条** `ELodOpType::BuildRoomFurniture`(`floorIndex` 复用)进 `floorTierOpQueue`，`Tick` 里和 `BuildFloorExtras` 同批，排在 `BuildElevatorCabins` 之前；没有家具的层不排。性能关键：**不是一件家具一个组件**——一栋塔楼几百个 room × 每户几十件，所以按网格软路径聚合成 `UInstancedStaticMeshComponent`(`nearFurnitureISM`，每栋楼每种网格一个，跨楼层复用，只 `AddInstances`)，`NoCollision`、`SetCastShadow(false)`、Static；网格走 `framework->ResolveMesh` 的全局缓存。Z 用 `BUILDING_HEIGHT_EPSILON + z*BUILDING_WORLD_SCALE`，yaw 弧度转度。`ClearNearSections` 把 ISM 整个销毁并清空 map——只在近处 LOD 显示，远处没有，和装饰一致(用户要求先跟建筑 LOD 显隐，之后按实测再细分距离/按层显隐)。资产在 `/Game/Asset/Room/{Mesh,Material,Texture}`，生成/导入见 `Tools/Furniture`。
- **0 层建筑**(`nearFloorCount==0`，如 `MoharBuilding`)：`Init` 不建进出触发盒(高度为 0)，也不排 `BuildShell`，远处什么都不显示。
- **退化的地板/天花板不摆**：layout 里宽或高为 0 的 ground/ceiling 矩形(例如 `y1==y2`)，任一边小于 `kMinSlabSize`(1cm) 就跳过。缩放为 0 的 `BlockAll` cube 画不出来，却会留下一片看不见、挡路的薄碰撞(斜坡出口曾因此被挡)。
- **`buildShell`(按层)**：`FloorAssetSpec::buildShell`(mod 在 `AssignFloor` 的 assets 里设，默认 true)为 false 时，`BuildFloorSection` 对该层跳过所有 `BuildWallsForElement`(房间/走廊墙与楼梯/坡道/电梯井道墙)和地板/天花板 slab；楼梯/坡道网格、电梯轿厢、Room 碰撞盒照常。该层远处同样没有外壳(只有别的层的)。给外观由 `AddDecoration` 成品模型提供的建筑用；注意 slab 同时是站立碰撞，关掉后站在楼里靠成品模型自己的碰撞。
- **地板/天花板按标签换材质**：`BuildFloorSection` 画每块 `Ground`/`Ceiling` 时用 `framework->ResolveMaterial(ground.GetMaterial(), 这层的floorMaterial/ceilingMaterial)`；`GetMaterial()` 是 Core 在 `Building::ReadFloor` 按 `.layout` 里的 `tag` 和 `BuildingMod::SetSurfaceMaterial` 登记的表抄上的软路径，空或加载失败就退回这一层资产/框架默认的白色(`PureColor`)。
- **地板/天花板 slab 的竖直微移**：墙仍占满 `floorBottom`~`floorTop`；`BuildFloorSection` 里地板 slab 整体下移、天花板 slab 整体上移 `BUILDING_SLAB_Z_SHIFT`(1 世界单位=1cm)，让 slab 的顶/底面不与墙顶/墙底共面(共面 z-fighting，顶层墙顶和屋顶共面时墙像冒出屋顶)。
- **`EndPlay` 安全性不依赖与框架 Actor 的调用顺序**：不同 Actor 之间 `EndPlay` 顺序不保证(`AForeverFrameworkActor::EndPlay` 会 delete Core 的 Map)。只需保证"自己的 `EndPlay` 一跑完 Tick 不再解引用 `building`"，所以 `EndPlay` 只把 `building` 置空。
- **Overlap 回调只用烘焙好的字符串**：`OnOverlapBegin/End` 只用 `buildingNameOnly`/`zoneNameOnly`，`OnRoomOverlapBegin/End` 只用 `roomBoxLabels`(`TMap<UPrimitiveComponent*, Room*>`，一栋楼多个 Room 盒)里的指针与现场生成的调试文本；回调里不解引用 `building`，关卡卸载顺序不保证 Core 对象还活着。`roomBoxLabels` 不能标 `UPROPERTY`(`Room*` 是 Core 类型，UHT 不认)，GC 安全由 Actor 自己的组件树持有保证。
- **碰撞盒尺寸配对**：building 盒三个方向各 +0.01 地图单位，Room 盒各 -0.01，贴合边界不会因完全重合而在 Overlap 判定上抖动。Room 盒垂直范围取该 Room 所在楼层的 Z 范围。
- **`BUILDING_HEIGHT_EPSILON`(1.f 世界单位)**：Building 没有真实高度数据，楼层 cube 统一从这个 grade 起算；必须小于地形 mesh 的 z-fighting 偏移(2.f)，否则地下室天花板会从地面冒出来，又必须大于 0 以免共面。这依赖"可建造 lot 的地形高度恒为 0"。
- **电梯轿厢位置同步**：`FCabin::lastSyncedZ` 记录上次同步的高度，没动就不再 `SetWorldLocation`——即使位置没变每次也会触发重叠更新，近处楼多电梯多时实测占游戏线程 16ms。

## 依赖关系

- 依赖：`Framework/ForeverBuildingFrameworkComponent.h`(`framework` 弱引用回调)、`Core/map/{map,building,room,door,general_geometry}.h`、`Element/DoorComponent.h`、`ProceduralMeshComponent`(远处 LOD)、`UInstancedStaticMeshComponent`、`UBoxComponent`、`UMaterialInstanceDynamic`、`TQueue`。
- 被谁依赖：`UForeverBuildingFrameworkComponent::GenerateBuildings()`(`SpawnActor<ABuildingElement>()`+`Init()`)；`AForeverFrameworkActor::DescribeLocation`/玩家房间判定通过 `FindBuildingOfBox`/`FindRoomOfBox`/`FindForBuilding`；`RefreshCabinOptions` 供 `RefreshInteractOptions` 调用。

## 待办/后续阶段

- Building 不采样所在位置的真实地形高度(`BUILDING_HEIGHT_EPSILON` 的前提是 lot 地形高度恒为 0)，将来 lot 允许在非 0 高度地形上时需按 building 位置采样 `SampleHeight()`。
- 单栋 building 内楼层特别多/大时，一次近 LOD 转场里该楼组件数仍从 0 涨到全部楼层之和；若卡顿明显，可把挂载粒度细化成"每层楼一个 Actor"，见 `ForeverBuildingFrameworkComponent.md`"性能排查"一节末尾。

- **墙按侧面材质（Wall）**：`BuildWallsForElement` 多一个 `const WallStyle* styles`（4 项，下标=墙朝向）。某面墙的 `WallStyle` 有任意材质时，整面墙（门窗洞口切出的每一段都一样）走 `AddWallInstance`，用 `/Game/Asset/Building/Mesh/Wall.Wall`（边长 100，同 Cube；槽 0 东 1南 2西 3北 4上下），四个侧面未配材质的退回这层的墙材质，槽 4 固定 `White`；框架里的 ISM 池按(格子, 5 个材质的组合)分组。没有任何标签的墙继续走原来的单材质 Cube；`Wall` 资产不存在（`ResolveMesh` 失败）时也退回普通 Cube。Wall 只随建筑整体 `rotation` 旋转，不随墙方向旋转，所以东/南/西/北始终对应建筑局部坐标轴。

- **墙向元素内侧偏移 1/4 墙厚**：`BuildWallsForElement` 里每面墙的跨墙坐标朝自己元素内部偏移 `BUILDING_WALL_THICKNESS * 0.25`（西/北边 +，东/南边 −），相邻元素共用的边上两堵墙中心错开半个墙厚、部分重叠但不共面，材质不同也不会 z-fighting；沿墙长度和高度不变。

- **墙/地板/天花板cube的PerInstanceCustomData**：框架的`CreateShellISM`给每个ISM设`NumCustomDataFloats=2`，一次性提交时为每个实例写入建筑朝向的水平单位向量`(cos, sin)`(索引0/1，和路网ISM约定一致)，材质里用`PerInstanceCustomData`读。同一材质的墙和地板/天花板共用一个池，所以数据也一并带上。
- 带标签的墙(框架的Wall池)同样设`NumCustomDataFloats=2`并写入`(cos, sin)`，约定与上面一致。

- **楼体空隙填充**：`BuildFloorSection` 画 1 楼(level 0)时，若 `building->GetGapMaterialPath()` 非空，用 4 块普通 cube 把 building quad 与楼体矩形之间的回形空隙铺上（左右两条占满 quad 高度，上下两条只占楼体宽度，互不重叠），z/厚度与 1 楼 `Ground` 完全一致（20cm，含 `BUILDING_SLAB_Z_SHIFT`），材质 `ResolveMaterial(gapPath, 地板材质)`；走 `AddCubeInstance`，所以同样带 `(cos, sin)` 的 PerInstanceCustomData。
- **电梯轿厢竖直定位（2026-11-20）**：轿厢底面对齐每层地板面(`GetFloorBaseZ`+`BUILDING_HEIGHT_EPSILON`)，只固定抬高 `kCabinFloorLift`(2cm，不再按层高5%)避免和地板共面 z-fighting，高度=层高−2×`kCabinFloorLift`(顶面也比天花板低2cm)；水平方向仍缩到 `kCabinScale`(0.95) 避开井道墙面。层高很高时轿厢地板不会再被抬高到走不进去。
