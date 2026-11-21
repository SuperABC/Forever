# party_republican.h / party_republican.cpp

## 职责

`RepublicanParty`(`party_republican`)：共和党，主张提高税收和福利、收紧移民，穷人更倾向它。

## 关键设计

- 提议是**固定目标值**，每次选举前`Propose`都重填同一组数(不看当前值)；Core把它换算成相对当前值的比例并夹到[0.9, 1.1]，所以政策每月最多朝目标走10%，到了目标就不再动。目标值是占位初值，写成文件顶部`REPUBLICAN_*`宏，等用户调：

| 政策 | 目标值 |
|---|---|
| `CorporateTax` | 50 |
| `IncomeTax` | 30 |
| `ConsumptionTax` | 15 |
| `RentTax` | 30 |
| `ImportTariff` | 40 |
| `RetirementPension` | 8000 |
| `UnemploymentBenefit` | 8000 |
| `MinorAllowance` | 3000 |
| `ImmigrationRatio` | 1.2 |

- `GetCongress`忽略月份参数，恒返回每月1号12:00-15:00(调试用，便于PIE验证第一天的党内选举)。
- `ChooseMeetingRoom`发post `random room`(type=`room_meeting`)，地址写进`meetingAddress`；和别的党重叠由Core重调(最多4次)。
- `AssignPowers`：每个关联政策的权力随机挑1名党员(见`party_common.md`)；没有关联政策的权力不指派。党首目前不参与指派。
- `SetProperty`里`memberCount = REPUBLICAN_MEMBERS`(12)：新开局时Core从成年市民里随机抽这么多人入党。

## 依赖关系

- 依赖：`Dependence/polit/party_mod.h`、`policy_ids.h`、`party_common.h`。
