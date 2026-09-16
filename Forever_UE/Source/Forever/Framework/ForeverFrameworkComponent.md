# ForeverFrameworkComponent

## 职责
`UForeverFrameworkComponent`是所有Framework域组件的抽象基类,继承`UActorComponent`,当前完全没有逻辑,只作为类型标签用——之所以单独抽出这一层而不是让各个域组件直接继承`UActorComponent`,是为了阶段4之后能统一按`UForeverFrameworkComponent::StaticClass()`查询/遍历`AForeverFrameworkActor`身上的所有域组件,而不用逐个列举具体类型。

具体域组件(`ForeverAssetFrameworkComponent.h`等)都各自是一个空`UCLASS()`,只做了继承声明,没有独立`.md`——因为它们目前没有任何独特逻辑,单独配文档只会是重复的样板文字。每个组件对应旧Framework Actor及其C++ Base类的关系如下:

| 域组件 | 旧Framework Actor | 旧C++ Base类(Core层) |
|---|---|---|
| `UForeverAssetFrameworkComponent` | Asset | `AssetBase` |
| `UForeverBuildingFrameworkComponent` | Building | `BuildingBase`（阶段4-1已落地，独立文档见`ForeverBuildingFrameworkComponent.md`） |
| `UForeverPopulaceFrameworkComponent` | Populace | `PopulaceBase`（进入populace域已落地，独立文档见`ForeverPopulaceFrameworkComponent.md`） |
| `UForeverRoadnetFrameworkComponent` | Roadnet | `RoadnetBase`（阶段4-1已落地，独立文档见`ForeverRoadnetFrameworkComponent.md`） |
| `UForeverRoomFrameworkComponent` | Room | `RoomBase`（第N轮迁移已落地，独立文档见`ForeverRoomFrameworkComponent.md`） |
| `UForeverStoryFrameworkComponent` | Story | `StoryBase` |
| `UForeverTerrainFrameworkComponent` | Terrain | `TerrainBase`（阶段4-1已落地，独立文档见`ForeverTerrainFrameworkComponent.md`） |
| `UForeverTrafficFrameworkComponent` | Traffic | `TrafficBase` |
| `UForeverZoneFrameworkComponent` | Zone | `ZoneBase`（阶段4-1已落地，独立文档见`ForeverZoneFrameworkComponent.md`） |

**没有`UForeverGlobalFrameworkComponent`（阶段4-1移除）**：阶段2最初按旧工程9个Framework Actor（含Global）1:1建了10个域组件，但`Global`（旧C++ Base类`GlobalBase`）原本的作用就是"旧工程里那个唯一放在关卡里、串起其它Framework Actor的入口"——这个角色现在整个由`AForeverFrameworkActor`自己承担了，不需要再在它内部嵌一个名叫"Global"的子组件重复扮演"可放置入口"这件事。`GlobalBase`真正的业务逻辑（`GlobalPause`/`DrawMap`/`InitPhone`等）将来直接落在`AForeverFrameworkActor`自己身上，不会有对应的域组件，详见`ForeverFrameworkActor.md`和`PHASE4_PLAN.md`。

`StartBase`(Core层另一个Base类)不在这份对照表里——`REFACTOR_PLAN.md`阶段2原文只列了以上Framework Actor的收敛范围,没有提到Start,本次按原文执行,有意排除,不是遗漏。`Blueprint/Element/*`(AssetElement/BuildingElement等)也不在收敛范围内,保持各自独立的Actor类型不变,详见`REFACTOR_PLAN.md`阶段2说明。

## 关键设计
- 基类和各子类当前都是空`UCLASS()`,没有构造函数、没有成员——阶段2的目标只是把"内部按域划分"的骨架和命名固定下来,不是过早设计每个域的接口(那些接口形状要等阶段4读到对应Core层Base类的实际逻辑后才能确定,提前设计容易和真实需求对不上)。
- 用`UActorComponent`而不是让`AForeverFrameworkActor`直接塞好几组虚函数,是因为组件天然对应UE的"子系统挂载"惯用法,阶段4给某个域填逻辑时改动范围只在该组件自己的.h/.cpp里,不会牵动其他域或Actor本体。

## 依赖关系
- 依赖:无(不依赖Dependence/Core内核代码)。
- 被谁依赖:被`AForeverFrameworkActor`在构造函数里用`CreateDefaultSubobject`创建并持有。

## 待办/后续阶段
- 阶段4:按系统迁移到哪个域,就在对应域组件的.h/.cpp里把该系统在Core层Base类(如`BuildingBase`)里的图表逻辑实现进去,并从"空实现"升级为有实际职责的类时,记得为该组件补上独立的`.md`(不再和这份共享文档合用)。`UForeverTerrainFrameworkComponent`/`UForeverRoadnetFrameworkComponent`(阶段4-1)/`UForeverZoneFrameworkComponent`/`UForeverBuildingFrameworkComponent`/`UForeverRoomFrameworkComponent`是已经这样毕业的,其余5个组件仍在这份共用文档里,毕业方式照抄即可。
