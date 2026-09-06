# Forever 重构 —— 顶层路线图

## Context

原工程分两部分:
- `E:\Projects\Forever_UE`:主UE工程。UE侧大量玩法逻辑写在蓝图图表里;同时有一套与UE无关、按 `Dependence → Core → Basic` 三层组织的纯C++城市模拟内核(以 common/industry/map/player/populace/society/story/traffic 8个domain划分),被UE层的Base类引用,也被各Mod引用。
- `E:\Projects\Forever_Mods`:若干Mod,每个Mod有自己的Cpp工程(继承内核暴露的基类)、Resource(布局等资源)、UE壳工程三部分。

用户要在新路径 `Forever_UE` / `Forever_Mod` 下重新搭建整套系统,过程中:
1. 尽量把蓝图图表里的逻辑收回到C++(UI蓝图则是"C++基类 + UMG在编辑器里连线布局"的模式),这样后续所有开发都能在代码里看到完整逻辑。
2. 顺带做几处架构性改造:Actor蓝图从"一堆各自继承不同C++类"收敛成"一个通用C++驱动的蓝图外壳";按键改成可配置文件驱动;Mod从"开关"升级为"开关+命令行式参数";新增第一/三人称切换、车辆导航、可自定义楼梯/坡道/电梯、玩家与NPC数据结构统一(可切换被控制角色)等新功能。
3. 工程体量很大(仅内核三层就有150+组h/cpp,UE侧还有~40+个Framework/Element/Player/UI蓝图,以及至少2个Mod),不适合一次性规划到底,因此本次只产出"先做哪块方向、再做哪块方向"的顶层顺序,每个阶段的实现细节留到后续新session里单独细化。

## 现状勘察结论(供后续各阶段session复用,避免重复探索)

- 旧UE工程只有一个UE模块 `Forever`(Runtime)。其 `Source/Forever/Base/` 下已经有一层C++基类(`BuildingBase/RoomBase/PopulaceBase/RoadnetBase/StoryBase/TerrainBase/TrafficBase/ZoneBase/AssetBase/GlobalBase/StartBase`),分别对应 `Content/Blueprint/Framework/` 下的同名蓝图——即这些蓝图并非纯蓝图,已经继承了C++基类,只是图表里还挂着逻辑,任务是把图表逻辑"收回"到对应Base类里,而不是从零建类。
- `Forever.Build.cs` 链接了两个预编译静态库 `Dependence.lib`、`Core.lib`,源码分别在 `Source/Dependence`、`Source/Core`,按 common/industry/map/player/populace/society/story/traffic 8个domain组织,是与UE无关的纯C++城市模拟内核。另外还有一个结构相同但未被 `Forever.Build.cs` 引用的 `Source/Basic`,以及看起来专供Mod使用的 `Source/WXDJ`、`Source/Test`。这几层的准确分工(哪层是Mod可扩展的公共接口层)需要在阶段1细化时进一步确认。
- `Content/Blueprint` 下分四组:`Framework`(9个:Asset/Building/Global/Populace/Roadnet/Room/Story/Terrain/Traffic/Zone,均有同名C++ Base对应)、`Element`(10个,如 AssetElement/BuildingElement/CharacterElement/ElevatorElement/RoomElement/VehicleElement/ZoneElement 等,代表场景里各类具体实例对象)、`Player`(5个:MainCharacter/MainController/MainGameMode/MenuController/MenuGameMode)、`UI`(19个面板/组件)。
- 需求#6要收敛的是 **Framework** 这一组:原来场景里要分别放Asset/Building/Populace/Roadnet/Room/Story/Terrain/Traffic/Zone等好几个Actor,每个各自继承不同的C++ Base类;现在只需要在场景里放一个C++驱动的Actor,内部整合这些原本分散在多个Framework Actor里的职责即可,不用再区分那么多个Actor。**Element 这组不在收敛范围内**——每种Element(AssetElement/BuildingElement/CharacterElement/ElevatorElement/RoomElement/VehicleElement/ZoneElement等)代表场景里不同类型的具体实例对象,本来就应该各是各的Actor类型,只需要按#1把图表逻辑迁移到C++,不涉及架构收敛。
- Mod(如 `Test`、`Wxdj`)的Cpp工程会继承内核暴露的基类(例如已看到的 `BuildingMod`),通过重写 `LayoutBuilding` 等方法 + 引用 `.layout` 资源文件来定义内容,这是阶段8设计"Mod参数化"时的基础。
- 新UE工程目前是全新空壳(`Source/Forever` 只有引擎模板文件,`Content` 只有默认 `World` 关卡,GameMode为空),新Mod目录 `Forever_Mod` 为空目录,一切从零搭建。
- 原工程车辆用的是第三方资产 `Content/3rdParty/CitySampleVehicles`,体积大且效果不理想,新工程不再引入该资产,车辆先用一个可操控移动的立方体代替(占位但可跑通导航/驾驶逻辑),后续如需要更好的表现再单独替换美术资产。

