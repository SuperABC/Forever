# puzzle_fault.h

## 职责

故障注入用的小游戏mod（id `puzzle_fault`）。不是给玩家玩的，只给CoreTest的`fault`套件用：验证mod dll里抛出的异常能被
宿主正确接住、归到"mod错误"、并指名到这个mod。

## 关键设计

- **出错方式由`SetParam`指定**，在`Init`里出错：
  - `"std"`：抛`std::runtime_error`（不是`ExceptionBase`的异常，宿主靠"当时在这个mod里"判定为mod错误）。
  - `"data"`：用`THROW_EXCEPTION(DataException, ...)`抛（抛的是本dll自己那份Dependence里的异常类）。
  - `"unknown"`：`throw 42`（不是`std::exception`）。
  - 其它（包括不给参数）：什么都不做。ModTest的通用检查走的是这条路，所以照常通过。
- **消息故意比15个字符长**：短字符串存在对象内部，长的才在堆上分配，"一侧分配、另一侧释放"的问题只有长字符串才暴露。
- `Loop`恒返回`nullptr`，没有画面。
- 默认不启用（`config.json`的`puzzle_mods`里没有它）；`fault`套件在子进程里临时启用。

## 依赖关系

- 依赖：`player/puzzle_mod.h`、`common/error.h`（都在Dependence）。
- 被谁依赖：`Source/Test/Core/suite_fault.cpp`。登记在`mod_test.cpp`的`GetModPuzzles`/`RegisterModPuzzles`。

## 待办/后续阶段

- 无。
