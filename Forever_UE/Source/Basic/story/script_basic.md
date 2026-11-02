# script_basic.h / script_basic.cpp

## 职责

`ScriptBasic`(`script_basic`)：纯占位的默认`ScriptMod`实现，没有真正的剧情内容。

## 关键设计

- Basic编译为运行时DLL，和`Forever_Mod`下的mod走同一套加载机制，见`Source/Basic/README.md`。
- 真正的"空脚本壳"是`Forever_Mod/Empty`里的`script_empty`(大量`Basic` mod依赖它)；这里只是让`Basic`自己也提供一个能被创建的`ScriptMod`。
- `GetName()`必须全局唯一：static计数器+id+`GetName`里现拼。

## 依赖关系

- 依赖：`story/script_mod.h`。
- 被谁依赖：`basic.cpp`注册；`Core/story/script.h`创建`Script`时通过`ScriptFactory`使用。

## 待办/后续阶段

- 无。
