# party_factory.h / party_factory.cpp

## 职责

`PartyFactory`是`PartyMod`(党派)的注册表/工厂：创建、销毁、枚举已启用的党派类型。

## 关键设计

- 和`GreenFactory`/`ZoneFactory`同一套最小注册表模式(全部公开方法`virtual`、`creator`/`deleter`裸函数指针、`DestroyParty`走注册时的deleter、`SetModArgs`/`GetLiveInstances`)，理由见`zone_factory.md`。
- **没有权重函数**：和Building/Zone不同，党派不是"每个位置按权重选一个"，而是**每个启用的id各创建一个实例**(`Polit::Init`遍历`GetRegisteredIds()`)。`GetRegisteredIds()`返回**按字典序排序**的id，党派创建顺序因此确定，成员随机抽样才可复现。
- `CreateParty`之后工厂立刻调一次`SetProperty()`(填`memberCount`等)，再由`RandomRegistry::BindInstance`绑定每实例随机流；`DestroyParty`解绑。
- 配置键：`config.json`的`"party_mods"`(概念key`Parties`，`Config::ConceptKeyToJsonKey`对`ies`结尾特判成`y`)；id必须是`party_<name>`(`CheckModIdentity("party", ...)`)。

## 依赖关系

- 依赖：`polit/party_mod.h`、`common/random.h`。
- 被谁依赖：`Core/common/registry.h`持有唯一实例，`Core/polit/polit.h`。

## 待办/后续阶段

- 无。
