# zone_basic.h / zone_basic.cpp

## 职责

`ResidenceZone`：Zone域唯一的默认实现，一个带围墙/大门/内部道路的园区测试场景，园区内放两栋住宅楼。

## 关键设计

- **只有一个通用占位类型**，用来验证"显式指定矩形"这条分配链路(Zone没有权重/CDF方式，见`Dependence/map/zone_mod.md`)。文件名仍按`<concept>_basic`命名，让`Basic/map`下每个concept都对应唯一一份`xxx_basic.h/.cpp`。
- **"一个本体独占一个mod实例"模型**：static的`Assign()`(决定占哪块lot、哪个方向、边距和进深)和非static的`Layout()`(围墙/大门/内部道路/内部建筑的实际布局)分开。
- **`Assign`**：目标面积固定常量`TARGET_ACREAGE`(20000)，换算成近似正方形边长；从WEST开始找第一个有边界路、临街长度够用的方向，**贴着这条边的起点那个角、不居中**，把完整的大块空地留给后面的火车站/机场。不按`lot->GetArea()`筛选地块类型，任意有边界路的lot一视同仁(井字中心lot也允许)。
- **`Layout`**：大门放在`direction`这条边正中间(滑动双开门)，该边大门以外和其余3条边整段铺围墙(围墙进深`WALL_DEPTH`=0.2、大门宽度占边长`GATE_WIDTH_RATIO`=0.2)；垂直于大门所在边的一条内部人行道从大门位置贯穿到正对面的边，把园区分成两半；每半各放一个`building_residence_high`(用字符串明文引用，因为Basic层文件间不允许互相`#include`)，天然复用住宅的`Layout()`，和独立建筑在`ForeverBuildingFrameworkComponent`里走同一套近/远LOD渲染。大门只接人行道，不声明车行出入口(车行/人行出入口是三个独立数组，`Map::ConnectZoneAccessPoint`按`isVehicle`分别处理)。
- **`LayoutTrees`**：沿四面栅栏内侧每隔一段种一棵树，大门那一段让出来。坐标按"朝北"写，Core按大门方向旋转。
- **自定义模型/树的测试配置**用`#define`(`ZONE_KIT_*`急救包、`ZONE_TREE_*`树的网格路径/缩放/间距/内缩)，先给默认值，看过效果再调。
- 挂示例脚本`entity_event`(进出事件只发给这个实体自己的`Script`)，见`Resource/Story/entity_event.script`。
- **内部布局细节**：两栋内部建筑沿内部道路两侧各占一半，沿路方向长`0.6*side`、垂直方向深`0.6*(side/2)`，不贴围墙/内部道路；`roadIndices`记它贴着`internalRoads[0]`的哪一面，两半相反，`direction`取该面朝向。内部道路宽度复用大门宽度`side*GATE_WIDTH_RATIO`。大门是滑动双开门(`Cube`资产占位)。
- **`LayoutTrees`**：沿西/东边从北角种到南角，南边各往里收一个间距避开已种的角，北边(大门边)分成大门两侧两段，开口两侧各再让出半个间距(和行道树避让开口同一规则)。树的位置用`PointParams`的ratio+offset，坐标按朝北写，原点在quad西北角(+X东、+Y南)。
- **急救包装饰**(`ZONE_KIT_*`)：大门开口两端各一个，离北边`ZONE_KIT_INSET`。

## 依赖关系

- 依赖：`map/zone_mod.h`、`Lot`/`Quad`/`PlacementEmitFunc`(`general_geometry.h`)。
- 被谁依赖：`basic.cpp`注册(依赖`ResidenceBuilding`)；`Core/map/zone.h`消费。

## 待办/后续阶段

- `ZONE_*`的网格路径和数值是测试配置，看过效果再调。
