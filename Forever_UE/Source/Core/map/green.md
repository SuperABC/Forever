# green.h / green.cpp

## 职责

`Green`是一块已落地的**绿化区**：路网文件(`.roadnet`的`greens[]`)指定的闭合2D多边形，独占一个`GreenMod`实例。绿化区可以是任意(含凹)多边形，不局限在地块里。`Green`持有多边形、连路信息、保留下来的市民活动圆、自定义装饰物快照；真正的"选哪个mod、布置、接导航"在`Map::InitGreens`(见`map.md`)。

## 关键设计

- **多边形规整**：读入后用`EnsurePositiveArea`统一成有符号面积>0(`Dependence/map/polygon_util.md`)；顶点顺序翻转时每条连路边的下标按`(n-2-i+2n)%n`重映射(编辑器导出时已规整过，Core再做一次兜底)。自交/少于3点的多边形跳过并打警告。
- **连路(`Link{edge, road}`)**：多边形第`edge`条边(`points[edge]→points[(edge+1)%n]`)连到一条`Road`。可以多条边连多条路，也可以一条都不连(`IsLinked()==false`，只做景观，没有行人入口、不能地址导航)。接入点**不存**，按每个圆各自算(见下)。
- **`Layout(xy, links)`**：先拷多边形/包围盒，在mod实例的`RandomScope(GetInstanceStream(mod))`里调`mod->Layout(shape)`，然后把mod输出整理成Core自己的快照(读档时mod是空实例，运行时只读快照)：
  - **装饰物**：包围盒系→世界坐标(`position[0]*宽+position[1]`，高同理)，**多边形外的丢弃**；`yaw`度→弧度。`z`是离地高度，UE层再加地形高度。
  - **活动圆**：圆心必须在多边形内；没连路的绿化区圆原样保留(不建导航点)；连了路的，对每个圆按"圆心到每条连路边**线段**的最短距离(垂足在段外取端点)"从近到远试，取第一条使"圆心→最近点"整条连线都在多边形内(`SegmentInsidePolygon`，凹角会被判出界)的边，记下`edge/footX/footY`，都不行就**丢弃该圆**。保留的圆重新编号(`GetCircles()`下标)。
- **地址**：连了路==`<首条连路的路名> -1 <绿化名>`，圆==再加` <圆序号>`(`GetCircleAddress`)；没连路==绿化名本身(不支持寻址)。`-1`让`LocateRoom/Building/Zone`返回空，`Map::LocateGreen`再接手(`Roadnet::LocateLot`对`index<0`的既有处理)。
- **导航节点**(`Circle::centerNode/footNode`)归`Map::navAnchorNodes`所有，`Green`只借用指针；由`Map::InitGreens`建：圆心→垂足一条行人`Connection`，垂足投影到连路边的那条路上、按物理顺序接入路网(`pendingBuildingRoadAccess`→`FlushPendingBuildingRoadAccess`，必须在异步寻路开始前完成)。只有行人图，**车辆不进绿化区**，车规划回退到最近车道点。
- **随机点**：`RandomPointInCircle(i)`圆内均匀(半径取sqrt)，用于导航终点；调用方负责随机作用域(UE层的测试入口用`FMath`)。
- 不记录"玩家在哪块绿化里"，也没有包围盒注册表；只有市民对象记自己在哪块绿化(`Citizen::GetCurrentGreen()`)。

## 存档

`map_save.cpp`在structures段末尾追加greens块：mod id+mod状态、名字、多边形、links(road下标)、保留的圆(含节点id)、装饰物快照；读档`CreateGreen`+`ReadModState`再回填，**不重跑Layout**。按项目约定没升版本。

## 依赖关系

- 依赖：`map/green_mod.h`、`map/green_factory.h`、`map/polygon_util.h`、`map/general_geometry.h`。
- 被谁依赖：`map/map.h/.cpp`(`greens`、`InitGreens`、`LocateGreen`、`PlanPedestrianRouteToGreen/FromGreen`、植被展开)、`map_save.cpp`、`populace/citizen.*`(`currentGreen`)、UE层`ForeverGreenFrameworkComponent`/`ForeverTrafficFrameworkComponent`。
