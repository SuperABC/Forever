# ForeverPopulaceFrameworkComponent.h / .cpp

## 职责

对应旧Framework Actor `Populace`（C++ Base:`PopulaceBase`）。阶段2-3是空骨架，这次进入
populace域时第一次真正填内容：**按玩家距离流式生成/销毁`ACitizenElement`**——和用户明确
确认过的架构决定（老工程`APopulaceBase`同款做法），不是像`ABuildingElement`那样"开局全
建好、常驻到关卡结束"——人口规模通常比建筑数量大得多，流式管理是更保险的默认选择。

`AForeverFrameworkActor::EnsureMapGenerated()`在`map->Checkin(*populace)`（Core侧人口
分配完成）之后、其它Forever层`Generate*`渲染调用之后调用一次`GenerateCitizens(map,
populace)`——这一步**不`SpawnActor`任何东西**，只是把`populace->GetCitizens()`缓存成
一份`TArray<Citizen*>`，真正的生成/销毁全部在`TickComponent`里按距离做。

## 关键设计

### 流式生成/销毁：轮询分片 + 迟滞区间

老工程`APopulaceBase::Tick`按`step/stride`轮询分片（每帧只处理`1/stride`的citizen），
避免citizen总数很大时每帧全量距离检测——这次照抄同一个节流思路：`streamingBatchStride`
（默认20）决定每帧处理`allCitizens.Num()/streamingBatchStride`个citizen，`pollCursor`
循环回绕推进。

对轮到的每个citizen：
- **还没有`activeInstances`条目**：估算它当前的"逻辑位置"（`Citizen::HasPosition()`为
  true用记录值，否则用房间中心，**不加随机抖动**——抖动是`ACitizenElement::Init()`真正
  生成时才加的一次性效果，估算阶段只是为了判断距离，不应该影响citizen最终落地的位置）
  到玩家`GetPlayerPawn(0)`的距离，小于`citizenSpawnDistance`（默认2.0地图单位，老工程
  同款字面值）就`SpawnActor<ACitizenElement>()`+`Init()`，记入`activeInstances`。
- **已有`activeInstances`条目**：读对应Actor的`GetActorLocation()`到玩家的距离，大于
  `citizenDespawnDistance`（默认4.0地图单位，老工程同款字面值）就先把这个位置转回地图
  单位`SetPosition`回`Citizen`（这样下次重新进入范围直接用这份记录，不再随机抖动一次
  ——满足"反之直接在记录的位置出现"的要求），再`Destroy()`该Actor、从`activeInstances`
  移除。

两个距离阈值不同（`citizenSpawnDistance < citizenDespawnDistance`）形成迟滞区间，和
building的LOD双阈值同一个防抖动理由：避免玩家在临界距离附近小范围来回走动时citizen反复
生成/销毁。

### `FindOrSpawnCitizenByName`/`SpawnCitizen`：按姓名强制生成，不看玩家距离

阶段4 Story域接入`ChangeControlChange`（剧情/T键之外，第三种"切换玩家操控权"的触发源，见
`ForeverStoryFrameworkComponent.md`"ApplyControlChange"一节）之后新增：剧情指定的目标市民
不一定在玩家附近，不能像`TickComponent`那样等距离小于`citizenSpawnDistance`才生成——
`FindOrSpawnCitizenByName(name)`按`Citizen::GetName()`（UTF-8转`FString`比较）在
`allCitizens`里线性查找同名citizen，找到后：已经在`activeInstances`里就直接返回那个
`ACitizenElement*`（弱指针失效则先从map里移除，走强制生成分支）；否则调用`SpawnCitizen`
强制生成一个，不做任何距离判断。找不到同名citizen或生成失败统一返回`nullptr`。

`SpawnCitizen(Citizen*)`是从`TickComponent`原来内联的"估算位置+`SpawnActor`+`Init`+登记
`activeInstances`"这段逻辑里提炼出来的私有辅助函数——`TickComponent`的按距离生成分支和
`FindOrSpawnCitizenByName`的强制生成分支现在共用同一份实现，不再各自维护一份几乎相同的
代码。调用方（这两处）自己负责保证传入的`citizen`不为空、且不在`activeInstances`里，
`SpawnCitizen`内部不重复检查这两个前提。

### `activeInstances`：谁映射谁，生命周期归谁管

`TMap<Citizen*, TObjectPtr<ACitizenElement>> activeInstances`是这个组件自己维护的映射
——`Citizen`（Core）不知道自己有没有对应的Actor，`ACitizenElement`也不需要在别处登记
自己。`Destroy()`一个Actor后必须同步`activeInstances.Remove(citizen)`，否则下一帧还会
认为它"已经有实例"从而跳过重新估算距离/生成的判断。

### `EndPlay`：和`UForeverBuildingFrameworkComponent`同一个模式

`EndPlay`只把`map`置空，**不主动`Destroy()`场上还活着的`ACitizenElement`**——关卡卸载
会自动清理所有Actor，`activeInstances`这个`TMap`本身也会在组件销毁时一起被回收，不需要
手动清空。

## 依赖关系

- 依赖：`Source/Forever/Element/CitizenElement.h`（`SpawnActor<ACitizenElement>()`+
  `Init()`）、`Source/Core/populace/populace.h`/`citizen.h`（`Populace::GetCitizens()`/
  `Citizen`）、`map/map.h`/`map/building.h`/`map/room.h`（估算逻辑位置要用）、
  `Kismet/GameplayStatics.h`（`GetPlayerPawn`）。
- 被谁依赖：`AForeverFrameworkActor::EnsureMapGenerated()`（`GenerateCitizens(map,
  populace)`）、`UForeverStoryFrameworkComponent::ApplyControlChange`
  （`FindOrSpawnCitizenByName`，剧情`ChangeControlChange`指定切换控制的市民不一定在附近，
  需要强制生成，见`ForeverStoryFrameworkComponent.md`）。

## 待办/后续阶段

- `streamingBatchStride`/`citizenSpawnDistance`/`citizenDespawnDistance`是按经验给的
  初始值（后两个直接照抄老工程字面值2.0/4.0），实际人口规模/地图尺寸确定后可能需要
  按PIE验证结果调整，和这次会话早前building LOD阈值的调优是同一类后续工作。
- 目前每次判定只处理"生成/销毁"，不处理"citizen在两次判定之间发生了变化"（比如未来加入
  真正的AI移动之后，一个已经生成的citizen走出很远——这次的`GetActorLocation()`读回判定
  已经能正确处理这种情况，不需要额外改动）。
