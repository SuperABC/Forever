# building_mod.h / building_mod.cpp

## 职责

`BuildingMod`：具体建筑类型的mod基类。Building这次有两种生成方式（显式占位+权重CDF随机
填充，见`Source/Core/map/map.md`"InitBuildings"一节），落地之后通过`Layout(...)`一次性
声明楼体footprint/楼层高度/LOD材质，以及每层楼内部的布局（`AssignFloor`/`AssignRoom`/
`ArrangeRow`），由`Source/Core/map/building.h`的`Building::Layout()`解析成真正的
`Floor`/`Room`/`Component`数据。

## 关键设计

- **一个本体独占一个mod实例**：和`Zone`一样，`RandomAcreage`/`GetAcreageMin`/
  `GetAcreageMax`/`GetPower`/`Assign`全部是不需要实例的`static`方法（通过
  `BuildingFactory::RegisterBuilding`的函数指针参数注册），只有真正落地成功时才会
  `CreateBuilding`一次。
- **`Layout`签名是`int& direction`引用参数**：`direction==-1`（权重CDF/`FillRemainder`
  落地，没有方向概念）时，如果这个mod的楼层内部布局需要按方向摆，应该从`boundaryRoads`
  里挑一个有真实边界路的方向回写进`direction`——building内部行人导航的`"outside"`端点
  要靠这个最终方向找到该连去哪条路（见`Source/Core/map/building.md`"行人导航"一节），
  不这么做的话`FillRemainder`落地的building的`"outside"`端点永远连不上道路网。
- **`BuildingFootprintSpec`**：楼体在Building自身`Quad`里的位置——两个比例float表示楼体
  中心在Quad里的位置(0.5,0.5=居中)，两个比例float表示楼体长宽相对Quad长宽的比例。不复用
  `Quad`类型本身（那样会把`Quad`正常的"中心点+尺寸"语义挪用成"比例袋子"，容易搞混）。
- **`FloorAssetSpec`/`FloorLayoutSpec`**：一层楼可配置的资产——墙体材质（这次不分内外墙，
  统一一份）、地板材质、天花板材质、楼梯3D网格、坡道3D网格，留空用
  `ForeverBuildingFrameworkComponent`的组件级默认值（墙/地/顶默认White，楼梯/坡道默认
  老工程同款的`Stair.Stair`/`Ramp.Ramp`）。`.layout`模板本身完全不携带材质/网格信息（和
  老工程一致），这几项必须由`AssignFloor`显式指定。
- **`AssignFloor`/`AssignFloors`(x2)/`AssignRoom`/`ArrangeRow`四个方法**，把调用记录进
  `floors`(`unordered_map<int,FloorLayoutSpec>`，key是人类直觉的层号：0=1楼，负数=地下室)、
  `singles`/`rows`(都是`unordered_map<pair<string,int>,vector<tuple<...>>,
  BuildingComponentKeyHash>`，key是`(component名字,id)`分组)——纯值类型容器增长，全在
  mod自己编译的代码里发生，不涉及跨DLL问题，和`footprint`/`floorHeights`同一个已验证安全
  的模式。`Building::Layout()`只读这几个容器，不会往里写。
- **`BuildingComponentKeyHash`**：`(component名字,id)`分组key的hash函数对象，
  `unordered_map`要用。

## 依赖关系

- 依赖：`Source/Dependence/map/geometry.h`（`Quad`/`Road`/`FACE_DIRECTION`/
  `PlacementEmitFunc`）。
- 被谁依赖：`Source/Dependence/map/building_factory.h`（`RegisterBuilding`的函数指针
  参数）、`Source/Core/map/building.h`/`.cpp`（`Building::Layout()`读取`floors`/
  `singles`/`rows`）、`Source/Forever/Framework/ForeverBuildingFrameworkComponent.cpp`
  （按`Building::GetMod()->floors[level].assets`解析这一层的材质/网格）、各具体
  `XxxBuilding`子类（`Source/Basic/map/building_basic.h`/`.cpp`等）。
