# ForeverFrameworkComponent

## 职责

`UForeverFrameworkComponent`是所有Framework域组件的抽象基类，继承`UActorComponent`，没有任何逻辑，只作为
类型标签用。`AForeverFrameworkActor`在构造函数里用`CreateDefaultSubobject`创建16个具体子类，每个子类把一个
Core域(或一类表现)翻译成UE里的网格/Actor/效果：

| 域组件 | 负责的内容 |
|---|---|
| `UForeverTerrainFrameworkComponent` | 地形网格(clipmap式LOD)、地形材质 |
| `UForeverWaterFrameworkComponent` | 水面、水下后处理 |
| `UForeverRoadnetFrameworkComponent` | 道路/路口网格、桥墩 |
| `UForeverZoneFrameworkComponent` | 园区围墙、园区触发盒 |
| `UForeverGreenFrameworkComponent` | 绿化区的装饰物 |
| `UForeverBuildingFrameworkComponent` | 建筑LOD调度与`ABuildingElement`的生成 |
| `UForeverRoomFrameworkComponent` | 目前是空实现(房间的表现都在`ABuildingElement`里)，原因见`ForeverRoomFrameworkComponent.md` |
| `UForeverDoorFrameworkComponent` | 全图门的流式生成/销毁 |
| `UForeverVegetationFrameworkComponent` | 全图植被实例与风 |
| `UForeverLampFrameworkComponent` | 路灯实例与昼夜开关 |
| `UForeverSkyFrameworkComponent` | 昼夜循环 |
| `UForeverWeatherFrameworkComponent` | 天气表现(云/雨雪/地面湿润) |
| `UForeverPopulaceFrameworkComponent` | 市民Actor的流式生成、走路/通勤/任务的执行 |
| `UForeverTrafficFrameworkComponent` | 载具Actor、线路车辆、导航线、车位 |
| `UForeverAssetFrameworkComponent` | 地上物品Actor、背包相关Change、伤害/特效/音效 |
| `UForeverStoryFrameworkComponent` | 事件广播、对话/小游戏/商店界面、计时器、操控权切换 |

每个子类各有同名的独立`.md`，这份文档只说明基类本身。

## 关键设计

- 基类是空`UCLASS()`，没有构造函数、没有成员——每个域组件的接口形状由该域自己的需要决定，不在基类里预设。
- 用组件而不是把所有逻辑堆在`AForeverFrameworkActor`上：给某个域改逻辑时改动范围只在该组件自己的`.h/.cpp`里。
- "串起各个域"的全局逻辑(分阶段生成世界、全局时钟、`StepWorld`、`ApplyChange`分发、存读档编排)在
  `AForeverFrameworkActor`自己身上，没有对应的"Global"域组件，见`ForeverFrameworkActor.md`。
- 域组件和Core域对象是两回事：Core域(`Map`/`Populace`/`Traffic`…)由`AForeverFrameworkActor`创建并持有，域组件
  只拿裸指针读它们，世界销毁时(`EndPlay`)随Actor一起失效。

## 依赖关系

- 依赖：无(基类不依赖Dependence/Core)。
- 被谁依赖：`AForeverFrameworkActor`(创建并持有全部子类)。

## 待办/后续阶段

- 无。
