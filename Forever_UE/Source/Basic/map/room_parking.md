# room_parking.h / room_parking.cpp

## 职责

`ParkingRoom`(`room_parking`)：Shop/Factory地下停车场room，ShopBuilding和FactoryBuilding共用。

Room域的默认实现。`SetProperty()`里按类型声明房间的功能属性(`isResidence`/`isWorkspace`/`isStorage`/`isManufacture`及对应容量、房租、门规格)，没声明的属性保持`RoomMod`基类默认值。

## 关键设计

- 带`isWorkspace`的房间(商店/仓库/工坊)，Organization按容纳量一半定员。
- **`ParkingRoom`**：只表示"这个房间语义上是停车场"，`SetProperty()`为空——不再有任何车位信息。车位几何来自layout的`parkings`，由各停车场建筑的`Layout()`用`BuildingMod::AssignParking`挂到这个房间上(间距/可停种类直接写在各停车场建筑的`Layout()`里)，Core生成`ParkingSlot`，见`Core/map/room.md`"停车位"。
- 三种工坊房间由`IndustryBuilding`等概率随机选一种；生产类型是`ManufactureMod`的id，Industry按它为房间生成`Manufacture`。

## 依赖关系

- 依赖：`Source/Dependence/map/room_mod.h`(`RoomMod`基类)。
