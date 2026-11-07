# TransitVehicleElement

## 职责

公共交通线路车辆(公交/火车/飞机)在场景里对应的 `ATransitVehicleElement`(`APawn`)：**运动学**地沿 `Route` 算出的 transform 移动，不模拟物理；公交/火车(骨骼/静态网格且 `VehicleMod::boardable`)可被玩家搭乘当乘客。

## 关键设计

- **运动学跟随，不是物理驱动**：`Tick` 每帧读 `vehicle->GetTransform()`(Core 层 `Route::Update` 按时刻表算好的地图单位坐标+偏航角)乘 `TRANSIT_WORLD_SCALE`(1000，与 `ForeverTrafficFrameworkComponent.cpp` 的 `TRAFFIC_WORLD_SCALE` 同值，各 `.cpp` 各维护一份)，再读 `vehicle->GetPitch()` 设俯仰(火车沿坡道/隧道口倾斜，其余恒为 0)，一并 `SetActorLocationAndRotation`。与 `AVehicleElement` 不同，不写回 Core。
- **外观按 `VehicleMod` 字段选**(`Init`)：`transitMeshPath` 非空时先 `StaticLoadObject` 再判断类型(直接 `LoadObject<T>` 类型不符会刷警告)——骨骼网格(公交 `SKM_paz_652`)用子组件 `skeletalBody`；静态网格：可搭乘(火车)用子组件 `staticBody`，不可搭乘(飞机)直接放 `bodyMesh`。子组件的缩放只作用在自己身上，root 保持 1 倍，触发盒/相机才不被缩。路径为空或加载失败(mod 的 Plugin 没挂载上/路径打错)退化为 `/Engine/BasicShapes/Cube` 按 `sizeX/Y/Z` 除以 100 缩放的占位。`/Test/...` 这类包路径前缀由 `ForeverModSubsystem` 运行时挂载出来(见 `Source/Core/common/config.md`"GetPlugins/GetPakFiles")。
- **`meshScale`/`meshYawOffsetDegrees`**：真实网格按 `GetMeshScale()` 统一缩放；`GetMeshYawOffsetDegrees()` 修正模型建模正前方与行驶方向不一致。**不能在 `Init` 里对 `bodyMesh` 设 `SetRelativeRotation`**：`bodyMesh` 是 `RootComponent`，`Tick` 每帧设的是 Root 的世界旋转，会立刻覆盖；所以缓存进 `meshYawOffsetDegrees`，在 `Tick` 里叠加到 yaw。
- **碰撞**：`bodyMesh`/子组件都 `NoCollision` 且不影响导航；可搭乘的车另建只 overlap 的触发盒。
- **可搭乘(`BuildBoardingComponents`，由骨骼/静态网格包围盒决定)**：触发盒=包围盒(乘 `meshScale`)外扩 XY150/Z50；相机弹簧臂挂车顶中心，臂长 1100，跟随控制器旋转。玩家的 `AForeverCharacter` 进盒 -> 把车辆 options 推给 `UMeetOptionWidget`(上车选项由 `vehicle_bus.script` 在停站时通过 `add_option`/`remove_option` 控制)；Overlap 回调只在玩家当前 pawn 且是 `AForeverCharacter` 时响应。
- **搭乘**：`UForeverTrafficFrameworkComponent::ApplyBoardVehicle` 让玩家 `Possess` 本 Pawn，`PossessedBy` 只加 `IMC_MouseLook`(没有 `IMC_Default`，WASD 无效)、绑鼠标视角和 F/滚轮选项输入；乘坐期间**一直显示** options(不需要触发盒)；没有 Q 下车键，下车只走 script 的"下车"选项(`leave_vehicle` Change)或 `force_leave`。`ShowOptions` 先 `RemoveName` 再逐个 `AddOption`，不用 `ReplaceOptions`(它在名下没有按钮时是空操作，停站前选项为空，之后新增的选项会丢)。存档记 `controlledVehicle`，读档走 `ApplyBoardVehicle(bSilent)`，位置由 Core Route 算，不用存档位姿覆盖。
- **运行中选项变化**：`add_option`/`remove_option` 后 `AForeverFrameworkActor::RefreshInteractOptions` 调 `RefreshOptions()` 重新烘焙快照；玩家在盒内或乘坐中时同步刷新屏幕上的选项。Overlap 回调只读 `vehicleNameOnly`/`cachedOptions` 快照，不解引用 Core 对象。
- **挂起 `SetSuspended`**：线路公交（蓝图是 `AVehicleElement` 子类时）在玩家附近(地形 `busPhysicsLodLevel` 级 LOD 范围内)换成物理 `AVehicleElement` + `ABusController`，玩家搭乘的是那辆物理公交(`SetRideMode`)；这个运动学 Actor 被挂起(隐藏、关 Tick、关触发盒并撤选项)，出范围再恢复。搭乘中的不应被挂起(调用方保证)。火车/飞机仍沿用这里的搭乘逻辑。
- `vehicle` 是裸指针引用，不持有所有权，`EndPlay` 只置空。

## 依赖关系

- 依赖：`Core/traffic/vehicle.h`(`GetTransform`/`GetPitch`/`GetSize`/`GetTransitMeshPath`/`GetMeshScale`/`GetMeshYawOffsetDegrees`/`IsBoardable`)、`UMeetOptionWidget`、`Input/ForeverKeyBindingSubsystem`(`MeetOptionUp/Down/Select`)、`IA_Look`/`IA_MouseLook`/`IMC_MouseLook`。
- 被谁依赖：`ForeverTrafficFrameworkComponent` 的 `GenerateVehicles`(对 `vehicle->GetRoute()` 非空的车辆生成)、`ApplyBoardVehicle`/`ExitVehicle`。

## 待办/后续阶段

- 不支持操控(`VehicleMod::drivable` 预留)；路线车改成 AI Controller 物理驾驶后，事件/Change/script/触发盒可复用。
