# component_mod.h / component_mod.cpp

## 职责

`ComponentMod`：一个"组合"类型的标签(比如"office"/"warehouse")，公司/组织以后按这个类型名撮合。

## 关键设计

- 和`RoomMod`同理：`Component`永远是`Building`自己在`Layout()`里通过`AssignRoom`/`ArrangeRow`(组合名字, id)间接创建的，**不参与地块竞争**，所以接口只有`GetType()`/`GetName()`两个虚方法，没有`Assign`/`RandomAcreage`/`GetPower`那套static注册机制。
- 带命名计数器的存档钩子，默认不存任何东西，见`common/mod_state.md`。

## 依赖关系

- 依赖：`common/mod_state.h`。
- 被谁依赖：`component_factory.h`创建它；`Core/map/component.h`/`building.md`消费它；组织(`OrganizationMod::ComponentRequirements`)按组合类型名撮合。

## 待办/后续阶段

- 无。
