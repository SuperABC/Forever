# player.h / player.cpp

**注意**：这是"物件/道具/手机"domain（`Asset`/`App`/`Puzzle`/`Weapon`四个Mod扩展点）的
`Player`类，不是UE的`AForeverPlayerController`——两者是完全不同的东西，动手前先确认没有
认错文件。

`Player`承载：全局时钟（`Time*`+`Init/Tick/GetTime/SetTime/CrossDay`）、天气（`Weather`，见
`weather.md`）、生命值（`GetHealth`/`TakeDamage`/`Heal`/`IsDead`）、资产系统（`Asset`的五个槽位+
不动产/载具表+房外物品`worldAssets`+路径寻址+脚本驱动的收纳/扣除）。`Phone`不由`Player`持有，
由UE侧`PhoneWidget`持有。

## 时钟行为

- `Init()`：创建`Time`，默认设为8点（年份等其余字段是`Time()`默认构造的"无效时间"）。
  紧接着`AForeverFrameworkActor::EnsurePlayerGenerated()`会调用一次`SetTime`把年份改成
  市民繁衍模拟算出来的年份，见下"开局时间=人口模拟结束年份"一节——`Init()`本身不处理
  这件事，只负责先把时钟建好。
- `Tick(float delta)`：每帧按`delta * 60 * 1000 * timeFlowRatio`毫秒推进时钟，即
  `timeFlowRatio == 1.0`时"1真实秒 = 1游戏分钟"。默认值`10.0`（1现实秒=10游戏分钟，一天
  1440分钟需要144现实秒）。**不要把`timeFlowRatio`调得太高**：流速太快时`Populace::Tick`/
  `Society::Tick`的`jobTimerSet`/`organizationTimerSet`会在同一时刻堆积大量到期节点，
  每帧固定上限(`kMaxJobTimersPerTick`=16)吐不完积压，表现为9点/12点这类"大量市民同一时刻
  下班/上班"场景卡顿。

  `timeFlowRatio`是可写成员，`SetTimeFlowRatio(ratio)`覆盖默认值：
  `AForeverFrameworkActor::EnsurePlayerGenerated()`在`player->Init()`之后遍历
  `Config::GetMainStoryScriptPaths()`，对每个主线脚本读`Script::GetGlobalSettings(path)`的
  `time_flow_ratio`，有声明就调`SetTimeFlowRatio`（多个文件后者覆盖前者），没有就保持默认值。
  `global_settings`是主线脚本文件级别的顶层字段，见`story/script.md`。运行时也可由
  `global_setting` Change（`AForeverFrameworkActor::ApplyChange`处理）改写。
- `SetTime(const Time&)`/`CrossDay()`：`SetTime`记下跳变前的天数，让`CrossDay()`（相对上一次
  `Tick`/`SetTime`时的天数）能正确检测跨天；`AForeverFrameworkActor::Tick`据此判断
  `crossedDay`（第一帧强制当作跨天），`SetTime`目前用于开局时间。

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

- `CreateAsset(id)`/`DestroyAsset(asset)`：名字不再由调用方传入，取mod的`GetName()`（mod保证全局唯一，读档时`PlayerSaveCodec`把名字覆写回存档里的）。`SplitAsset(src, n)`从堆叠拆出n个：再`CreateAsset`一个同type的新实例（名字由它的mod保证唯一）、`count=n`，`src->count`减n。`id`需要在`config.json`的
  `"asset_mods"`数组里启用，否则`CreateAsset`返回`nullptr`（内部`new`后立刻检查
  `IsValid()`失败就`delete`）。独占持有权转交给调用方，调用方要么通过`AddByPath`等接口
  把它放进某个槽位/容器/房间，要么自己负责`DestroyAsset`。
- 路径寻址（`SplitPath`/`GetByPath`/`RemoveByPath`/`AddByPath`）：`"left"`/`"right"`/
  `"back"`/`"leftShoulder"`/`"rightShoulder"`对应五个槽位，`"room"`对应
  `GetCurrentRoom()`（`"room/<assetName>"`按`Room::GetAssets()`查），多段路径（如
  `"back/PistolAmmo"`）按`'/'`切分后逐段解析到容器再查`GetContents()`。
- `AddByPath`的放入规则：`left`/`right`(手)要求`mobility==Object`、`count==1`且槽位为空
  （武器也可以拿在手上，只是激活仍然只走肩膀）；`back`同上，额外要求
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
`SplitAsset(found, 1)`新建一个`count=1`的Asset(名字由mod保证唯一)，`count==1`时整个摘出，所有权交给
调用方；`HasThrowableInHand`只认手槽里**直接**拿着的，在手持容器里面的不算(那只手仍被容器占用)。
UE侧缓存/激活逻辑见`Source/Forever/Player/ForeverThrowableComponent.md`。

## 房间外的物品：`worldAssets`

不在任何房间、也不在身上/容器里的物品（目前就是被扔出去的投掷物）归 Player 直接持有，key=物品名，带一份位置朝向（UE 世界坐标，厘米/度；Core 只存数不解释）。物品最终落在室内也照样记在这里，不转进 `Room::assets`。位置由 UE 侧在存档前从场景里的 Actor 同步回来（见 `ForeverAssetFrameworkComponent.md`），Core 平时不更新。拆出的新物品是新建的实例，名字由它的 mod 保证全局唯一。

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

- 依赖：`player/weather.h`、`Dependence/common/utility.h`（`Time`类）、`Dependence/player/asset_mod.h`/
  `asset_factory.h`（通过`Core/player/asset.h`的`Asset`/`AssetFactory`）、
  `Core/map/room.h`（`Room::GetAssets`/`AddAsset`/`RemoveAsset`）。
- 被谁依赖：`Core/common/implement.h`（`PostImplement`构造函数+`"game time"`查询）、
  `Source/Forever/Framework/ForeverFrameworkActor.h/.cpp`（持有+每帧`Tick`驱动）、
  `Source/Forever/Player/ForeverWeaponComponent`（生命值/备弹查询）、
  `Source/Forever/Player/ForeverCharacter.cpp`（资产槽位读写）。

## 范围边界

- 不含存款相关字段/方法。
- 没有暂停机制（对应`AGlobalBase::GlobalPause/GlobalResume`那一套）。

## 脚本驱动的收纳/扣除
- `StoreAsset(asset)`：背包→容器→右手→左手的收纳顺序(见`change.md`的pick_asset)，失败不改变状态。
- `CountOwned(type)`/`TakeOwned(type, amount, out)`：左右手+背包(递归容器，不含肩膀)里的数量与摘取(必要时拆堆叠)，数量不够不改变状态。
- `FindAssetByName(name)`：手/肩/背及容器内容、estate、worldAssets里按名字找；房间里的见`Map::FindAssetByName`。
- `Player::ApplyChange`处理资产的`add_option`/`remove_option`(房间里的资产由`Map::ApplyChange`处理)。
