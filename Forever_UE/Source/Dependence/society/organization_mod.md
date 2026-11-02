# organization_mod.h / organization_mod.cpp

## 职责

`OrganizationMod`：一种组织类型(比如商店)的行为定义——需要哪些`Component`、每个工位房间配什么`Job`。

## 关键设计

- **Dependence层看不到`Job`/`Component`/`Room`/`Citizen`这些Core类**(Core依赖Dependence，反过来不行)，所以接口只用字符串/int/`Time`/`Change*`表达"要什么""配什么"，真正的`Component`/`Room`遍历和`new Job(...)`全部在Core侧的`Organization`类里做，见`Core/society/organization.md`。
- **`GetPower()`**是static，通过`OrganizationFactory::RegisterOrganization`的`PowerFunc`登记，不是mod实例方法：`Society::Init`要对着每个`Component`实例x每种已注册组织类型做加权判断，不能为每个组合构造实例。
- **`ComponentRequirements()`**：写进`this->requirements`(`Component`类型 -> [min, max]数量范围)，可以列多种类型，也可以对同一种类型要多个。
- **`DesignJobsForRoom(...)`**：Core侧`Organization`遍历已认领`Component`的每个workspace房间，每个房间调一次(调用前Core先清空`vacancies`)，把这个房间的完整上下文(组合类型/名字、房间类型/名字、工位数`workspaceCapacity`)喂给mod；mod自己决定在这个房间配几个、什么类型的Job(可以混着配)，把job类型字符串逐条push进`this->vacancies`；Core读完后按顺序`new Job(...)`，position都设成这个房间。不需要配Job就不push。
- **`DailyPlan`/`ExecNode`**可选(默认空实现)：大部分组织不需要自己的调度(比如发工资)；真要用的具体组织类型重写，直接写自己的`plans`/`changes`成员字段，和`JobMod`同一个模式、接口保持对称。
- **Script配置**：`scriptModName`/`milestoneNames`，和`JobMod`同一组字段/同一套安全模式；组织也能挂一个自己的`Script`。

## 依赖关系

- 依赖：`common/mod_state.h`、`common/utility.h`、`common/handle.h`、`Change`(前向声明)。
- 被谁依赖：`organization_factory.h`创建它；`Core/society/organization.h`/`society.h`消费它；具体实现在`Source/Basic/society/organization_basic.*`。

## 待办/后续阶段

- 无。
