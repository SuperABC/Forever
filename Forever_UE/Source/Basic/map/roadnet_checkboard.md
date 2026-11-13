# roadnet_checkboard（棋盘格路网）

`CheckboardRoadnet : RoadnetMod`（行道树/路灯布局从井字复制，不继承）。config：
`"roadnet_checkboard --merge 0.2 --bridge 0.5 --min 24 --max 48"`（合并概率、跨河路保留概率、相邻路最小/最大间距，单位=地图元素=10m）。

## 流程（DistributeRoadnet）
1. 旋转坐标系(u东、v南，角度同井字 ±0.2rad)里，从北到南铺横路、从西到东铺竖路，间距为 [min,max] 随机整数；交点变换到世界坐标得到 node。
2. 丢弃不在 `construction` 元素上、或离地图四边不足 `EDGE_MARGIN_RATIO`(10%)的 node(路网不靠边)。
3. 相邻存活 node 连"抽象边"；沿线有山的边标 special（河/海不处理）。
3a. 有效 node 必须连着至少两条边：断头/孤立的 node 删掉(连带那条边)，迭代到稳定；补长连接按这个"有效 node"判断。
3b. 补长连接：遍历初始横/竖线(横线、竖线都隔一条选一条，各自随机起始奇偶，避免相邻山路太近)，同一条线上相邻两个有效 node 之间隔了≥4 个无效 node 且其中有山里的，按 50% 概率补一条长边(铺路走和普通边相同的抬升/隧道逻辑，不挂 lot)；新补的长边之间不能交叉(横、竖长边在同一网格点上相交即冲突，丢弃)。长边参与联通性判断，端点强制保留成路口。
4. 联通性：只留最大联通分量。
5. 四 node+四边都在、整块 lot 都落在 construction 且无 special 边的格子生成 lot；删掉两侧都没有 lot 的边（跨山边例外，保留并迭代剪断头；只跨河的无 lot 边按 `--bridge`(默认0.5)概率保留，没保留的再按并查集补回"连通两块原本不连通区域所必需"的），然后再做一次联通性清理。
6. 横竖都隔一个(奇数下标)取中心 node，周围四个 lot 都在则按 `--merge` 概率合成大 lot：删中心十字四段，外圈每边取第一段所在的路作边界。
6b. 主干道：在隔一条选一条的横线/竖线上(奇偶同 3b)，把线上实际连续的道路(沿线相邻存活 node 靠普通边或长边一路连着，断开即另一段，看的是步骤 6 之后留下的边)按所含格点(node)数从多到少排，横向、竖向各取前 1/4(向上取整)，每侧车行道改成 `MAIN_ROAD_LANES`(2，即双向 4 车道)。同一条链里的边必在同一段道路上，铺路时按链取车道数。
7. 只连两条共线非 special 边的 node 删除，两边并成一条链(chain)。
8. 落地：存活 node→Intersection；每条链调用 `layRoad`（照抄 JingRoadnet::buildArm 的抬升/隧道逻辑，终点固定为 b）；lot 边界 Road 指向 `roads` 内元素。
   lot 密度按到地图中心/半对角线的距离分环 0.1/0.3/0.5：全高 / 高中 / 中低 / 全低，类型住宅商业工业办公均匀随机(工业高密度换成绿化区 AREA_GREEN)。

## 注意
- externs：东南西北各一条出口路。北=最初生成的最北东西向线(j最小)，若这条线上还有存活 node 就从中随机选一个，没有就看下一条线；南/东/西同理。选中的 node 强制保留为路口，沿棋盘格方向铺到地图边界，边界点是 extern；出口路过山同样走抬升/隧道。
- special(跨山)边两侧不建 lot，因此保留的跨山路没有 lot 邻接；隧道放不下(山前后平地<8格)时按井字规则退化为抬升。
- 路名 "东西第N路/南北第N路"（N为线序号）；无随机种子参数，每次生成不同。
- 可视化工具：Tools/RoadnetViz。
- 车道配置对称：普通路 (1,0,1,1,0,1)，资产 `default_1_0_1_1_0_1`；主干道 (2,0,1,2,0,1)，资产 `default_2_0_1_2_0_1`(隧道内都加 `_T`，均在 Roadnet/Default/Mesh 里)；`addRoad`/`layRoad`/`LayoutRows` 与 `JingRoadnet` 的同名逻辑是复制的(规则见 `roadnet_jing.md`)，改一边要同步另一边。
- 构造参数 `--merge`/`--bridge` 夹到[0,1]，`--min` 至少 2、`--max` 不小于 `--min`。
- 各步骤的 `special`/`keep` 标记都是针对"沿线采样到山/河"的抽象边；大 lot 合并后外圈每边只取第一段所在的链作边界 Road，因此边界 Road 与 lot 该边不一定等长。

- 路口缩进：`DistributeRoadnet`最后调用`AssignJunctionSetbacks`给每条路两端写缩进（规则见`roadnet_jing.md`"路口缩进"，复制而来）：
  两侧平角→0，最小夹角直角→两侧邻路半宽最大值(不含自己)，有锐角→锐角几条路半宽最大值的两倍。

## 依赖关系

- 依赖：`map/roadnet_mod.h`(`RoadnetMod`基类)、`common/utility.h`。
- 被谁依赖：`Source/Basic/basic.cpp`(`GetModRoadnets`/`RegisterModRoadnets` 注册，creator 把 args 传给构造函数)。

## 待办/后续阶段

- 与 `JingRoadnet` 重复的建路/行道树几何逻辑尚未抽成共享代码(Basic 层文件间不互相 include)。
