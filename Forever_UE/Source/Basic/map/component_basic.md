# component_basic.h/.cpp

## 职责

`ResidenceComponent`/`ShopComponent`/`FactoryComponent`是Component域三个默认内容，全部
trivial占位（老工程对应的`InitComponent`逻辑本来就是空实现），合并进同一份
`component_basic.h/.cpp`(不再按residence/shop/plant各开一个文件，和`terrain_basic.h/.cpp`
里`OceanTerrain`/`MountainTerrain`合并的方式一样，见`Source/Basic/README.md`)。

三个类各自只有`GetId()`/`GetType()`/`GetName()`三个身份接口，没有任何字段/构造逻辑，
`.cpp`只有一行`#include "component_basic.h"`（没有任何out-of-line实现，见
`CONVENTIONS.md`"每个concept的`.h`必须配一份同名`.cpp`"一条）。

## 文件合并说明

原来这三个类型分别放在`component_residence.h`、`component_shop.h`、`component_plant.h`
三个文件里（`FactoryComponent`文件名避开和`Source/Dependence/map/component_factory.h`的
同名冲突）。这次合并成一份`component_basic.h/.cpp`之后不再需要为了避让`_factory`后缀而
单独起名，和`terrain_basic`/`roadnet_basic`保持同一个组织方式。

## 依赖关系

- 依赖：`Source/Dependence/map/component_mod.h`（`ComponentMod`基类）。
- 被谁依赖：`Source/Basic/basic.cpp`（`RegisterModComponents`注册三个类型）、
  `building_basic.cpp`（`Layout()`里`kComponent`常量按类型分别引用三个`GetId()`字符串）。
