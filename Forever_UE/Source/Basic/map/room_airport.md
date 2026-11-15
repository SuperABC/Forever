# room_airport.h / room_airport.cpp

## 职责

`AirportRoom`(`room_airport`)：room域的默认实现之一。

Room域的默认实现。`SetProperty()`里按类型声明房间的功能属性(`isResidence`/`isWorkspace`/`isStorage`/`isManufacture`及对应容量、房租、门规格)，没声明的属性保持`RoomMod`基类默认值。

## 关键设计

- 带`isWorkspace`的房间(商店/仓库/工坊)，Organization按容纳量一半定员。
- **`AirportRoom`**：`SetProperty()`为空。机位不再由房间定义：`building_airport.cpp`用`preset_airport_single_fg`布局，布局里有2个`parkings`矩形(机位，朝向/位置在布局编辑器里调)，`AssignParking(0,0,i,0.6f,{"plane"})`按0.6间距切分、只停`plane`。
- 三种工坊房间由`IndustryBuilding`等概率随机选一种；生产类型是`ManufactureMod`的id，Industry按它为房间生成`Manufacture`。

## 依赖关系

- 依赖：`Source/Dependence/map/room_mod.h`(`RoomMod`基类)。

## 待办/后续阶段

- 无。
