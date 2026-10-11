# ForeverFrameworkComponent

## 职责

`UForeverFrameworkComponent`是所有Framework域组件的抽象基类，继承`UActorComponent`，没有
任何逻辑，只作为类型标签用——单独抽出这一层而不是让各个域组件直接继承`UActorComponent`，
是为了能统一按`UForeverFrameworkComponent::StaticClass()`查询/遍历`AForeverFrameworkActor`
身上的所有域组件，而不用逐个列举具体类型。

`AForeverFrameworkActor`按域划分出9个具体子类，分别对应旧Framework Actor及其C++ Base类：

| 域组件 | 旧Framework Actor | 旧C++ Base类(Core层) |
|---|---|---|
| `UForeverAssetFrameworkComponent` | Asset | `AssetBase`（独立文档见`ForeverAssetFrameworkComponent.md`） |
| `UForeverBuildingFrameworkComponent` | Building | `BuildingBase`（独立文档见`ForeverBuildingFrameworkComponent.md`） |
| `UForeverPopulaceFrameworkComponent` | Populace | `PopulaceBase`（独立文档见`ForeverPopulaceFrameworkComponent.md`） |
| `UForeverRoadnetFrameworkComponent` | Roadnet | `RoadnetBase`（独立文档见`ForeverRoadnetFrameworkComponent.md`） |
| `UForeverRoomFrameworkComponent` | Room | `RoomBase`（当前空实现，独立文档见`ForeverRoomFrameworkComponent.md`） |
| `UForeverStoryFrameworkComponent` | Story | `StoryBase`（独立文档见`ForeverStoryFrameworkComponent.md`） |
| `UForeverTerrainFrameworkComponent` | Terrain | `TerrainBase`（独立文档见`ForeverTerrainFrameworkComponent.md`） |
| `UForeverTrafficFrameworkComponent` | Traffic | `TrafficBase`（独立文档见`ForeverTrafficFrameworkComponent.md`） |
| `UForeverZoneFrameworkComponent` | Zone | `ZoneBase`（独立文档见`ForeverZoneFrameworkComponent.md`） |

这份共用文档现在只覆盖基类本身——9个具体子类全部已经有各自的独立`.md`（含
`UForeverRoomFrameworkComponent`，虽然当前是空实现，但有自己的文档说明原因）。

**没有`UForeverGlobalFrameworkComponent`**：旧工程`GlobalBase`"放在关卡里、串起其它
Framework Actor"的角色，现在整个由`AForeverFrameworkActor`自己承担，不需要再在它内部嵌
一个名叫"Global"的子组件重复扮演这个角色。`GlobalBase`真正的业务逻辑（`GlobalPause`/
`DrawMap`/`InitPhone`等）将来直接落在`AForeverFrameworkActor`自己身上，不会有对应的域
组件，详见`ForeverFrameworkActor.md`。

`StartBase`(Core层另一个Base类)不在这份对照表里——按既定收敛范围有意排除，不是遗漏。
`Blueprint/Element/*`(AssetElement/BuildingElement等)也不在这份对照表里，保持各自独立的
Actor类型不变。

## 关键设计

- 基类本身是空`UCLASS()`，没有构造函数、没有成员——每个具体域组件的接口形状由该域自己的
  Core层Base类逻辑决定，不在基类里预设。
- 用`UActorComponent`而不是让`AForeverFrameworkActor`直接塞好几组虚函数，是因为组件天然
  对应UE的"子系统挂载"惯用法，给某个域填逻辑时改动范围只在该组件自己的.h/.cpp里，不会
  牵动其他域或Actor本体。

## 依赖关系

- 依赖：无（不依赖Dependence/Core内核代码）。
- 被谁依赖：被`AForeverFrameworkActor`在构造函数里用`CreateDefaultSubobject`创建并持有。

## 待办/后续阶段

无——9个域组件均已有独立文档，这份共用文档只剩基类本身的说明。
