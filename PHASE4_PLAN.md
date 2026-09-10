# Forever 重构 —— 阶段4 详细计划：按系统迁移城市模拟内核

## 本文档的定位

`REFACTOR_PLAN.md` 阶段4只给了顶层框架（"对 Map、Populace、Traffic、Society、Industry、Story
等每个系统依次做 a) 内核 b) Framework/Element c) Mod"），具体先迁哪个系统、系统间顺序、每个
domain 内部怎么再拆，原文写明"本阶段开始时另开session确定"。本文档就是这次细化的产出，覆盖
阶段4涉及的**全部 domain**，给出总顺序和每步的验收标准；某个具体 concept（如"Terrain"、
"Building"）的类设计、字段、方法签名，留到用户逐个点名要实现时再当场细化——旧内核体量太大
（仅 Core 一层 map domain 就有8000+行），提前把每个类的接口都设计出来既不现实也容易和实际读
到的旧代码对不上。

## 一、复核：阶段0~3已经搭好的地基

- **8个domain的21个可扩展concept骨架已铺满**（阶段3）：`Source/Dependence/README.md` 有完整
  的"domain × concept × Mod接口 × Factory × 探测符号"对照表，21个 `<Concept>Mod`/
  `<Concept>Factory` 现在都只有 `GetType()`/`GetName()`/`ApplyArgs()`，业务接口留白。
  `Source/Basic/<domain>/<concept>_basic.h` 对应21个占位默认实现。
- **Framework Actor 收敛骨架已搭好**（阶段2）：`AForeverFrameworkActor` + 9个
  `UForeverFrameworkComponent` 子类（Asset/Building/Populace/Roadnet/Room/Story/
  Terrain/Traffic/Zone），等阶段4填逻辑（`Terrain`已在阶段4-1完成）。**没有Global域组件**——
  阶段4-1确认这个角色已经由`AForeverFrameworkActor`自己承担，不需要单独的子组件，详见
  `ForeverFrameworkActor.md`。
- **Mod加载/参数化链路已跑通**（阶段3）：`Config` + `ModLoader` + `ForeverModSubsystem` 验证过
  "config.json → 扫描dll → 加载注册 → 创建实例(自动ApplyArgs) → 读身份信息"整条链路，21个
  concept全覆盖，但目前是`ForeverModSubsystem`临时代管，`Core/common/loader.md`已经写明
  阶段4要把这份职责转移给"真正的领域系统类"（如`Map`/`Story`）。
- **视角/输入/PlayerController骨架已就绪**（阶段1）：`ForeverCharacter`/
  `ForeverPlayerController`/`ForeverGameMode`/`ForeverPlayerState`。`MainController`旧蓝图里
  1334行图表已经dump（`.dump/MainController.txt`）并分析成热键清单，写在
  `MAINCONTROLLER_TODO.md`——那份清单里几乎每一条热键的阻塞依赖，都是"某个阶段4的domain"或
  "某个阶段5的UI"，本文档会在下面每个阶段标注解锁哪些热键。
- **跨模块new/delete的落地手法已经在Factory层验证过一次**（阶段3实现期修正）：
  `Register`/`Create`/`Destroy`必须全部`virtual`（vtable保证调用落在宿主自己编译的实现上），
  销毁必须用注册时mod提供的deleter而不是Factory自己`delete`。阶段4给21个`<Concept>Factory`
  补真正业务方法、以及给各domain新增别的跨DLL传递点时，要延续这个手法，不要重新踩一遍坑。

## 二、关键发现1：内核不是分层DAG，是强耦合的一体化模拟内核

读旧工程 `Source/Core/*/*.h(.cpp)` 的 `#include` 关系发现（详见下方证据），8个domain之间**互相
引用、存在真实环**，不是"Map在底层、其余domain单向依赖Map"这种干净的分层结构：

