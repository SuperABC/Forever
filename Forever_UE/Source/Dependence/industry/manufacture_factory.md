# manufacture_factory.h / manufacture_factory.cpp

## 职责

`ManufactureFactory`是`ManufactureMod`(Manufacture(工坊))的注册表/工厂：负责创建、销毁、枚举已启用的Manufacture(工坊)类型。

## 关键设计

- **最小注册表**：没有`Temp`暂存区+`MergeTemp()`两段式注册——全部公开方法(含析构函数)都是`virtual`，调用永远落在构造这个Factory实例的那一侧，registries内部节点的分配和析构使用同一侧的堆分配器，不会跨DLL堆损坏。详见`Source/Dependence/README.md`"关键设计"一节。
- **`creator`/`deleter`是裸函数指针，不用`std::function`**：mod侧注册的都是无捕获lambda，天然可以转成函数指针；`std::function`内部堆缓冲由一侧分配、另一侧释放，实测会在Factory析构时崩溃。
- **`DestroyManufacture`必须走注册时mod提供的`deleter`**，不能Factory直接`delete`：通过`liveInstances`反查实例对应的注册id再取出`deleter`调用，保证跨DLL的`new`/`delete`成对。
- **"启用"的含义**：已注册且在`config.json`的`manufacture_mods`数组里列出(即`configuredArgs`里有这个id)才算启用，`CreateManufacture`/`CheckRegistered`/`GetRegisteredIds`都据此判断，见`Source/Core/story/script.md`"mod_dependences"一节。
- **参数传递**：调用方在`RegisterConcept`之前调用`SetModArgs`，把`config.json`数组解析出的(id, 参数字符串)表交给Factory，一直保留在`configuredArgs`里；`CreateManufacture`直接按id查出参数字符串传给`creator`，由mod自己的`creator`决定怎么用，Factory不替mod调用任何"创建后初始化"钩子。
- **`GetLiveInstances()`**：返回当前存活实例和它们的注册id，存档收集各mod的命名计数器/状态版本用，见`Source/Dependence/common/mod_state.md`。

## 依赖关系

- 依赖：`industry/manufacture_mod.h`。
- 被谁依赖：`Source/Core/common/registry.h`的`Registry`持有唯一实例，由各域管理类通过引用使用——`Core/industry/industry.h`(`Industry::manufactureFactory`引用)、`Core/industry/manufacture.h`。

## 待办/后续阶段

- 无。
