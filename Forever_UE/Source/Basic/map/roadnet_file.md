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
  - `greens[]`：`{points:[[x,y]…], links:[{edge, road}]}`。绿化区的闭合多边形（地图单位，不重复首点，可以是凹多边形，不能自交；导出时统一成有符号面积>0）；`links` 是"多边形第 edge 条边（points[edge]→points[(edge+1)%n]）连到 roads[] 下标 road 那条路"，可以为空（只做景观、不能地址导航）。接入点不存，Core 按每个活动圆的垂足投影到那条路上再算。读入时 edge/road 越界的 link 被丢弃，点数<3 的绿化被丢弃。绿化类型/圆/树由 Core 的 `Map::InitGreens` 选 green mod 决定，不在文件里。
- **路口缩进（setback）**：`AssignJunctionSetbacks` 是 `roadnet_jing.cpp` 里那份的拷贝（Basic 里 mod 互不 include），改一处要同步。
- **`source` 段**：编辑器自己的编辑模型，mod 完全忽略，让同一个文件能被编辑器原样读回继续编辑。
- **路径**：同 `terrain_file`，绝对路径或相对进程工作目录，`--path=x` 与 `--path x` 都行，路径里可以有空格。
- 文件缺失/损坏：`debugf` 后路网留空。矩形不合法的地块（`Lot` 构造抛异常）跳过并警告。
- **桥墩**：`kind=bridge` 的段按 `chain`（编辑器把经过"只连着两条桥的节点"的连续桥归成一组）分链，`order` 是行进顺序，`reverse=true` 表示该段是反着画的，mod 拼链时把首尾和控制点倒过来，保证链首尾相接；整体 `AddPier("", chains, 4.0, 2.0, 0.2)`（墩距40m/最小20m/墩半径2m，单位element）。链里是端点沿用路 Node（保留 id）的 `Connection` 副本，Core 靠 id 把本链的路排除出障碍；墩顶离地不足的点 Core 不放墩。没有 `chain` 字段的旧文件每条桥自成一链。
- **行道树/路灯（tag）**：`roads[]` 的每个 `kind=ground` 段带 `treeTag`/`lampTag`（字符串，空=这条路没有）。mod 读进 `treeTags`/`lampTags`（下标=`roads` 下标；Core 的 `hostRoads` 前面和 `mod->roads` 一一对应，起点 id 对不上就跳过），`LayoutTrees/LayoutLamps` 按 tag 查 `roadnet_file.cpp` 里的 `TreeAssetFor`/`LampAssetFor`（tag→网格/蓝图、间距、人行道内偏移比例等）选资产。现在只有 `default`：资产和间距常量与 `roadnet_checkboard.cpp` 完全一致（`TREE_*`/`LAMP_*`，两份各抄，改要同步）。未知 tag 告警并跳过；要加新资产就在两个查表函数里加一行，编辑器里给道路填同名 tag。
  排点几何同 checkboard 的 `LayoutRows`（扣掉路口缩进、避开 `Road::GetOpenings`、按人行道位置偏移、跳过隧道段/小路）：直路一整段连续树位发一条 `AddTreeLine/AddLampLine`；**曲线路**按弧长逐点取位置和切线，每点发一个 `num=1` 的线（朝向取该点切线）。路灯朝向=路方向±90°(side 0 为-90°)+资产的 `yawOffset`。

## 依赖关系
只依赖 Dependence（`map/roadnet_mod.h`、`common/json.h`）。

## 待办/后续阶段
- 无。

- **隧道口端墙**：`mouth` 的路可带 `wallFrom`/`wallTo`(bool，from=t1端/to=t2端)，编辑器导出时把"这一端节点连着 `tunnel` 路"的那端标 true(和两端高度无关，山坡上两端等高也行)，游戏侧据此给 `AddTunnelHatch` 传端墙墙底 "该端节点高度+`TUNNEL_ROOF_CLEARANCE`(1=10m，隧道资产房顶)"。不连隧道的 mouth 段(如 mouth→mouth→tunnel 里靠外的)不加。

- **两条路的路口(拐角)的缩进（2026-11-20）**：`AssignJunctionSetbacks`里只连两条路的节点单独算：夹角phi(两条路离开节点方向的夹角，<=PI)，沿本路中线缩进 `(对方半宽 + 本路半宽*cos(phi)) / sin(phi)`(等宽时=半宽/tan(phi/2))，让两条路朝向对方那侧的路缘线正好在角点相交、不再重叠；平角(phi>=PI-EPS)缩进0，缩进不超过路长的45
- **两条路的路口(拐角)的缩进（2026-11-20）**：`AssignJunctionSetbacks`里只连两条路的节点单独算：夹角phi(两条路离开节点方向的夹角，<=PI)，沿本路中线缩进 `(对方半宽 + 本路半宽*cos(phi)) / sin(phi)`(等宽时=半宽/tan(phi/2))，让两条路朝向对方那侧的路缘线正好在角点相交、不再重叠；平角(phi>=PI-EPS)缩进0，缩进不超过路长的45%。jing/checkboard/file三份同步。
