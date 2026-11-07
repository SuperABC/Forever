# organization_basic.h / organization_basic.cpp

## 职责

两个默认组织：`ShopOrganization`(`organization_shop`，商店)和`FactoryOrganization`(`organization_factory`，工厂)。

## 关键设计

- `GetRequirements()`(static，随注册交给工厂，不需要实例)：商店要求1~2个`component_shop`(组织随机占1~2个商店Component)，工厂要求1个`component_factory`(里面是一间工坊房间)。`static float GetPower()`返回1.f，作为`PowerFunc`随`RegisterOrganization`注册，供`Society::Init`做加权判断。
- **岗位设计**`DesignJobsForRoom`：每次先`vacancies.clear()`(必须由mod自己清空，见`organization_mod.h`)，再按房间容纳量的一半定员(`workspaceCapacity/2`，容纳量0或1则不配)。商店：`room_shop`配`job_shop_saler`，`room_warehouse`配`job_warehouse_keeper`；工厂：`room_farm`/`room_ranch`/`room_food_factory`配`job_factory_worker`。
- 经济参数两者相同：初始资金20万元(20000000分)、10个股东各占0.1、日薪100元。
- 商店构造里设`scriptModName="script_empty"`+`milestoneNames={"organization_shop"}`，让`Organization`据此创建一个挂了`Resource/Story/organization_shop.script`的`Script`，见`Core/society/organization.md`"Script配置"一节；工厂没有脚本。
- 商店服务：`CollectServices`遍历`stockTypes`，`self.stock.<type>`库存>0的货物，通过`post`查`"product info"`，服务类型为空的(小麦/牛肉)跳过，其余按零售价(固定，不参与市场调控)、服务质量、容量=库存上报`ServiceOffer`；`ApplyServiceUsage`直接扣`self.stock.<name>`(不低于0)。
- `GetName()`必须全局唯一：static计数器+id+现拼。

## 依赖关系

- 依赖：`society/organization_mod.h`、`common/json.h`。
- 被谁依赖：`basic.cpp`注册(商店依赖`job_shop_saler`/`job_warehouse_keeper`/`script_empty`，工厂依赖`job_factory_worker`)；`Core/society/organization.h`/`society.h`消费。

## 待办/后续阶段

- 无。
