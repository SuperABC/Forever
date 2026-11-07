# name_factory.h / name_factory.cpp

## 职责

`NameFactory`是`NameMod`(Name(姓名生成))的注册表/工厂：负责创建、销毁、枚举已启用的Name(姓名生成)类型。

## 关键设计

- **最小注册表**：没有`Temp`暂存区+`MergeTemp()`两段式注册——全部公开方法(含析构函数)都是`virtual`，调用永远落在构造这个Factory实例的那一侧，registries内部节点的分配和析构使用同一侧的堆分配器，不会跨DLL堆损坏。详见`Source/Dependence/README.md`"关键设计"一节。
- **`creator`/`deleter`是裸函数指针，不用`std::function`**：mod侧注册的都是无捕获lambda，天然可以转成函数指针；`std::function`内部堆缓冲由一侧分配、另一侧释放，实测会在Factory析构时崩溃。
- **`DestroyName`必须走注册时mod提供的`deleter`**，不能Factory直接`delete`：通过`liveInstances`反查实例对应的注册id再取出`deleter`调用，保证跨DLL的`new`/`delete`成对。
- **没有`IsEnabled`门槛**(与大多数Factory不同)：`CreateName`/`CheckRegistered`/`GetRegisteredIds`只看是否已注册；`CreateName`在`configuredArgs`里找不到参数时传空字符串给`creator`。"启用"由`SetConfig`单独表达。
- **参数传递**：调用方在`RegisterConcept`之前调用`SetModArgs`，把`config.json`数组解析出的(id, 参数字符串)表交给Factory，一直保留在`configuredArgs`里；`CreateName`直接按id查出参数字符串传给`creator`，由mod自己的`creator`决定怎么用，Factory不替mod调用任何"创建后初始化"钩子。
- **`GetLiveInstances()`**：返回当前存活实例和它们的注册id，存档收集各mod的命名计数器/状态版本用，见`Source/Dependence/common/mod_state.md`。
- 和`RoadnetFactory`同一个模式：一次只应该有一个取名算法生效，所以多了`SetConfig(id, enabled)`/`GetName()`——`SetConfig`记录某个注册id是否启用(`config.json`的`name_mods`数组里的条目)，`GetName`返回第一个启用的id(找不到返回空字符串)，由`Populace::InitNames`决定用哪个mod做取名算法。这里没有Temp暂存/合并的两段式注册。

## 依赖关系

- 依赖：`populace/name_mod.h`。
- 被谁依赖：`Source/Core/common/registry.h`的`Registry`持有唯一实例，由各域管理类通过引用使用——`Core/populace/populace.h`(`Populace::nameFactory`引用)、`Core/populace/name.h`。

## 待办/后续阶段

- 无。
