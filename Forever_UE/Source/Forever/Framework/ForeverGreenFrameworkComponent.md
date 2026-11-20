# ForeverGreenFrameworkComponent

绿化区(`Green`，设计见`Source/Core/map/green.md`)的UE层。**只画自定义装饰物**：

- `GenerateGreens(Map*)`：遍历`Map::GetGreens()`，把每个`Green::GetDecorations()`(Core已按包围盒换算成世界坐标、多边形外的已丢弃)按mesh路径合并成`UInstancedStaticMeshComponent`(Static，`RF_Transient`，挂在FrameworkActor上)。
  Z = `Map::SampleHeight(x,y)` + `decoration.z`(离地高度，地图单位)，再乘1000转cm；缩放是资产原尺寸的倍数；`yaw`是弧度(Core已乘过π/180)，UE侧转度。
- 绿化区常驻、不做LOD(数量少)；mesh加载失败只警告一次，同一路径不再重试。
- 树不在这里：`Map::InitVegetation`把绿化区的树请求并进全图植被，由`ForeverVegetationFrameworkComponent`渲染。
- 活动圆、导航点不画；玩家导航线见`ForeverTrafficFrameworkComponent.md`"绿化区目标"。
- 调用时机：`AForeverFrameworkActor::GenerateGreenMesh()`，在"生成植被模型"之前(Z取地形高度，必须在地形生成之后)。读档同样重画(装饰物来自存档)。
