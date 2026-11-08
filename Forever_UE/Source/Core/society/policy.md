# policy.h / policy.cpp

## 职责
政府财政参数：三种补助的日额（养老金50元、失业补助50元、未成年补助20元）和各税率（企业税25%、
所得税20%、消费税10%、房租税20%、关税25%）。`TaxOf`统一做向下取整到分的税额计算。

## 关键设计
- 是模拟参数，不是状态，不进存档。年龄分界：未满18岁的未成年补助发给所在户的成年人均摊(户里没有成年人时发给本人)，18岁至60岁无工作领失业补助，满60岁领养老金。
- 关税只对进口征收，出口无关税。税收一律在交易发生时即时缴纳，见`economy.md`。
- 税和补助的实际收支由`Economy`/`Settlement`使用，见`economy.md`/`settlement.md`。
- 金融参数：`depositRateBp`（存款年利率，基点，默认300）、`bondPremiumBp`（国债利率比存款高的部分，默认300）、`bondTermDays`（国债期限，默认360天）、`reserve`（市民自留现金缓冲，默认2000元）、`investIntervalDays`（每个市民几天决策一次，默认7），用法见`treasury.md`/`investor.md`。

## 依赖关系
- 被谁依赖：`Society`持有一份`GovernmentPolicy`，`Settlement`读取。

## 三重负反馈

以开局外部注资`I`(`Ledger::GetInitialInjection`，`Settlement::InitFunds`注资完记下，随社会段存档)为基准，`Economy::SyncIndustry`每天重算：

- **出口阈值**：`x`=当前全部注资(城市外账户余额取反)/`I`，阈值`1-1/(x+1)`(x=1时50%)，写进`Industry::SetExportKeepRatio`——城里钱越多，工坊产出仓留得越多、出口越少。
- **税率系数**`taxFactor`：`g`=政府净头寸/`I`(带符号，净头寸=政府账户−存款−国债本金，借款不抬高它，印钞会)，`1-atan(g)*2/pi`，范围(0,2)：企业税、所得税、消费税、房租税的实际税率=基础税率×系数(`Tax()`)；关税不参与。
- **补助系数**`allowanceFactor`：`exp(g)`，养老金、失业补助、未成年补助的实际额=基础额×系数(`Allowance()`)。

两个系数是派生数据，不存档。
