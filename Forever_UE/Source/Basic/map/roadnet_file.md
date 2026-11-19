# roadnet_file

## 职责
读 `Tools/MapEditor` 导出的 `.roadnet`（宽松 JSON）里的 `output` 段，原样生成路网：路口、城外连接点、道路（含隧道口/贴地形爬升/桥）、地块。配置示例：`"roadnet_file --path=C:/maps/city.roadnet"`（放进 `roadnet_mods`，路网是单选）。

## 关键设计
- **mod 只读结果，不做几何决策**：路口切分（T 口、十字）、贴合路的地形采样、头尾节点高度不同时的扁 S 坡控制点、桥墩链分组、地块矩形都在编辑器导出时算好。Core 不会自动建路口（`Map::InitRoadnet` 只对 `intersections` 里的点建 `RoadJunction`），所以编辑器必须把被接的路切开、让端点共用同一个节点 id。
- **`output` 结构**：
  - `nodes[]`：`{id, x, y, z, type}`，`type` = `intersection`（登记进 `intersections`）/ `extern`（登记进 `externs`，城外连接点）/ `plain`（普通衔接点，两条路首尾相接）。度数不为2的点编辑器一律导出为 `intersection`。
  - `roads[]`：`{name, from, to, controls:[[x,y,z,weight]…], lanes:[左车行,左停车,左人行,右车行,右停车,右人行], kind}`。`from/to` 是 node id；`controls` 是有理 Bezier 控制点（直线不写）；断面 mesh 由 `lanes` 拼出 `default_*`（`kind=tunnel` 加 `_T`），车道宽 0.5。
  - `kind`：`ground` 平地；`hug` 贴合地形（请求 `AddTerrainFit`，宽=路总宽，falloff 1.5，编辑器已按地形每 1 单位采样成一段）；`mouth` 隧道口（用平地的路面资产，整段 `AddTunnelHatch` 给地形开口，两侧用无人行道资产）；`tunnel` 隧道内（`_T` 资产，没有开口）；`bridge` 桥（桥墩由 Core 选位，见下）。一条编辑器里的路对应一条 `roads[]`（hug 除外）；头尾节点高度不同就是扁 S 坡（`controls` 里两个控制点，z 取头尾高度，两端水平）。这样隧道口/隧道/平地不再靠高度差猜，由用户显式选。
  - `lots[]`：`{corners:[[x,y]×4], margin:[西,东,北,南], boundary:{"0":road下标…}, area:"AREA_…"}`。corners 按顺时针（屏幕坐标 y 向下）给出，第一条边 n1→n2 为"北"边，其余依次东、南、西（同 `Lot` 构造的约定）；`boundary` 的下标是 `roads[]` 的下标，只在该边正好是一条平直道路时才写；area 用枚举名字符串。
- **路口缩进（setback）**：`AssignJunctionSetbacks` 是 `roadnet_jing.cpp` 里那份的拷贝（Basic 里 mod 互不 include），改一处要同步。
- **`source` 段**：编辑器自己的编辑模型，mod 完全忽略，让同一个文件能被编辑器原样读回继续编辑。
- **路径**：同 `terrain_file`，绝对路径或相对进程工作目录，`--path=x` 与 `--path x` 都行，路径里可以有空格。
- 文件缺失/损坏：`debugf` 后路网留空。矩形不合法的地块（`Lot` 构造抛异常）跳过并警告。
- **桥墩**：`kind=bridge` 的段按 `chain`（编辑器把经过"只连着两条桥的节点"的连续桥归成一组）分链，`order` 是行进顺序，`reverse=true` 表示该段是反着画的，mod 拼链时把首尾和控制点倒过来，保证链首尾相接；整体 `AddPier("", chains, 4.0, 2.0, 0.2)`（墩距40m/最小20m/墩半径2m，单位element）。链里是端点沿用路 Node（保留 id）的 `Connection` 副本，Core 靠 id 把本链的路排除出障碍；墩顶离地不足的点 Core 不放墩。没有 `chain` 字段的旧文件每条桥自成一链。
- 树和路灯本版忽略（`LayoutTrees/LayoutLamps` 用默认实现）。

## 依赖关系
只依赖 Dependence（`map/roadnet_mod.h`、`common/json.h`）。

## 待办/后续阶段
- 无。
