# room_office.h / room_office.cpp

## 职责

`OfficeRoom`(`room_office`)：办公室room：OfficeBuilding(三档密度)的房间。

Room域的默认实现。`SetProperty()`里按类型声明房间的功能属性(`isResidence`/`isWorkspace`/`isStorage`/`isManufacture`/`isParking`及对应容量、房租、停车位、门规格)，没声明的属性保持`RoomMod`基类默认值。

## 关键设计

- 带`isWorkspace`的房间(商店/仓库/工坊)，Organization按容纳量一半定员。
- 三种工坊房间由`IndustryBuilding`等概率随机选一种；生产类型是`ManufactureMod`的id，Industry按它为房间生成`Manufacture`。

## 依赖关系

- 依赖：`Source/Dependence/map/room_mod.h`(`RoomMod`基类)。

## 待办/后续阶段

- 无。