| 发起方 | 引用了 | 说明 |
|---|---|---|
| `map/building.cpp`/`room.cpp`/`zone.cpp` | `populace/person.h` | 建筑/房间/地块要知道谁在里面 |
| `map/map.cpp`/`room.cpp` | `industry/storage.h`、`industry/manufacture.h` | 房间要挂产业设施 |
| `map/*.cpp` | `story/script.h`、`story/change.h` | 建筑/房间/地图行为受剧情脚本驱动 |
| `populace/person.cpp`/`populace.cpp` | `map/*`、`society/job.h`、`story/*`、`player/asset.h`、`traffic/vehicle.h` | NPC几乎和所有domain打交道 |
| `society/organization.cpp`/`society.cpp` | `map/*`、`populace/person.h`、`populace/scheduler.h`、`story/*` | 组织依赖场地和人 |
| `traffic/traffic.cpp`/`route.cpp` | `map/*`、`populace/person.h`、`player/asset.h`、`story/*` | 车辆/路线依赖路网、乘客 |
| `story/story.cpp` | `map/zone.h`、`map/building.h` | 剧情反过来也依赖地图 |
| `common/implement.h`（顶层门面单例） | 全部7个domain的顶层聚合头 | 见下 |

结论：**这套内核本来就是按"一个互相引用的整体"设计的**，不是可以严格拓扑排序、逐个独立编译
通过的模块链。这决定了阶段4不能指望"迁完Map，Map就能单独编译验证，完全不碰其它domain"，而是
要接受：

1. 早期domain的`.cpp`里，会出现对**还没轮到迁移**的domain类型的引用（如迁Map阶段的
   `Building.cpp`要用到`Person`）。这类引用先用**前置声明+仅保留当前用得到的最小接口**
   （和阶段3给21个`<Concept>Mod`只写`GetType/GetName`占位、业务接口留白是同一种手法），等到该
   domain真正轮到时再补全，不要为了"先跑通"而提前臆测未读到的旧代码。
2. `Core.lib`大概率要等**几个相互耦合的domain都迁完一批**之后才能整体编译通过一次，不追求
   "每个concept迁完都单独跑一次完整build"，但每个concept迁完都应该做语法/类型层面的自查（缺
   的符号提前用占位声明补上）。
3. 判断"这个domain先迁"的依据不是"零依赖"（本来就没有真正零依赖的domain），而是**被引用得
   最多、且能最快在场景里看到效果**的domain——这正是下面的Map。

## 三、关键发现2：Dependence层里藏着一套通用脚本/条件引擎，必须最先迁移

`Source/Dependence/story/{condition,change,event}.h`（旧工程原文件，还没迁到新工程）不是"Story
domain的业务代码"，而是一套**通用、不感知任何具体domain**的规则引擎：

- `condition.h`：`Expression`/`VariableExpression`等表达式类 + `BinaryOperator`/
  `UnaryOperator`，通过`std::function<pair<bool,ValueType>(const string&)>`回调查变量值，纯
  字符串变量名 + 泛型`ValueType`，**不`#include`任何domain的头**。
- `change.h`：`Change`基类 + `ForRangeChange`等具体变化节点，同样只依赖`condition.h`和通用
  `utility.h`。
- `event.h`：（同目录，1677行，同样是通用事件节点定义，未展开细读，但`#include`模式与前两者
  一致，属于同一套引擎）。

也就是说，Story domain其实分两层：

- **底层：脚本引擎本身**（`condition.h`/`change.h`/`event.h`，另加`common/utility.h`提供
  `ValueType`、`common/handle.h`）——这几个文件**当前新工程完全没有**（`Source/Dependence/
  common`目前只有`error`/`json`，`utility.h`/`handle.h`还没迁），但`map`/`populace`/`society`/
  `traffic`四个domain的`.cpp`都要用`story/script.h`（脚本引擎的具体domain绑定层，见下）或
  `story/change.h`等原语——**这是全内核最先应该迁移的一批文件**，比Map还早，因为它是被引用
  最广的公共基础设施，且本身不依赖任何domain数据。
