# ForeverFrameworkComponent

## 职责
`UForeverFrameworkComponent`是所有Framework域组件的抽象基类,继承`UActorComponent`,当前完全没有逻辑,只作为类型标签用——之所以单独抽出这一层而不是让10个域组件直接继承`UActorComponent`,是为了阶段4之后能统一按`UForeverFrameworkComponent::StaticClass()`查询/遍历`AForeverFrameworkActor`身上的所有域组件,而不用逐个列举10个具体类型。

10个具体域组件(`ForeverAssetFrameworkComponent.h`等)都各自是一个空`UCLASS()`,只做了继承声明,没有独立`.md`——因为它们目前没有任何独特逻辑,单独配文档只会是10份重复的样板文字。每个组件对应旧Framework Actor及其C++ Base类的关系如下:

| 域组件 | 旧Framework Actor | 旧C++ Base类(Core层) |
|---|---|---|
| `UForeverAssetFrameworkComponent` | Asset | `AssetBase` |
| `UForeverBuildingFrameworkComponent` | Building | `BuildingBase` |
| `UForeverGlobalFrameworkComponent` | Global | `GlobalBase` |
| `UForeverPopulaceFrameworkComponent` | Populace | `PopulaceBase` |
| `UForeverRoadnetFrameworkComponent` | Roadnet | `RoadnetBase` |
| `UForeverRoomFrameworkComponent` | Room | `RoomBase` |
| `UForeverStoryFrameworkComponent` | Story | `StoryBase` |
| `UForeverTerrainFrameworkComponent` | Terrain | `TerrainBase` |
| `UForeverTrafficFrameworkComponent` | Traffic | `TrafficBase` |
| `UForeverZoneFrameworkComponent` | Zone | `ZoneBase` |

`StartBase`(Core层另一个Base类)不在这份对照表里——`REFACTOR_PLAN.md`阶段2原文只列了以上10个Framework Actor的收敛范围,没有提到Start,本次按原文执行,有意排除,不是遗漏。`Blueprint/Element/*`(AssetElement/BuildingElement等)也不在收敛范围内,保持各自独立的Actor类型不变,详见`REFACTOR_PLAN.md`阶段2说明。

## 关键设计
- 基类和10个子类当前都是空`UCLASS()`,没有构造函数、没有成员——阶段2的目标只是把"内部按域划分"的骨架和命名固定下来,不是过早设计每个域的接口(那些接口形状要等阶段4读到对应Core层Base类的实际逻辑后才能确定,提前设计容易和真实需求对不上)。
- 用`UActorComponent`而不是让`AForeverFrameworkActor`直接塞10组虚函数,是因为组件天然对应UE的"子系统挂载"惯用法,阶段4给某个域填逻辑时改动范围只在该组件自己的.h/.cpp里,不会牵动其他域或Actor本体。

## 依赖关系
- 依赖:无(不依赖Dependence/Core内核代码)。
- 被谁依赖:被`AForeverFrameworkActor`在构造函数里用`CreateDefaultSubobject`创建并持有。

## 待办/后续阶段
- 阶段4:按系统迁移到哪个域,就在对应域组件的.h/.cpp里把该系统在Core层Base类(如`BuildingBase`)里的图表逻辑实现进去,并从"空实现"升级为有实际职责的类时,记得为该组件补上独立的`.md`(不再和这份共享文档合用)。
