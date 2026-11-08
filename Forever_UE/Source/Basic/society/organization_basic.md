# organization_basic.h / organization_basic.cpp

## 职责

两个默认组织：`ShopOrganization`(`organization_shop`，商店)和`FactoryOrganization`(`organization_factory`，工厂)。

## 关键设计

- `GetRequirements()`(static，随注册交给工厂，不需要实例)：商店要求1~2个`component_shop`(组织随机占1~2个商店Component)，工厂要求1个`component_factory`(里面是一间工坊房间)。`static float GetPower()`返回1.f，作为`PowerFunc`随`RegisterOrganization`注册，供`Society::Init`做加权判断。
- **月度结算**`MonthlySettle(state)`（两个组织共用一组辅助函数）：缺钱增发——账户为正但低于初始资金30%、总股本不到初始4倍时`issueShares=totalShares/5`；岗位就地调`vacancies`——吃紧（余额低于初始资金30%，单月亏损不算）就把主力岗位计划裁掉约10%；岗位已满且（当月利润高于全组织一个月人力成本的5%，或账户高出初始资金的部分超过一个月人力成本）时扩招约20%，新增人手一个月工资不超过预算（当月利润的50%与账户高出初始资金部分的1/12取大者），每个房间的计划人数在满编的50%~200%之间；商店的仓库管理员单独处理（平时满编，吃紧时缩到一半）。阈值是初值，等跑出效果再调。
- **岗位设计**`DesignJobsForRoom(..., reset)`：`reset`为true时先`vacancies.clear()`(必须由mod自己清空，见`organization_mod.h`)，再往`vacancies`追加`{房间名, 岗位类型}`，每个房间按容纳量满编定员(`workspaceCapacity`个，容纳量0则不配)；超编由月结扩招实现。商店：`room_shop`配`job_shop_saler`，`room_warehouse`配`job_warehouse_keeper`；工厂：`room_farm`/`room_ranch`/`room_food_factory`配`job_factory_worker`。
- 经济参数两者相同：初始资金20万元(20000000分)、10个股东各占0.1、日薪100元。
- 商店构造里设`scriptModName="script_empty"`+`milestoneNames={"organization_shop"}`，让`Organization`据此创建一个挂了`Resource/Story/organization_shop.script`的`Script`，见`Core/society/organization.md`"Script配置"一节；工厂没有脚本。
- 商店服务：`CollectServices`遍历`stockTypes`，`self.stock.<type>`库存>0的货物，通过`post`查`"product info"`，服务类型为空的(小麦/牛肉)跳过，其余按零售价(固定，不参与市场调控)、服务质量、容量=库存上报`ServiceOffer`；`ApplyServiceUsage`直接扣`self.stock.<name>`(不低于0)。
- `GetName()`必须全局唯一：static计数器+id+现拼。

## 依赖关系

- 依赖：`society/organization_mod.h`、`common/json.h`。
- 被谁依赖：`basic.cpp`注册(商店依赖`job_shop_saler`/`job_warehouse_keeper`/`script_empty`，工厂依赖`job_factory_worker`)；`Core/society/organization.h`/`society.h`消费。

## 待办/后续阶段

- 无。
