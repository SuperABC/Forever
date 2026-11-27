# ForeverTrafficLightFrameworkComponent.h / .cpp

## 职责

路口红绿灯在场景里的表现和信号开关。信号本身(分组、相位、放不放车)在 Core `JunctionTraffic`(`Core/traffic/junction.md`"红绿灯")，这个组件只决定**哪些路口有灯**，并把信号显示出来。

## 关键设计

- **登记**：Tick里发现交通组件的路口通行表(`GetJunctionTraffic()`)和上次登记的不是同一个(第一次、换了地图)就重新登记：遍历`Map::GetJunctions()`，`JunctionTraffic::GetSignalGroups`有表的路口(多于两条路)登记一条：路口中心、每个灯的世界变换、每个灯显示哪条路。只记路口id，不留Core指针。
- **灯位**：每条有驶入车道的路(组号>=0)左右各一个，从`RoadJunctionApproach`的路缘角点(`curbLeft/curbRight`，setback线上、整条路横断面的最外缘)算：这一侧有人行道就和行道树对齐——放在离道路中线 车道+停车道+人行道[0]×`kSidewalkRatio`(0.25，照抄 Basic 路网 mod 的`TREE_SIDEWALK_RATIO`) 处，也就是从路缘往里收`GetSideWidth(side)`减去这个距离；没有人行道就往外放`kCurbOutset`(0.04地图单位=40cm)到路外；然后两种情况都沿道路往路口外(背离路口中心)挪`kAwayFromJunction`(0.05=0.5米)，完全放在道路单元上，免得压在setback线上的人行横道上挡市民；模型统一放大`kTrafficLightScale`(3)倍；高度`curbZ`+10cm(和路网组件抬高路面的量一样)。`curbLeft/curbRight`不对应真实左右手，isStart端side0是curbLeft、另一端side0是curbRight(`roadnet.cpp`)，人行道看`pedestrianSide[side]`。朝向：灯罩朝模型-X，要朝驶来的车(这条路离开路口的方向`angle`)，yaw=`angle`+180°。
- **流式**：按玩家到路口中心的水平距离，<`trafficLightSpawnDistance`(16地图单位)生成该路口的`ATrafficLightElement`并`SetSignalActive(true, 交通时钟)`(从第0组绿灯开始)；>`trafficLightDespawnDistance`(32)销毁并`SetSignalActive(false)`。没有数量上限和每帧上限。没生成灯的路口没有信号，车照原来的冲突表通行。
- **刷新**：每帧对生成着的路口调`ATrafficLightElement::RefreshSignals(traffic, 交通时钟)`，信号变了才改灯。交通时钟是现实秒、暂停时停，所以暂停时灯也停。
- **EndPlay**：只销毁灯、清登记，不碰Core(路口表可能已经释放)。

## 补充说明

- 只有流式生成、由控制器开的车受红绿灯影响(它们向`JunctionTraffic::Request`请求通行)；Core推算的车照常匀速走，玩家自己开的车不参与路口调度，红绿灯对它只是显示。跳时间期间车没有Actor，红绿灯自然不起作用。
- 走远(>32)再回来，灯重新生成，周期从第0组重新开始。

## 依赖关系

- 依赖：`AForeverFrameworkActor`(取Map和交通组件)、`UForeverTrafficFrameworkComponent`(`GetJunctionTraffic`、`GetTrafficClock`)、`ATrafficLightElement`、Core `map/roadnet.h`、`traffic/junction.h`。
- 被谁依赖：`AForeverFrameworkActor`创建。
