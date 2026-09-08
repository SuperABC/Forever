# ForeverFrameworkActor

## 职责
`AForeverFrameworkActor`是场景里唯一的Framework入口Actor,对应`REFACTOR_PLAN.md`阶段2(需求#6)"把9个各自独立的Framework Actor收敛成一个C++驱动的Actor"的目标。内部按域划分为10个`UForeverFrameworkComponent`子类(见`ForeverFrameworkComponent.md`的对照表),分别对应旧的Asset/Building/Global/Populace/Roadnet/Room/Story/Terrain/Traffic/Zone这9(原文列了10个名字,计数与名单对不上,按名单执行)个Framework Actor。

本阶段(阶段2)只搭这个划分骨架,所有域组件都是空实现,不含任何具体系统逻辑。

## 关键设计
- 构造函数里手动创建一个`USceneComponent`当`RootComponent`,让这个Actor在编辑器里可以被放置、有transform(旧Framework Actor作为场景里放置的对象,新Actor延续这个可放置的定位)。
- 10个域组件全部用`CreateDefaultSubobject`在构造函数里创建并作为`AForeverFrameworkActor`的固定组成部分——不做成运行时按需添加,因为这个Actor本身就是"场景里唯一一份、职责固定"的单例式入口,不需要动态增删域。
- `BeginPlay`里打一条临时日志列出10个已初始化的域组件名字。这是阶段2专属的验证手段:现在没有任何视觉/玩法效果,只能靠Output Log确认骨架构造正常;阶段4填入真实逻辑后,这条日志可以删除或替换成更有意义的调试信息。

## 依赖关系
- 依赖`Framework/ForeverFrameworkComponent.h`及其10个具体子类头文件。
- 被`AForeverGameMode::BeginPlay`引用:场景里没有找到已放置的`AForeverFrameworkActor`实例时,会动态`SpawnActor`一个兜底(和`ForeverGameMode.md`里占位地板的逻辑同一套模式),详见该文档。

## 待办/后续阶段
- 阶段4:按系统迁移进度,逐个把对应域组件从空实现填成真正的逻辑(读取Core层对应Base类+对应Framework蓝图dump)。
- 阶段4/关卡搭建阶段:一旦有人在`World.umap`里手动放置了真实的`AForeverFrameworkActor`,应该移除`ForeverGameMode`里的动态兜底生成逻辑,不要误以为是正式生成方案。
