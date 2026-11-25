# ForeverVegetationFrameworkComponent

## 职责

全图植被的统一渲染。Core的`Map::GetVegetation()`里存着已经展开好的每一棵树（见
`Source/Core/map/vegetation.md`），这个组件把它们按mesh分组，每个StaticMesh一个
`UInstancedStaticMeshComponent`，所有树都是ISM的实例。

## 关键设计

- **加载放在最开始，运行时只增删实例**——`GenerateVegetation(map)`先对`GetMeshPaths()`逐个
  `LoadObject<UStaticMesh>`并建好ISM(加载失败打Warning，那个mesh的树全部跳过)，再按mesh分组
  每个ISM只调一次`AddInstances`批量添加(只触发一次渲染数据更新)。之后`AddTree`/`RemoveTree`只增删
  实例；`AddTree`遇到没预加载过的mesh会现场加载一次并打Warning，正常情况下不应该发生。
- **只支持StaticMesh**——ISM只能实例化StaticMesh，蓝图Actor不能这样批量渲染。
- **树id和实例下标的映射**——ISM开了`SetRemoveSwap()`(基类ISM支持)，删除时最后一个实例会被挪到被删的位置；
  `treeSlots`(treeId→mesh+实例下标)和`instanceTreeIds`(每个ISM的实例下标→treeId)两张表在
  `RemoveTree`里一起修正。初始树沿用Core的`TreeInstance::id`，运行时新增的id接着往后数。
- **Transform**——位置=地图element坐标×1000，Z=`Map::SampleHeight`(含细化高度)×1000，
  `FRotator(0, yaw°, 0)`，等比缩放直接用Core算好的最终scale(随机已经在Core里叠加过)。
- **剔除距离**——`cullDistanceStart/End`(UU，默认0~60000)是`EditDefaultsOnly`，ISM按实例
  剔除距离隐藏远处的树(没有HISM那种层次树/逐实例LOD，2026-11-18由HISM改成ISM：树初始化后不再增删，不需要层次树)。mobility设成Static，和FrameworkActor的sceneRoot一致。
- 调用时机：`AForeverFrameworkActor::EnsureMapGenerated`在`GenerateBuildings`之后调用。

## 风(树随风摇摆)

树的摇摆是材质里的World Position Offset(按顶点相对树根高度加权的正弦位移，相位加
`PerInstanceRandom`)，参数全部来自`/Game/3rdParty/Mossy_Tree/Materials/MPC_Wind`
(`VEGETATION_WIND_COLLECTION_PATH`)：标量`WindPhase`、`WindStrength`，向量`WindDirection`(RG水平分量)。
`UpdateWind(deltaTime)`由`AForeverFrameworkActor::Tick`每帧调用(在`UpdateSky`之后)，
- **相位在C++里累加**(`windPhase += dt*windSpeed`，对2π取模)，材质只读相位；不让材质用`Time*速度`，
  是因为运行时改风速会让相位跳变、树突然抽一下。
- `windSpeed`(默认1.5)/`windStrength`(cm，默认60)/`windDirection`(默认(1,0.4,0)，Z忽略、归一化)是
  `EditAnywhere`基准值；天气再乘上倍数(见下)，阵风以后可在这里叠噪声。
- MPC加载失败/缺参数各只打一次Warning，不会每帧刷。
- 材质高度遮罩=顶点`Absolute World Position`.Z − `Object Position`.Z(实例的树根)，再除以`TreeHeight`、
  `Saturate`、`Power`，不需要每实例自定义数据。WPO链里遮罩和Sine必须用`Multiply`，不是`Add`。
- 材质里的`TreeHeight`(树高，用于高度遮罩)是材质自己的ScalarParameter，不归C++管。

## 依赖关系

- 依赖：`map/map.h`(`Map::GetVegetation`/`Map::SampleHeight`)、UE `HierarchicalInstancedStaticMeshComponent`。
- 被谁依赖：`AForeverFrameworkActor`。

## 待办/后续阶段

- 行道树的位置/间距/缩放都是粗估，还没有避开建筑出入口(RoadOpening)。
- 运行时增删目前只有组件接口，还没有接到Story/Change之类的调用方。

**摇摆权重用每棵树的根高度（2026-11-25）**：材质(`Mossy_Tree/Materials/base`)里权重是`saturate((顶点世界Z-树根Z)/TreeHeight)^p`。原来树根Z用的`Object Position`，在ISM上是整个组件包围盒的中心而不是每棵树的根，所以高海拔处(如贴合路面抬高的地形)的树根权重不为0、树根也跟着摇。现在每个树ISM设`NumCustomDataFloats=1`，`GenerateVegetation`/`AddTree`把树根世界Z(cm)写进自定义数据0，材质的Subtract的B改接`PerInstanceCustomData[0]`(脚本改的，原材质备份过)。

## 天气对风的影响
`SetWindScale(strengthScale, speedScale)`由`UForeverWeatherFrameworkComponent`每帧写入：`WindStrength`写MPC时乘`strengthScale`，相位累加乘`speedScale`。`GetWindDirection()`给雨雪粒子借用风向。1=微风基准。
