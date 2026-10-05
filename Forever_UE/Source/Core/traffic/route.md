# route.h / route.cpp

## 职责

`Route`：一类线路的实体，独占持有一个`RouteMod`实例(照抄`Vehicle`/`Job`持有mod的模式)。
`Build()`收集`stationType`匹配的`Station`接口、调`mod->LayoutRoute()`拿到拓扑描述，自己
把每条拓扑边变成真实几何(有向图)+组装成若干条环线；`Update()`按时刻表驱动绑定在这条线路
上的载具。

## 关键设计

- **两段式：mod只给拓扑，Route自己把拓扑变成几何**——`mod->LayoutRoute()`只产出
  `(station,interfaceIndex)`下标对组成的`links`/`lines`，不碰任何真实坐标/`Connection`；
  `Route::Build()`拿到这份拓扑后逐条调`BuildEdgeGeometry()`才真正构造几何。这个分工和
  `cross-dll-allocator-crash`那次教训同源：几何构造涉及`new`/`delete`/容器增长，必须发生
  在Core编译的代码里，不能让运行在mod DLL里的`RouteMod::LayoutRoute()`直接碰。
- **`BuildEdgeGeometry`两种边的构造方式**：
  - `useRoadnet==true`(公交)：两端都成功`Station::EnsureRoadLink()`之后，**同车道特例**——
    若两端落在同一段原车道上(`sourceEdge`相同)且起点在上游(`t`更小)，直接用两个
    `stationNode`间的一条直线弦，不绕一圈路径搜索；否则用起点的`outEdge`+
    `Map::FindVehiclePath`(起点车道下游锚点→终点车道上游锚点)+终点的`inEdge`拼起来——这三段
    全部是"借用"Station/Map已有的`Connection`(`owned=false`)，`Route`自己不负责delete。
  - `useRoadnet==false`(火车/飞机)：三次贝塞尔曲线，起止点就是两个接口的世界坐标，两个控制
    点分别沿起点的`departDir`/终点的`arriveDir`伸出`controlLength`——这样相邻两条边在同一个
    站点接口处的切线必然连续(同一个`departDir`/`arriveDir`同时决定了上一条边的终点切线方向
    和下一条边的起点切线方向)，不会出现尖角。控制点`z`分别取两端自己的`z`，支持飞机这类
    有高度变化的线路。四个接口共线、朝向相同时(比如`TrainStation`/`AirStation`同一条跑道的
    两个接口)，两个控制点天然落在同一条直线上，贝塞尔曲线退化成直线，不需要额外判断。
- **有向图+双向边**：`graph[fromKey][toKey]`是单向边；`RouteLink.bidirectional==true`时
  额外登记一条`graph[toKey][fromKey]`，**复用同一份`segments`**(`reversed=true`，
  `owned=false`，只有正向entry负责delete)，`EvaluateEdge`/驾驶逻辑按`reversed`决定是否
  倒序遍历`segments`+取`1-f`+切线取反，不复制几何。这次三个内置线路都是单向环线，没有真正
  用到这条路径，但接口已经打好。
- **`RouteLeg.dwellAtDestination`(2026-10-04新增)**——`Build()`组装`legs`时直接搬运目的地
  那一站的`RouteStop::dwell`过来；`false`表示到达`toStation`后不停留，`DriveVehicle`/
  `Update`的耗时计算都要跳过这一腿的`dwellSeconds`(不只是跳过停靠时的渲染，否则时刻表总
  周期`period`会被白白算长，车辆相位会跟着漂移)。背景见`route_mod.md`"RouteStop.dwell"
  一节——这是为了让火车/飞机只在跑道/轨道中点真正停靠一次，入站/出站接口和地图边缘点全部
  变成纯过路点。
