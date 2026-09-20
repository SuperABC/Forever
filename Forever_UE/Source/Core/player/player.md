# player.h / player.cpp

**注意**：这是老工程的"物件/道具/手机"domain（`Asset`/`App`/`Puzzle`三个Mod扩展点），不是
UE的`AForeverPlayerController`——两者是完全不同的东西，动手前先确认没有认错文件，见
`PHASE4_PLAN.md`"关于player domain改名的说明"一节。

这次迁移了老工程`Player`里的"全局时钟"这一小块（`Time*`+`Init/Tick/GetTime/SetTime/
CrossDay`），照抄老工程`Source/Core/player/player.cpp`同名方法的实现。手机(`Phone`)、
资产(`Asset`)、存款等老`Player`剩下的字段/方法还没有迁移，留到`PHASE4_PLAN.md`阶段4-7
再点名做——这次的目的是让`AForeverFrameworkActor::Tick`能驱动一个真正会走的游戏时钟，
不是把整个player域一次性做完。

## 时钟行为

- `Init()`：创建`Time`，默认设为8点（年份等其余字段是`Time()`默认构造的"无效时间"）。
  紧接着`AForeverFrameworkActor::EnsurePlayerGenerated()`会调用一次`SetTime`把年份改成
  市民繁衍模拟算出来的年份，见下"开局时间=人口模拟结束年份"一节——`Init()`本身不知道
  这件事，只负责先把时钟建好。
- `Tick(float delta)`：每帧按`delta * 60 * 1000 * time_flow_ratio`毫秒推进时钟，即
  `time_flow_ratio == 1.0`时"1真实秒 = 1游戏分钟"。

  **`time_flow_ratio`这次写死常量`1.0`**，不接受`Story*`参数——老工程这个倍率来自
  `Story`的全局设置字典（`Config`的`global_setting`，脚本可用`GlobalSettingChange`改），
  新工程的`Story`目前只有`systemScript`这一个`Script*`做`system.`前缀变量池，没有独立的
  settings字典（见`Story.md`"变量系统"一节），重建那一整套机制超出这次"先让时钟走起来"
  的范围，以后真要支持脚本调速再加。
- `SetTime(const Time&)`/`CrossDay()`：直接照抄老工程实现，供以后`change_time`一类
  Change或Society/Populace/Industry的"跨天触发日程"逻辑使用——这次`CrossDay()`还没有
  任何调用方接入，只是把接口先落地；`SetTime`这次唯一的调用方是下面这条"开局时间=
  人口模拟结束年份"的逻辑。

## 开局时间=人口模拟结束年份

老工程`Populace`的"市民繁衍模拟"（`GenerateCitizens`）从2000年起演化一个虚拟人口史（至少
100年，命中人口目标或撑到4096年上限为止），演化到第几年就把`Player`的时钟年份直接改成
`2000+那个年份`——这样市民的出生年/结婚年才和开局时钟自洽。新工程的`Populace::
GenerateCitizens`（`Core/populace/populace.cpp`）同样做了这个演化，结果存进
`Populace::currentYear`成员（`GetCurrentYear()`读取，`Init()`跑完之前默认`2000`），但
一直没有喂回`Player`的时钟——这次在`AForeverFrameworkActor::EnsurePlayerGenerated()`里
`player->Init()`之后补上：

```cpp
player->SetTime(Time(populace->GetCurrentYear(), 1, 1, 8));
```

开局时间固定是模拟结束那一年的**1月1日8点**（月/日/时这三个字段是这次显式指定的，不是
老工程"先建时钟(Jan 1, 00:00)+`SetHour(8)`+`SetYear`三步分开改"那种历史遗留写法的巧合
结果，效果一致）。实测：某次PIE跑出`populace->GetCurrentYear()==2410`，开局时钟正确显示
`2410-01-01 08:00`。

## 持有方式

和`Map`/`Populace`/`Story`一样，直接挂在`AForeverFrameworkActor`身上（`Player* player`
成员，生命周期管理方式相同）：`EnsurePlayerGenerated()`里`new Player()`后紧接着调用
`player->Init()`（和`story = new Story(); story->Init();`同一个写法），不是被
`UForeverAssetFrameworkComponent`持有。

`AForeverFrameworkActor`这次把`PrimaryActorTick.bCanEverTick`从`false`改成了`true`，
新增的`Tick(float DeltaTime)`覆写里唯一做的事就是`player->Tick(DeltaTime)`——这是这个
Actor第一次真正需要每帧更新的逻辑，见`ForeverFrameworkActor.md`"Tick"一节。

## 对外查询

`Core/common/implement.h`的`PostImplement`新增了`"game time"`这个post类型（和已有的
`"random citizen"`同一个模式），mod/脚本可以通过`Post({"post":"game time"})`拿到
`{"result":"success","date":"YYYY-MM-DD","time":"HH:mm"}`（`Init()`还没跑完/`time`为空
时返回`{"result":"fail","msg":"no game time available."}`），照抄老工程
`Core/common/implement.cpp`同一个post类型的字段命名。

## 依赖关系

- 依赖：`Dependence/common/utility.h`（`Time`类，字段/方法和老工程逐一对得上，这次
  没有改动）。
- 被谁依赖：`Core/common/implement.h`（`PostImplement`构造函数+`"game time"`查询）、
  `Source/Forever/Framework/ForeverFrameworkActor.h/.cpp`（持有+每帧`Tick`驱动）。

## 范围边界（这次明确不做的事）

- 不迁移手机(`Phone`)、资产(`Asset`/`GiveObjectChange`等)、存款相关字段/方法——留到
  `PHASE4_PLAN.md`阶段4-7。
- 不重建`Story::GetSetting`/`Config::GetGlobalSettings`那一套全局设置字典——
  `time_flow_ratio`这次写死常量。
- 不把`system.time.year/month/...`写进`Story`的`systemScript`变量池（老工程
  `Story::Tick`里做的事）——这次没有新增`Story::Tick`。
- 不接入`CrossDay()`的任何消费方（老工程`Society::Tick`/`Populace::Tick`/
  `Industry::Tick`里"跨天触发DailyPlan"那一套）——`Society`/`Industry`目前都是空骨架。
- 不处理老工程`AGlobalBase::GlobalPause/GlobalResume`（`SetGlobalTimeDilation`那一套
  暂停机制）——新工程目前没有暂停功能。
