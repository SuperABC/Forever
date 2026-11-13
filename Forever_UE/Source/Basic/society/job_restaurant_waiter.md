# job_restaurant_waiter.h / job_restaurant_waiter.cpp

## 职责

`RestaurantWaiterJob`(`job_restaurant_waiter`)：餐厅服务员，`organization_restaurant`给`room_restaurant`配的岗位；脚本只有上下班(`Resource/Story/job_restaurant_waiter.script`)，没有玩家交互选项(餐厅不卖汉堡，玩家买food服务留待以后)。

## 关键设计

- **脚本驱动 vs C++驱动**：`RestaurantWaiterJob`和`FactoryWorkerJob`的上下班、到岗选项、玩家交互(工人还有小游戏发工资)都在`Resource/Story/job_restaurant_waiter.script`/`job_factory_worker.script`里(计时器+`npc_navigate`)，`SetProperty`设`scriptModName="script_empty"`+`milestoneNames`让`Job`据此创建挂了脚本的`Script`，`DailyPlan`留空(`plans`保持为空)；改上下班时间改脚本里的time。见`Core/society/job.md`。
- **导航目的地必须是具体房间地址**：`NPCNavigateChange`的destination要能被`Map::LocateRoom(address)`解析，不能写"home"/"workplace"这类描述性文本。Dependence层看不到`Citizen`/`Room`，所以`QueryAddress`通过`PostHandle`发`"citizen home address"`/`"citizen workplace address"`(实现见`Core/common/implement.cpp`)按姓名查字符串；查询失败(理论上不会发生)则这次调度作废，不产出任何`Change`。
- `GetName()`必须全局唯一：static计数器+id+现拼，计数器存档见`Dependence/common/mod_state.md`。

## 依赖关系

- 依赖：`society/job_mod.h`、`story/change.h`(`NPCNavigateChange`)、`common/json.h`。
- 被谁依赖：`basic.cpp`注册(`RestaurantWaiterJob`依赖`room_shop`和`script_empty`)；`ShopOrganization::DesignJobsForRoom`/`ManufactureOrganization::DesignJobsForRoom`按id配岗；`Core/society/job.h`消费。

## 待办/后续阶段

- 无。