- **`Update()`是纯函数式实现**——只读`elapsedSeconds`(`Traffic::Tick`按
  `Time::DifferenceInSeconds`累加的游戏总秒数，单调递增)，不在`Route`自己身上累积任何状态；
  `DriveVehicle`内部用`localTime`(`elapsedSeconds`按整条线路的`period`取模，每辆车再加一个
  均匀分布的`phase`)逐腿累减"行驶耗时(`edge->length/speed`)+停靠耗时"直到落在某一段区间，
  同一个`elapsedSeconds`重复调用结果完全一致，不依赖调用顺序/调用次数。`speed`/
  `dwellSeconds`的单位是游戏内秒不是真实秒，具体换算见`route_mod.md`。
  - **这是2026-10-04才修好的**：`Time::DifferenceInSeconds`原来的符号写反了(`this`比
    `other`早时返回负数，`Traffic::Tick`的`if (delta > 0.0)`因此每一帧都被跳过)，
    `elapsedSeconds`从头到尾停在初始值不动，表现为"所有公共交通车辆生成之后位置再也不变"。
    排查过程见`Dependence/common/utility.cpp`里`Time::DifferenceInSeconds`声明处的完整
    记录；这个符号bug全局只有`Traffic::Tick`这一处调用方，修好之后不需要再额外处理。
