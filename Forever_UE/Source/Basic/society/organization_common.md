# organization_common.h / organization_common.cpp

## 职责

ShopOrganization/FactoryOrganization共用的岗位计划调整和股本增发规则：`DistressIssue`(缺钱增发)、`PlannedCount`/`AdjustPlan`(按房间容量调整岗位计划)、`Distress`(账户吃紧判定)、`HeadcountDelta`(按月度状态算扩招/裁员人数)；阈值常量(`kDistressBalancePercent`等)也在这里。

## 依赖关系

- 被谁依赖：`organization_shop`、`organization_manufacture`。
