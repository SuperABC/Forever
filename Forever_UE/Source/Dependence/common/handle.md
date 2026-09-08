# handle.h / handle.cpp

原样移植自旧工程`E:\Projects\Forever_UE\Source\Dependence\common\handle.h/.cpp`，未做任何
修改。`PostHandle`是定义在Dependence层、供Mod使用的"向Core发起查询"抽象接口——`Post(request)`
提交一个`JsonValue`请求，`GetResult()`取回结果的**引用**（不是返回值），避免跨模块传递返回值
对象时触发"由哪个模块的分配器释放"的问题（呼应`REFACTOR_PLAN.md`的"跨模块new/delete安全"
约定：结果对象的生命周期留在提供查询的一侧管理，Mod侧只读引用，不持有、不释放）。

## 依赖关系

- 依赖：`JsonValue`（前置声明，实际定义在`common/json.h`）。
- 被谁依赖：目前没有代码实现或使用`PostHandle`——旧工程里这是Mod向宿主查询Core状态（如某个
  建筑当前是否存在）的通道，具体哪个系统会用到、由谁实现`Post`/`GetResult`，留到阶段4对应
  domain迁移、确认Mod侧需要反向查询Core数据时再接入，目前只是把接口骨架先搭好。
