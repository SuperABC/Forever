# component_port.h / component_port.cpp

## 职责

`PortComponent`(`component_port`)：component域的默认实现之一，港口建筑(`building_port`)里小房间归入的组合。

Component域的默认实现，无字段、无逻辑的trivial占位。

## 关键设计

- 只有`GetId()`/`GetType()`/`GetName()`和命名计数器(`count`/`id`)。`GetName()`按`<类名><id>`现拼，保证全进程唯一(见`CONVENTIONS.md` §5)。
- `GetInstanceCounter()`/`SetInstanceCounter()`供存档保存/恢复`count`，见`Source/Dependence/common/mod_state.md`。
- `.cpp`里只有计数器定义、构造函数和`GetName()`，没有其它逻辑；保留它是为了满足"每个`.h`配一份`.cpp`"的约定。

## 依赖关系

- 依赖：`Source/Dependence/map/component_mod.h`(`ComponentMod`基类)。
- 被谁依赖：`Source/Basic/basic.cpp`(`RegisterModComponents`)；`building_port.cpp`里`Layout()`以字符串形式引用`GetId()`(Basic层文件间不互相`#include`)。
