# roadnet_file

## 职责
读 `Tools/MapEditor` 导出的 `.roadnet`（宽松 JSON）里的 `output` 段，原样生成路网：路口、城外连接点、道路（含隧道口/贴地形爬升/桥）、地块。配置示例：`"roadnet_file --path=C:/maps/city.roadnet"`（放进 `roadnet_mods`，路网是单选）。

## 关键设计
- **mod 只读结果，不做几何决策**：路口切分（T 口、十字）、贴合路的地形采样、头尾节点高度不同时的扁 S 坡控制点、桥墩链分组、地块矩形都在编辑器导出时算好。Core 不会自动建路口（`Map::InitRoadnet` 只对 `intersections` 里的点建 `RoadJunction`），所以编辑器必须把被接的路切开、让端点共用同一个节点 id。
- **`output` 结构**：
  - `nodes[]`：`{id, x, y, z, type}`，`type` = `intersection`（登记进 `intersections`）/ `extern`（登记进 `externs`，城外连接点）/ `plain`（普通衔接点，两条路首尾相接）。度数不为2的点编辑器一律导出为 `intersection`。
  - `roads[]`：`{name, from, to, controls:[[x,y,z,weight]…], asset, lanes:[左车行,左停车,左人行,右车行,右停车,右人行], kind}`。`from/to` 是 node id；`controls` 是有理 Bezier 控制点（直线不写）；`asset`=车道资产名（缺省/空=`default`），`lanes`=车道类型；断面 mesh 路径 = `/Game/Asset/Roadnet/<首字母大写的asset>/Mesh/<asset>_<lanes 用_连接>`（如 `default_1_0_1_1_0_1`，`kind=tunnel` 加 `_T`，开口用的无人行道版同理），车道宽 0.5。**方向**：`lanes` 前三项（左\*，side0）是 from→to 方向通行的那一侧，后三项（右\*，side1）是 to→from 那一侧；某一侧车行道为 0 就是单行道（见 `Core/map/roadnet.h` 的 outbound/inbound 约定，未经 PIE 验证）。
  - `kind`：`ground` 平地；`hug` 路面贴合地形（路面高度跟着地形走，请求 `AddTerrainFit`，宽=路总宽，falloff 1.5，编辑器已按地形每 1 单位采样成一段）；`fit` 地形贴合路面（2026-11-23：路面高度取节点 z、整段一条路不随地形起伏，同样请求 `AddTerrainFit`，游戏里把路两侧地形削平/填平到路面——两端 z=1、中间地形有起伏的路用它）；`mouth` 隧道口（用平地的路面资产，整段 `AddTunnelHatch` 给地形开口，两侧用无人行道资产）；`tunnel` 隧道内（`_T` 资产，没有开口）；`bridge` 桥（桥墩由 Core 选位，见下）。一条编辑器里的路对应一条 `roads[]`（hug 除外）；头尾节点高度不同就是扁 S 坡（`controls` 里两个控制点，z 取头尾高度，两端水平）。这样隧道口/隧道/平地不再靠高度差猜，由用户显式选。
  - `lots[]`：`{corners:[[x,y,z?]×4], margin:[西,东,北,南], boundary:{"0":road下标…}, area:"AREA_…"}`。第3个数z（可省，缺省0）是地块高度（2026-11-23，`Lot::GetZ()`；非0高度平台上的地块用，4个角z相同）；corners 按顺时针（屏幕坐标 y 向下）给出，第一条边 n1→n2 为"北"边，其余依次东、南、西（同 `Lot` 构造的约定）；`boundary` 的下标是 `roads[]` 的下标，只在该边正好是一条平直道路时才写；area 用枚举名字符串。
  - `greens[]`：`{points:[[x,y]…], links:[{edge, road}], id}`。`id`(可缺省/为空)=指定的 green mod id：非空就用它，不走权重随机；空，或 id 没注册/没启用(Core 告警)，都退化成 Core 的按权重随机。绿化区的闭合多边形（地图单位，不重复首点，可以是凹多边形，不能自交；导出时统一成有符号面积>0）；`links` 是"多边形第 edge 条边（points[edge]→points[(edge+1)%n]）连到 roads[] 下标 road 那条路"，可以为空（只做景观、不能地址导航）。接入点不存，Core 按每个活动圆的垂足投影到那条路上再算。读入时 edge/road 越界的 link 被丢弃，点数<3 的绿化被丢弃。绿化类型/圆/树由 Core 的 `Map::InitGreens` 选 green mod 决定，不在文件里。
  - `presets[]`(2026-11-21)：`{lot, id, direction, rect:[x0,x1,y0,y1]}`。`lot`=`lots[]`下标，`id`=要放的zone/building mod id(`zone_`开头=园区，其余=建筑，可为空；为空或没注册/没启用时，空间照占，Core 退化成按该 lot 的分区权重随机放一栋建筑，占位分区/没有候选则只占位)，`direction`0..3=矩形贴靠的有路的边，`rect`=顶层lot本地坐标(缩进后的lot，西北角为原点，x东y南)。direction越界、lot不存在的项告警跳过。MapEditor的"添加预设"产生它。
  - **`AREA_PLACEHOLDER`(2026-11-21)**：lots[].area 可写这个占位分区，专给预设/显式Assign用——`Map::InitBuildings`的权重登记跳过它，`BuildingFactory::GetPower`对它恒0，所以这块地只会有预设和各mod `Assign`放出的东西，不会按权重填充；**不检查可建设性**(编辑器导出不报"不是可建设区域"，roadnet_file读入时本来就不看isConstruction)，允许放在非平地上。编辑器的地块类型里也有"占位(只放预设)"。路网mod(roadnet_jing/checkboard等)也可以直接`SetArea(AREA_PLACEHOLDER)`。