- **上层：Story业务内容**（`story/milestone.h`/`story.h`及其`.cpp`，`story/script.h`里
  domain相关的具体绑定，`StoryBase.ScriptMessage`入口）——这一层要等其它domain都有真实数据后
  才有意义去写（剧情条件/里程碑本来就是"某个建筑达到某状态"、"某人完成某任务"这类跨domain
  判断），应该放在本阶段接近末尾。

`map/geometry.h`（`Node`、`FACE_DIRECTION`等几何原语，被`block.h`引用）性质类似——通用几何工
具，不依赖具体domain数据，应该和脚本引擎一起最先迁移。

## 四、全景表：8个domain的完整类清单

下表把21个Mod可扩展concept（阶段3已铺好骨架）和**同一domain下没有Mod扩展点的"骨干/聚合"类**
放在一起，后者是阶段4真正新增的部分（目前`Source/Core`除了`common`空空如也）。

| Domain | 有Mod扩展点的concept（阶段3已有骨架） | 无Mod扩展点的骨干类（阶段4全新写） | 对应Framework组件 | 对应Element蓝图 | 已有Mod实例 |
|---|---|---|---|---|---|
| map | Terrain、Roadnet、Zone、Building、Component、Room | `Block`、`Map`（聚合）、`geometry.h`原语 | Terrain/Roadnet/Zone/Building/Room（5个） | BuildingElement、RoomElement、ZoneElement、ElevatorElement | Building→`Xiaohua`、`Yuanshen`（`Forever_Mod/Test`） |
| populace | Name、Scheduler | `Person`、`Commute`、`Experience`、`Populace`（聚合） | Populace | CharacterElement | 无 |
| traffic | Route、Station、Vehicle | `Traffic`（聚合） | Traffic | VehicleElement | 无 |
| society | Job、Calendar、Organization | `Society`（聚合） | 无独立Framework组件（是否新增第9个域组件，还是直接落在`AForeverFrameworkActor`自己身上，待细化时确认——**不再考虑"并入Global"，因为Global域组件本身已经在阶段4-1移除**） | 无 | 无 |
| industry | Product、Storage、Manufacture | `Industry`（聚合） | 同上，待确认 | 无 | 无 |
| story | Script | `Milestone`、`Story`（聚合）、脚本引擎原语（`condition`/`change`/`event`，见上节） | Story | 无 | Script→`Wxdj`（`Forever_Mod/Wxdj`） |
| player（**注意**：这是旧内核的domain名，和阶段1已经做完的`Source/Forever/Player`模块**不是一回事**，见下方专门说明） | Asset、App、Puzzle | `Phone`、`Player`（聚合，游戏内"玩家存档态"，不是`AForeverPlayerController`） | Asset | AssetElement | 无 |
| （无对应Dependence domain） | 无 | `GlobalBase`对应的编排逻辑（`GlobalPause`/`DrawMap`/`InitPhone`等，通过`common/implement.h`门面单例引用其余7个domain） | 无独立Framework组件——**阶段4-1确认这个角色已经由`AForeverFrameworkActor`自己承担**（旧工程`GlobalBase`本来就是"放在关卡里、串起其它Framework Actor"的入口，新工程里这件事整个由`AForeverFrameworkActor`做了），`GlobalBase`剩下的编排逻辑将来直接落在这个Actor自己身上，不会有单独的`UForeverGlobalFrameworkComponent`，详见`ForeverFrameworkActor.md` | 无 | 无 |

**关于"player"domain改名的说明**：为避免和阶段1已经写好的`ForeverPlayerController`等混淆，
后续在新工程`Source/Core/`落地这个domain时建议目录名保持`player/`（和`Source/Dependence/
player`、`Source/Basic/player`已有目录一致，改名反而制造新的不一致），但每次涉及这个domain的
`.md`文档开头都要显式加一句"这是旧内核的物件/道具/手机domain，不是UE PlayerController"，动手
实现前也会跟用户再确认一次范围，防止认错文件。

## 五、迁移顺序与理由

### 4-0　共享基础设施（不算独立"系统"，但必须最先做）

