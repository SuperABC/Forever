# component_basic.h/.cpp

## 职责

`ResidenceComponent`/`ShopComponent`/`FactoryComponent`是Component域三个默认实现，均为
trivial占位，合在同一份`component_basic.h/.cpp`里(每个concept统一一份
`<concept>_basic.h/.cpp`，不按业务场景拆文件)。

## 关键设计

三个类各自只有`GetId()`/`GetType()`/`GetName()`三个身份接口，没有任何字段/构造逻辑，
`.cpp`只有一行`#include "component_basic.h"`(没有out-of-line实现，见`CONVENTIONS.md`
"每个concept的`.h`必须配一份同名`.cpp`"一条)。

## 依赖关系

- 依赖：`Source/Dependence/map/component_mod.h`(`ComponentMod`基类)。
- 被谁依赖：`Source/Basic/basic.cpp`(`RegisterModComponents`注册三个类型)、
  `building_basic.cpp`(`Layout()`里`kComponent`常量按类型分别引用三个`GetId()`字符串)。
