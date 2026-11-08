# settlement.h / settlement.cpp

## 职责
把钱和Industry的货物流水对上。Industry只记录"谁给谁多少货"（`GoodsFlow`），付款收款全部在这里：
`InitFunds`新开局注资，`SettleGoods`按流水结算货款。

## 关键设计
- **注资**：城市外账户给每个市民`N(5000元, 2000元)`（截断到不小于0），给每个组织其mod的`initialFunds`。
- **货款**（每笔流水 = 数量 × 单价）：
  - 城内流转：买方付给卖方，单价=批发价（初始=进出口价平均值）；
  - 进口：买方付给城市外，单价=进口价；进口价**已含关税**：`tariffPercent`（25%）是对不含税价的税率，对应含税进口价的25/125=20%。买方总共只付进口价，其中20%当场付给政府，其余付给城市外；
  - 出口：城市外付给卖方，单价=出口价，**没有关税**。
- **买卖双方是谁**：流水里的Storage经`Industry::GetStorageRoom`得到房间，再由`Society::OwnerOf(room)`
  得到占用该房间的组织；没有组织占用的房间归政府。工坊cache所在房间就是工坊房间。
- 货款不检查买方余额：余额为负就是企业倒闭，由Society判断。
- 结算只动账本，总和不变；每日调用后应校验`Ledger::Total()==0`。

## 房租（Step 4）
`PayRents`日结：每个`Rent()>0`的房间，使用者付给房东——房东是房间owner（私有）或政府（公有/无主）。
住宅由里面的户付：多户合住时房租按户数均分（余数分依次多给前几户，总额不变），每户再由成年成员均摊；
没人住的住宅不收租。工坊、商店等由占用该房间的组织付。房东（非政府）当场按`rentTaxPercent`（20%）把房租收入的一部分交给政府（税收一律即时缴纳）。

## 依赖关系
- 依赖：`Society`（账本、政策、`OwnerOf`）、`Industry`（流水、价格目录）。
- 被谁依赖：每日经济驱动（EconSim、`AForeverFrameworkActor`）。

## `InitFunds`
`InitFunds(society, citizen, amountCents=-1, resetBaseline=true)`：给一个市民建钱包并注入初始资金(`Outside→citizen, INJECTION`，默认计入`initialInjection`基准；迁入人口传`resetBaseline=false`，基准保持开局值)，缺省金额取N(5000元, 2000元)。spawn_npc出来的每个市民都调用，不管是否接入经济系统，这样之后被玩家操控时直接有钱可用。
