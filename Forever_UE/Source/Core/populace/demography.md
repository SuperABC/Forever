# demography.h / demography.cpp

## 职责

运行时的人口推进：每天跨天时，在 Core 里直接算完自然死亡、出生、怀孕、配对结婚同居、成年搬出。不发脚本事件；UE 层只按报告挪 Actor（销毁死者、登记新生儿、把搬家的人瞬移到新家）并写日志。开局的人口和亲属关系仍由 `Populace::Init` 的生成期模拟产生（`populace.md`），这里只管开局之后。

## 入口

- `Demography::Daily(populace, map, society, now, controlled, config)`：由 `WorldStep::Step` 在跨天段调用，位置是 `Economy::Daily`（含月初离城）之后、`Immigration::Daily` 之前，这样死亡和搬家腾出的空房当天就能住进迁入的人。返回 `DemographyReport`（死亡、出生、结婚、搬家四张表），`WorldStepHooks::onDemography` 把整份报告交给调用方。
- `controlled` 是玩家所扮演的市民（`WorldStepHooks::protectedCitizen`，开车/坐公交时也算）：他不会自然死亡、不配对、不搬家；他怀孕/生孩子照常。
- 外层是 `CORE_RANDOM_SCOPE("core.populace")`，按 `Populace::GetCitizens()` 下标顺序遍历，同一种子结果逐字节可复现（CoreTest `demography` 套件比对事件日志）。当天新出生的孩子不参与后面的步骤。
- `DemographyConfig` 里是概率和天数（默认值就是游戏用的数，先写死在代码里，不进 Config）；测试把某一项拉到 1、其余置 0 来确定性地覆盖每个分支。

## 每天的顺序

1. **自然死亡**：活着、年满 `deathStartAge`(60) 岁的市民，每天概率 `(年龄-60)×0.00001`（没有上限，可能活过 100 岁）。善后走 `Demography::Kill`：
   - 继承人：在世配偶；没有就在世的子女（按出生顺序）。
   - 遗产：`Emigration::Liquidate` 先把股票按市价卖给组织库存股、取出存款、赎回国债，余额为正就平分给继承人（零头给第一个，`Flow::INHERIT`），没有继承人归政府；为负由城市外账户核销（`DEBT_WRITE_OFF`）。
   - 名下的不动产（`EstateAsset` 和对应 Zone/Building/Room 的 owner）和载具（`VehicleAsset::ownerCitizen`）给第一个继承人；没有继承人不动产清空所有者，载具留着不动（没人会开）。另外按离城的做法扫一遍建筑/房间/园区的 owner 兜底。
   - 释放工作（`Job::SetOccupant(nullptr)`）、清怀孕和待搬出标记、`Citizen::Die()`、`Room::RemoveResident`（独居的人死了房子就空出来）。党员资格靠 `Polit::RemoveGoneMembers` 自动移除。
   - 市民对象不删，留着当死亡记录：存档、`scheduler:<下标>` 定时器、交通/社会/政治存档都按 `Populace::citizens` 下标认人。
2. **出生**：活着、怀孕、预产期不晚于今天的母亲。`Populace::SpawnNewborn` 建孩子（性别各半，姓取父亲的，没有父亲取母亲的，重名重试 8 次，生日=今天，日程按权重随机）。住处：父亲在世且和母亲同住就是那间，否则母亲家（开局遗留的分居夫妻），母亲没有家孩子也没有。父母各自 `AddChild`，和父母、已有的兄弟姐妹互相写亲属关系（`LinkKin`，熟人+随机关系值+`KinshipExperience`）。钱包不注资：账户第一次有进账时才建，开局余额就是 0。母亲记 `lastBirth`=今天、清怀孕状态。
3. **怀孕**：活着、`fertileMinAge`~`fertileMaxAge`(20~45) 岁、配偶在世、没在怀孕、距上次生育满 `birthSpacingDays`(365) 天（或运行时没生过）的女性，每天概率 0.0004（一年约 0.15，一辈子大约 3 个孩子，接近生成期），预产期 = 今天 + 280 天。
4. **配对**：活着、成年(18)、没有在世配偶（丧偶的人算单身）的市民，每天按 `DatingHazardForAge(年龄)/365` 的概率找对象（年概率摊到每天，函数和开局恋爱史共用，见 `kinship.h`）。在单身名单里随机试 10 次：异性、今天还没配过对、不是近亲（`IsCloseRelative`）。配上就互相设为配偶、写配偶亲属关系，然后同居：
   - 两人都有家：随机一方搬进另一方家（同一间就不搬）；只有一方有家：没家的搬过去；都没家：从空房里挑一间两人一起住，没有空房就只结婚。
   - 搬的人和他同住的未成年子女一起搬（`Map::MoveCitizen`）；原来的家没人了就变回空房。
   - 运行时不考虑离异、分居；开局就分居的夫妻不会自动团聚。
5. **成年搬出**：今天满 18 岁、或之前标了 `pendingMoveOut` 的市民，家里还有别的活人（而且他没结婚）就搬到随机一间空房（`Map::PickVacantResidence`，和迁入同一个"没有任何户的住宅"判定）。没有空房就标 `pendingMoveOut`，之后每天重试；已经单独住或结了婚就清掉标记。

## 搬家（`Map::MoveCitizen`）

从原来的家 `Room::RemoveResident`（户空了删掉，房子变回空房），结束行走、清绿化和自由位置，再 `SettleCitizen` 到新家（家、当前房间、户都设好），有工作的重新 `Job::SetOccupant` 刷新工作脚本里的 `home` 地址。Core 侧这就是瞬移；UE 侧 `onDemography` 对每条搬家调 `UForeverPopulaceFrameworkComponent::TeleportCitizen`，有 Actor 的直接挪过去，同时作废在途寻路结果、通勤和跳时间的路段。

## 存档

`pregnancyDue`、`lastBirth`、`pendingMoveOut` 追加在人口段每个市民记录的最后（`populace_save.md`，按项目约定不升段版本）。户的成员变化（移出）直接体现在存档的户成员表里。`Flow::INHERIT` 让账本的流水总表多一类，旧存档读不了（游戏未发布，按约定不做迁移）。

## 依赖关系

- 依赖：`Populace`（`SpawnNewborn`）、`Map`（`MoveCitizen`、`PickVacantResidence`、`SettleCitizen`）、`Society`（账本、`Emigration::Liquidate`）、`kinship.h`、`Room`/`Household`、`EstateAsset`/`VehicleAsset`。
- 被谁依赖：`WorldStep`（UE 的 `AForeverFrameworkActor::StepWorld` 和 CoreTest 共用）。
