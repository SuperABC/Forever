# TransitVehicleElement

## 职责

公共交通线路车辆(公交/火车/飞机)在场景里对应的Actor——纯展示物，运动学地沿`Route`算出来
的transform移动，不模拟物理、不接受操控/搭乘。

## 关键设计

- **运动学跟随，不是物理驱动**：`Tick`每帧直接读`vehicle->GetTransform()`(`Core`层
  `Route::Update`早已按时刻表算好的世界坐标+偏航角)写`SetActorLocationAndRotation`，
  和`AVehicleElement`(真实可驾驶载具，`bSimulatePhysics=true`的骨骼网格)完全是两套机制——
  这里没有刚体、没有碰撞响应，`Tick`只是把Core算好的数字誊抄到Actor transform上。
  `TRANSIT_WORLD_SCALE`(=1000)和`ForeverTrafficFrameworkComponent.cpp`的
  `TRAFFIC_WORLD_SCALE`是同一个值、同一套"地图单位→UE厘米"换算，这个项目的既有约定是每个
  需要它的`.cpp`文件各自维护一份，不额外抽公共常量。
- **纯展示物，不参与碰撞/导航**：构造函数里`bodyMesh->SetCollisionEnabled(NoCollision)`+
  `SetCanEverAffectNavigation(false)`——这次不做操控/搭乘，不需要挡住别的东西，也不需要被
  别的东西的重叠检测测到，见`VehicleMod::drivable`/`boardable`字段(这次公交/火车/飞机三个
  `VehicleMod`都设成`false`，预留给以后真的要做"公共交通能上下车"时用)。
- **立方体占位，按`VehicleMod::sizeX/Y/Z`缩放；`transitMeshPath`非空时换成真实静态网格**：
  `Init()`先检查`vehicle->GetTransitMeshPath()`，非空就`LoadObject<UStaticMesh>`，成功则
  直接用这个mesh；留空(Bus/Train目前都是空)或加载失败(mod的Plugin没挂载上/路径打错)才
  退化回`/Engine/BasicShapes/Cube`按`sizeX/sizeY/sizeZ`除以Cube默认边长(100cm)算缩放系数
  的占位方案。`PlaneVehicle`这次接的是独立mod(`Forever_Mod/Test/UE/Test`，一个只含
  `Content`的迷你UE工程里的`Test`Plugin)迁移过去、重命名成`airplane`的测试飞机模型，路径
  `/Test/Airplane/StaticMeshes/airplane.airplane`——`/Test/`这个包路径前缀是
  `ForeverModSubsystem`运行时挂载出来的(编辑器/PIE直接挂Plugin的Content目录，打包后挂该
  Plugin自己`pak.bat`cook出来的`Test.pak`)，不是这个主工程`Content/`下的东西，见
  `vehicle_mod.h::transitMeshPath`/`Source/Core/common/config.md`"GetPlugins/GetPakFiles"
  一节。以后要再换/再加真实网格资产，只需要照抄这个模式，不需要改这个类本身。
- **`VehicleMod::meshTransform`修正真实mesh的缩放/朝向，和`sizeX/Y/Z`互不影响**：
  真实模型(`transitMeshPath`非空时)加载成功后，`SetWorldScale3D`按
  `GetMeshScale()`(统一缩放系数，不是`sizeX/Y/Z`)缩放。这个修正解决的是"模型自己建模时的
  正前方"和"Actor正前方(沿行驶方向)"没对齐的问题，`PlaneVehicle`的测试飞机模型本身是
  "横着"建的，需要叠加90度(具体+90还是-90由实测确定，机头朝前还是朝后一次性判断)。
  **`GetMeshYawOffsetDegrees()`不能在`Init()`里直接对`bodyMesh`调`SetRelativeRotation`**
  ——`bodyMesh`是这个Actor的`RootComponent`(构造函数`RootComponent = bodyMesh;`)，
  `Tick()`里每帧`SetActorLocationAndRotation`直接设的就是`RootComponent`的世界旋转，
  `Init()`时设的任何`SetRelativeRotation`值会立刻被下一次`Tick`覆盖掉、完全没有效果
  (PIE/打包exe实测反馈"旋转没有生效"才发现)。修复：`Init()`里把这个偏移缓存进成员字段
  `meshYawOffsetDegrees`，`Tick()`每帧算朝向时叠加`yaw + meshYawOffsetDegrees`，和
  `GetTransform()`算出来的行驶方向一起设进同一次`SetActorLocationAndRotation`调用，不是
  设在`bodyMesh`自己的局部旋转上。
- `vehicle`字段是裸指针引用，不持有所有权——`EndPlay`只置空不delete，`Vehicle`对象的生命
  周期归`Traffic`管(和`ACitizenElement`/`AVehicleElement`同一套"Forever层Element只引用、
  不管理Core对象生命周期"的安全原则)。

## 依赖关系

- 依赖：`Core/traffic/vehicle.h`(`GetTransform`/`GetSize`)。
- 被谁依赖：`Forever/Framework/ForeverTrafficFrameworkComponent.cpp`的`GenerateVehicles`
  (对`vehicle->GetRoute()`非空的车辆生成这个Actor，而不是`AVehicleElement`)。

## 待办/后续阶段

- 不支持操控/搭乘——`VehicleMod::drivable`/`boardable`已经预留了这个开关，真要做的话这个
  类需要补充和`AVehicleElement`类似的`Possess`/上下车逻辑，这次不实现。
