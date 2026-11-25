# policy.h / policy.cpp

## 职责
政府财政参数`GovernmentPolicy`(补助日额、各税率、移民目标比例、年龄分界、金融参数)和可调政策目录`PolicyCatalog`。
政策归政治域所有；`Society`通过`BindPolicy`引用`Polit`持有的那一份，
所有`society.GetPolicy()`调用点不用改。

## 关键设计
- **政策值是状态**：由`Polit`持有，党派选举后调整基础值，随`world.polit`段存档(见`polit_save.md`)。没有启用任何党派时一直是默认值，
  经济结果和没有政治域时逐字节一致(迁移时用同一种子的`econ`报告对比证实过)。
- 默认值：养老金50元/天、失业补助50元/天、未成年补助20元/天；企业税25%、所得税20%、消费税10%、房租税20%、关税25%；
  移民目标比例`immigrationTargetRatio`=1.5。
- 税率是`double`(党派每次调整±10%会产生小数)：`TaxOf(amount, percent)`、`Tariff(cost)`向下取整到分，
  默认整数税率下结果与旧整数算法完全一致。
- 年龄分界：未满18岁领未成年补助(发给所在户的成年人均摊，户里没有成年人时发给本人)，18~60岁无工作领失业补助，满60岁领养老金。
  年龄分界与金融参数(存款利率、国债、自留现金、决策间隔，见`treasury.md`/`investor.md`)目前**不可调**。
- 关税只对进口征收，出口无关税。税收一律在交易发生时即时缴纳，见`economy.md`。

## 可调政策目录`PolicyCatalog`
9项政策(id见`Dependence/polit/policy_ids.h`，统一两个单词)：`corporate_tax`/`income_tax`/`consumption_tax`/`rent_tax`/`import_tariff`、
`retirement_pension`/`unemployment_benefit`/`minor_allowance`、`immigration_ratio`。每项`PolicyDef{id,name,kind,get,set,defaultValue,minValue,maxValue}`：
- 下限=默认值×0.25，上限=默认值×4，税率另外不超过100%；
- `PolicyCatalog::Apply`把值夹到[min,max]后写入(补助取整到分)，返回实际写入的值；
- 以后加政策：在目录里多登记一项并在`policy_ids.h`加id即可，党派mod用同一个id提议。

## 三重负反馈

以开局外部注资`I`(`Ledger::GetInitialInjection`，`Settlement::InitFunds`注资完记下，随社会段存档)为基准，`Economy::SyncIndustry`每天重算：

- **出口阈值**：`x`=当前全部注资(城市外账户余额取反)/`I`，阈值`1-1/(x+1)`(x=1时50%)，写进`Industry::SetExportKeepRatio`——城里钱越多，工坊产出仓留得越多、出口越少。
- **税率系数**`taxFactor`：`g`=政府净头寸/`I`(带符号，净头寸=政府账户−存款−国债本金，借款不抬高它，印钞会)，`1-atan(g)*2/pi`，范围(0,2)：企业税、所得税、消费税、房租税的实际税率=基础税率×系数(`Tax()`)；关税不参与。
- **补助系数**`allowanceFactor`：`exp(g)`，养老金、失业补助、未成年补助的实际额=基础额×系数(`Allowance()`)。

两个系数是派生数据，不存档。党派选举调整的是"基础税率/基础补助"，和这两个系数叠加(两套负反馈：财政盈余反馈 + 连任疲劳，见`polit.md`)。

## 依赖关系
- 被谁依赖：`Polit`持有；`Society::BindPolicy`绑定后`Settlement`/`Economy`/`Immigration`读取。
