# investor.h / investor.cpp

## 职责
市民的理财策略：把富余现金存进政府、买股票或认购国债，缺钱时取款、卖股。纯函数式，不持有状态。

## 关键设计
- **风险偏好**`Risk`∈[0,1]：`(自信 + (1−理性))/2`，由性格推出。保守的多存款，激进的多买股。
- **每日决策**（`Daily`）：成年、在场（`IsActive`）、在经济里的市民每隔`policy.investIntervalDays`天决策一次，按市民下标错开，避免每天遍历全员动手。
  - 现金低于缓冲`policy.reserve`：先取存款；还不够就按股息率从低到高挂卖单。
  - 现金有富余：富余×(1−风险)×0.5存款，富余×风险×0.5做股票预算；从随机挑的几支股票里按股息率（加一点噪声）选前3支挂买单；股息率过低的持仓挂卖一部分。
  - 存取款当场执行，股票只挂单，由`Market::Clear`撮合。
- **国债认购**（`SubscribeBonds`）：政府账户为负时由`Economy::FundGovernment`调用，从随机位置起让有富余现金的市民依次认购，每人最多拿富余×(1−风险)×0.5，直到凑够缺口或没人愿意买。
- 参数都在`GovernmentPolicy`（`reserve`、`investIntervalDays`、利率等），不存档。

## 依赖关系
- 依赖：`Society`（账本、Treasury、Market、政策）、`Organization`、`Citizen`（性格）。
- 被谁依赖：`Economy`。
