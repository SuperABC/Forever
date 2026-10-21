# player.h / player.cpp

**注意**：这是"物件/道具/手机"domain（`Asset`/`App`/`Puzzle`/`Weapon`四个Mod扩展点）的
`Player`类，不是UE的`AForeverPlayerController`——两者是完全不同的东西，动手前先确认没有
认错文件。

`Player`目前承载三块：全局时钟（`Time*`+`Init/Tick/GetTime/SetTime/CrossDay`）、生命值
（`GetHealth`/`TakeDamage`/`Heal`/`IsDead`）、资产系统（`Asset`的五个槽位+不动产/载具表+
路径寻址）。手机(`Phone`)、存款等字段/方法尚未迁移。

## 时钟行为

- `Init()`：创建`Time`，默认设为8点（年份等其余字段是`Time()`默认构造的"无效时间"）。
  紧接着`AForeverFrameworkActor::EnsurePlayerGenerated()`会调用一次`SetTime`把年份改成
  市民繁衍模拟算出来的年份，见下"开局时间=人口模拟结束年份"一节——`Init()`本身不处理
  这件事，只负责先把时钟建好。
- `Tick(float delta)`：每帧按`delta * 60 * 1000 * timeFlowRatio`毫秒推进时钟，即
  `timeFlowRatio == 1.0`时"1真实秒 = 1游戏分钟"。默认值`10.0`（1现实秒=10游戏分钟，一天
  1440分钟需要144现实秒）。**不要把`timeFlowRatio`调得太高**：流速太快时`Populace::Tick`/
  `Society::Tick`的`jobTimerSet`/`organizationTimerSet`会在同一时刻堆积大量到期节点，
  每帧固定上限(`kMaxJobTimersPerTick`=4)吐不完积压，表现为9点/12点这类"大量市民同一时刻
  下班/上班"场景卡顿。

  `timeFlowRatio`是可写成员，`SetTimeFlowRatio(ratio)`覆盖默认值：
  `AForeverFrameworkActor::EnsurePlayerGenerated()`在`player->Init()`之后读一次主线
  剧情`.script`的`global_settings.time_flow_ratio`字段（`Script::GetGlobalSettings(
  Config::GetMainStoryScriptPath())`），有声明就调`SetTimeFlowRatio`覆盖默认值，没有
  就保持默认值不变。`global_settings`是主线剧情`.script`文件级别的顶层字段（不是
  `config.json`级别，也不是运行时`Change`可改的），在`Story`对象创建之前、`Player`生成
  的时候一次性读完，见`Core/story/script.md`"主线剧情.script新增三个顶层字段"一节。
  `Story`目前只有`systemScript`一个`Script*`做`system.`前缀变量池，没有独立的settings
  字典。
- `SetTime(const Time&)`/`CrossDay()`：供以后`change_time`一类Change或Society/
  Populace/Industry的"跨天触发日程"逻辑使用——`CrossDay()`目前没有任何调用方接入，只是
  接口先落地；`SetTime`目前唯一的调用方是下面这条"开局时间=人口模拟结束年份"的逻辑。

## 开局时间=人口模拟结束年份

`Populace`的"市民繁衍模拟"（`GenerateCitizens`）从2000年起演化一个虚拟人口史（至少100年，
命中人口目标或撑到4096年上限为止），演化到第几年就存进`Populace::currentYear`成员
（`GetCurrentYear()`读取，`Init()`跑完之前默认`2000`）。`AForeverFrameworkActor::
EnsurePlayerGenerated()`在`player->Init()`之后把这个年份喂回`Player`的时钟，保证市民的
出生年/结婚年和开局时钟自洽：

```cpp
player->SetTime(Time(populace->GetCurrentYear(), 1, 1, 8));
```

开局时间固定是模拟结束那一年的**1月1日8点**。

## 生命值

进入武器系统时新增的最小闭环——只做"扣血/回血/是否死亡"，不做死亡后果（复活/game over
等），见`Source/Forever/Player/ForeverWeaponComponent.md`。`health`/`maxHealth`默认都是
`100.f`（随手给的默认值）。`TakeDamage(amount)`/`Heal(amount)`都假定`amount`是正数，不做
防御性纠正，结果clamp到`[0, maxHealth]`。

## 资产系统

`leftHand`/`rightHand`/`backPack`/`leftShoulder`/`rightShoulder`五个槽位+
`estateAssets`（不动产/载具表，key是`Asset::GetName()`）均持有所有权，析构时全部
`delete`。

- `CreateAsset(id, name)`/`DestroyAsset(asset)`：`id`需要在`config.json`的
  `"asset_mods"`数组里启用，否则`CreateAsset`返回`nullptr`（内部`new`后立刻检查
  `IsValid()`失败就`delete`）。独占持有权转交给调用方，调用方要么通过`AddByPath`等接口
  把它放进某个槽位/容器/房间，要么自己负责`DestroyAsset`。
- 路径寻址（`SplitPath`/`GetByPath`/`RemoveByPath`/`AddByPath`）：`"left"`/`"right"`/
  `"back"`/`"leftShoulder"`/`"rightShoulder"`对应五个槽位，`"room"`对应
  `GetCurrentRoom()`（`"room/<assetName>"`按`Room::GetAssets()`查），多段路径（如
  `"back/PistolAmmo"`）按`'/'`切分后逐段解析到容器再查`GetContents()`。
