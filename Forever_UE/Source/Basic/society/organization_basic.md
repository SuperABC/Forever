# organization_basic.h / organization_basic.cpp

## 职责

`ShopOrganization`(`organization_shop`)：商店组织，用来测试"一个组织随机占1~2个`component_shop`"。

## 关键设计

- `GetRequirements()`(static，随注册交给工厂，不需要实例)要求1~2个`component_shop`类型的`Component`；`DesignJobsForRoom`给每个`room_shop`类型的workspace房间恰好配一个店员(`job_shop_saler`)，不管工位数具体多少，`workspaceCapacity==0`就不配。
- `static float GetPower()`返回1.f，作为`PowerFunc`随`RegisterOrganization`注册，供`Society::Init`做加权判断。
- `SetProperty`/构造设`scriptModName`+`milestoneNames={"organization_shop"}`，让`Organization`据此创建一个挂了`Resource/Story/organization_shop.script`的`Script`，见`Core/society/organization.md`"Script配置"一节。
- `GetName()`必须全局唯一。

## 依赖关系

- 依赖：`society/organization_mod.h`。
- 被谁依赖：`basic.cpp`注册；`Core/society/organization.h`/`society.h`消费。

## 待办/后续阶段

- 无。

## 经济参数与服务
商店初始资金20万元（20000000分）、10个股东各占0.1、日薪100元。`CollectServices`遍历`stockTypes`，库存>0且有服务类型的货物，按零售价上报服务；`ApplyServiceUsage`直接扣`self.stock.<name>`。

## FactoryOrganization
`organization_factory`：占用1个`component_factory`（里面是一间工坊房间），经济参数与商店相同（初始资金20万、10个股东各占0.1、日薪100元）。目前没有岗位设计，后续阶段按房间容纳量的一半定员。
