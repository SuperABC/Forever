# vegetation_spec.h

## 职责

植被请求的数据结构：`TreeLineRequest`（沿线段均匀种树）和`TreeAreaRequest`（在一个Quad里随机种树）。
mod只负责"提请求"，Core把请求展开成一棵棵树，UE层统一渲染，见`Source/Core/map/vegetation.md`。

## 关键设计

- **mod-facing的接口挂在已有Mod类里，不新增基类**——`RoadnetMod::AddTreeLine`、
  `BuildingMod::AddTreeArea`都是非虚工具方法，和`RoadnetMod::AddHatch`/
  `BuildingMod::AssignFloor`同一个写法：往mod对象自己的`treeLines`/`treeAreas`里追加，
  Mod类的继承关系和虚函数签名都不变。
- **只记请求，不在mod里展开随机**——每棵树的随机旋转(绕Z轴0~2π均匀)、随机缩放(在请求的
  `scale`基础上乘一个均值1、σ=0.1、夹到[0.8,1.2]的高斯随机数)、"不种在水面/挖洞格子上"的
  过滤全部由Core统一处理，规则只有一份，以后调整不用改任何mod。
- **跨DLL安全**——mod只往自己对象的vector里写，Core只按值拷贝/只读遍历，不跨DLL释放内存，
  符合CONVENTIONS.md §4。
- **`meshPath`是StaticMesh资产软路径**（如`/Game/3rdParty/Mossy_Tree/StaticMeshes/
  mossy-tree.mossy-tree`）——UE层每个mesh一个HISM实例化渲染，蓝图Actor不能这样实例化，
  所以不支持蓝图路径。
- **坐标约定**：都是地图element单位。`TreeLineRequest`是世界坐标；`TreeAreaRequest::quad`沿用
  发出者自己的坐标系——BuildingMod传的是`LayoutTrees()`拿到的building quad(中心是世界坐标、尺寸沿
  楼体局部轴向)，Core展开时绕quad中心旋转`building->GetRotation()`。

## 依赖关系

- 依赖：`map/geometry.h`（`Quad`）。
- 被谁依赖：`roadnet_mod.h`、`building_mod.h`、`Source/Core/map/vegetation.h`。
