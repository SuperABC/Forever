# emigration.h / emigration.cpp

## 职责
离开城市：一户里所有在场（`IsActive`）的成年人净资产都为负，整户搬走。

## 关键设计
- **净资产**（`NetWorth`）= 账户余额 + 存款 + 国债本金 + 持股按当前股价的市值。目前市民没有别的资产。
- **每月检查**（`Monthly`，由`Economy::Monthly`调用，不引入新的存档状态）：遍历有户的住宅房间，户里至少有一个在场成年人且全部净资产<0就整户离开。玩家当前控制的市民所在的户跳过（`protectedCitizen`）。
- **离开流程**（`Depart`）：
  1. 清算：持股按市价卖给组织库存股、取回存款、提前赎回国债；
  2. 账户归零：负余额由城市外账户核销（`DEBT_WRITE_OFF`），正余额付给城市外（`EMIGRATE`）；
  3. 放弃名下房间的房主身份；
  4. `Citizen::Depart()`：置`departed`标记、断开岗位、结束行进、离开当前房间并清位置。
- 市民对象保留（和`isDead`一样用标记），存档下标不变；`departed`随人口段存档。户、`Room::households`里的成员关系也保留，但`Household::GetAdults`会过滤不在场的人，租金、饭钱、补助等经济结算都只看`IsActive`。
- UE层：`Economy::Daily`把当天离城的市民放进`DailyReport::departed`，`AForeverFrameworkActor`据此调用`UForeverPopulaceFrameworkComponent::HandleCitizenDeath`销毁Actor，之后流式生成都因`!IsActive()`跳过他们。

## 待办
- 离城后空出的房间/岗位目前不会有新人迁入。

## 依赖关系
- 依赖：`Society`（账本、Treasury、Market）、`Organization`、`Room`、`Household`、`Citizen`。
- 被谁依赖：`Economy`。
