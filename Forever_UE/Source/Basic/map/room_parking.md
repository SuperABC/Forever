# room_parking.h / room_parking.cpp

## 职责

`ParkingRoom`(`room_parking`)：Shop/Factory地下停车场room，ShopBuilding和FactoryBuilding共用。

Room域的默认实现。`SetProperty()`里按类型声明房间的功能属性(`isResidence`/`isWorkspace`/`isStorage`/`isManufacture`/`isParking`及对应容量、房租、停车位、门规格)，没声明的属性保持`RoomMod`基类默认值。

## 关键设计

- 带`isWorkspace`的房间(商店/仓库/工坊)，Organization按容纳量一半定员。
- **`ParkingRoom`**：车位种类`{"car","bus","truck"}`，供`Traffic::Init()`在这里生成预置车辆，见`Source/Core/traffic/traffic.md`；不想要某种车就从这个列表里删。`CommerceBuilding`和`IndustryBuilding`共用同一个`ParkingRoom`类。
- 三种工坊房间由`IndustryBuilding`等概率随机选一种；生产类型是`ManufactureMod`的id，Industry按它为房间生成`Manufacture`。

## 依赖关系

- 依赖：`Source/Dependence/map/room_mod.h`(`RoomMod`基类)。

## 待办/后续阶段

- 无。
