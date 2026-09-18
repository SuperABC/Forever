# handle.h / handle.cpp

原样移植自旧工程`E:\Projects\Forever_UE\Source\Dependence\common\handle.h/.cpp`，未做任何
修改。`PostHandle`是定义在Dependence层、供Mod使用的"向Core发起查询"抽象接口——`Post(request)`
提交一个`JsonValue`请求，`GetResult()`取回结果的**引用**（不是返回值），避免跨模块传递返回值
对象时触发"由哪个模块的分配器释放"的问题（呼应`REFACTOR_PLAN.md`的"跨模块new/delete安全"
约定：结果对象的生命周期留在提供查询的一侧管理，Mod侧只读引用，不持有、不释放）。

## 依赖关系

- 依赖：`JsonValue`（前置声明，实际定义在`common/json.h`）。
- 被谁依赖：`Core/common/implement.h`的`PostImplement`是第一个具体实现（阶段4 Story域接入，
  聚合全部7个domain指针，目前只实现"random citizen"一种查询，见`Core/common/
  implement.md`）；`Dependence/story/script_mod.h`的`ScriptMod::WrapScript`新增了
  `PostHandle*`参数，`Forever_Mod/Empty/Cpp/Empty/empty_mods.h`的`EmptyScript::WrapScript`
  是第一个真正调用`Post()`/`GetResult()`的mod侧代码。
