# job_basic.h / job_basic.cpp

## 职责

三个默认岗位：`ShopSalerJob`(`job_shop_saler`，店员，9:00出门/18:00回家)、`WarehouseKeeperJob`(`job_warehouse_keeper`，商店仓库管理员，9:00/18:00)、`FactoryWorkerJob`(`job_factory_worker`，工坊工人，8:00/17:00)。

## 关键设计

- **脚本驱动 vs C++驱动**：`ShopSalerJob`和`FactoryWorkerJob`的上下班、到岗选项、玩家交互(工人还有小游戏发工资)都在`Resource/Story/job_shop_saler.script`/`job_factory_worker.script`里(计时器+`npc_navigate`)，`SetProperty`设`scriptModName="script_empty"`+`milestoneNames`让`Job`据此创建挂了脚本的`Script`，`DailyPlan`留空(`plans`保持为空)；改上下班时间改脚本里的time。见`Core/society/job.md`。
- `WarehouseKeeperJob`没有脚本，仍在C++里：`DailyPlan`排`leave_home`(9:00)/`leave_work`(18:00)两个节点，`ExecNode`每次先清空并释放上一次的`changes`，再产出一个`NPCNavigateChange`。
- **导航目的地必须是具体房间地址**：`NPCNavigateChange`的destination要能被`Map::LocateRoom(address)`解析，不能写"home"/"workplace"这类描述性文本。Dependence层看不到`Citizen`/`Room`，所以`QueryAddress`通过`PostHandle`发`"citizen home address"`/`"citizen workplace address"`(实现见`Core/common/implement.cpp`)按姓名查字符串；查询失败(理论上不会发生)则这次调度作废，不产出任何`Change`。
- `GetName()`必须全局唯一：static计数器+id+现拼，计数器存档见`Dependence/common/mod_state.md`。

## 依赖关系

- 依赖：`society/job_mod.h`、`story/change.h`(`NPCNavigateChange`)、`common/json.h`。
- 被谁依赖：`basic.cpp`注册(`ShopSalerJob`依赖`room_shop`和`script_empty`)；`ShopOrganization::DesignJobsForRoom`/`FactoryOrganization::DesignJobsForRoom`按id配岗；`Core/society/job.h`消费。

## 待办/后续阶段

- 无。
