# job_warehouse_keeper.h / job_warehouse_keeper.cpp

## 职责

`WarehouseKeeperJob`(`job_warehouse_keeper`)：job域的默认实现之一。

## 关键设计

- `WarehouseKeeperJob`没有脚本，仍在C++里：`DailyPlan`排`leave_home`(9:00)/`leave_work`(18:00)两个节点，`ExecNode`每次先清空并释放上一次的`changes`，再产出一个`NPCNavigateChange`。
- **导航目的地必须是具体房间地址**：`NPCNavigateChange`的destination要能被`Map::LocateRoom(address)`解析，不能写"home"/"workplace"这类描述性文本。Dependence层看不到`Citizen`/`Room`，所以`QueryAddress`通过`PostHandle`发`"citizen home address"`/`"citizen workplace address"`(实现见`Core/common/implement.cpp`)按姓名查字符串；查询失败(理论上不会发生)则这次调度作废，不产出任何`Change`。
- `GetName()`必须全局唯一：static计数器+id+现拼，计数器存档见`Dependence/common/mod_state.md`。

## 依赖关系

- 依赖：`society/job_mod.h`、`story/change.h`(`NPCNavigateChange`)、`common/json.h`。

## 待办/后续阶段

- 无。