## 贯穿全程的约定

- **文档配对**:阶段1(内核代码)开始,每一组新增/迁移的 `.h/.cpp`(尤其是 Dependence、Core 两层)都配一个同名 `.md`,记录该文件的逻辑/设计思路/注意事项,取代代码注释。这个约定从阶段0起就要定下格式,后续阶段照做。
- **蓝图内容获取方式**:每当某阶段要迁移某个具体蓝图,由用户dump该蓝图内容(图表逻辑、变量、事件绑定等),我再据此写C++;不在没有dump内容的情况下臆测蓝图逻辑。
- **每阶段独立细化**:本计划只定顺序和范围,具体的类设计、文件清单、迁移步骤在该阶段开始时另开session详细规划。
- **跨模块 new/delete 安全**:整个项目(主程序 + Dependence/Core/Basic三层 + 各Mod)是靠加载大量DLL来实现实际逻辑的,跨DLL边界的内存分配/释放如果处理不当会导致堆错乱、崩溃等问题。原项目里已经把能想到的这类情况都做了处理(例如:跨模块传递的对象由分配它的模块负责释放、暴露跨DLL边界的接口避免直接 `new`/`delete`、必要处需要提供由分配方模块导出的销毁函数或改用共享的分配器等具体手法要在阅读原代码时确认)。重构过程中:
  1. 迁移代码时要识别并保留原有的这类跨模块保护写法,不能因为"重构顺手" 或"看起来多余"就删掉;
  2. 每当调整了模块边界或新增了跨DLL传递对象的地方,都要重新检查该处的分配/释放是否仍然安全;
  3. 如果在梳理原代码或新增功能时发现新的跨模块new/delete风险点(包括原项目未覆盖到的),要主动修复,不能绕过或留到以后处理;
  4. 这一条从阶段0确定Source模块划分方式起就要考虑在内,并在阶段1起的每组h/cpp配套的 `.md` 文档里,凡涉及跨模块生命周期管理的地方要专门记录清楚。
- **资源目录调整**:原项目里非UE资源(`layouts`、`public`、`scripts` 等)放在 `Source/Resources` 下;新工程改为直接放在 `Forever_UE/Resource`(与 `Source`、`Content` 平级),不再挂在 `Source` 目录里。阶段0搭工程骨架时就要按新位置建目录,后续任何阶段涉及读取这类资源路径的代码(内核、Mod等)都要用新路径,不要沿用旧的 `Source/Resources` 路径。

## 阶段路线图

设计原则做了一次调整:不再"整块内核先全部迁完、整块Framework再全部迁完",而是先把几个**不依赖内核内容**的骨架/机制搭成空壳(视角与输入、通用Actor框架、Mod加载与参数化框架),然后以**系统为单位**(Map、Populace、Traffic、Society、Industry、Story……)逐个把内核代码和对应的Framework/Element蓝图一起迁移、一起验证,迁一个系统就能在游戏里看到一个系统的效果,而不是等全部迁完才能看到东西。

### 阶段0 —— 工程骨架
目标:让新工程能跑起来。
- 确定新工程的Source模块划分方式(是否延续 Dependence/Core/Basic 三层静态库 + Forever模块的旧结构,还是重新设计),先把目录骨架和构建配置(.Build.cs等)搭起来,哪怕内容是空的。同时按"资源目录调整"约定建好 `Forever_UE/Resource` 目录(与 `Source`、`Content` 平级),取代旧的 `Source/Resources`。
- 搭建最小可玩骨架:C++版 `GameMode`/`PlayerController`/`Character`/`PlayerState`,让默认 `World` 关卡里出现一个可行走的第三人称角色("小白人"),替代目前完全空的GameMode。
- 定下 `.md` 配对文档的格式模板,以及代码风格/命名规范。
依赖:无,是后续一切的地基。

