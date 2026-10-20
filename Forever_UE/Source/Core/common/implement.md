# implement.h / implement.cpp

## 职责

`PostHandle`（`Dependence/common/handle.h`，定义在Dependence层供所有mod使用的"向Core发起
查询"抽象接口，见`Dependence/common/handle.md`）的具体实现`PostImplement`：聚合Core全部7个
domain（Map/Populace/Society/Story/Industry/Traffic/Player）的裸指针，供Mod通过`Post()`
反向查询Core状态。

典型用法：`ScriptMod::WrapScript`（`Dependence/story/script_mod.h`）遇到剧情脚本里的
`PlaceHolderChange`占位符时，向Core要一个"随机挑一个citizen"的姓名，构造出
`ChangeControlChange`去替换占位符，见`Dependence/story/script_mod.md`"典型用法"一节、
`Forever_Mod/Empty/Cpp/Empty/empty_mods.h`的`EmptyScript::WrapScript`。

## 关键设计

### 不做成全局单例

`Map`/`Populace`/`Story`本身都不是单例（见对应`.md`），`PostImplement`延续同一个原则：
不持有任何全局状态，是一个按需现场构造的普通对象，构造函数直接把7个指针传进来存好，谁来
构造它、指针从哪里来它完全不关心。目前是`UForeverStoryFrameworkComponent::
BroadcastGameStart`/`AForeverFrameworkActor::Tick`在UE层从`AForeverFrameworkActor`身上
取到7个指针后现场构造一个，只活这一次调用的生命周期，见`Forever/Framework/
ForeverStoryFrameworkComponent.md`"PostImplement"一节。

### 目前实现七种post类型，其余domain指针先占位

构造函数接收全部7个指针并存成成员，`Post()`识别以下请求：

- `request["post"] == "random citizen"`：只从`populace->GetCitizens()`里"已经有家"
  （`GetRoom()`非空）的citizen中`GetRandom(size)`随机挑一个，成功返回
  `{"result":"success","name":"<姓名>"}`；`populace`为空返回
  `{"result":"fail","msg":"no citizen available."}`，筛选后列表为空返回
  `{"result":"fail","msg":"no housed citizen available."}`。**必须过滤掉没有Room的
  citizen**——地图生成的住宅room数量如果覆盖不了所有成年citizen，会有citizen始终分不到
  `Room`/`CurrentRoom`；这类citizen如果被挑中交给玩家操控，`ComputeLogicalPosition`
  算不出世界坐标，`SpawnCitizen`会失败，玩家只能停留在开局的飞行相机上且没有任何报错——
  只挑有家的citizen是为了避免这个失败模式。
- `request["post"] == "game time"`：读`player->GetTime()`（`Player`全局时钟，见
  `Core/player/player.md`），成功返回`{"result":"success","date":"YYYY-MM-DD",
  "time":"HH:mm"}`，`player`为空或时钟还没`Init()`时返回
  `{"result":"fail","msg":"no game time available."}`。
- `request["post"] == "citizen home address"` / `"citizen workplace address"`
  （供`JobMod::DailyPlan`/`ExecNode`用，见`Core/society/job.md`"按需查地址：
  PostHandle参数"一节）：`request["name"]`按`Citizen::GetName()`在
  `populace->GetCitizens()`里线性查到`Citizen*`，前者取
  `citizen->GetRoom()->GetAddress()`（家），后者取
  `citizen->GetJob()->GetPosition()->GetAddress()`（工位room的地址），成功返回
  `{"result":"success","address":"<地址字符串>"}`；`populace`为空、查不到同名citizen、
  citizen没有家/没有job（未成年、没分到住处、还没被`Society::RecruitCitizens`招聘）都
  返回`{"result":"fail","msg":"..."}`。这两个post类型存在的原因：
  `NPCNavigateChange::destination`不能写`"home"`/`"workplace"`这种描述性文本（会在
  Dependence层编码Core概念，违反分层原则），必须是能被`Map::LocateRoom(address)`直接
  解析回`Room*`的具体地址字符串——`JobMod`所在的Dependence层看不到`Citizen`/`Room`
  这些Core类型，只能通过这两个post类型向Core要地址字符串。