- 迁移`Source/Dependence/common/utility.h(.cpp)`、`handle.h(.cpp)`（新工程`common`目前只有
  `error`/`json`，这两个还没搬，`ValueType`等类型在`utility.h`里，是`condition.h`的直接依赖）。
- 迁移`Source/Dependence/story/{condition,change,event}.h`（连同其`.cpp`，如果有）——通用脚本
  引擎，见上节"关键发现2"。
- 迁移`Source/Dependence/map/geometry.h`——通用几何原语。
- 这几个文件迁完后，`story/script_mod.h`可以从"只有GetType/GetName"升级出`Script`真正能挂载
  的接口雏形（但`Script`具体和哪些domain绑定，仍然留到4-6再定，这里只打通引擎本身）。

### 4-1　Map域（Terrain / Zone / Block / Component / Room / Building / Roadnet / Map聚合）

**先做的理由**：被其余6个domain的`.cpp`引用最多（建筑/房间/地图几乎是所有其它系统的"容器"）；
且已经有`Xiaohua`/`Yuanshen`两个真实Building Mod可以拿来端到端验证参数化链路（阶段3只验证过
"能注册"，阶段4要验证"注册的mod能真正参与建造出一栋楼"）；迁完能立刻在场景里看到城市结构，
符合"迁一个系统就能看到一个系统效果"的总原则。

内部再排序建议（同domain内部本身也是环状引用，建议顺序是"先写全部类的骨架签名，再统一实现
细节"，不是完全线性一个个啃完）：
1. `Terrain`、`Zone`、`Block`——最基础的地形/地块划分，`Block`依赖`Zone`+`geometry.h`。
2. `Component`、`Room`——`Component`是`Room`的组成部分（旧代码`component.cpp`引用
   `building.h`/`room.h`，`room.cpp`也反过来引用`component.h`，是真实的环，两者建议一起做）。
3. `Building`——引用前面几乎所有类型，且是Mod扩展点最成熟的一个，`Xiaohua`/`Yuanshen`在这一
   步接入`Forever_Mod/Test`并验证。
4. `Roadnet`——路网数据，为阶段4-3 Traffic和阶段7车辆导航打基础。
5. `Map`（聚合类）——持有以上6个的`<Concept>Factory`，取代`ForeverModSubsystem`临时代管
   Building/Script两个Factory的做法（`Core/common/loader.md`待办项之一）。

**实现期修正**：`Map`的类骨架实际上从第1步`Terrain`就开始存在了，不是等到第5步才创建——
`Terrain`（`Source/Core/map/terrain.h`的`Terrain`包装类）本身不持有任何格子数据，没有一个
持有`Element`格子数据+驱动`DistributeTerrain`的东西，就没有能在PIE里实际看到的地形。所以
`Terrain`落地时顺带创建了`Source/Core/map/map.h`的`Map`类，但**只实现Terrain需要的部分**
（宽高、`Element{terrain,height,water,hatches}`、`TerrainFactory`归属、地形分发+
construction晋升规则），详见`Source/Core/map/map.md`。第2-4步（Zone/Component/Room/
Building/Roadnet）不是"创建"`Map`，是在同一个类上继续扩展字段和方法（各自的Factory、
`zone`/`building`归属字段等）——这和本节开头"先写全部类的骨架签名，再统一实现细节"的建议是
一致的，只是骨架创建的时间点比第5步这个字面顺序更早。

**实现期修正（顺序调整）**：`Roadnet`（本节建议排在第4步，Zone/Component/Room/Building之后）
实际上在`Terrain`完成后就直接由用户点名实现了，跳过了Zone/Block/Component/Room/Building——
这是合法的顺序调整（本文档开头就说明"具体先后顺序由用户在逐个点名时决定，本文档给的是默认
建议，不是强制顺序"）。`Roadnet`落地时在`Map`上新增了车行/行人双导航图、`RoadJunction`路口
数据结构、`Lot`产出（含边界`Road`地址编号）——这些是这次会话跟用户逐条确认后的**新设计**，
老工程完全没有对应实现（导航图分离、车道级数据、路口mesh），不是照抄老工程代码，详见
`Source/Core/map/roadnet.md`。Zone/Block/Component/Room/Building仍然未迁移，`Lot`目前只有
纯几何+边界`Road`关联，不带`zones`/`buildings`挂载字段，等这几个concept自己迁移时再加。

