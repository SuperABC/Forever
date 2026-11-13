# storage_restaurant.h / storage_restaurant.cpp

## 职责

`RestaurantStorage`(`storage_restaurant`)：`room_restaurant`的`storageType`，每个餐厅房间一点food库存。

## 关键设计

- `SetProperty()`：`categories={food}`(对应`product_burger`的`categories`)，`capacity=10`(单个餐厅房间的共享容量池)。
- `canTake=false`/`canPut=true`：只能放不能取。`Industry::Distribute`只把`canTake`的仓库当货源，所以餐厅之间不会互搬货，货只会被市民买走。
- `serviceTypeDemand={food:1.0}`：比例1.0=每天向汉堡厂补到容量满；市民买走后空出的容量就是向上游补货的需求，汉堡厂输出缓存腾出空间后次日继续生产，从而驱动生产。见`Core/industry/industry.md`。
- `GetName()`全局唯一(static计数器+id+现拼)，计数器存档见`Dependence/common/mod_state.md`。

## 依赖关系

- 依赖：`industry/storage_mod.h`。
- 被谁依赖：`basic.cpp`注册；`Core/industry/storage.h`消费；被`RestaurantRoom`按`storageType`引用。

## 待办/后续阶段

- `capacity=10`是占位测试值，按实际销量调整。
