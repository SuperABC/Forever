# green_park.h / green_park.cpp

## 职责

`ParkGreen`(`green_park`)：公园。对路网指定的每块绿化多边形放**一个**市民活动圆(多边形能容纳的最大圆，`FindLargestCircle`)，并在多边形范围内随机种树。

## 关键设计

- **权重**：没连路网(`GetPowerFree`)和连了路网(`GetPowerLinked`)都是`1.f`，`Map::InitGreens`在启用的绿化mod里按对应那套权重做加权随机。要做"只在有路网/只在没路网的绿化区出现"的mod，把另一套权重写0。
- **活动圆**：`LayoutGreen()`里调一次`FindLargestCircle`，`PARK_CIRCLE_SCALE`(默认1)缩放半径后`AddCircle`；是否能连到路由Core校验(圆心到连路边的最短连线要整条在多边形内，不满足这个圆就被丢弃，绿化区变成没有活动区)。
- **树**：`AddTreeArea`在**包围盒**(参考系`(0,0)-(1,1)`)里撒点，数量按包围盒面积/`PARK_ELEMENTS_PER_TREE`(2个element²一棵)算，Core展开时丢掉多边形外的点，所以多边形内密度才是设定值。mesh是和行道树同一棵`mossy-tree`。
- 命名：`green_park<序号>`(静态计数器，存档恢复)，无空格——地址按空白切分。

## 依赖关系

- 依赖：`Source/Dependence/map/green_mod.h`(`GreenMod`基类、`FindLargestCircle`/`AddCircle`/`AddTreeArea`)。
- 被谁依赖：`Source/Basic/basic.cpp`(`RegisterModGreens`)；`Resource/Config/config.json`的`green_mods`要有`"green_park"`才启用。

## 待办/后续阶段

- 装饰物(长椅/路灯等)还没放，`AddDecoration`接口已就绪(位置按包围盒比例，Core丢掉多边形外的)。
