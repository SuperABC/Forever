# room_shop.h / room_shop.cpp

## 职责

`ShopRoom`(`room_shop`)：商店营业room。

Room域的默认实现。`SetProperty()`里按类型声明房间的功能属性(`isResidence`/`isWorkspace`/`isStorage`/`isManufacture`/`isParking`及对应容量、房租、停车位、门规格)，没声明的属性保持`RoomMod`基类默认值。

## 关键设计

- **`ShopRoom`的门有名字"商店大门"、挂`door_shop_entrance`脚本**(开局加"敲门"选项，靠近会弹交互选项)。挂在房间门而不是`CommerceBuilding::doorSpecs["entrance"]`上，是因为商店用的一楼模板(`preset_lobby_linear_fg+`/`preset_circle_double_fg+-`)里门标签全是`room`，没有`entrance`，`CommerceBuilding`的`entrance`规格对不上任何门。
- 带`isWorkspace`的房间(商店/仓库/工坊)，Organization按容纳量一半定员。
- 三种工坊房间由`IndustryBuilding`等概率随机选一种；生产类型是`ManufactureMod`的id，Industry按它为房间生成`Manufacture`。

## 依赖关系

- 依赖：`Source/Dependence/map/room_mod.h`(`RoomMod`基类)。
- 被谁依赖：`Source/Basic/basic.cpp`(`RegisterModRooms`注册全部类型，`ShopRoom`依赖`script_empty`做门脚本)；`building_*.cpp`里各`Layout()`按房间类型调`ArrangeRow`/`AssignRoom`；Society域`ShopSalerJob`依赖`ShopRoom`。

## 待办/后续阶段

- 无。