- `AddByPath`的放入规则：`left`/`right`(手)要求`mobility==Object`且槽位为空且
  `!asset->IsWeapon()`（武器不能拿在手上，只能挂肩膀）；`back`同上，额外要求
  `asset->GetBackpack()==true`；`leftShoulder`/`rightShoulder`要求
  `asset->IsWeapon()==true`且槽位为空（只能放武器）；`room`要求
  `GetCurrentRoom()!=nullptr`，转发给`currentRoom->AddAsset(asset)`；多段路径转发给
  `container->AddContent(asset)`，无mobility限制。
- 不动产/载具（`AddEstateAsset`/`GetEstateAsset`/`RemoveEstateAsset`）不走五槽位，
  `mobility`必须是`Estate`/`Vehicle`。
- `GetCurrentRoom()`/`SetCurrentRoom()`维护的是真实物理位置（不是剧情/场景语义），由
  `BuildingElement`的Room重叠检测回调维护，见
  `Source/Forever/Element/BuildingElement.cpp`的`OnRoomOverlapBegin`/`End`。
- `CountByType`/`ConsumeByType`只转发到`backPack`（不存在返回0/false），供
  `ForeverWeaponComponent`换弹时查/扣备弹用。

## 投掷物查询：`CollectThrowables`/`TakeOneThrowable`/`HasThrowableInHand`

投掷物(`Asset::IsThrowable()`)可能在手上、手持容器里、背包及嵌套容器里。三个方法用同一套固定扫描顺序：
**左手 -> 右手 -> 背包**，每个位置先看资产本身再递归进`contents`(子项按name排序，`unordered_map`遍历顺序
不确定，不排序会让"按3切换"的顺序飘)。`CollectThrowables`按type合并数量并带上第一个堆叠的
`meshPath`/`meshScale`(供UE侧显示预览模型)；`TakeOneThrowable`取出一个单位——堆叠`count>1`时`count-1`并
新建一个`count=1`的Asset(名字`type_thrown<n>`，容器按name索引需要唯一)，`count==1`时整个摘出，所有权交给
调用方；`HasThrowableInHand`只认手槽里**直接**拿着的，在手持容器里面的不算(那只手仍被容器占用)。
UE侧缓存/激活逻辑见`Source/Forever/Player/ForeverThrowableComponent.md`。

## 房间外的物品：`worldAssets`

不在任何房间、也不在身上/容器里的物品（目前就是被扔出去的投掷物）归 Player 直接持有，key=物品名，带一份位置朝向（UE 世界坐标，厘米/度；Core 只存数不解释）。物品最终落在室内也照样记在这里，不转进 `Room::assets`。位置由 UE 侧在存档前从场景里的 Actor 同步回来（见 `ForeverAssetFrameworkComponent.md`），Core 平时不更新。`TakeOneThrowable` 拆出新物品时的起名会跳过这里已有的名字（计数器不存档，读档后会从 0 重数）。

## 持有方式

和`Map`/`Populace`/`Story`一样，直接挂在`AForeverFrameworkActor`身上（`Player* player`
成员，生命周期管理方式相同）：`EnsurePlayerGenerated()`里`new Player()`后紧接着调用
`player->Init()`（和`story = new Story(); story->Init();`同一个写法），不是被
`UForeverAssetFrameworkComponent`持有。

`AForeverFrameworkActor`的`PrimaryActorTick.bCanEverTick`为`true`，`Tick(float
DeltaTime)`覆写里唯一做的事就是`player->Tick(DeltaTime)`，见
`ForeverFrameworkActor.md`"Tick"一节。

## 对外查询

`Core/common/implement.h`的`PostImplement`提供`"game time"`这个post类型（和
`"random citizen"`同一个模式），mod/脚本可以通过`Post({"post":"game time"})`拿到
`{"result":"success","date":"YYYY-MM-DD","time":"HH:mm"}`（`Init()`还没跑完/`time`为空
时返回`{"result":"fail","msg":"no game time available."}`）。

## 依赖关系

- 依赖：`Dependence/common/utility.h`（`Time`类）、`Dependence/player/asset_mod.h`/
  `asset_factory.h`（通过`Core/player/asset.h`的`Asset`/`AssetFactory`）、
  `Core/map/room.h`（`Room::GetAssets`/`AddAsset`/`RemoveAsset`）。
- 被谁依赖：`Core/common/implement.h`（`PostImplement`构造函数+`"game time"`查询）、
  `Source/Forever/Framework/ForeverFrameworkActor.h/.cpp`（持有+每帧`Tick`驱动）、
  `Source/Forever/Player/ForeverWeaponComponent`（生命值/备弹查询）、
  `Source/Forever/Player/ForeverCharacter.cpp`（资产槽位读写）。

## 范围边界

- 不迁移手机(`Phone`)、存款相关字段/方法。
- 不维护`Story::GetSetting`/`Config::GetGlobalSettings`那一套全局设置字典——
  `time_flow_ratio`走主线剧情`.script`的`global_settings`顶层字段（见上"时钟行为"
  一节），不支持`GlobalSettingChange`这类运行时改写。
- 不把`system.time.year/month/...`写进`Story`的`systemScript`变量池——`Story`没有
  `Tick()`。
- `CrossDay()`目前没有任何消费方接入（`Society`/`Industry`都是空骨架）。
- 没有暂停机制（对应`AGlobalBase::GlobalPause/GlobalResume`那一套）。
