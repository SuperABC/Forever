# economy.h / economy.cpp

## 职责
每日经济驱动链：`Economy::Daily`把Industry当天的货物流水、市民的服务需求、组织的服务供给、工资房租补助和月结税收串成一条线。

## 税收一律即时缴纳
每笔交易发生时当场缴税，没有"当月累计、月末统一缴"的环节：
- 关税：进口价已含关税，25%的税率对应进口价的20%。买方付进口价，其中20%当场付给政府，其余付给城市外（出口无关税）；
- 消费税：商店每笔销售当场把销售额的10%交给政府；
- 房租税：房东（非政府）收到房租时当场把20%交给政府；
- 所得税：发工资时当场代扣20%，股东拿到分红、市民收到存款/国债利息时当场缴20%；
- 企业税：不对应单笔交易，按"利润实现时缴"处理——月度分红时对可分配利润先缴25%，再分红。

## 倒闭与重启（`UpdateBankruptcies`，每天最先做）
- **倒闭**：企业账户为负就倒闭（`Organization::IsBankrupt`，随社会段存档）。倒闭企业：员工**全部裁掉**（`Organization::LayOffAll`，所有岗位关闭，员工变失业者领失业补助、可去别家求职）、不发工资、不提供服务、不付房租、不参与月度分红和`MonthlySettle`；重启时`ReopenInitial`让mod重新设计满编岗位计划，空缺由每天的`Society::MatchUnemployed`补上；占用的房间通过`Economy::SyncIndustry`同步给Industry，仓库不再参与物流、工坊不再开工。`SyncIndustry`同时把各工位房间的到岗率(开放岗位上的在岗人数/房间容量，扩招超编时大于1)同步给Industry，工坊产能比例=50%×(1+到岗率)。
- **重启**：之后每天查询，对mod声明的**每个股东位**找一位存款超过"初始资金×占股比例"的成年市民（同一人只担一个股东位）；每个位置都找到才重启：旧负债核销（由城市外账户承担），选中的市民按占股比例集资凑齐初始资金，新的股东按`Organization::SplitShares`换算成股数、替换旧的（旧股东的股份作废、库存股清零），股市行情复位，重新营业。只要有一个位置找不到就继续倒闭。
- 因为要求"存款超过初始资金×占股比例"（商店/工厂是2万元/位），而普通市民只有五千元左右，重启在经济初期很难发生，要等有人靠房租和分红富起来。

## 每日顺序（`Daily`）
0. **倒闭与重启**（见上），随后同步停业房间。
0. **跨月先做月度分红**：当前月份（年*12+月）与`Society::lastMonthKey`（存档状态）不同就先执行`Monthly`，再开始今天。
1. **货款日结**（`Settlement::SettleGoods`）：按Industry当天的货物流水付款（含进口关税）。Industry必须已经Tick过。
2. **服务**（`RunServices`）：
   1. 把各组织（有script的）仓库里的货物写进`script`的`self.stock.<货物类型>`（整数库存）；
   2. 收集全城组织的服务：组织mod的`CollectServices`在自己的`services`里填(服务名,类型,单价,质量,capacity)；
   3. 市民按户反馈需求：本版成年人每天1份`food`、未成年(<18)吃一半，整户合计向上取整到整份；
   4. 按服务类型把需求**加权平均**分给提供者——权重是各自`capacity`（当日能提供的份数）。供给不足时每家卖光，充足时按份数比例分，余数按小数部分补；
   5. 各户依次从配额里买：成年成员均摊付款。**市民账户允许为负**，不设"买不起"限制，每天的饭只受库存限制，没货的部分算"没买到"（`unmet`）；
   6. 组织mod的`ApplyServiceUsage`只改script变量（扣`self.stock.*`），系统按变量值回写Storage库存，mod不碰Core对象；商店当场缴消费税。
3. **房租日结**（`Settlement::PayRents`，含房租税）。
4. **工资日结**：每个有人上岗的岗位，组织付`mod->wage`给员工，当场代扣所得税。
5. **补助日结**：政府付给未成年人（<18）20元（发给所在户的成年人均摊）、18岁到60岁无工作者失业补助50元、满60岁养老金50元，补助不征税。
6. **金融**（见下"金融与政府融资"）：天序号+1，到期国债还本，`Investor::Daily`市民存取款/挂股票单，`Market::Clear`撮合，最后`FundGovernment`补政府缺口。

