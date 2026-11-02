# name_mod.h / name_mod.cpp

## 职责

`NameMod`：一个姓名生成算法的具体实现(比如"chinese")，三个纯虚方法构成取名接口：`GetSurname`(从完整姓名截取姓)、`GenerateName`(全随机)、`GenerateName(surname, ...)`(给定姓只生成名)。

## 关键设计

- **必须是"传一个set结果的回调进去"的写法，不能按值返回`std::string`**：mod侧构造的临时`std::string`按值返回穿过DLL边界是被禁止的跨DLL模式(SSO会把问题掩盖到字符串超过15字节)。这里用`std::function<void(const std::string&)>`按`const&`传入mod侧重写的虚方法，mod内部构造的临时字符串全程在mod自己编译的代码里构造/使用/析构，跨边界的只有回调对`const string&`参数的读(拷贝进调用方自己的变量)。和`RoadnetMod::DistributeRoadnet`/`TerrainMod::DistributeTerrain`在用的回调传参是同一类安全模式。
- `GetSurname`：假定姓氏总是姓名的第一个UTF-8字符，不同取名算法(英文名等)可以按自己的规则重写；截取失败(比如空字符串)时不调用`setResult`。
- `GenerateName`的`allowMale`/`allowFemale`/`allowNeutral`是"可以从男/女/中性三个候选名词库里各自抽取"的**独立开关**(不是互斥的性别选择，一个名字可以混合多个词库)，三个都是false时由具体实现自己决定兜底；生成失败(候选词库为空)时不调用`setResult`。
- 给定姓只生成名：用于新生儿继承父亲的姓时复用同一套取名逻辑。

## 依赖关系

- 依赖：`common/mod_state.h`、`<functional>`。
- 被谁依赖：`name_factory.h`创建它；`Core/populace/name.h`/`populace.h`消费它；具体实现在`Source/Basic/populace/name_chinese.*`。

## 待办/后续阶段

- 无。