- `request["post"] == "list zones"` / `"list buildings"` / `"list rooms"`（供mod在C++里
  按type找到地图上具体的实例，比如全图只有一栋的`supergp`建筑）：`request["type"]`是要找的
  mod type（`Zone`/`Building`/`Room::GetType()`），遍历`map->GetZones()`/`GetBuildings()`
  （rooms则遍历每个building的`GetRooms()`），把type匹配的实例的`GetAddress()`收进数组，成功
  返回`{"result":"success","addresses":["<地址>",...]}`，没有匹配也是success+空数组；`map`
  为空返回`{"result":"fail","msg":"no map available."}`。返回顺序不保证稳定（底层是无序容器）。
  地址可以直接给`Map::LocateRoom`/`LocateBuilding`、`npc_navigate`的`destination`用。

请求不认识（`post`字段不是上面几种，或请求本身不是JSON对象）统一返回
`{"result":"fail","msg":"post not found."}`。`industry`/`traffic`（`map`同理）目前没有
分支用到，`society`也没有直接用到（"citizen workplace address"走的是
`Citizen::GetJob()`，不经过`Society`）——等对应域迁移出需要被查询的数据时在
`if/else if`链上加新分支即可，不需要改构造函数签名。

### `GetResult()`返回成员的引用，不按值返回

```cpp
virtual const JsonValue& GetResult() const override { return result; }
```

`result`是`PostImplement`自己的成员（每次`Post()`调用开头`result = JsonValue(DATA_OBJECT)`
重置），`GetResult()`返回它的引用——这是`PostHandle`接口本身的约定（见`handle.md`），结果
对象的生命周期留在`PostImplement`这一侧管理，调用方（可能是mod DLL，比如
`EmptyScript::WrapScript`）只读引用，不持有、不释放，避免跨模块传递返回值对象触发"由哪个
模块的分配器释放"的问题，和`[[memory:cross_dll_allocator_crash]]`是同一类考量。

## 依赖关系

- 依赖：`common/handle.h`（`PostHandle`基类）、`common/json.h`（`JsonValue`）、
  `populace/populace.h`/`populace/citizen.h`（`Populace::GetCitizens()`/`Citizen`）、
  `player/player.h`（`Player::GetTime()`）、`society/job.h`（`Citizen::GetJob()->
  GetPosition()`）、`map/room.h`（`Room::GetAddress()`）、`map/map.h`/`map/zone.h`/`map/building.h`（`list *`三种查询）、`common/utility.h`
  （`GetRandom`、`Time::Format`）、`Map`/`Society`/`Story`/`Industry`/`Traffic`（仅
  前置声明或暂未使用，构造函数存指针）。
- 被谁依赖：`Forever/Framework/ForeverStoryFrameworkComponent.cpp`
  （`BroadcastGameStart()`现场构造`PostImplement`，传给`Story::BroadcastGameStart`的
  `post`参数）、`Forever/Framework/ForeverFrameworkActor.cpp`（`Tick()`现场构造
  `PostImplement`，传给`populace->Tick(...)`/`society->Tick(...)`的`post`参数）、
  `Dependence/story/script_mod.h`/`Dependence/society/job_mod.h`/
  `Forever_Mod/Empty/Cpp/Empty/empty_mods.h`（间接：`ScriptMod::WrapScript`/
  `JobMod::DailyPlan`/`ExecNode`拿到的`PostHandle*`在运行时实际指向一个
  `PostImplement`，但mod侧代码只认`PostHandle`接口，不直接引用`PostImplement`类型）。

## 待办/后续阶段

- `industry`/`traffic`两个domain指针目前只存不用，等对应域迁移出需要查询的数据时再加
  新的`else if`分支。
- 请求/响应的JSON字段命名（`post`/`result`/`name`/`date`/`time`/`address`/`msg`）目前
  没有形成统一约定或校验，查询类型多起来后可能需要梳理一份规范。
