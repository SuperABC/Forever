# CommandQueue.h

## 职责

`TCommandQueue<TCommand>`：载具/人物Controller共用的"**指令列表+当前执行到哪条**"，纯模板，header-only，没有`.cpp`。

## 关键设计

- Controller自己决定每条指令怎么执行，这里**只负责排队**：`Push`入队、`Current()`取当前(队空返回`nullptr`)、`Advance()`当前做完进入下一条、`Clear()`清空、`Num()`剩余条数(含当前这条)。
- 指令全部执行完(或`Clear`)后内部数组复位(`Reset`+游标归零)，数组不会无限增长。
- **注意**：`Advance()`/`Clear()`之后，之前拿到的`Current()`指针失效。

## 依赖关系

- 依赖：UE `TArray`。
- 被谁依赖：`AVehicleCommandController`(`FVehicleCommand`)、`ACitizenController`。

## 待办/后续阶段

- 无。