解锁的Framework组件：`UForeverTerrainFrameworkComponent`（已完成，见其独立`.md`）/
`UForeverRoadnetFrameworkComponent`（已完成，见其独立`.md`）/`ZoneFrameworkComponent`/
`BuildingFrameworkComponent`/`RoomFrameworkComponent`。
解锁的Element：`BuildingElement`/`RoomElement`/`ZoneElement`/`ElevatorElement`（电梯楼层逻辑，
阶段8会再深化自定义电梯，这里先按现有旧蓝图逻辑迁）。
解锁的`MAINCONTROLLER_TODO.md`热键：无直接热键（`M`键需要`GlobalBase::DrawMap`，属于4-8
Global（现在直接落在`AForeverFrameworkActor`自己身上，不是单独的域组件，见下）；但`M`键
依赖的`RoadnetBase::GetNavigations`间接依赖这里的`Roadnet`）。

### 4-2　Populace域（Name / Scheduler / Person / Commute / Experience / Populace聚合）

依赖4-1的Map（NPC要有地方住/工作）。解锁`CharacterElement`。为4-3 Traffic（乘客）、4-4
Society（组织成员）、阶段6（玩家/NPC数据统一）打基础。

### 4-3　Traffic域（Route / Station / Vehicle / Traffic聚合）

依赖4-1的`Roadnet`/`Building`、4-2的`Person`（乘客）。解锁`VehicleElement`、
`UForeverTrafficFrameworkComponent`。解锁`MAINCONTROLLER_TODO.md`的**`Q`键**（退出载具，
`Traffic_C::QuitVehicle`）。是阶段7"车辆导航系统"的直接地基，车辆本身仍按"现状勘察结论"用可
操控立方体占位，不引入`CitySampleVehicles`。

### 4-4　Society域（Job / Calendar / Organization / Society聚合）

依赖4-1的`Component`/`Room`、4-2的`Person`/`Scheduler`。无对应Framework组件（Framework
组件列表里没有"Society"，Global域组件本身也已经在阶段4-1移除，不再是"并入Global"的候选），
落地位置留到实现时确认——大概率直接落在`AForeverFrameworkActor`自己身上（呼应Global的处理
方式），或者证明确实需要给它新增第9个域组件（如果这样，需要回头更新
`ForeverFrameworkComponent.md`的对照表并说明是阶段2遗漏还是有意排除）。

### 4-5　Industry域（Product / Storage / Manufacture / Industry聚合）

依赖4-1的`Room`/`Component`，可能依赖4-4的`Organization`（产业归属于组织，具体关系要读旧代码
`industry.cpp`才能确认）。同样没有对应Framework组件，落地位置和Society一起在实现时确认。

### 4-6　Story域业务内容（Script具体业务接口 / Milestone / Story聚合 / `StoryBase.ScriptMessage`）

此时Map/Populace/Traffic/Society/Industry都已经有真实数据，剧情条件/里程碑才有东西可判断。
这一步把`Wxdj`Mod（`script_wxdj.h`）的真实剧情内容接进`Forever_Mod/Wxdj`并验证参数化。解锁
`UForeverStoryFrameworkComponent`，解锁`MAINCONTROLLER_TODO.md`的**`ScriptMessage`函数**（供
外部蓝图调用）。

### 4-7　Player域（Asset / App / Puzzle / Phone / Player聚合）

依赖4-1的`Room`（`Asset`是挂在房间里的可交互物件/容器，见`AssetBase.cpp`的
`NavigateRoomAsset`嵌套寻址逻辑）、4-6的`Story`（`AssetBase`引用`story/event.h`）。这个domain
里`App`/`Puzzle`/`Phone`和UMG强耦合（手机界面、小游戏面板），阶段4只迁移不依赖具体UI Widget
的**数据/规则部分**，UI呈现部分留给阶段5同步做，避免两边来回改。解锁`AssetFrameworkComponent`、
`AssetElement`。解锁`MAINCONTROLLER_TODO.md`的**`P`键**（`GlobalBase::InitPhone`，但完整解锁
还需要阶段5的`PhoneFrame`Widget）。

