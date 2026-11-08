# treasury.h / treasury.cpp

## 职责
政府的负债簿：市民存款和国债。钱本身已经转进`Ledger`的政府账户，这里只记政府欠谁多少。`Society`持有一个`Treasury`。

## 关键设计
- **政府净头寸 = 政府账户 − 存款总额 − 国债本金**（`NetPosition`）。存款和国债同时抬高账户与负债，借钱不会改善净头寸，所以税率/补助的负反馈（`Economy::SyncIndustry`里用`Society::GovernmentNetRatio`）不会因为借钱而松掉；印钞（`PRINT_MONEY`）没有负债，直接抬高净头寸。
- 存款：`Deposit`市民→政府（`DEPOSIT`），`Withdraw`政府→市民（`WITHDRAW`），可以把政府账户取成负数（账户本来就允许为负）。
- 国债：`Subscribe`记一张`Bond{持有人, 本金, 年利率基点, 到期天序号}`；`RepayMatured`每天把到期本金还给持有人（`BOND_REPAY`），`RedeemBonds`是离城时的提前赎回。到期还款可能把政府账户打成负数，由`Economy::FundGovernment`当天重新发债/印钞补上。
- 天序号`day`：每次`Economy::Daily`加1（`AdvanceDay`），国债到期按它算。
- **结息**（`PayInterest`，每月一次，由`Economy::Monthly`调用）：存款按`policy.depositRateBp`、债券按各自利率，年利率/12，政府付给市民，市民当场按所得税率缴税，和分红的处理一致。
- 状态：天序号、存款表、国债表随社会段存档（`society_save.md`）。利率等参数在`GovernmentPolicy`里，不存档。

## 依赖关系
- 依赖：`Ledger`、`GovernmentPolicy`、`Citizen`（只用指针）。
- 被谁依赖：`Society`、`Economy`、`Investor`、`Emigration`。
