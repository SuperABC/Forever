# room_residence.h / room_residence.cpp

## 职责

`ResidenceRoom`(`room_residence`)：Room域默认内容：各类型的属性和设计见room_*.md。

Room域的默认实现。`SetProperty()`里按类型声明房间的功能属性(`isResidence`/`isWorkspace`/`isStorage`/`isManufacture`/`isParking`及对应容量、房租、停车位、门规格)，没声明的属性保持`RoomMod`基类默认值。

## 关键设计

- **`ResidenceRoom`**：每间住宅room=1个名额，多人合住靠`Map::Checkin()`的名额池逻辑，不靠这个capacity强制。
- **`ResidenceRoom`登记`doorSpecs["home"]`**(读卡器门，网格/动画蓝图路径直接写在`SetProperty()`里)：高密度住宅四种布局(wing/side/twin/nshape)的所有 single/row 门洞标签统一是`home`；corridor 的门洞没有标签，仍是缺口。其余房间类型仍用 `doorSpecs["room"]`。
- 带`isWorkspace`的房间(商店/仓库/工坊)，Organization按容纳量一半定员。
- 三种工坊房间由`IndustryBuilding`等概率随机选一种；生产类型是`ManufactureMod`的id，Industry按它为房间生成`Manufacture`。

## 依赖关系

- 依赖：`Source/Dependence/map/room_mod.h`(`RoomMod`基类)。

## 待办/后续阶段

- 无。
