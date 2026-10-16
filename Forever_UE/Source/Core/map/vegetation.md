# vegetation.h / vegetation.cpp

## 职责

`Vegetation`是全图植被的统一存储：把mod提交的`TreeLineRequest`/`TreeAreaRequest`（定义在
`Source/Dependence/map/feature_spec.h`）展开成一棵棵`TreeInstance`（位置、绕Z轴旋转、最终缩放、
mesh下标）。由`Map`持有（`Map::GetVegetation()`），UE层`UForeverVegetationFrameworkComponent`一次性
读出来按mesh分组实例化渲染。

## 关键设计

- **随机规则只有这一份**——每棵树`yaw`在[0,2π)均匀随机；`scale = request.scale ×
  clamp(GetRandomNormal(1, 0.1), 0.8, 1.2)`(0.8~1.2正好是±2σ)。mod只描述"在哪里种几棵"，
  以后调整规则不用动任何mod。
- **展开规则**：`ExpandLine`沿线段均匀取`num`个点(含两端，`num==1`取中点)；`ExpandArea`在quad
  局部范围(中心原点、沿quad自己的轴向)均匀随机采样，再绕quad中心旋转`rotation`——和
  `Building::LocalToWorld`同一套cos/sin写法。
- **过滤交给Map**——`PlaceFilter`由调用方提供，目前是`Map::CanPlaceTree`：越界、格子有水面、
  格子有hatch(隧道口挖洞)都不种，过滤掉的点直接跳过(不补种)。所以河上的桥、隧道口附近的
  行道树会自动缺掉。
- **植被是整张地图最后一个阶段(`Map::InitVegetation`)**——在`InitBuildings`之后调用，这时路网上
  Zone/Building/小路加的开口(`Road::GetOpenings`)、园区、建筑、房间都已经齐全，mod布局树时能避开
  出入口。先`roadnet->LayoutTrees()`(调`RoadnetMod::LayoutTrees(Core持有的roads)`，再按值拷贝
  `mod->treeLines`)展开行道树；再对每栋building调`BuildingMod::LayoutTrees(quad, boundaryRoads)`，
  以`building->GetRotation()`展开`mod->treeAreas`(只读遍历mod的vector，跨DLL安全)。两个
  `LayoutTrees`都是有默认空实现的虚函数，不种树的mod不用管。
- **mesh路径去重**：`RegisterMesh`返回下标，`TreeInstance::meshIndex`指向`GetMeshPaths()`——UE层
  先把所有mesh一次性加载好、每个mesh建一个HISM，再批量添加实例。
- **不存高度**——Z由UE层按`Map::SampleHeight`(含细化高度)现场取，不在Core里重复存一份。
- `id`从0递增，目前只供诊断/UE层映射使用；运行时的增删在UE层组件里做，Core这份是初始列表。

## 依赖关系

- 依赖：`map/feature_spec.h`(Dependence)、`common/utility.h`(`GetRandom`/`GetRandomNormal`)。
- 被谁依赖：`map.h`(`Map::vegetation`)、`Source/Forever/Framework/ForeverVegetationFrameworkComponent`。
