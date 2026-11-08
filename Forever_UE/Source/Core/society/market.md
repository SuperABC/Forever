# market.h / market.cpp

## 职责
股市：每个组织一支股票，总股数是`Organization::GetTotalShares()`（开局10000，缺钱增发会变多）。市民持股在`Organization::shareholders`里（股东+股数），没被市民持有的是组织自己的**库存股**。`Society`持有一个`Market`。

## 关键设计
- **价格**（`Price`）：`max(kMinPrice, 组织账户余额/总股数) × priceFactor`；倒闭的组织返回0（不能交易）。`priceFactor`在[0.5, 2]之间，随买卖压力每天最多动2%，并每天向1回落1%。
- **挂单**：`Bid/Ask`只登记当天的买单卖单，不立即成交。买单按买家现金能买得起的股数封顶，卖单按实际持股封顶。
- **撮合**（`Clear`，每天一次，在`Investor::Daily`之后）：每支股票用同一个成交价；成交量=min(总买量, 市民卖单+库存股)，买方按比例缩量，卖方先用市民卖单、不够再用库存股；每笔`Transfer(买方→卖方, STOCK_TRADE)`，卖方是库存股时付给组织账户，所以账本总和不变。价格系数看买单与全部供给（市民卖单+库存股）之差占总盘子的比例，每天最多动2%，没有上下限；买单没人接就涨，供给卖不掉就跌。
- **分红**：`Economy::Monthly`按持股数×每股可分配利润发红，库存股那份留在组织账户，并用`RecordDividend`记下每股分红，`AnnualYield`（分红×12/股价）给投资策略选股用。
- 离城清算用`SellAllToTreasury`：持股按当前价卖给组织库存股，组织付钱。
- 破产重启后`ResetQuote`复位行情。
- 状态：每支股票的`priceFactor`和`dividendPerShare`随社会段存档；挂单是当天的临时数据，不存档。

## 依赖关系
- 依赖：`Ledger`、`Organization`（持股、库存股）、`Citizen`。
- 被谁依赖：`Society`、`Economy`、`Investor`、`Emigration`。
