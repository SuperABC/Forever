# room_warehouse.h / room_warehouse.cpp

## 职责

`WarehouseRoom`(`room_warehouse`)：Shop布局里过道/地下室用的仓储room，只有ShopBuilding用到。

Room域的默认实现。`SetProperty()`里按类型声明房间的功能属性(`isResidence`/`isWorkspace`/`isStorage`/`isManufacture`及对应容量、房租、门规格)，没声明的属性保持`RoomMod`基类默认值。

## 关键设计

- 带`isWorkspace`的房间(商店/仓库/工坊)，Organization按容纳量一半定员。
- 三种工坊房间由`IndustryBuilding`等概率随机选一种；生产类型是`ManufactureMod`的id，Industry按它为房间生成`Manufacture`。

- **房间里摆10个汉堡**(2026-11-26，物品流式加载和捡起的测试内容)：`Layout`在房间中央按2排×5个、间距0.5米、贴地，`PlaceAsset`十次`asset_burger`(每个`count=1`，十个独立的物品)，位置夹在房间范围内。不用随机数。`asset_burger`没启用时Core跳过并打一条警告。这些汉堡和`storage_shop`的库存数字没有任何关系，捡走不扣库存、不付钱。

## 依赖关系

- 依赖：`Source/Dependence/map/room_mod.h`(`RoomMod`基类)。
