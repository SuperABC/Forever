# component.h / component.cpp

## 职责

`Component`(组合)：若干`Room`的集合，用来表达"一个公司/组织在一栋building里占据的一片
连续或不连续的房间"（比如某公司租了写字楼的半层，这半层楼的所有房间就是一个`Component`）。
一个`Building`可以有多个`Component`（比如同一栋写字楼里租给不同公司的不同楼层）。

## 关键设计

- **`Component`只绑定在单个`Building`内部**，不跨building；跨building的撮合由`Organization`
  （`Core/society/organization.h`，持有若干`Component*`且不持有生命周期）负责，不需要改动`Component`自己的字段。
- **创建时机**：`Building::Layout()`里`AssignRoom`/`ArrangeRow`记录的`(component名字,id)`
  是一个分组key，真正实例化`Room`之前先按这个key去重创建`Component`（同一个
  `(component,id)`只创建一次，后续`Room`都挂到同一个`Component`上），`Component`归属
  `parentBuilding`，不做任何跨building撮合。
- **`ComponentMod`极简，只有`GetType()`/`GetName()`**：和`RoomMod`同理，`Component`不参与
  地块竞争，不需要`Assign`/`RandomAcreage`这套static注册机制——`GetType()`返回的字符串
  就是`AssignRoom`/`ArrangeRow`调用时传的`component`参数（比如`"office"`/`"warehouse"`，
  `Organization`按这个类型名撮合）。
- **独占持有一个`ComponentMod`实例**，和`Room`同一个模式：构造时创建、析构时
  `factory->DestroyComponent(mod)`；`Component`对象本身由`Building`统一`new`/`delete`。
- **`rooms`不持有生命周期**：`Room`由`Building`自己的`rooms`数组统一持有/销毁，
  `Component::rooms`只是一份引用列表（`AddRoom`时`push_back`，不`delete`）。
- **`Component`不参与寻址体系**（没有`GetAddress`，不像`Room`/`Building`/`Zone`），只能通过`Room`/`Building`间接找到。

## 依赖关系

- 依赖：`Source/Dependence/map/component_mod.h`（`ComponentMod`）、`component_factory.h`。
- 被谁依赖：`Source/Core/map/building.h`/`.cpp`（`Building::Layout()`按`(component,id)`
  去重创建，`~Building()`销毁）、`Source/Core/map/room.h`（`Room::parentComponent`）、`Source/Core/society/organization.h`。

## 待办/后续阶段

- 无。
