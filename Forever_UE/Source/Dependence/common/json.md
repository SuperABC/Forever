# json.h / json.cpp

## 职责

JsonCpp衍生的宽松JSON解析/输出器（`JsonValue`/`JsonReader`/`FastWriter`/`StyledWriter`等），是整个项目（Dependence/Core/Basic/
Mod）共用的JSON实现；`Core/common/config`、存档的JSON段、`PostHandle`的请求/结果都用它。本文件按第三方衍生库对待，
不逐函数写文档。

## 关键设计

- **支持`//`和`/* */`注释**：选它而不是UE自带`Json`模块的原因见`Source/Core/common/config.md`——UE的`FJsonSerializer`是严格
  JSON，不支持注释，而`config.json`需要保留注释能力；同时纯C++、不依赖UE，Mod DLL也能用。
- **类型转换失败抛`JsonFormatException`**（`AsString`/`AsInt`/`AsUInt`/`AsDouble`/`AsBool`等），解析失败也抛它，
  异常体系见`error.md`。
- `FastWriter`输出紧凑格式，`StyledWriter`输出带缩进的可读格式。

## 依赖关系

- 依赖：`error.h`、标准库。
- 被谁依赖：`handle.h`（前置声明）、`Core/common/config.*`、`Core/common/save.*`、各domain里读写JSON的代码。

## 待办/后续阶段

- 无。
