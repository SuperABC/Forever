# organization_restaurant.h / organization_restaurant.cpp

## 职责

`RestaurantOrganization`(`organization_restaurant`)：餐厅组织，一个组织认领一个`component_restaurant`(即一个餐厅房间)。

## 关键设计

- `GetRequirements()`：`{component_restaurant, {1,1}}`；`GetPower()`=1。`CommerceHighBuilding`一楼每个餐厅房间各是一个独立Component(`ArrangeRowEach`)，所以每间餐厅一个组织。
- 岗位：`DesignJobsForRoom`只对`room_restaurant`按容纳量配`job_restaurant_waiter`；`MonthlySettle`同商店(增发、按利润扩缩服务员)。
- 经济参数同商店：初始资金20万元、10个股东各占0.1、日薪100元；`scriptModName="script_empty"`+milestone `organization_restaurant`(`Resource/Story/organization_restaurant.script`，空的game_start)，有Script才会被`Economy::RunServices`当作卖家。
- 服务：`CollectServices`只上报`self.stock.<type>`>0且服务类型为`food`的货物(汉堡)，**单价=商品零售价x2(汉堡10000)、质量固定0.5**(不看商品自身质量)，容量=库存(库存来自房间`storage_restaurant`，由物流从汉堡厂补)；`ApplyServiceUsage`扣`self.stock.<name>`。餐厅不开放玩家买汉堡，玩家侧买餐厅food服务以后再做。
- `GetName()`必须全局唯一：static计数器+id+现拼。

## 依赖关系

- 依赖：`society/organization_mod.h`、`organization_common.h`、`common/json.h`。
- 被谁依赖：`basic.cpp`注册(依赖`component_restaurant`/`room_restaurant`/`job_restaurant_waiter`/`script_empty`)；`Core/society/organization.h`/`society.h`消费。

## 待办/后续阶段

- 玩家在餐厅购买food服务(Core API/UI)，玩家家庭按钱选超市汉堡或餐厅服务。
