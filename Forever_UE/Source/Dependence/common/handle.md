# handle.h / handle.cpp

`PostHandle`是定义在Dependence层、供Mod使用的"向Core发起查询"抽象接口——`Post(request)`
提交一个`JsonValue`请求，`GetResult()`取回结果的**引用**（不是返回值），避免跨模块传递
返回值对象时触发"由哪个模块的分配器释放"的问题（结果对象的生命周期留在提供查询的一侧
管理，Mod侧只读引用，不持有、不释放）。

## 依赖关系

- 依赖：`JsonValue`（前置声明，实际定义在`common/json.h`）。
- 被谁依赖：`Core/common/implement.h`的`PostImplement`是具体实现，聚合全部7个domain指针，
  目前实现"random citizen"/"game time"/"citizen home address"/"citizen workplace
  address"四种查询，见`Core/common/implement.md`；`Dependence/story/script_mod.h`的
  `ScriptMod::WrapScript`带`PostHandle*`参数，`Forever_Mod/Empty/Cpp/Empty/
  empty_mods.h`的`EmptyScript::WrapScript`是调用`Post()`/`GetResult()`的mod侧代码
  示例。
