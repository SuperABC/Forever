# room_airport.h / room_airport.cpp

## 职责

`AirportRoom`(`room_airport`)：room域的默认实现之一。

Room域的默认实现。`SetProperty()`里按类型声明房间的功能属性(`isResidence`/`isWorkspace`/`isStorage`/`isManufacture`/`isParking`及对应容量、房租、停车位、门规格)，没声明的属性保持`RoomMod`基类默认值。

## 关键设计

- 带`isWorkspace`的房间(商店/仓库/工坊)，Organization按容纳量一半定员。
- **`AirportRoom`的停机位**用房间局部坐标系(u轴沿正面墙、v轴朝正面，原点在房间角点，见`ParkingSpot`注释/`room.md`)，房间按朝向旋转时车位跟着转。u轴ratio相对房间角点：`kLeftAlong=-0.15`/`kRightAlong=1.15`落在房间沿正面墙两端的外面(0/1是房间边缘)，再各向外挪`kExtraOffset`(绝对距离，地图单位，不受房间尺寸影响)；v轴`0.5`和房间同一条中线。`rotationDegrees=180`即背对路、机头朝向房间深处的跑道。位置是估计值，常量在`AirportRoom::SetProperty`里，按实际机翼宽度、和跑道/楼体的间距调。
- 三种工坊房间由`IndustryBuilding`等概率随机选一种；生产类型是`ManufactureMod`的id，Industry按它为房间生成`Manufacture`。

## 依赖关系

- 依赖：`Source/Dependence/map/room_mod.h`(`RoomMod`基类)。

## 待办/后续阶段

- 无。