### 4-8　Global域（`GlobalBase`编排逻辑，收口）

`GlobalBase`通过`common/implement.h`这个门面单例引用全部7个domain，是名副其实的"最后一块拼
图"——只有前面全部迁完，`GlobalPause`/`DrawMap`才有真实内容可以编排。**不落地到单独的域
组件**——阶段4-1（Terrain）实现期间确认`UForeverGlobalFrameworkComponent`这个域组件本身没有
存在的必要：旧工程`GlobalBase`原本就是"放在关卡里、串起其它Framework Actor"的那个入口，这个
角色现在整个由`AForeverFrameworkActor`自己承担了，已经把对应的域组件删掉（见
`ForeverFrameworkActor.md`）。所以这一步的`GlobalPause`/`DrawMap`/`InitPhone`等编排逻辑要
直接实现成`AForeverFrameworkActor`自己的方法，不是某个`UForeverGlobalFrameworkComponent`
的方法。解锁`MAINCONTROLLER_TODO.md`剩余的**`Tab`键**（暂停菜单，
还需阶段5`PausePanel`）、**`M`键**（地图面板，还需新建`CanvasBuffer`工具类+阶段5地图UI）、
**`B`键**（背包面板，还需阶段5`BagPanel`）。这几个热键即使Global域内核逻辑到位，仍然会因为
对应UMG Widget未迁移（阶段5）而无法完整接回，需要在`MAINCONTROLLER_TODO.md`里更新阻塞依赖
状态，不要误以为是阶段4的遗漏。

## 六、每个domain统一执行的动作清单

对上面4-1~4-8每一步，具体到某个concept（如"Building"、"Vehicle"）时，重复下面这套动作（是
`REFACTOR_PLAN.md`阶段4原文a/b/c的细化版）：

1. **读旧代码**：`Source/Core/<domain>/<concept>.h(.cpp)`（业务逻辑）+
   `Source/Dependence/<domain>/<concept>_mod.h`（该concept现有的Mod占位，业务接口从这里对照
   旧`_mod.h`补齐）+ `Source/Basic/<domain>/<concept>_basic.h`（默认内容）。三层一起读，不要
   只读Core层。
2. **确认跨domain引用点**：对还没轮到迁移的domain类型，只保留当前用得到的最小前置声明/占位
   （不臆测未读到的旧代码的完整接口）。
3. **蓝图内容按约定获取**：涉及`Blueprint/Framework/*`或`Blueprint/Element/*`的具体某个蓝图
   时，由用户dump该蓝图内容后再写C++，不在没有dump的情况下凭空还原图表逻辑（`.dump/`目录里
   目前只有Player组三个蓝图的dump，Framework/Element蓝图都还没dump）。
4. **写Core业务逻辑** + **升级对应`<Concept>Mod`/`<Concept>Factory`为真正业务接口**（新增的
   公开方法记得标`virtual`，销毁走deleter，延续阶段3实现期修正踩过的坑）+ **升级
   `<Concept>Basic`为真正默认内容**，三者各自配/升级`.md`。
5. **把该concept逻辑填进对应`ForeverFrameworkComponent`子类**，从空实现升级为有真实职责的类
   时记得单独配`.md`（不再和`ForeverFrameworkComponent.md`共用）。
6. **该domain相关Element蓝图**（如有）逻辑迁移到独立C++类。
7. **该domain若有现成Mod实例**（Building→Xiaohua/Yuanshen，Script→Wxdj），把内容迁移进
   `Forever_Mod`对应工程并验证参数化生效。
