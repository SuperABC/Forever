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

## 开局注资

`initialInjection`：开局`InitFunds`给全城(市民+组织)的注资总额，三重负反馈的基准，随社会段存档。`InjectionRatio()`=当前全部注资(城市外账户取反)/开局注资，`GovernmentRatio()`=政府余额/开局注资(带符号)，见`policy.md`。