- **`DriveVehicle`的缓动按"行驶区段"(两次真正停靠之间的若干条leg合起来)整体套梯形速度
  曲线，不是按单条leg自己的`[0,1]`区间**——这是改了两版才定下来的：
  - 第一版不分青红皂白给每一条`leg`都按`smoothstep`(两端速度钳死为0)缓动，结果火车/飞机
    经过`leftIn`/`rightOut`这类纯过路接口(见`station_basic.cpp` `Layout()`的"顺序必须是
    `[leftIn, rightOut, rightIn, leftOut]`"一节)时也诡异地"减速再加速"——**载具运行时
    完全不感知接口本身的存在，只认"是不是一次真正的停靠"**(`RouteLeg::dwellAtDestination`)。
  - 第二版改成按`dwellAtDestination`在单条leg两端各自判断要不要缓动(三次Hermite插值)，
    结果PIE/打包exe实测反馈"又变成瞬间全速移动了"——接口到站内停靠点那一小段(跑道/轨道
    半截)比相邻的长途巡航段短得多，缓动压缩在这么短一条leg的`travelTime`里，表现上还是
    跟突变没区别。
  - 第三版：往回/往前找到当前leg所在的"行驶区段"(上一次真正停靠到下一次真正停靠之间首尾
    相接的所有leg，`DriveVehicle`开头往回扫`dwellAtDestination`找区段起点`segStart`)，
    累计这整段的总时长`segTotalTime`/总长度`segTotalLength`，套梯形速度曲线(匀加速
    `ta`->匀速->匀减速`td`，`ta`/`td`钳在`mod->easeSeconds`和区段总时长一半之间取小)
    算出区段内的累计弧长`segArcLength`。这版PIE/打包exe实测又反馈一个新问题："飞机到了
    接口处明显卡顿一下，然后瞬移到中间的站点"——**根因是这一版拿"累计时间"顺出来的
    `segLengthBeforeCurrentLeg`去减`segArcLength`，而`segLengthBeforeCurrentLeg`是按
    "各leg自己名义匀速`travelTime=length/speed`"累计出来的，`segArcLength`却是按缓动
    曲线(加减速阶段速度比`mod->speed`时快时慢)算出来的——两者只在区段的最开头(都是0)和
    最末尾(都是`segTotalLength`)才保证相等，在区段内部任意一条leg的边界(正好是接口！)
    上并不相等，用名义时间累计出来的"属于哪条leg"去切缓动后的真实弧长，自然会在接口处
    对不上(卡顿)、再跳到按缓动曲线算出来的真实位置(瞬移)**。
  - 现在的版本：`segElapsed`(定位在缓动曲线上的哪一点)依然用"累计名义时间"算，这部分没
    问题(时间本身是连续的，不受速度曲线影响)；但算出`segArcLength`之后，**改成按累计
    长度(不是累计时间)重新从`segStart`走一遍**找`segArcLength`真正落在区段内哪条leg
    上——和`EvaluateEdge`内部按弧长在多段`segments`里定位同一个道理，只是这里定位的是
    "哪条leg"。这样缓动曲线和"用哪条leg的几何算世界坐标"用的是同一套长度基准，接口处
    不会再对不上。缓动因此天然跨越leg边界、从相邻的长途巡航段"借"时间，不受某一条leg
    多短的限制。匀速段速度`cruiseSpeed`比配置的`mod->speed`略高一点(补偿两头比匀速慢的
    那部分距离)，确保`segTotalTime`(=`Σlength/speed`，和原来完全一样)内还是正好走完
    `segTotalLength`，`Update()`按这个值算的时刻表/`period`完全不用动。`easeSeconds`是
    `RouteMod`的字段(默认3秒，照抄公交的尺度)，不是硬编码常量——火车/飞机体型大、巡航
    距离长，PIE实测反馈默认的3秒"加速度还是太大"，`TrainRoute`/`AirRoute`/`BusRoute`
    各自在`SetProperty()`里按手感逐步调大，见`route_basic.cpp`里各自的注释(当前值不是
    一次定的，PIE反馈觉得还大就继续往上调，没有"标准答案"，纯粹手感校准)。公交这类"每一站
    都要停"(`dwell`恒为`true`)的线路，区段退化成单条leg，效果等价于那条leg自己两端各
    缓动`ta`/`td`。
    - **漏了一个单位换算，调了好几轮`easeSeconds`都没用，才发现**：`mod->easeSeconds`
      是真实秒(`route_mod.h`/`route_mod.md`"easeSeconds的单位"一节有写)，但
      `segTotalTime`/`segElapsed`这些全是游戏内秒(和`speed`/`dwellSeconds`同一套单位)，
      `DriveVehicle`里一开始直接拿`kEaseSeconds = mod->easeSeconds`去跟这些游戏内秒的量
      比大小/做减法，少乘了"1真实秒=120游戏内秒"(默认`timeFlowRatio=2.0`)这个换算系数——
      相当于把"40真实秒"的缓动窗口当成"40游戏内秒"(≈1/3真实秒)在用，PIE/打包exe实测
      "调大了`easeSeconds`数值，加速度看起来还是很猛"，反复调大数值(`3→15→30→40`)其实
      都被这个漏乘的120系数吞掉了，不是真的需要更夸张的名义值。修复：
      `kEaseSeconds = mod->easeSeconds * 120.f`，换算系数和既有的"默认`timeFlowRatio=2.0`"
      假设一致，如果以后`timeFlowRatio`被剧情脚本改了要跟着重新校准(和`speed`/
      `dwellSeconds`的既有校准方式一样，不是这次新引入的限制)。

## 依赖关系

- 依赖：`Core/map/map.h`(`FindVehiclePath`)、`Core/traffic/station.h`(`EnsureRoadLink`/
  `GetInterfaces`)、`Core/traffic/vehicle.h`(`SetTransform`)、`traffic/route_mod.h`/
  `route_factory.h`。
- 被谁依赖：`Core/traffic/traffic.cpp`的`InitRoutes`(创建`Route`、调`Build`)/
  `InitTransitVehicles`(按`GetVehiclesPerLine()`创建车辆、`AddVehicleToLine`)/`Tick`
  (调`Update`)；`Forever/Framework/ForeverTrafficFrameworkComponent.cpp`的
  `BuildRouteDebugMesh`(读`GetLines()`画调试线)/`BuildTracks`(读`ShouldDrawPath()`/
  `GetTrackMesh()`)。

## 待办/后续阶段

- `trackMesh`铺真实轨道mesh的逻辑没有任何内置线路用到，见`route_mod.md`"待办"一节。
- `bidirectional`双向边的实际驾驶路径(`reversed`分支)没有被任何内置线路触发过，只验证过
  代码逻辑自洽，没有PIE实测——以后真的有双向线路类型时要重新验证一遍。