8. **回看`MAINCONTROLLER_TODO.md`**，该concept是否解锁了某条热键；解锁了就走已有的
   `UForeverKeyBindingSubsystem`接回（不重新写死按键），暂时因UI未就绪解锁不了的，更新清单里
   的阻塞依赖状态。
9. **编译 + PIE验证**：按`compile_before_claiming_done`的约定，C++改动必须跑一次真实UBT
   build，不能只凭代码审查判断"应该没问题"。

## 七、"concept"最小交付单元与验收标准

用户后续会"逐个concept"点名实现，这里明确"一个concept"具体指什么颗粒度，避免范围模糊：

- **一个concept = 上面全景表里的一行"有Mod扩展点的concept"或"无Mod扩展点的骨干类"**（如
  "Building"、"Room"、"Person"、"Traffic聚合"），包含它在Core/Dependence/Basic三层的代码 +
  它对应的Framework子组件那部分逻辑 + 它直接对应的Element蓝图（如有）。
- **不包含**：还没轮到的domain的完整实现（只到"最小前置声明"程度）、阶段5的UMG Widget（除非
  该concept的验证手段就是最简单的Output Log/已有UI）、阶段6/7/8的衍生新功能。
- **验收标准**：能编译通过（真实UBT build）；能在PIE里看到或验证到这个concept的效果（哪怕只
  是Output Log打印、或者场景里出现一个新Actor）；如果该concept有现成Mod实例，端到端验证一次
  "config.json传参数 → mod产出内容 → 场景里体现"；不能让已经跑通的阶段3验证链路（21个concept
  的Mod注册）出现回归。

## 八、建议的起步顺序

按第五节的4-0/4-1顺序，具体到第一批可以点名的concept，建议：

1. `common/utility.h`+`handle.h`+`story`引擎原语（`condition`/`change`/`event`）+
   `map/geometry.h`——这一步体量大但都是"抄旧代码、无业务决策"，可以作为热身，不需要蓝图dump。
2. `Terrain`（Map域里最简单、依赖最少的一个）。
3. `Zone` → `Block` → `Component`+`Room`（一起） → `Building`（含Xiaohua/Yuanshen接入）→
   `Roadnet` → `Map`聚合。

如果用户更想先看到"能走进一栋楼"的完整效果，也可以调整成优先把`Terrain`/`Zone`/`Room`/
`Building`串起来做一个纵向小闭环，`Block`/`Component`/`Roadnet`的完整实现往后放——具体先后
顺序由用户在逐个点名时决定，本文档给的是默认建议，不是强制顺序。

## 九、开放问题（需要在对应concept开始前和用户确认）

- Framework/Element蓝图的dump目前一份都没有（`.dump/`只有Player组三个），每迁一个具体蓝图前
  都要先请用户dump。
- ~~`Basic.lib`是否要链进`Forever.Build.cs`~~——已确认并修正：`Source/Basic`核对旧工程后
  发现应该是`DynamicLibrary`（产出`Basic.dll`），和`Forever_Mod`下的Mod一样由
  `Config`/`ModLoader`在运行时扫描加载，**不**静态链进`Forever.Build.cs`，详见
  `Source/Basic/README.md`"架构修正"一节。阶段4给`Building`等concept填真实默认内容时，
  只需要把`Source/Basic/<domain>/<concept>_basic.h`里的占位类换成旧工程真正的默认内容目录，
  并在根目录`Basic.cpp`里把对应`RegisterMod<Concept>`从"注册一个占位类"改成"注册多个真实
  类"，不涉及链接方式的变动。
- Society/Industry两个domain没有对应的Framework组件，落地位置（直接落在`AForeverFrameworkActor`
  自己身上，还是新增第9个域组件——Global域组件本身已经在阶段4-1移除，不再是候选之一）留到
  4-4开始时确认，确认后要回头补一句更新到`ForeverFrameworkComponent.md`的对照表说明里。
- `industry`是否依赖`society/organization`（产业归属组织），需要读`industry.cpp`实际代码后
  才能确认4-5是否要反过来放到4-4之前，目前只是按"读到的include关系"做的初步排序，不是最终结论。
