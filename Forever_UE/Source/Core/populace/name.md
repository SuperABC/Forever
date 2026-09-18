# name.h / name.cpp

## 职责

`Name`是某个具体`NameMod`实例的薄包装——构造时向`NameFactory`要一个实例（按id，如
`"chinese"`），把`GetType`/`GetName`/`GetSurname`/`GenerateName`（两个重载）原样转发出去，
析构时交还给Factory销毁。写法逐字照抄`Core/map/terrain.h/.cpp`的`Terrain`类。

## 补做原因：架构一致性

最初迁移Name这个concept时（见`populace.md`"姓名生成"一节）图省事，直接让`Populace`持有
`NameMod* nameMod`，跳过了Core层包装类这一步——这和其它concept的既有约定不一致：`Terrain`/
`Roadnet`/`Zone`/`Building`（Map域）、`Script`（Story域）等聚合类，从来不直接持有/调用
`<Concept>Mod*`，一律通过一个Core层的薄包装类（`Terrain`/`Script`等）转发。`Populace`直接摸
`NameMod*`是这条约定唯一的例外，被指出后按同一个模式补上了这个类，`Populace`现在只持有
`Name* name`，见`populace.md`同一节的"架构修正"说明。

## 依赖关系

- 依赖：`name_mod.h`、`name_factory.h`、`common/error.h`（`NullPointerException`）。
- 被谁依赖：`Source/Core/populace/populace.h/.cpp`（`Populace::InitNames()`构造
  `Name*`，`GenerateCitizens()`调`name->GenerateName(...)`/`name->GetSurname(...)`）。
