# name_chinese.h / name_chinese.cpp

## 职责

`ChineseName`(`name_chinese`)：中文取名算法的具体实现——姓氏词库+男/女/中性名词库+取名逻辑。

## 关键设计

- 接口是"传一个set结果的回调进去"的写法，不按值返回`std::string`，原因见`Dependence/populace/name_mod.md`。
- `InitializeSurnames()`/`InitializeNames()`填`surnames`/`maleNames`/`femaleNames`/`neutralNames`四个词库；`GenerateName`按`allowMale`/`allowFemale`/`allowNeutral`合并候选再随机抽取；`GetSurname`取第一个UTF-8字符。

## 依赖关系

- 依赖：`populace/name_mod.h`。
- 被谁依赖：`basic.cpp`注册；`Core/populace/name.h`/`populace.h`消费，详见`Core/populace/populace.md`。

## 待办/后续阶段

- 无。
