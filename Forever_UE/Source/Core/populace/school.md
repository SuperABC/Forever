# school.h / school.cpp

## 职责

虚拟学校/班级——项目里之前完全没有学校实体（老工程`Populace::GenerateEducations`里的
`SchoolClass`只是生成函数内部的临时局部变量，模拟完就丢弃，从未成为可查询的游戏对象）。
这次要求"先构建虚拟学校和班级实体来用"，`SchoolClass`落地成一个真正的、被`Populace`
持有的Core实体，供同学关系生成用。

## 关键设计

- **没有Mod/Factory背书的纯Core实体**——写法照抄`Citizen`（`citizen.md`："自己没有
  factory/工厂查表机制"）：一个POD式的类，只存"名字+学历阶段+起始年份+学生列表"，没有
  `XxxMod*`/`XxxFactory*`成员，不参与`Registry`的20个concept注册。由
  `Populace::GenerateEducations()`按需`new`，存进`Populace::schoolClasses`
  （`vector<SchoolClass*>`），`~Populace()`统一delete。
- **"虚拟"体现在没有对应的Building/Room实体**——`SchoolClass`不关联任何`Building`/
  `Room`，纯粹是"哪些人在同一届同一班"这条分组事实的载体，不参与地图/建筑系统。以后如果
  要给学校配上真正的建筑物，需要另外设计，这次范围只到"够用来生成同学关系"为止。
- **一个班级只做"起始年份相同、学历阶段相同"的分组**，不模拟老工程那套逐年转学/留级/
  跳级——`Populace::GenerateEducations()`按`(EDUCATION_LEVEL, startYear)`分组，超过
  `kMaxClassSize`（35人）就新开一个班，见`populace.md`"四类人际关系生成"一节。
- **`GetStudents()`不持有学生的所有权**——`Citizen*`列表只是引用，真正的所有权在
  `Populace::citizens`。

## 依赖关系

- 依赖：`class.h`（`Citizen`前向声明）。
- 被谁依赖：`Source/Core/populace/populace.h/.cpp`（`schoolClasses`成员、
  `GenerateEducations()`生成/析构）、`Source/Core/populace/experience.h`
  （`EducationExperience::GetSchoolClass()`不持有所有权的引用）。