## 月度结算（`Monthly`）
0. **月度结算与岗位调整**（每个非倒闭企业，在企业税/分红之前）：当月利润=月结时余额−`lastSettledBalance`（上次月结完成后的余额，开局用初始资金）；填`OrganizationMonthlyState`调`mod->MonthlySettle`，mod就地改岗位计划`vacancies`/分红率/增发股数；`vacancies`非空的mod由`Organization::ReconcileJobs`对账——计划多了就重开已关闭的Job、不够再新建，少了就先关空缺岗位、再随机裁在岗的（被裁者`SetJob(nullptr)`变失业）。Job只增不删（存档下标和脚本键`job:i:k`依赖顺序），关闭用`closed`标记。企业税和分红的基数改为`min(当月利润, 余额−初始资金)`，留存利润不会下月重复征税/分红。
1. **增发**（`IssueShares`，分红之后、结息之前）：只处理`mod->issueShares>0`的企业（缺钱触发的规则在mod里，Basic是账户为正但低于初始资金30%时增发总股数的20%），总股本最多到初始发行量的4倍。新股先进库存股，老股东按持股比例按当前股价优先配售（受现金扣除备用金限制），付款进企业账户（`STOCK_TRADE`），认不完的留在库存股等市场买。
1. **分红**：企业账户超过`initialFunds`的部分是可分配利润：先缴企业税，税后按**持股数**分给股东（每股可分配额=可分配利润/总股数，库存股那份留在企业，整除的零头也留在企业），股东当场缴所得税。少于初始资金不分红。每股分红记进`Market`供选股用。
2. **结息**：`Treasury::PayInterest`，政府付存款和国债利息，市民当场缴所得税。
3. **离城检查**：`Emigration::Monthly`，一户里所有在场成年人净资产都为负就整户离开，见`emigration.md`。离城市民放进当天`DailyReport::departed`，`MonthlyReport`记户数和人数。

## 金融与政府融资
- **股市**：每个组织固定1万股，股价=净资产/总股数×浮动系数，每天按同一成交价撮合买卖单，见`market.md`。市民的存款、买卖股票、认购国债按风险偏好决定，见`investor.md`。
- **存款**：政府当银行，市民存款转进政府账户并记负债，政府按年利率（`depositRateBp`，默认3%）每月付息，见`treasury.md`。
- **政府净头寸** = 政府账户 − 存款 − 国债本金。税率/补助的负反馈用净头寸（`Society::GovernmentNetRatio`），所以借来的钱（国债、存款）抬高账户的同时抬高负债，不改善指标；印钞没有负债，直接改善。
- **补缺口**（`FundGovernment`，每日最后）：政府账户为负时，先向市民发国债，利率=存款利率+`bondPremiumBp`（默认再加3%），期限`bondTermDays`；国债补不满的剩余缺口由城市外账户直接印钞打给政府（`Flow::PRINT_MONEY`，城市外账户更负、政府账户变正，总和仍为0）。到期国债本金也从政府账户还，不够时当天重新发债/印钞。
- 离线压力测试：`EconSim`设环境变量`ECON_DRAIN`（每天从政府账户划走的分数）可把政府打成赤字，观察发债和印钞。

## 关键设计
- 全部金额int64分，资金只经`Ledger::Transfer`流动，总和恒为0。
- "需求类型"目前写死`food`（成年人1份、未成年半份），没有按积蓄选质量的逻辑。
- 员工不会因为企业亏损离职。
- 每日顺序里 `UpdateBankruptcies` 与 `SyncIndustry` 在货款结算之前，Industry 必须已经 `Tick`。

## 依赖关系
- 依赖：`Society`（账本、月份状态、政策、Treasury、Market）、`Industry`（库存）、`Settlement`、`Investor`、`Emigration`、组织mod接口。
- 被谁依赖：EconSim、`AForeverFrameworkActor`的每日结算。

## 玩家参与经济（`PayWage` / `PlayerPurchase`）
玩家的钱就是当前被操控市民在Ledger里的账户(`Party::Of(citizen)`)，不另设玩家钱包。
- `PayWage(society, org, worker, amount)`：由岗位所属组织给市民发一笔工资(`Flow::WAGE`)，代扣所得税，同`PayWages`的口径。玩家操控的市民去不去上班不影响每日工资结算，所以玩家手动打工是**额外一份**工资。
- `PlayerPurchase(society, industry, player, seller, buyer, assetType)`：买一件物品。按`asset->GetProductType()`在Industry里查零售价；先判余额，再创建物品并`StoreAsset`，放不下(无背包/已满)则销毁物品**不扣钱**；成功才付款(buyer→seller `SERVICE_PURCHASE`)，并由seller缴消费税。返回`PurchaseResult`{Success, NoMoney, NoSpace, Invalid}。
