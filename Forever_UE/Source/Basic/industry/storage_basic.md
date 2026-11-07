# storage_basic.h / storage_basic.cpp

## 职责

工业系统的默认四个仓库：`WheatStorage`(`grain`，容量200)、`BeefStorage`(`meat`，容量200)、`BurgerStorage`(`food`，容量100)、`ShopStorage`(商店仓库，`food`，容量30)。

## 关键设计

- `SetProperty()`里填`categories`和`capacity`；`categories`分别对应`product_basic.md`里三个产品的`categories`，任一交集即可入库。
- `capacity`是共享容量池，不是每种产品各自的配额，见`Dependence/industry/storage_mod.md`。
- `ShopStorage`(`storage_shop`)是`room_warehouse`的`storageType`：`canTake=false`/`canPut=true`(只能放不能取，物流不能搬出去，货只被市民买走)，`serviceTypeDemand={food:1.0}`——市民买走汉堡后空出的容量就是向上游补货的需求。单个仓库房间容量30，一栋商店有很多仓库房间，全城总容量要覆盖日需求(每人1份)。见`Core/industry/industry.md`。
- `GetName()`全局唯一(static计数器+id+现拼)，计数器存档见`Dependence/common/mod_state.md`。

## 依赖关系

- 依赖：`industry/storage_mod.h`。
- 被谁依赖：`basic.cpp`注册；`Core/industry/storage.h`消费；`ShopStorage`被`WarehouseRoom`按`storageType`引用(见`map/room_basic.md`)。

## 待办/后续阶段

- `capacity`是占位测试值，按实际存量变化速度调整。
