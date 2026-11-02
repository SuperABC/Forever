# class.h

## 职责

Core层所有跨文件用到的**前向引用声明**集中地：按domain分组(`// Common`/`// Map`/`// Populace`/`// Society`/`// Story`/`// Industry`/`// Traffic`/`// Player`)声明`class Foo;`。

## 关键设计

- Core下其余`.h`/`.cpp`**不再自己写任何前向引用**，一律`#include "class.h"`(项目IncludePath已把`Core/common`加进去，不用带`common/`前缀)。新增一个需要前向引用的类型时，直接加进对应分组，不要在具体文件里就地声明——避免同一个类在多处重复前向声明、改名时漏改。
- 只有声明，没有对应的`.cpp`。

## 依赖关系

- 依赖：无。
- 被谁依赖：Core下几乎所有头文件。

## 待办/后续阶段

- 无。