- **路口缩进（setback）**：`AssignJunctionSetbacks` 是 `roadnet_jing.cpp` 里那份的拷贝（Basic 里 mod 互不 include），改一处要同步。
- **`source` 段**：编辑器自己的编辑模型，mod 完全忽略，让同一个文件能被编辑器原样读回继续编辑。
- **路径**：同 `terrain_file`，绝对路径或相对进程工作目录，`--path=x` 与 `--path x` 都行，路径里可以有空格。
- 文件缺失/损坏：`debugf` 后路网留空。矩形不合法的地块（`Lot` 构造抛异常）跳过并警告。
- **桥墩**：`kind=bridge` 的段按 `chain`（编辑器把经过"只连着两条桥的节点"的连续桥归成一组）分链，`order` 是行进顺序，`reverse=true` 表示该段是反着画的，mod 拼链时把首尾和控制点倒过来，保证链首尾相接；整体 `AddPier("", chains, 4.0, 2.0, 0.2)`（墩距40m/最小20m/墩半径2m，单位element）。链里是端点沿用路 Node（保留 id）的 `Connection` 副本，Core 靠 id 把本链的路排除出障碍；墩顶离地不足的点 Core 不放墩。没有 `chain` 字段的旧文件每条桥自成一链。
- **行道树/路灯（tag）**：`roads[]` 的每一段（**不分 kind**）带 `treeTag`/`lampTag`（字符串，空=这条路没有）；游戏只看这个指定，指定放什么就放什么：不再跳过隧道/桥/隧道口，Core 也不再对路树/路灯做水面/挖洞过滤（mod 的 `filterRoadPlants=false`，只丢出图的点）。编辑器给 `ground`/`hug` 路填 `default`，其余类型留空是用户的选择，不是代码限制。mod 读进 `treeTags`/`lampTags`（下标=`roads` 下标；Core 的 `hostRoads` 前面和 `mod->roads` 一一对应，起点 id 对不上就跳过），`LayoutTrees/LayoutLamps` 按 tag 查 `roadnet_file.cpp` 里的 `TreeAssetFor`/`LampAssetFor`（tag→网格/蓝图、间距、人行道内偏移比例等）选资产。现在只有 `default`：资产和间距常量与 `roadnet_checkboard.cpp` 完全一致（`TREE_*`/`LAMP_*`，两份各抄，改要同步）。未知 tag 告警并跳过；要加新资产就在两个查表函数里加一行，编辑器里给道路填同名 tag。
  排点几何同 checkboard 的 `LayoutRows`（扣掉路口缩进、避开 `Road::GetOpenings`、按人行道位置偏移、跳过小路；不按路的类型/高度筛）：直路一整段连续树位发一条 `AddTreeLine/AddLampLine`；**曲线路**按弧长逐点取位置和切线，每点发一个 `num=1` 的线（朝向取该点切线）。路灯朝向=路方向±90°(side 0 为-90°)+资产的 `yawOffset`。

- **内嵌版(`Tools/MapEmbed`)**：`roadnet_file.cpp` 里 `// [map_embed:load begin]`/`// [map_embed:load end]` 之间是"按 --path 读文件进 `text`"的一段，`embed_map.py` 拿这两个文件当模板生成内嵌数据的 mod 时只换掉这一段(加类名/id/显示名/`--path` 解析几处替换)。改这两个文件要保持标记和那几处写法，改完把已生成的内嵌 mod 重新生成一遍。

## 依赖关系
只依赖 Dependence（`map/roadnet_mod.h`、`common/json.h`）。

## 补充说明
- **隧道口端墙**：`mouth` 的路可带 `wallFrom`/`wallTo`(bool，from=t1端/to=t2端)，编辑器导出时把"这一端节点连着 `tunnel` 路"的那端标 true(和两端高度无关，山坡上两端等高也行)，游戏侧据此给 `AddTunnelHatch` 传端墙墙底 "该端节点高度+`TUNNEL_ROOF_CLEARANCE`(1=10m，隧道资产房顶)"。不连隧道的 mouth 段(如 mouth→mouth→tunnel 里靠外的)不加。
- **路口缩进规则已统一（2026-11-25）**：`AssignJunctionSetbacks`不再单独处理两条路的路口，所有路口(含拐角)每条路分别和左/右相邻路算缩进(夹角<=直角精确算、>直角取对方半宽、>=平角不缩进)取较大值；只连两条路时平角容差为5°。规则见`roadnet_jing.md`"路口缩进"，jing/checkboard/file三份同步。

## output.transit（2026-11-23）

`output.transit = {nodes:[{x,y,z,extern?}], conns:[{from,to,ctrl:[x,y]|null,kind,route}], loops:[{route, items:[{preset:<output.presets 下标>,stop}|{node:<nodes 下标>}]}]}`。conns 是二次 Bezier（`ctrl` 空=直线），`kind` ∈ ground/hug/mouth/tunnel/bridge，`route`=route mod id。解析时丢弃：from/to 越界或相同的连线、缺 route 的连线、引用越界的 node 项、预设下标<0 的项、不足 2 项的环；`preset.outputIndex` 记原下标。数据原样放进 `RoadnetMod::transit`，由 route mod（`--skip`）使用，见 `route_mod.md`"手动线路"。

`output.transit.roadLines = [{route, stops:[{road:<output.roads 下标>, x, y, side:0|1, station}]}]`（公交，2026-11-23）：路下标越界/缺 road 或 station 字段的站丢弃（`debugf` 告警），不足 2 站的线丢弃；解析进 `TransitData::roadLines`（`side` 1=反向，`rightSide=false`）。站由 `station_bus::AssignManualRoads` 摆，线由 `route_bus --skip` 连，见 `station_bus.md`/`route_bus.md`"手动线路"。