### 阶段1 —— 视角与输入基础(对应需求#1、#8、#9)
目标:小白人能走之后,立刻把视角/输入的基础机制搭好,后续所有阶段都直接用这套机制,不用再回头补。
- 迁移 `Blueprint/Player/*`(MainCharacter/MainController/MainGameMode/MenuController/MenuGameMode)到C++。
- 实现V键切换第一/三人称(#8)。
- 实现配置文件驱动的按键绑定框架:把原来写死的按键都改成可通过外部配置文件修改(#9),默认值沿用旧项目按键,暂不做交互式设置UI。
依赖:阶段0。

### 阶段2 —— Framework收敛为单一Actor的框架空壳(对应需求#6)
目标:先把"收敛Framework Actor结构"的架构搭出来,但先不填具体某个系统的逻辑——设计一个C++驱动的单一Actor(放到场景里的唯一入口),内部按子系统/组件划分,对应原来 Asset/Building/Global/Populace/Roadnet/Room/Story/Terrain/Traffic/Zone 这9个Framework Actor各自承担的职责,先用空实现搭好这个内部划分骨架。后续阶段4逐个系统迁移时,把具体系统的逻辑填进这个Actor内部对应的子系统里,而不是继续在场景里放多个各自独立的Framework Actor。
注意:`Blueprint/Element/*`(AssetElement/BuildingElement/CharacterElement/ElevatorElement/RoomElement/VehicleElement/ZoneElement等)不在本阶段收敛范围内,它们各自类型独立的Actor结构保持不变,只在阶段4按需把图表逻辑迁移到各自对应的C++类里。
依赖:阶段0。不依赖内核内容,可以用空实现先验证架构。

### 阶段3 —— Mod加载框架与参数化机制(对应需求#7,提前到内核迁移之前)
目标:先把Mod的"发现/加载/启用禁用/参数传递"这套通用机制搭出来(空框架,不含Test/Wxdj的具体玩法内容),然后在这个空框架上就把参数化能力加上——即把原来"只能启用/禁用"升级为"启用 + 命令行式参数"(如 `--density 1.0 --max 1000`),用一个最简单的示例/空mod跑通"传参数进去、mod里能读到"这条链路。Test、Wxdj两个mod的具体玩法内容,等到阶段4对应系统(如Wxdj相关的系统)迁移到时再接进这个框架。
依赖:阶段0。不依赖内核内容,只依赖Mod的加载壳机制。

### 阶段4 —— 按系统逐个迁移城市模拟内核 + 对应Framework/Element(对应需求#1、#6落地填充)
目标:这是主体工作量所在。不再整体一次性迁移,而是一个系统一个系统地做完整闭环:
对 Map、Populace、Traffic、Society、Industry、Story 等每个系统,依次做:
  a) 迁移该系统在 `Dependence → Core → Basic` 三层的C++代码(配md,同时确认清楚该系统里 Basic 层与Mod的关系,即Mod是否通过Basic层的接口扩展该系统);
  b) 把该系统对应的 `Blueprint/Framework/*` 蓝图图表逻辑收回C++,填进阶段2搭好的单一Actor框架里对应的子系统(而不是新建独立的Framework Actor子类);同时把该系统相关的 `Blueprint/Element/*` 蓝图图表逻辑迁移到各自独立的C++类里(Element保持多类型,不做收敛,只做#1的蓝图转C++);
  c) 如该系统涉及Mod的可扩展点(例如Building之于 `BuildingMod`),把相关Mod(Test/Wxdj)在该系统上的具体内容迁移进阶段3搭好的Mod框架并验证参数化生效。
具体先迁哪个系统、系统间顺序如何排(大概率Map/Building一类基础系统优先,因为其余系统多依赖建筑/地图数据),在本阶段开始时另开session确定。
依赖:阶段1(视角输入)、阶段2(Actor框架空壳)、阶段3(Mod框架空壳)先就位。

### 阶段5 —— UI蓝图迁移到C++(对应需求#1、#2)
目标:把 `Blueprint/UI/*` 的19个面板/组件按"C++ UserWidget基类 + UMG在编辑器里做布局并绑定C++"的模式逐个迁移。
依赖:阶段4——不少UI面板(背包、商店、地图等)要读取对应系统已迁移到C++的数据,建议该系统迁完后再迁对应UI,不必等全部系统都迁完。

### 阶段6 —— 玩家与NPC数据结构统一、可切换控制角色(对应需求#12)
目标:在阶段4完成Populace系统迁移、阶段1完成Player C++化的基础上,把两者的数据结构统一,实现"没有固定主角、任意NPC可被玩家操控"。
依赖:阶段4的Populace系统、阶段1。

### 阶段7 —— 车辆导航系统(对应需求#10)
目标:在原有行人导航基础上新增车辆导航,包括建筑外自动生成车辆导航图,以及建筑内Layout新增与天花板/地面/房间/导航并列的"载具导航"分类。车辆本身不用原来的 `CitySampleVehicles`,改用一个可操控移动的立方体占位(见"现状勘察结论"),降低资产体积和复杂度。具体方案细节等本阶段开始时再明确。
依赖:阶段4的Map、Traffic系统。

### 阶段8 —— 楼梯/坡道/电梯自定义化(对应需求#11)
目标:把原来固定资产实现的楼梯、斜坡、电梯改造成可自定义/参数化的系统。具体方案细节等本阶段开始时再明确。
依赖:阶段4的Building、Room系统。

## 说明

以上0~8阶段是建议的执行顺序:先搭不依赖内核的骨架/机制(视角输入、Actor框架壳、Mod框架壳),再以系统为单位逐个打通内核+Framework+对应Mod内容,最后做依赖具体系统数据的UI和衍生新功能。这不是最终细节方案,请审阅顺序和范围划分是否符合预期,之后可以调整阶段拆分或顺序;确认后,每个阶段开始前会另开新session针对该阶段(包括阶段4里具体的系统迁移顺序)单独细化实现计划。
