# pier

## 职责
桥墩的数据结构（`Pier`）和选位算法（`PierPlacer`）。火车线路（`Route::PlacePiers`，见 `traffic/route.md`"桥墩"）和公路桥（`Roadnet::PlacePiers`）共用同一份，避免两套墩距/避让逻辑分叉。

## 关键设计
- **`Pier{x,y,topZ,radius,mesh}`**：墩顶是链上该点，墩底不存——UE 画时取当时地形高度（水面不管，贴水底），所以地形贴合后高度变了也对得上。`mesh` 空串=UE 侧默认圆柱（`/Game/Asset/Common/Mesh/Cylinder`）。
- **请求**：mod 用 `RouteMod::AddPier` / `RoadnetMod::AddPier(mesh, chains, defaultSpacing, minSpacing, radius)` 提交 `PierRequest`；链 = 一段桥的 `Connection` 序列（按值存）。公路版里 `roadnet_file` 把同一条用户路展开出的所有 `bridge` 段按 `chain` 编号归成一条链。
- **`PlaceAlongChains`**：每条链链首视为已有支撑，`s` 从 0 起，剩余长度 ≤ 默认墩距时收尾。每根墩先试默认墩距，碰到障碍就每次缩短 `PIER_SPACING_STEP`（0.5=5m）直到最短墩距；都不行再从默认墩距每次加长 0.5，直到超出链尾（找不到就停止这条链）。候选点 `topZ - SampleHeight < PIER_MIN_HEIGHT`（0.05=0.5m，贴地/隧道内/过山抬升段）不放墩，但 `s` 照样前进，此时不做障碍检测。
- **障碍**：由调用方给 `blocked(x,y)`；`SampleObstacle` 把一根 `Connection` 按固定间距采样成折线+AABB+半宽，`HitsObstacle` 判"到折线距离 < 墩半径+半宽"。只看 xy，不考虑多层桥，水面不影响，不判建筑。
  - 火车：路网所有路 + 其它 route 的轨道（本 route 请求链附近的轨道排除）。
  - 公路：**不属于请求链的所有路**（按 `(起点id<<32)|终点id` 两个方向匹配，链上的路段自己不算障碍）。
- **链必须是 Core 深拷贝**：mod DLL 里分配的 `Connection` 内部缓存不能在 Core 里直接改（跨 DLL 分配器），调用方先在 Core 里复制再传进来。

## 依赖关系
`map/general_geometry.h`（`Connection`、`PierRequest`），读地形高度用 `Map::SampleHeight`（`pier.cpp` 里 include `map/map.h`）。使用方：`traffic/route.cpp`、`map/roadnet.cpp`。
