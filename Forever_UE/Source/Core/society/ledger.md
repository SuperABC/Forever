# ledger.h / ledger.cpp

## 职责
全城账本：城市外、政府、每个市民、每个组织各一个账户，金额int64、单位分。显示时才除以100（`FormatMoney`）。

## 关键设计
- **总和恒为0**：资金只能经`Transfer(from, to, amount)`在两个账户间转移，没有凭空加减的接口；
  开局由城市外账户向市民和组织注资，城市外账户因此为负。`Total()`每日校验，必须为0。
- **不检查余额**：账户可以为负。企业资金为负即倒闭，由上层（Society）判断，账本只记账。
- 账户按需创建，没出现过的账户余额为0。
- 存档：`SocietySaveCodec`在社会段里保存全部余额（市民、组织按下标引用）。

## 依赖关系
- 被谁依赖：`Society`持有一个`Ledger`；`Settlement`通过它转账。

## 金融类转账（Flow）
`Flow`除了货款、税、工资等，还有：`DEPOSIT`/`WITHDRAW`（存取款）、`DEPOSIT_INTEREST`、`STOCK_TRADE`（股票成交款）、`BOND_ISSUE`/`BOND_REPAY`/`BOND_INTEREST`（国债）、`PRINT_MONEY`（城市外账户直接打给政府的印钞）、`EMIGRATE`（离城市民带走的正余额付给城市外）；`DEBT_WRITE_OFF`除了企业重启，也用于核销离城市民的欠款。`flowTotals`的长度随`Flow::COUNT`变化。存款、国债本金这类"政府欠谁"的记录不在账本里，在`Treasury`。

## 开局注资

`initialInjection`：开局`InitFunds`给全城(市民+组织)的注资总额，三重负反馈的基准，随社会段存档。`InjectionRatio()`=当前全部注资(城市外账户取反)/开局注资，政府一侧的比例用`Society::GovernmentNetRatio()`（政府净头寸/开局注资，见`treasury.md`、`policy.md`）。
