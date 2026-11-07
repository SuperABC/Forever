# error.h / error.cpp

## 职责

项目统一的异常体系：基类`ExceptionBase`（继承`std::exception`，带消息、文件、行号、函数名）、一组按语义分类的派生类，
以及抛异常的`THROW_EXCEPTION`宏。

## 关键设计

- **派生类**：`NullPointerException`（访问空指针）、`CommandException`（命令行输入有误）、`InvalidArgumentException`
  （函数参数有误）、`IOException`、`OutOfRangeException`（数组访问越界/字段范围不合法）、`DeadLoopException`、
  `NetworkException`、`MemoryException`、`JsonFormatException`、`RuntimeException`（其他）。都只是用`using`继承构造函数，
  没有额外字段，靠类型区分语义。
- **`THROW_EXCEPTION(类, 消息)`**自动带上`__FILE__`/`__LINE__`/`__FUNCTION__`；模板函数`ThrowException`标`[[noreturn]]`。
- **`what()`返回缓存**：构造时由`FormatWhat()`拼好"消息 [at 文件:行] in 函数"存进`whatCache`，`what()`直接返回缓存的
  `data()`；`GetDetailedInfo()`则每次现拼多行文本。派生类可重写`FormatWhat()`。
- 异常对象带`std::string`成员，只应在同一个模块内抛出并捕获，不要让它穿过DLL边界（CONVENTIONS.md §4）。

## 依赖关系

- 依赖：只有标准库。
- 被谁依赖：`json.cpp`（`JsonFormatException`）、`utility.cpp`（`Time`字段校验抛`OutOfRangeException`/
  `InvalidArgumentException`）、`Core/common/config.cpp`等。

## 待办/后续阶段

- 无。
