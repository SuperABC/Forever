# mod_test

## 职责
21个concept的mod基类各有一个纯虚`bool UnitTest(TestPrintFn print)`，自动化测试管线(`Source/Test/Core`的mods套件)对每个已注册的mod创建实例并调用它，返回false即该mod测试失败。

## 关键设计
- `TestPrintFn`是裸函数指针`void(*)(const char*)`：函数体在宿主一侧，参数只有`const char*`，不跨DLL传std容器/std::string(见CONVENTIONS第4节)。
- `FOREVER_MOD_GENERIC_UNIT_TEST("<concept>")`放在具体mod类体里，展开成通用身份检查(`CheckModIdentity`：type以`<concept>_`开头、name非空)。要写深度测试的mod，把这一行换成自己的`UnitTest`实现。
- 宿主调`UnitTest`前要和Core包装类调其他虚函数一样包`RandomScope(RandomRegistry::GetInstanceStream(mod))`，测试里的`GetRandom`才受种子控制。
- 基类纯虚会改变vtable：Basic、所有外部mod DLL必须一起重编(`build.bat`已覆盖)。

## 依赖关系
- 依赖：无。被21个`<concept>_mod.h`包含，被Basic/外部mod的类体使用。
- 测试管线：`Source/Test/Core/suite_mods.cpp`。

## 待办/后续阶段
- 逐concept写深度测试(地形/路网/园区/建筑/房间/组织/岗位/脚本/puzzle/app/武器等)，替换通用宏。
